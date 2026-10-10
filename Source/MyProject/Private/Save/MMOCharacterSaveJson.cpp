// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOCharacterSaveJson.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "JsonObjectConverter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/EnumProperty.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr TCHAR SchemaVersionField[] = TEXT("SchemaVersion");
	constexpr TCHAR DataField[] = TEXT("Data");

	// Boyut tavanlari (guvenilmeyen/bozuk blob'a karsi). Gercek bir karakter ~1-30 KB tutar.
	constexpr int32 MaxJsonChars = 2 * 1024 * 1024;
	constexpr int32 MaxArrayElements = 4096;
	constexpr int32 MaxMapEntries = 4096;
	constexpr int32 MaxNameChars = 256;
	constexpr int32 MaxStringChars = 1024;
	constexpr int32 MaxDepth = 6;
	// FJsonSerializer yapisal dogrulamadan once ayristirir ve derin girdide ozyineleme kullanir.
	// Guvenilmeyen bulut blob'unda yigin tasmasini onlemek icin ayristirmadan ONCE ham tavan.
	constexpr int32 MaxJsonNestingDepth = 16;
	// JSON sayilari double'dir: bundan buyuk tam sayilar v1'de (sayi olarak) guvenle tasinamaz.
	constexpr double MaxExactJsonInt = 9007199254740992.0;
	constexpr double MinInt64AsDouble = -9223372036854775808.0;
	constexpr double Int64UpperBoundAsDouble = 9223372036854775808.0;

	void SetError(FString* OutError, const FString& Message)
	{
		if (OutError)
		{
			*OutError = Message;
		}
	}

	bool Fail(FString& OutError, const FString& Path, const TCHAR* Message)
	{
		OutError = FString::Printf(TEXT("%s: %s"), Path.IsEmpty() ? TEXT("(kok)") : *Path, Message);
		return false;
	}

	bool IsIntegralFinite(double Value)
	{
		return FMath::IsFinite(Value) && Value == FMath::FloorToDouble(Value);
	}

	bool HasSafeJsonNesting(const FString& Json)
	{
		int32 Depth = 0;
		bool bInString = false;
		bool bEscaped = false;
		for (const TCHAR Ch : Json)
		{
			if (bInString)
			{
				if (bEscaped)
				{
					bEscaped = false;
				}
				else if (Ch == TEXT('\\'))
				{
					bEscaped = true;
				}
				else if (Ch == TEXT('"'))
				{
					bInString = false;
				}
				continue;
			}

			if (Ch == TEXT('"'))
			{
				bInString = true;
			}
			else if (Ch == TEXT('{') || Ch == TEXT('['))
			{
				if (++Depth > MaxJsonNestingDepth)
				{
					return false;
				}
			}
			else if (Ch == TEXT('}') || Ch == TEXT(']'))
			{
				Depth = FMath::Max(0, Depth - 1);
			}
		}
		return true;
	}

	// Yalniz [-]rakamlar, bas sifir/+/bosluk yok, int64'e sigar (Atoi64 gidis-donusu ile dogrulanir).
	bool ParseStrictInt64(const FString& Text, int64& OutValue)
	{
		const int32 Start = (Text.Len() > 0 && Text[0] == TEXT('-')) ? 1 : 0;
		if (Text.Len() <= Start || Text.Len() > 20)
		{
			return false;
		}
		for (int32 Index = Start; Index < Text.Len(); ++Index)
		{
			if (Text[Index] < TEXT('0') || Text[Index] > TEXT('9'))
			{
				return false;
			}
		}
		OutValue = FCString::Atoi64(*Text);
		return LexToString(OutValue) == Text;
	}

	bool ValidateEnum(const UEnum* Enum, const TSharedPtr<FJsonValue>& Value, const FString& Path, FString& OutError)
	{
		int64 EnumValue = INDEX_NONE;
		if (Value->Type == EJson::String)
		{
			// Birincil bicim: yazar adi (ad degisirse blob okunamaz; bkz. MMOCharacterSaveJson.h: enum politikasi).
			EnumValue = Enum->GetValueByName(FName(*Value->AsString()), EGetByNameFlags::CheckAuthoredName);
			if (EnumValue == INDEX_NONE)
			{
				return Fail(OutError, Path, TEXT("bilinmeyen enum adi"));
			}
		}
		else if (Value->Type == EJson::Number)
		{
			// Eski/elle yazilmis blob'lar icin sayisal deger tolere edilir (gecerli bir degere denk gelmeli).
			const double Number = Value->AsNumber();
			// double -> int64 donusumu aralik disinda tanimsizdir; cast'ten once acikca sinirla.
			if (!IsIntegralFinite(Number) || Number < MinInt64AsDouble || Number >= Int64UpperBoundAsDouble)
			{
				return Fail(OutError, Path, TEXT("gecersiz enum degeri"));
			}
			const int64 NumericEnumValue = static_cast<int64>(Number);
			if (!Enum->IsValidEnumValue(NumericEnumValue))
			{
				return Fail(OutError, Path, TEXT("gecersiz enum degeri"));
			}
			EnumValue = NumericEnumValue;
		}
		else
		{
			return Fail(OutError, Path, TEXT("enum icin ad (string) bekleniyor"));
		}
		const int32 Index = Enum->GetIndexByValue(EnumValue);
		// Metadata is absent in Game/Server targets. Keep the persisted enum policy
		// explicit at runtime; new enum types must be reviewed before saving them.
		const bool bAllowedValue = (Enum == StaticEnum<EMMOClass>()
			&& EnumValue >= static_cast<int64>(EMMOClass::Warrior) && EnumValue <= static_cast<int64>(EMMOClass::Priest))
			|| (Enum == StaticEnum<EMMOItemRarity>()
				&& EnumValue >= static_cast<int64>(EMMOItemRarity::Common) && EnumValue < static_cast<int64>(EMMOItemRarity::MAX));
		if (Index == INDEX_NONE || !bAllowedValue)
		{
			return Fail(OutError, Path, TEXT("gizli/gecersiz enum degeri"));
		}
		return true;
	}

	bool ValidateStruct(const UStruct* Definition, const TSharedPtr<FJsonObject>& Object, int32 Version,
		const FString& Path, int32 Depth, FString& OutError);

	// Tek bir JSON degerini hedef property'ye gore (tip, aralik, boyut) dogrular. Bilinmeyen property
	// tipinde reddeder: yeni alan eklenip burasi guncellenmezse gidis-donus testi hemen kirilir.
	bool ValidateValue(const FProperty* Property, const TSharedPtr<FJsonValue>& Value, int32 Version,
		const FString& Path, int32 Depth, FString& OutError)
	{
		if (!Value.IsValid() || Value->IsNull())
		{
			return Fail(OutError, Path, TEXT("deger eksik veya null"));
		}
		if (Depth > MaxDepth || Property->ArrayDim != 1)
		{
			return Fail(OutError, Path, TEXT("desteklenmeyen yapi"));
		}

		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			return ValidateEnum(EnumProperty->GetEnum(), Value, Path, OutError);
		}
		if (const FNumericProperty* Numeric = CastField<FNumericProperty>(Property))
		{
			if (const UEnum* Enum = Numeric->GetIntPropertyEnum())
			{
				return ValidateEnum(Enum, Value, Path, OutError);
			}
			if (Numeric->IsFloatingPoint())
			{
				if (Value->Type != EJson::Number || !FMath::IsFinite(Value->AsNumber()))
				{
					return Fail(OutError, Path, TEXT("sonlu sayi bekleniyor"));
				}
				return true;
			}
			if (CastField<FInt64Property>(Property))
			{
				if (Version >= 2)
				{
					int64 Parsed = 0;
					if (Value->Type != EJson::String || !ParseStrictInt64(Value->AsString(), Parsed))
					{
						return Fail(OutError, Path, TEXT("int64 icin ondalik-siz tam sayi metni (string) bekleniyor"));
					}
					return true;
				}
				if (Value->Type != EJson::Number || !IsIntegralFinite(Value->AsNumber())
					|| FMath::Abs(Value->AsNumber()) > MaxExactJsonInt)
				{
					return Fail(OutError, Path, TEXT("int64 (v1) icin 2^53 alti tam sayi bekleniyor"));
				}
				return true;
			}
			double Min = 0.0;
			double Max = 0.0;
			if (CastField<FIntProperty>(Property))
			{
				Min = static_cast<double>(MIN_int32);
				Max = static_cast<double>(MAX_int32);
			}
			else if (CastField<FByteProperty>(Property))
			{
				Max = 255.0;
			}
			else
			{
				return Fail(OutError, Path, TEXT("desteklenmeyen sayi tipi"));
			}
			if (Value->Type != EJson::Number || !IsIntegralFinite(Value->AsNumber())
				|| Value->AsNumber() < Min || Value->AsNumber() > Max)
			{
				return Fail(OutError, Path, TEXT("aralik disi veya tam sayi olmayan deger"));
			}
			return true;
		}
		if (CastField<FBoolProperty>(Property))
		{
			return Value->Type == EJson::Boolean ? true : Fail(OutError, Path, TEXT("bool bekleniyor"));
		}
		if (CastField<FNameProperty>(Property) || CastField<FStrProperty>(Property))
		{
			const int32 Limit = CastField<FNameProperty>(Property) ? MaxNameChars : MaxStringChars;
			if (Value->Type != EJson::String)
			{
				return Fail(OutError, Path, TEXT("string bekleniyor"));
			}
			return Value->AsString().Len() <= Limit ? true : Fail(OutError, Path, TEXT("string cok uzun"));
		}
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			if (Value->Type != EJson::Array)
			{
				return Fail(OutError, Path, TEXT("dizi bekleniyor"));
			}
			const TArray<TSharedPtr<FJsonValue>>& Elements = Value->AsArray();
			if (Elements.Num() > MaxArrayElements)
			{
				return Fail(OutError, Path, TEXT("dizi cok buyuk"));
			}
			for (int32 Index = 0; Index < Elements.Num(); ++Index)
			{
				if (!ValidateValue(ArrayProperty->Inner, Elements[Index], Version,
					FString::Printf(TEXT("%s[%d]"), *Path, Index), Depth + 1, OutError))
				{
					return false;
				}
			}
			return true;
		}
		if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
		{
			if (Value->Type != EJson::Object || !MapProperty->KeyProp
				|| !(CastField<FNameProperty>(MapProperty->KeyProp) || CastField<FStrProperty>(MapProperty->KeyProp)))
			{
				return Fail(OutError, Path, TEXT("nesne (harita) bekleniyor"));
			}
			const TSharedPtr<FJsonObject> Object = Value->AsObject();
			if (!Object.IsValid() || Object->Values.Num() > MaxMapEntries)
			{
				return Fail(OutError, Path, TEXT("harita cok buyuk"));
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Object->Values)
			{
				if (Entry.Key.IsEmpty() || Entry.Key.Len() > MaxNameChars)
				{
					return Fail(OutError, Path, TEXT("gecersiz harita anahtari"));
				}
				if (!ValidateValue(MapProperty->ValueProp, Entry.Value, Version,
					FString::Printf(TEXT("%s.%s"), *Path, *Entry.Key), Depth + 1, OutError))
				{
					return false;
				}
			}
			return true;
		}
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (Value->Type != EJson::Object || !Value->AsObject().IsValid())
			{
				return Fail(OutError, Path, TEXT("nesne bekleniyor"));
			}
			return ValidateStruct(StructProperty->Struct, Value->AsObject(), Version, Path, Depth + 1, OutError);
		}
		return Fail(OutError, Path, TEXT("desteklenmeyen property tipi"));
	}

	// SaveGame isaretli HER alan blob'da bulunmak zorunda (eksik alan sessizce varsayilana dusmesin);
	// blob'daki bilinmeyen ek alanlar yok sayilir (ileri uyumluluk). Yeni alan eklenirse sema surumu artar
	// ve bu islev o alani eski surumler icin istege bagli sayar.
	bool ValidateStruct(const UStruct* Definition, const TSharedPtr<FJsonObject>& Object, int32 Version,
		const FString& Path, int32 Depth, FString& OutError)
	{
		for (TFieldIterator<FProperty> It(Definition); It; ++It)
		{
			const FProperty* Property = *It;
			if (!Property->HasAnyPropertyFlags(CPF_SaveGame) || Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient))
			{
				continue;
			}
			const FString Name = Definition->GetAuthoredNameForField(Property);
			const FString ChildPath = Path.IsEmpty() ? Name : FString::Printf(TEXT("%s.%s"), *Path, *Name);
			const TSharedPtr<FJsonValue>* Child = Object->Values.Find(Name);
			if (!Child)
			{
				return Fail(OutError, ChildPath, TEXT("zorunlu alan eksik"));
			}
			if (!ValidateValue(Property, *Child, Version, ChildPath, Depth, OutError))
			{
				return false;
			}
		}
		return true;
	}

	// Anlamsal (oyun) aralik denetimi: tip dogru olsa da saçma degerleri reddeder. Asil kelepceleme yine
	// ApplySaveData'dadir; bu yalniz bozuk bir blob'un sessizce kabul edilmesini onler.
	bool CheckRanges(const FMMOCharacterSaveData& Data, FString& OutError)
	{
		const auto BadValue = [&OutError](const TCHAR* Field)
		{
			OutError = FString::Printf(TEXT("%s: aralik disi"), Field);
			return false;
		};
		if (Data.Level < 1 || Data.Level > 999) return BadValue(TEXT("Level"));
		if (Data.Experience < 0) return BadValue(TEXT("Experience"));
		if (Data.UnspentStatPoints < 0) return BadValue(TEXT("UnspentStatPoints"));
		if (Data.UnspentSkillPoints < 0) return BadValue(TEXT("UnspentSkillPoints"));
		if (Data.Strength < 0 || Data.Dexterity < 0 || Data.Intelligence < 0 || Data.Vitality < 0)
		{
			return BadValue(TEXT("Strength/Dexterity/Intelligence/Vitality"));
		}
		if (Data.Gold < 0) return BadValue(TEXT("Gold"));
		if (Data.NextInstanceId < 1) return BadValue(TEXT("NextInstanceId"));
		if (Data.ClassChangeCooldownExpiryUtcSeconds < 0) return BadValue(TEXT("ClassChangeCooldownExpiryUtcSeconds"));
		for (const TPair<FName, int64>& Pair : Data.SkillCooldownExpiryUtcSeconds)
		{
			if (Pair.Value < 0) return BadValue(TEXT("SkillCooldownExpiryUtcSeconds"));
		}
		for (const FMMOItemStack& Stack : Data.Items)
		{
			if (Stack.Count < 0 || Stack.Count > 10000000) return BadValue(TEXT("Items.Count"));
		}
		const auto CheckEquipment = [&](const TArray<FMMOEquipmentInstance>& List, const TCHAR* Field)
		{
			for (const FMMOEquipmentInstance& Instance : List)
			{
				if (Instance.InstanceId < 0 || Instance.Plus < 0 || Instance.Plus > 99)
				{
					return BadValue(Field);
				}
			}
			return true;
		};
		if (!CheckEquipment(Data.EquipmentBag, TEXT("EquipmentBag"))
			|| !CheckEquipment(Data.EquippedItems, TEXT("EquippedItems"))
			|| !CheckEquipment(Data.Warehouse, TEXT("Warehouse")))
		{
			return false;
		}
		for (const int32 Rank : Data.MasteryRanks)
		{
			if (Rank < 0 || Rank > 1000) return BadValue(TEXT("MasteryRanks"));
		}
		for (const FMMOQuestProgress& Quest : Data.ActiveQuests)
		{
			if (Quest.KillProgress < 0) return BadValue(TEXT("ActiveQuests.KillProgress"));
		}
		constexpr double MaxCoord = 1.0e8;
		if (FMath::Abs(Data.Location.X) > MaxCoord || FMath::Abs(Data.Location.Y) > MaxCoord
			|| FMath::Abs(Data.Location.Z) > MaxCoord)
		{
			return BadValue(TEXT("Location"));
		}
		return true;
	}

	bool HasFiniteTransform(const FMMOCharacterSaveData& Data)
	{
		return FMath::IsFinite(Data.Location.X) && FMath::IsFinite(Data.Location.Y) && FMath::IsFinite(Data.Location.Z)
			&& FMath::IsFinite(Data.Rotation.Pitch) && FMath::IsFinite(Data.Rotation.Yaw) && FMath::IsFinite(Data.Rotation.Roll);
	}
}

bool FMMOCharacterSaveJson::ToJson(const FMMOCharacterSaveData& Data, FString& OutJson)
{
	OutJson.Reset();
	// Yazar, okuyucunun reddedecegi bir blob uretmemeli: sonlu donusum + oyun araliklari ayni kapidan gecer.
	FString ValidationError;
	if (!HasFiniteTransform(Data) || !CheckRanges(Data, ValidationError))
	{
		return false;
	}

	// int64 JSON sayisi (double) olarak 2^53'u asinca kesinligini yitirir: v2'de metin olarak yazilir.
	const FJsonObjectConverter::CustomExportCallback ExportInt64AsString =
		FJsonObjectConverter::CustomExportCallback::CreateLambda(
			[](FProperty* Property, const void* Value) -> TSharedPtr<FJsonValue>
			{
				if (CastField<FInt64Property>(Property))
				{
					return MakeShared<FJsonValueString>(LexToString(*static_cast<const int64*>(Value)));
				}
				return nullptr;
			});

	const TSharedRef<FJsonObject> DataObject = MakeShared<FJsonObject>();
	if (!FJsonObjectConverter::UStructToJsonObject(
		FMMOCharacterSaveData::StaticStruct(), &Data, DataObject, CPF_SaveGame, 0, &ExportInt64AsString))
	{
		return false;
	}
	// Dizi/harita/string tavanlari ve enum gorunurlugu de FromJson ile simetrik olsun.
	if (!ValidateStruct(FMMOCharacterSaveData::StaticStruct(), DataObject, CurrentSchemaVersion,
		FString(), 0, ValidationError))
	{
		return false;
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(SchemaVersionField, CurrentSchemaVersion);
	Root->SetObjectField(DataField, DataObject);

	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(Root, Writer) || OutJson.Len() > MaxJsonChars)
	{
		OutJson.Reset();
		return false;
	}
	return true;
}

bool FMMOCharacterSaveJson::FromJson(const FString& Json, FMMOCharacterSaveData& OutData,
	int32* OutSchemaVersion, FString* OutError)
{
	// Basarisizlikta OutData'ya DOKUNULMAZ (cagiran mevcut kaydi bu yuzden ezmemeli: false -> yazma).
	if (OutError)
	{
		OutError->Reset();
	}
	if (Json.Len() > MaxJsonChars)
	{
		SetError(OutError, TEXT("Blob cok buyuk"));
		return false;
	}
	if (!HasSafeJsonNesting(Json))
	{
		SetError(OutError, TEXT("JSON ic ice derinligi cok buyuk"));
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		SetError(OutError, TEXT("JSON okunamadi"));
		return false;
	}

	const TSharedPtr<FJsonValue>* RawVersionValue = Root->Values.Find(SchemaVersionField);
	if (!RawVersionValue || !RawVersionValue->IsValid() || (*RawVersionValue)->Type != EJson::Number)
	{
		SetError(OutError, TEXT("SchemaVersion eksik veya sayi degil"));
		return false;
	}
	const double RawSchemaVersion = (*RawVersionValue)->AsNumber();
	if (!IsIntegralFinite(RawSchemaVersion) || RawSchemaVersion < 1.0 || RawSchemaVersion > CurrentSchemaVersion)
	{
		SetError(OutError, TEXT("Desteklenmeyen SchemaVersion"));
		return false;
	}
	const int32 SchemaVersion = static_cast<int32>(RawSchemaVersion);

	const TSharedPtr<FJsonObject>* DataObject = nullptr;
	if (!Root->TryGetObjectField(DataField, DataObject) || !DataObject || !DataObject->IsValid())
	{
		SetError(OutError, TEXT("Data nesnesi eksik"));
		return false;
	}

	FString ValidationError;
	if (!ValidateStruct(FMMOCharacterSaveData::StaticStruct(), *DataObject, SchemaVersion, FString(), 0, ValidationError))
	{
		SetError(OutError, TEXT("Data gecersiz: ") + ValidationError);
		return false;
	}

	FMMOCharacterSaveData Candidate;
	if (!FJsonObjectConverter::JsonObjectToUStruct(
		DataObject->ToSharedRef(), FMMOCharacterSaveData::StaticStruct(), &Candidate, CPF_SaveGame, 0))
	{
		SetError(OutError, TEXT("Karakter verisi donusturulemedi"));
		return false;
	}
	if (!HasFiniteTransform(Candidate) || !CheckRanges(Candidate, ValidationError))
	{
		SetError(OutError, TEXT("Data gecersiz: ") + (ValidationError.IsEmpty() ? FString(TEXT("sonlu olmayan konum")) : ValidationError));
		return false;
	}

	if (OutSchemaVersion)
	{
		*OutSchemaVersion = SchemaVersion;
	}
	OutData = MoveTemp(Candidate);
	return true;
}

void FMMOCharacterSaveJson::FindDifferences(const FMMOCharacterSaveData& A, const FMMOCharacterSaveData& B,
	TArray<FString>& OutFields)
{
	OutFields.Reset();
	for (TFieldIterator<FProperty> It(FMMOCharacterSaveData::StaticStruct()); It; ++It)
	{
		const FProperty* Property = *It;
		if (Property->HasAnyPropertyFlags(CPF_SaveGame) && !Property->Identical_InContainer(&A, &B))
		{
			OutFields.Add(Property->GetName());
		}
	}
}

namespace
{
	FMMOCharacterSaveData BuildSelfTestFixture()
	{
		FMMOCharacterSaveData Before;
		Before.bValid = true;
		Before.CharacterClass = EMMOClass::Mage;
		Before.Level = 39;
		Before.Experience = 543210;
		Before.UnspentStatPoints = 7;
		Before.UnspentSkillPoints = 5;
		Before.Strength = 17;
		Before.Dexterity = 23;
		Before.Intelligence = 88;
		Before.Vitality = 61;
		Before.Gold = 7654321;

		FMMOItemStack StackA;
		StackA.ItemId = FName(TEXT("HealthPotion"));
		StackA.Count = 17;
		Before.Items.Add(StackA);
		FMMOItemStack StackB;
		StackB.ItemId = FName(TEXT("ScrollRage"));
		StackB.Count = 2;
		Before.Items.Add(StackB);

		FMMOEquipmentInstance BagItem;
		BagItem.InstanceId = 41;
		BagItem.ItemId = FName(TEXT("MithrilBlade"));
		BagItem.Plus = 7;
		BagItem.Rarity = EMMOItemRarity::Epic;
		Before.EquipmentBag.Add(BagItem);
		FMMOEquipmentInstance EquippedItem;
		EquippedItem.InstanceId = 42;
		EquippedItem.ItemId = FName(TEXT("AdamantinePlate"));
		EquippedItem.Plus = 3;
		EquippedItem.Rarity = EMMOItemRarity::Rare;
		Before.EquippedItems.Add(EquippedItem);
		FMMOEquipmentInstance WarehouseItem;
		WarehouseItem.InstanceId = 43;
		WarehouseItem.ItemId = FName(TEXT("DragonHelm"));
		WarehouseItem.Plus = 8;
		WarehouseItem.Rarity = EMMOItemRarity::Legendary;
		Before.Warehouse.Add(WarehouseItem);
		Before.NextInstanceId = 44;

		Before.LearnedSkills = { FName(TEXT("Mag_Fireball")), FName(TEXT("Mag_Starfall")) };
		Before.Hotbar = { FName(TEXT("Mag_Starfall")), NAME_None, FName(TEXT("Mag_Fireball")) };
		Before.MasteryRanks = { 1, 6, 4, 3, 2 };
		// 2^53 + 1: double'a sigmaz, yalniz metin olarak kayipsiz tasinir (v2 int64 kodlamasi).
		Before.ClassChangeCooldownExpiryUtcSeconds = 9007199254740993;
		Before.SkillCooldownExpiryUtcSeconds.Add(FName(TEXT("Mag_Fireball")), 1893456011);
		Before.SkillCooldownExpiryUtcSeconds.Add(FName(TEXT("Mag_Starfall")), MAX_int64);

		FMMOQuestProgress Quest;
		Quest.QuestId = FName(TEXT("Q_ThawStalkers"));
		Quest.KillProgress = 6;
		Quest.bClaimed = false;
		Before.ActiveQuests.Add(Quest);
		Before.CompletedQuests = { FName(TEXT("Q_FrostLich")), FName(TEXT("Q_EmberHounds")) };
		// Turkce karakterler (\u kacisi: kaynak ASCII kalir): g-ust-u, s-cedilla, dotless-i, o/u-umlaut.
		Before.ClanName = TEXT("Gümüşİnanç");
		Before.bClanLeader = true;
		Before.Friends = { TEXT("GezginÇ"), TEXT("Şahin_Ö") };
		Before.MapName = TEXT("Summer_scene");
		// 0.1 / 0.2 ikilik tabanda tam temsil edilmez: %.17g gidis-donusunu sinar.
		Before.Location = FVector(1234.5, -678.25, 0.1);
		Before.Rotation = FRotator(-12.5, 173.25, 0.2);
		return Before;
	}

	TSharedPtr<FJsonObject> ParseObject(const FString& Json)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Root);
		return Root;
	}

	FString WriteObject(const TSharedPtr<FJsonObject>& Root)
	{
		FString Out;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
		return Out;
	}

	TSharedPtr<FJsonValue> Str(const TCHAR* Text) { return MakeShared<FJsonValueString>(Text); }
	TSharedPtr<FJsonValue> Num(double Number) { return MakeShared<FJsonValueNumber>(Number); }
	TSharedPtr<FJsonValue> Bool(bool bValue) { return MakeShared<FJsonValueBoolean>(bValue); }
	TSharedPtr<FJsonValue> Null() { return MakeShared<FJsonValueNull>(); }

	TSharedPtr<FJsonObject> Sub(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Field);

	using FMutator = TFunctionRef<void(const TSharedPtr<FJsonObject>& Root, const TSharedPtr<FJsonObject>& Data)>;

	// Taban blob'u bozar ve FromJson'in reddettigini, hata mesaji verdigini ve OutData'yi ezmedigini sinar.
	bool ExpectRejected(const FString& BaseJson, const TCHAR* Label, FMutator Mutate, FString& OutError, int32& InOutCount)
	{
		const TSharedPtr<FJsonObject> Root = ParseObject(BaseJson);
		const TSharedPtr<FJsonObject> Data = Sub(Root, DataField);
		Mutate(Root, Data);
		const FString Mutated = WriteObject(Root);

		FMMOCharacterSaveData Sentinel;
		Sentinel.Level = 777;
		Sentinel.Gold = 4242;
		FString Error;
		const bool bAccepted = FMMOCharacterSaveJson::FromJson(Mutated, Sentinel, nullptr, &Error);
		if (bAccepted)
		{
			OutError = FString::Printf(TEXT("Negatif test reddedilmedi: %s"), Label);
			return false;
		}
		if (Error.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Negatif test hata mesaji bos: %s"), Label);
			return false;
		}
		if (Sentinel.Level != 777 || Sentinel.Gold != 4242)
		{
			OutError = FString::Printf(TEXT("Reddedilen blob OutData'yi ezdi: %s"), Label);
			return false;
		}
		++InOutCount;
		return true;
	}

	bool ExpectRawRejected(const FString& Raw, const TCHAR* Label, FString& OutError, int32& InOutCount)
	{
		FMMOCharacterSaveData Sentinel;
		Sentinel.Level = 777;
		FString Error;
		if (FMMOCharacterSaveJson::FromJson(Raw, Sentinel, nullptr, &Error) || Error.IsEmpty() || Sentinel.Level != 777)
		{
			OutError = FString::Printf(TEXT("Ham negatif test basarisiz: %s"), Label);
			return false;
		}
		++InOutCount;
		return true;
	}

	// Test yardimcilari: alan yoksa bos nesne/null doner (cokmez; negatif test "reddedilmedi" der).
	TSharedPtr<FJsonObject> Sub(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Field)
	{
		const TSharedPtr<FJsonValue>* Found = Parent->Values.Find(Field);
		return (Found && Found->IsValid() && (*Found)->Type == EJson::Object) ? (*Found)->AsObject() : MakeShared<FJsonObject>();
	}

	TSharedPtr<FJsonObject> FirstOfArray(const TSharedPtr<FJsonObject>& Data, const TCHAR* Field)
	{
		const TSharedPtr<FJsonValue>* Found = Data->Values.Find(Field);
		if (Found && Found->IsValid() && (*Found)->Type == EJson::Array && (*Found)->AsArray().Num() > 0
			&& (*Found)->AsArray()[0].IsValid() && (*Found)->AsArray()[0]->Type == EJson::Object)
		{
			return (*Found)->AsArray()[0]->AsObject();
		}
		return MakeShared<FJsonObject>();
	}

	bool RunNegativeTests(const FString& BaseJson, FString& OutError, int32& OutCount)
	{
#define MMO_EXPECT_REJECT(Label, ...) \
		if (!ExpectRejected(BaseJson, TEXT(Label), []([[maybe_unused]] const TSharedPtr<FJsonObject>& Root, [[maybe_unused]] const TSharedPtr<FJsonObject>& Data) { __VA_ARGS__ }, OutError, OutCount)) { return false; }

		// Ham metin: bozuk/bos/yanlis kok/asiri buyuk.
		for (const TCHAR* Raw : { TEXT(""), TEXT("{"), TEXT("[]"), TEXT("null"), TEXT("42"), TEXT("\"x\""), TEXT("{}") })
		{
			if (!ExpectRawRejected(Raw, Raw, OutError, OutCount)) return false;
		}
		if (!ExpectRawRejected(FString::ChrN(2 * 1024 * 1024 + 1, TEXT('x')), TEXT("asiri buyuk blob"), OutError, OutCount)) return false;
		{
			// Gecerli karakter blob'undaki bilinmeyen alan da on taramaya tabidir.
			// Yalniz dizi kok kullanmak, nesne kok zorunlulugu nedeniyle zaten reddedilirdi.
			const FString DeeplyNested = TEXT("{\"Deep\":") + FString::ChrN(MaxJsonNestingDepth, TEXT('[')) + TEXT("0")
				+ FString::ChrN(MaxJsonNestingDepth, TEXT(']')) + TEXT(",") + BaseJson.RightChop(1);
			if (!ExpectRawRejected(DeeplyNested, TEXT("asiri derin JSON"), OutError, OutCount)) return false;
		}

		// SchemaVersion
		MMO_EXPECT_REJECT("SchemaVersion eksik", Root->RemoveField(SchemaVersionField);)
		MMO_EXPECT_REJECT("SchemaVersion 0", Root->SetField(SchemaVersionField, Num(0));)
		MMO_EXPECT_REJECT("SchemaVersion -1", Root->SetField(SchemaVersionField, Num(-1));)
		MMO_EXPECT_REJECT("SchemaVersion 1.5", Root->SetField(SchemaVersionField, Num(1.5));)
		MMO_EXPECT_REJECT("SchemaVersion metin", Root->SetField(SchemaVersionField, Str(TEXT("2")));)
		MMO_EXPECT_REJECT("SchemaVersion ileri (999)", Root->SetField(SchemaVersionField, Num(999));)
		MMO_EXPECT_REJECT("SchemaVersion null", Root->SetField(SchemaVersionField, Null());)
		MMO_EXPECT_REJECT("SchemaVersion bool", Root->SetField(SchemaVersionField, Bool(true));)

		// Data zarfi
		MMO_EXPECT_REJECT("Data eksik", Root->RemoveField(DataField);)
		MMO_EXPECT_REJECT("Data null", Root->SetField(DataField, Null());)
		MMO_EXPECT_REJECT("Data dizi", Root->SetField(DataField, MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>()));)
		MMO_EXPECT_REJECT("Data bos nesne", Root->SetField(DataField, MakeShared<FJsonValueObject>(MakeShared<FJsonObject>()));)

		// Her ust duzey zorunlu alan tek tek silinince reddedilmeli.
		{
			TArray<FString> Keys;
			Sub(ParseObject(BaseJson), DataField)->Values.GetKeys(Keys);
			for (const FString& Key : Keys)
			{
				const FString Label = FString::Printf(TEXT("alan eksik: %s"), *Key);
				if (!ExpectRejected(BaseJson, *Label,
					[&Key](const TSharedPtr<FJsonObject>&, const TSharedPtr<FJsonObject>& Data) { Data->RemoveField(Key); },
					OutError, OutCount))
				{
					return false;
				}
			}
		}

		// Tip / aralik bozuklugu
		MMO_EXPECT_REJECT("Level metin", Data->SetField(TEXT("Level"), Str(TEXT("abc")));)
		MMO_EXPECT_REJECT("Level sayi-metin", Data->SetField(TEXT("Level"), Str(TEXT("39")));)
		MMO_EXPECT_REJECT("Level ondalik", Data->SetField(TEXT("Level"), Num(1.5));)
		MMO_EXPECT_REJECT("Level 1e12", Data->SetField(TEXT("Level"), Num(1.0e12));)
		MMO_EXPECT_REJECT("Level null", Data->SetField(TEXT("Level"), Null());)
		MMO_EXPECT_REJECT("Level bool", Data->SetField(TEXT("Level"), Bool(true));)
		MMO_EXPECT_REJECT("Level 0", Data->SetField(TEXT("Level"), Num(0));)
		MMO_EXPECT_REJECT("Level 100000", Data->SetField(TEXT("Level"), Num(100000));)
		MMO_EXPECT_REJECT("bValid metin", Data->SetField(TEXT("bValid"), Str(TEXT("true")));)
		MMO_EXPECT_REJECT("bValid sayi", Data->SetField(TEXT("bValid"), Num(1));)
		MMO_EXPECT_REJECT("Gold negatif", Data->SetField(TEXT("Gold"), Num(-1));)
		MMO_EXPECT_REJECT("Gold metin", Data->SetField(TEXT("Gold"), Str(TEXT("12")));)
		MMO_EXPECT_REJECT("Experience negatif", Data->SetField(TEXT("Experience"), Num(-5));)
		MMO_EXPECT_REJECT("NextInstanceId 0", Data->SetField(TEXT("NextInstanceId"), Num(0));)
		MMO_EXPECT_REJECT("Strength int32 tasmasi", Data->SetField(TEXT("Strength"), Num(3.0e9));)

		// Enum
		MMO_EXPECT_REJECT("sinif bilinmeyen ad", Data->SetField(TEXT("CharacterClass"), Str(TEXT("Necromancer")));)
		MMO_EXPECT_REJECT("sinif aralik disi sayi", Data->SetField(TEXT("CharacterClass"), Num(99));)
		MMO_EXPECT_REJECT("sinif int64 disi sayi", Data->SetField(TEXT("CharacterClass"), Num(1.0e300));)
		MMO_EXPECT_REJECT("sinif negatif sayi", Data->SetField(TEXT("CharacterClass"), Num(-1));)
		MMO_EXPECT_REJECT("sinif bool", Data->SetField(TEXT("CharacterClass"), Bool(false));)
		MMO_EXPECT_REJECT("sinif otomatik MAX ad", Data->SetField(TEXT("CharacterClass"), Str(*StaticEnum<EMMOClass>()->GetNameStringByValue(StaticEnum<EMMOClass>()->GetMaxEnumValue())));)
		MMO_EXPECT_REJECT("sinif otomatik MAX sayi", Data->SetField(TEXT("CharacterClass"), Num(StaticEnum<EMMOClass>()->GetMaxEnumValue()));)
		MMO_EXPECT_REJECT("nadirlik gizli MAX", FirstOfArray(Data, TEXT("EquipmentBag"))->SetField(TEXT("Rarity"), Str(TEXT("MAX")));)
		MMO_EXPECT_REJECT("nadirlik gizli MAX (sayi 5)", FirstOfArray(Data, TEXT("EquipmentBag"))->SetField(TEXT("Rarity"), Num(5));)

		// Diziler / ic yapilar
		MMO_EXPECT_REJECT("Items nesne", Data->SetField(TEXT("Items"), MakeShared<FJsonValueObject>(MakeShared<FJsonObject>()));)
		MMO_EXPECT_REJECT("Items elemani null",
			Data->SetField(TEXT("Items"), MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{ Null() }));)
		MMO_EXPECT_REJECT("Items[0].Count eksik", FirstOfArray(Data, TEXT("Items"))->RemoveField(TEXT("Count"));)
		MMO_EXPECT_REJECT("Items[0].Count metin", FirstOfArray(Data, TEXT("Items"))->SetField(TEXT("Count"), Str(TEXT("x")));)
		MMO_EXPECT_REJECT("Items[0].Count negatif", FirstOfArray(Data, TEXT("Items"))->SetField(TEXT("Count"), Num(-3));)
		MMO_EXPECT_REJECT("Items[0].ItemId sayi", FirstOfArray(Data, TEXT("Items"))->SetField(TEXT("ItemId"), Num(5));)
		MMO_EXPECT_REJECT("Items[0].ItemId uzun", FirstOfArray(Data, TEXT("Items"))->SetField(TEXT("ItemId"), MakeShared<FJsonValueString>(FString::ChrN(300, TEXT('a'))));)
		MMO_EXPECT_REJECT("Items cok buyuk (>4096)",
			{
				TArray<TSharedPtr<FJsonValue>> Many;
				for (int32 Index = 0; Index < 4097; ++Index)
				{
					Many.Add(Data->GetArrayField(TEXT("Items"))[0]);
				}
				Data->SetField(TEXT("Items"), MakeShared<FJsonValueArray>(Many));
			})
		MMO_EXPECT_REJECT("EquipmentBag[0].Plus ondalik", FirstOfArray(Data, TEXT("EquipmentBag"))->SetField(TEXT("Plus"), Num(2.5));)
		MMO_EXPECT_REJECT("Warehouse[0].InstanceId negatif", FirstOfArray(Data, TEXT("Warehouse"))->SetField(TEXT("InstanceId"), Num(-1));)
		MMO_EXPECT_REJECT("MasteryRanks negatif", Data->SetField(TEXT("MasteryRanks"), MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{ Num(-1) }));)
		MMO_EXPECT_REJECT("MasteryRanks metin", Data->SetField(TEXT("MasteryRanks"), MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{ Str(TEXT("x")) }));)
		MMO_EXPECT_REJECT("Friends elemani sayi", Data->SetField(TEXT("Friends"), MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{ Num(1) }));)
		MMO_EXPECT_REJECT("ActiveQuests[0].KillProgress negatif", FirstOfArray(Data, TEXT("ActiveQuests"))->SetField(TEXT("KillProgress"), Num(-2));)
		MMO_EXPECT_REJECT("ClanName sayi", Data->SetField(TEXT("ClanName"), Num(7));)
		MMO_EXPECT_REJECT("ClanName cok uzun", Data->SetField(TEXT("ClanName"), MakeShared<FJsonValueString>(FString::ChrN(2000, TEXT('c'))));)

		// Konum / donus
		MMO_EXPECT_REJECT("Location.Y eksik", Sub(Data, TEXT("Location"))->RemoveField(TEXT("Y"));)
		MMO_EXPECT_REJECT("Location.X metin", Sub(Data, TEXT("Location"))->SetField(TEXT("X"), Str(TEXT("1")));)
		MMO_EXPECT_REJECT("Location dizi", Data->SetField(TEXT("Location"), MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{ Num(1), Num(2), Num(3) }));)
		MMO_EXPECT_REJECT("Location asiri buyuk", Sub(Data, TEXT("Location"))->SetField(TEXT("X"), Num(1.0e12));)
		MMO_EXPECT_REJECT("Rotation.Yaw null", Sub(Data, TEXT("Rotation"))->SetField(TEXT("Yaw"), Null());)
		{
			FString Inf = BaseJson;
			Inf.ReplaceInline(TEXT("1234.5"), TEXT("1e999"));
			if (!ExpectRawRejected(Inf, TEXT("Location.X sonsuz (1e999)"), OutError, OutCount)) return false;
		}

		// int64 (v2: ondalik-siz tam sayi METNI; sayi, bosluk, +, bas sifir, tasma, bos reddedilir)
		for (const TCHAR* Bad : { TEXT("12x"), TEXT(""), TEXT(" 5"), TEXT("+5"), TEXT("05"), TEXT("-0"), TEXT("1.5"),
			TEXT("99999999999999999999"), TEXT("9223372036854775808"), TEXT("-5") })
		{
			const FString Label = FString::Printf(TEXT("int64 sinif cooldown metni '%s'"), Bad);
			if (!ExpectRejected(BaseJson, *Label,
				[Bad](const TSharedPtr<FJsonObject>&, const TSharedPtr<FJsonObject>& Data)
				{ Data->SetField(TEXT("ClassChangeCooldownExpiryUtcSeconds"), Str(Bad)); },
				OutError, OutCount))
			{
				return false;
			}
		}
		MMO_EXPECT_REJECT("int64 v2'de sayi", Data->SetField(TEXT("ClassChangeCooldownExpiryUtcSeconds"), Num(1893456000));)
		MMO_EXPECT_REJECT("int64 harita degeri sayi", Sub(Data, TEXT("SkillCooldownExpiryUtcSeconds"))->SetField(TEXT("Mag_Fireball"), Num(5));)
		MMO_EXPECT_REJECT("int64 harita degeri bozuk metin", Sub(Data, TEXT("SkillCooldownExpiryUtcSeconds"))->SetField(TEXT("Mag_Fireball"), Str(TEXT("12x")));)
		MMO_EXPECT_REJECT("int64 harita degeri null", Sub(Data, TEXT("SkillCooldownExpiryUtcSeconds"))->SetField(TEXT("Mag_Fireball"), Null());)
		MMO_EXPECT_REJECT("int64 harita degeri negatif", Sub(Data, TEXT("SkillCooldownExpiryUtcSeconds"))->SetField(TEXT("Mag_Fireball"), Str(TEXT("-7")));)
		MMO_EXPECT_REJECT("harita dizi", Data->SetField(TEXT("SkillCooldownExpiryUtcSeconds"), MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>()));)
		MMO_EXPECT_REJECT("harita bos anahtar", Sub(Data, TEXT("SkillCooldownExpiryUtcSeconds"))->SetField(FString(), Str(TEXT("5")));)
#undef MMO_EXPECT_REJECT
		return true;
	}

	// Pozitif tolerans testleri: kabul edilmesi GEREKEN eski/ek bicimler.
	bool RunToleranceTests(const FMMOCharacterSaveData& Before, const FString& BaseJson, FString& OutError)
	{
		FMMOCharacterSaveData Out;
		FString Error;

		// Bilinmeyen ek alan yok sayilir (ileri uyumluluk).
		{
			const TSharedPtr<FJsonObject> Root = ParseObject(BaseJson);
			// Tirnak, ters egik cizgi ve string icindeki parantezler derinlik sayacini etkilememeli.
			const FString QuotedBrackets = TEXT("\\\"") + FString::ChrN(32, TEXT('[')) + FString::ChrN(32, TEXT('{'));
			Sub(Root, DataField)->SetField(TEXT("FutureField"), MakeShared<FJsonValueString>(QuotedBrackets));
			Root->SetField(TEXT("Extra"), Num(1));
			if (!FMMOCharacterSaveJson::FromJson(WriteObject(Root), Out, nullptr, &Error)
				|| !FMMOCharacterSaveData::StaticStruct()->CompareScriptStruct(&Before, &Out, 0))
			{
				OutError = TEXT("Bilinmeyen ek alan tolere edilmedi: ") + Error;
				return false;
			}
		}
		// Ham tavanin kendisi kabul edilir: kok nesne + 15 dizi = 16.
		{
			const FString AtDepthLimit = TEXT("{\"Deep\":") + FString::ChrN(MaxJsonNestingDepth - 1, TEXT('[')) + TEXT("0")
				+ FString::ChrN(MaxJsonNestingDepth - 1, TEXT(']')) + TEXT(",") + BaseJson.RightChop(1);
			if (!FMMOCharacterSaveJson::FromJson(AtDepthLimit, Out, nullptr, &Error)
				|| !FMMOCharacterSaveData::StaticStruct()->CompareScriptStruct(&Before, &Out, 0))
			{
				OutError = TEXT("Derinlik tavani icindeki ek alan reddedildi: ") + Error;
				return false;
			}
		}
		// Enum: eski sayisal deger kabul edilir (Mage = 2).
		{
			const TSharedPtr<FJsonObject> Root = ParseObject(BaseJson);
			Sub(Root, DataField)->SetField(TEXT("CharacterClass"), Num(2));
			Out = FMMOCharacterSaveData();
			if (!FMMOCharacterSaveJson::FromJson(WriteObject(Root), Out, nullptr, &Error) || Out.CharacterClass != EMMOClass::Mage)
			{
				OutError = TEXT("Eski sayisal enum tolere edilmedi: ") + Error;
				return false;
			}
		}
		// v1 blob'u (int64 JSON sayisi) hala okunur ve ayni veriyi verir (sinir: 2^53 alti).
		{
			FMMOCharacterSaveData V1Source = Before;
			V1Source.ClassChangeCooldownExpiryUtcSeconds = 1893456000;
			V1Source.SkillCooldownExpiryUtcSeconds.Add(FName(TEXT("Mag_Starfall")), 1893456022);
			FString V2Json;
			if (!FMMOCharacterSaveJson::ToJson(V1Source, V2Json))
			{
				OutError = TEXT("v1 testi icin ToJson basarisiz");
				return false;
			}
			const TSharedPtr<FJsonObject> Root = ParseObject(V2Json);
			const TSharedPtr<FJsonObject> Data = Sub(Root, DataField);
			Root->SetField(SchemaVersionField, Num(1));
			Data->SetField(TEXT("ClassChangeCooldownExpiryUtcSeconds"),
				Num(static_cast<double>(V1Source.ClassChangeCooldownExpiryUtcSeconds)));
			const TSharedPtr<FJsonObject> Map = Sub(Data, TEXT("SkillCooldownExpiryUtcSeconds"));
			for (const TPair<FName, int64>& Pair : V1Source.SkillCooldownExpiryUtcSeconds)
			{
				Map->SetField(Pair.Key.ToString(), Num(static_cast<double>(Pair.Value)));
			}
			const FString V1Json = WriteObject(Root);
			int32 ReadVersion = 0;
			Out = FMMOCharacterSaveData();
			if (!FMMOCharacterSaveJson::FromJson(V1Json, Out, &ReadVersion, &Error) || ReadVersion != 1
				|| !FMMOCharacterSaveData::StaticStruct()->CompareScriptStruct(&V1Source, &Out, 0))
			{
				OutError = TEXT("Eski v1 blob okunamadi/esit degil: ") + Error;
				return false;
			}
			// v1'de metin biciminde int64 kabul edilmez (surume gore siki).
			const TSharedPtr<FJsonObject> BadRoot = ParseObject(V1Json);
			Sub(BadRoot, DataField)->SetField(TEXT("ClassChangeCooldownExpiryUtcSeconds"), Str(TEXT("1893456000")));
			Out = FMMOCharacterSaveData();
			if (FMMOCharacterSaveJson::FromJson(WriteObject(BadRoot), Out))
			{
				OutError = TEXT("v1'de metin int64 reddedilmedi");
				return false;
			}
			// v1'de 2^53 ustu sayi kabul edilmez (kesinlik kaybi).
			Sub(BadRoot, DataField)->SetField(TEXT("ClassChangeCooldownExpiryUtcSeconds"), Num(1.0e17));
			if (FMMOCharacterSaveJson::FromJson(WriteObject(BadRoot), Out))
			{
				OutError = TEXT("v1'de 2^53 ustu int64 reddedilmedi");
				return false;
			}
		}
		// Bos ama gecerli kayit (yeni karakter, bValid=false) de gidis-donus yapar.
		{
			FString EmptyJson;
			FMMOCharacterSaveData Empty;
			Out = FMMOCharacterSaveData();
			Out.Level = 9;
			if (!FMMOCharacterSaveJson::ToJson(Empty, EmptyJson) || !FMMOCharacterSaveJson::FromJson(EmptyJson, Out, nullptr, &Error)
				|| !FMMOCharacterSaveData::StaticStruct()->CompareScriptStruct(&Empty, &Out, 0))
			{
				OutError = TEXT("Bos kayit gidis-donusu basarisiz: ") + Error;
				return false;
			}
		}
		// NaN konum: ToJson sessizce bozuk blob uretmek yerine reddeder.
		{
			FMMOCharacterSaveData NanData = Before;
			NanData.Location.X = FMath::Sqrt(-1.0);
			FString Ignored;
			if (FMMOCharacterSaveJson::ToJson(NanData, Ignored))
			{
				OutError = TEXT("NaN konum ToJson tarafindan reddedilmedi");
				return false;
			}
		}
		// ToJson, FromJson'in aralik ve boyut kurallariyla simetrik olmali.
		{
			FMMOCharacterSaveData InvalidRange = Before;
			InvalidRange.Level = 0;
			FString Ignored;
			if (FMMOCharacterSaveJson::ToJson(InvalidRange, Ignored))
			{
				OutError = TEXT("Aralik disi Level ToJson tarafindan reddedilmedi");
				return false;
			}

			FMMOCharacterSaveData Oversized = Before;
			Oversized.Friends.SetNum(MaxArrayElements + 1);
			if (FMMOCharacterSaveJson::ToJson(Oversized, Ignored))
			{
				OutError = TEXT("Asiri buyuk dizi ToJson tarafindan reddedilmedi");
				return false;
			}
		}
		return true;
	}
}

bool FMMOCharacterSaveJson::RunRoundTripSelfTest(FString& OutError, int32& OutUtf8Bytes, int32* OutNegativeCases)
{
	OutError.Reset();
	OutUtf8Bytes = 0;
	if (OutNegativeCases)
	{
		*OutNegativeCases = 0;
	}

	const FMMOCharacterSaveData Before = BuildSelfTestFixture();

	FString Json;
	if (!ToJson(Before, Json))
	{
		OutError = TEXT("ToJson basarisiz");
		return false;
	}
	OutUtf8Bytes = FTCHARToUTF8(*Json).Length();

	FMMOCharacterSaveData After;
	int32 SchemaVersion = 0;
	if (!FromJson(Json, After, &SchemaVersion, &OutError))
	{
		return false;
	}
	if (SchemaVersion != CurrentSchemaVersion)
	{
		OutError = TEXT("Sema surumu gidis-donuste degisti");
		return false;
	}
	TArray<FString> Differences;
	FindDifferences(Before, After, Differences);
	if (!Differences.IsEmpty() || !FMMOCharacterSaveData::StaticStruct()->CompareScriptStruct(&Before, &After, 0))
	{
		OutError = TEXT("Kayit alanlari gidis-donuste degisti: ") + FString::Join(Differences, TEXT(","));
		return false;
	}
	// int64 > 2^53 metin olarak yazildigi icin kayipsiz donmeli (yukaridaki esitlik bunu da kapsar).
	if (After.ClassChangeCooldownExpiryUtcSeconds != 9007199254740993
		|| After.SkillCooldownExpiryUtcSeconds.FindRef(FName(TEXT("Mag_Starfall"))) != MAX_int64)
	{
		OutError = TEXT("int64 degerleri 2^53 ustunde kesinlik kaybetti");
		return false;
	}

	int32 NegativeCount = 0;
	int32 EnumVariants = 0;
	for (int32 Kind = 0; Kind < 2; ++Kind)
	for (int32 Numeric = 0; Numeric < 2; ++Numeric)
	for (int32 Value = 0; Value < (Kind == 0 ? 4 : 5); ++Value)
	{
		FMMOCharacterSaveData Expected = Before;
		if (Kind == 0) Expected.CharacterClass = static_cast<EMMOClass>(Value);
		else Expected.EquipmentBag[0].Rarity = static_cast<EMMOItemRarity>(Value);
		FString Variant;
		if (!ToJson(Expected, Variant)) { OutError = TEXT("valid enum writer rejected"); return false; }
		if (Numeric)
		{
			const TSharedPtr<FJsonObject> Root = ParseObject(Variant);
			const TSharedPtr<FJsonObject> Data = Sub(Root, DataField);
			if (Kind == 0) Data->SetNumberField(TEXT("CharacterClass"), Value);
			else FirstOfArray(Data, TEXT("EquipmentBag"))->SetNumberField(TEXT("Rarity"), Value);
			Variant = WriteObject(Root);
		}
		FMMOCharacterSaveData Actual;
		if (!FromJson(Variant, Actual, nullptr, &OutError)
			|| !FMMOCharacterSaveData::StaticStruct()->CompareScriptStruct(&Expected, &Actual, 0))
		{ OutError = TEXT("valid enum roundtrip failed: ") + OutError; return false; }
		++EnumVariants;
	}
	FMMOCharacterSaveData Invalid = Before;
	Invalid.CharacterClass = static_cast<EMMOClass>(StaticEnum<EMMOClass>()->GetMaxEnumValue());
	FString RejectedJson = TEXT("stale");
	if (ToJson(Invalid, RejectedJson) || !RejectedJson.IsEmpty())
	{ OutError = TEXT("invalid class writer accepted"); return false; }
	Invalid = Before; Invalid.EquipmentBag[0].Rarity = EMMOItemRarity::MAX;
	RejectedJson = TEXT("stale");
	if (ToJson(Invalid, RejectedJson) || !RejectedJson.IsEmpty())
	{ OutError = TEXT("invalid rarity writer accepted"); return false; }
	UE_LOG(LogTemp, Display, TEXT("[MMO_SAVE_ENUM_TEST] PASS variants=%d invalid_writers=2"), EnumVariants);
	if (!RunNegativeTests(Json, OutError, NegativeCount) || !RunToleranceTests(Before, Json, OutError))
	{
		return false;
	}
	if (OutNegativeCases)
	{
		*OutNegativeCases = NegativeCount;
	}
	return true;
}

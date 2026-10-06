// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOCharacterSaveJson.h"

#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	constexpr TCHAR SchemaVersionField[] = TEXT("SchemaVersion");
	constexpr TCHAR DataField[] = TEXT("Data");

	void SetError(FString* OutError, const TCHAR* Message)
	{
		if (OutError)
		{
			*OutError = Message;
		}
	}
}

bool FMMOCharacterSaveJson::ToJson(const FMMOCharacterSaveData& Data, FString& OutJson)
{
	OutJson.Reset();
	const TSharedRef<FJsonObject> DataObject = MakeShared<FJsonObject>();
	if (!FJsonObjectConverter::UStructToJsonObject(
		FMMOCharacterSaveData::StaticStruct(), &Data, DataObject, CPF_SaveGame, 0))
	{
		return false;
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(SchemaVersionField, CurrentSchemaVersion);
	Root->SetObjectField(DataField, DataObject);

	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	return FJsonSerializer::Serialize(Root, Writer);
}

bool FMMOCharacterSaveJson::FromJson(const FString& Json, FMMOCharacterSaveData& OutData,
	int32* OutSchemaVersion, FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		SetError(OutError, TEXT("JSON okunamadi"));
		return false;
	}

	double RawSchemaVersion = 0.0;
	if (!Root->TryGetNumberField(SchemaVersionField, RawSchemaVersion))
	{
		SetError(OutError, TEXT("SchemaVersion eksik"));
		return false;
	}
	const int32 SchemaVersion = static_cast<int32>(RawSchemaVersion);
	if (!FMath::IsNearlyEqual(RawSchemaVersion, static_cast<double>(SchemaVersion))
		|| SchemaVersion < 1 || SchemaVersion > CurrentSchemaVersion)
	{
		SetError(OutError, TEXT("Desteklenmeyen SchemaVersion"));
		return false;
	}
	if (OutSchemaVersion)
	{
		*OutSchemaVersion = SchemaVersion;
	}

	const TSharedPtr<FJsonObject>* DataObject = nullptr;
	if (!Root->TryGetObjectField(DataField, DataObject) || !DataObject || !DataObject->IsValid())
	{
		SetError(OutError, TEXT("Data nesnesi eksik"));
		return false;
	}

	FMMOCharacterSaveData Candidate;
	if (!FJsonObjectConverter::JsonObjectToUStruct(
		DataObject->ToSharedRef(), FMMOCharacterSaveData::StaticStruct(), &Candidate, CPF_SaveGame, 0))
	{
		SetError(OutError, TEXT("Karakter verisi donusturulemedi"));
		return false;
	}
	OutData = MoveTemp(Candidate);
	return true;
}

bool FMMOCharacterSaveJson::RunRoundTripSelfTest(FString& OutError, int32& OutUtf8Bytes)
{
	OutError.Reset();
	OutUtf8Bytes = 0;

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
	Before.ClassChangeCooldownExpiryUtcSeconds = 1893456000;
	Before.SkillCooldownExpiryUtcSeconds.Add(FName(TEXT("Mag_Fireball")), 1893456011);
	Before.SkillCooldownExpiryUtcSeconds.Add(FName(TEXT("Mag_Starfall")), 1893456022);

	FMMOQuestProgress Quest;
	Quest.QuestId = FName(TEXT("Q_ThawStalkers"));
	Quest.KillProgress = 6;
	Quest.bClaimed = false;
	Before.ActiveQuests.Add(Quest);
	Before.CompletedQuests = { FName(TEXT("Q_FrostLich")), FName(TEXT("Q_EmberHounds")) };
	Before.ClanName = TEXT("GumusDal");
	Before.bClanLeader = true;
	Before.Friends = { TEXT("GezginA"), TEXT("GezginB") };
	Before.MapName = TEXT("Summer_scene");
	Before.Location = FVector(1234.5, -678.25, 91.75);
	Before.Rotation = FRotator(-12.5, 173.25, 0.5);

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
	if (!FMMOCharacterSaveData::StaticStruct()->CompareScriptStruct(&Before, &After, 0))
	{
		OutError = TEXT("Kayit alanlari gidis-donuste degisti");
		return false;
	}

	FString FutureJson = Json;
	FutureJson.ReplaceInline(TEXT("\"SchemaVersion\":1"), TEXT("\"SchemaVersion\":999"));
	FMMOCharacterSaveData Ignored;
	if (FromJson(FutureJson, Ignored))
	{
		OutError = TEXT("Ileri sema surumu reddedilmedi");
		return false;
	}
	return true;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOSupabaseCharacterStore.h"
#include "Core/MMONameRules.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Save/MMOCharacterSaveJson.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	using SupabaseOut = EMMOSupabaseStoreOutcome;

	constexpr int32 SupabaseMaxNameLen = 32;
	constexpr int32 SupabaseLeaseTtlSeconds = 120;

	/** int64: ondalik-siz tam sayi METNI ya da 2^53 alti tam sayi sayisi. Digerleri RED. */
	bool SupabaseParseInt64(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Name, int64& Out)
	{
		if (!Obj.IsValid())
		{
			return false;
		}
		const TSharedPtr<FJsonValue>* Value = Obj->Values.Find(Name);
		if (!Value || !Value->IsValid())
		{
			return false;
		}
		if ((*Value)->Type == EJson::String)
		{
			const FString Text = (*Value)->AsString();
			if (Text.IsEmpty() || Text.Len() > 19)
			{
				return false;
			}
			for (const TCHAR Ch : Text)
			{
				if (Ch < TEXT('0') || Ch > TEXT('9'))
				{
					return false;
				}
			}
			if (Text.Len() == 19 && Text > TEXT("9223372036854775807"))
			{
				return false;
			}
			Out = FCString::Atoi64(*Text);
			return true;
		}
		if ((*Value)->Type == EJson::Number)
		{
			const double Number = (*Value)->AsNumber();
			if (Number < 0.0 || Number > 9007199254740992.0 || Number != FMath::FloorToDouble(Number))
			{
				return false;
			}
			Out = static_cast<int64>(Number);
			return true;
		}
		return false;
	}

	FString SupabaseInt64Text(int64 Value)
	{
		return FString::Printf(TEXT("%lld"), static_cast<long long>(Value));
	}

	SupabaseOut SupabaseMapOutcome(const FString& Outcome)
	{
		if (Outcome == TEXT("not_found")) return SupabaseOut::NotFound;
		if (Outcome == TEXT("already_exists")) return SupabaseOut::AlreadyExists;
		if (Outcome == TEXT("invalid")) return SupabaseOut::Invalid;
		if (Outcome == TEXT("revision_conflict")) return SupabaseOut::RevisionConflict;
		if (Outcome == TEXT("ownership_lost")) return SupabaseOut::OwnershipLost;
		if (Outcome == TEXT("idempotency_conflict")) return SupabaseOut::IdempotencyConflict;
		if (Outcome == TEXT("forbidden")) return SupabaseOut::Forbidden;
		return SupabaseOut::InternalError;
	}

	bool SupabaseIsCommitted(const FString& Outcome)
	{
		return Outcome == TEXT("committed") || Outcome == TEXT("ok");
	}

	bool SupabaseDataToObject(const FMMOCharacterSaveData& Data, TSharedPtr<FJsonObject>& Out)
	{
		FString Json;
		if (!FMMOCharacterSaveJson::ToJson(Data, Json))
		{
			return false;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, Out) && Out.IsValid();
	}

	FString SupabaseObjectToString(const TSharedRef<FJsonObject>& Obj)
	{
		FString Text;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
		FJsonSerializer::Serialize(Obj, Writer);
		Writer->Close();
		return Text;
	}

	/** get_op yaniti: islem KAYITLI ve commit mi; revision'i verir. */
	bool SupabaseParseCommittedOp(const FMMORpcResult& R, int64& OutRevision)
	{
		if (R.Status != EMMORpcStatus::Ok || R.Outcome != TEXT("ok") || !R.Json.IsValid())
		{
			return false;
		}
		bool bFound = false;
		if (!R.Json->TryGetBoolField(TEXT("found"), bFound) || !bFound)
		{
			return false;
		}
		const TSharedPtr<FJsonObject>* Result = nullptr;
		if (!R.Json->TryGetObjectField(TEXT("result"), Result) || !Result || !Result->IsValid())
		{
			return false;
		}
		FString Outcome;
		if (!(*Result)->TryGetStringField(TEXT("outcome"), Outcome) || Outcome != TEXT("committed"))
		{
			return false;
		}
		return SupabaseParseInt64(*Result, TEXT("revision"), OutRevision);
	}
}

struct FMMOSupabaseCharacterStore::FImpl : public TSharedFromThis<FMMOSupabaseCharacterStore::FImpl>
{
	using FStatusComplete = IMMOAsyncCharacterStore::FStatusComplete;

	struct FSlot
	{
		bool bLeased = false;
		int64 Epoch = 0;
		int64 Revision = 0;
		bool bFrozen = false;
		FString FrozenOpId;
		bool bInFlight = false;
		bool bHasPending = false;
		FMMOCharacterSaveData PendingData;
		TArray<FStatusComplete> PendingDone;
	};

	TSharedRef<FMMOSupabaseRestClient> Client;
	FString Holder;
	TMap<FString, FSlot> Slots;

	FImpl(const TSharedRef<FMMOSupabaseRestClient>& InClient, const FString& InHolder)
		: Client(InClient), Holder(InHolder)
	{
	}

	static void Deliver(const TArray<FStatusComplete>& Done, SupabaseOut Outcome)
	{
		const TArray<FStatusComplete> Copy = Done;
		for (const FStatusComplete& Cb : Copy)
		{
			if (Cb)
			{
				Cb(Outcome);
			}
		}
	}

	TSharedRef<FJsonObject> NewParams(const FString& Key, const FString& OpId) const
	{
		const TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
		P->SetStringField(TEXT("p_key"), Key);
		P->SetStringField(TEXT("p_op_id"), OpId);
		return P;
	}

	// ---- LoadEx ----
	void LoadEx(const FString& SlotName, IMMOAsyncCharacterStore::FLoadExComplete Completion)
	{
		FString Key, Display;
		if (!FMMOSupabaseCharacterStore::SlotToKey(SlotName, Key, Display))
		{
			Completion(SupabaseOut::Invalid, FMMOCharacterSaveData());
			return;
		}
		if (const FSlot* Existing = Slots.Find(Key); Existing && Existing->bInFlight)
		{
			Completion(SupabaseOut::Unavailable, FMMOCharacterSaveData()); // ucustaki yazim varken kira yenilenmez
			return;
		}
		const FString OpId = FMMOSupabaseRestClient::NewOpId();
		const TSharedRef<FJsonObject> P = NewParams(Key, OpId);
		P->SetStringField(TEXT("p_holder"), Holder);
		P->SetNumberField(TEXT("p_ttl_s"), SupabaseLeaseTtlSeconds);
		const TWeakPtr<FImpl> Weak = AsShared();
		Client->Call(TEXT("acquire_lease"), P, OpId, [Weak, Key, Completion](FMMORpcResult&& R)
		{
			if (const TSharedPtr<FImpl> Self = Weak.Pin())
			{
				Self->OnLoaded(Key, R, Completion);
			}
		});
	}

	void OnLoaded(const FString& Key, const FMMORpcResult& R, const IMMOAsyncCharacterStore::FLoadExComplete& Completion)
	{
		if (R.Status != EMMORpcStatus::Ok)
		{
			Completion(SupabaseOut::Unavailable, FMMOCharacterSaveData()); // ulasilamadi != "yok"
			return;
		}
		if (!SupabaseIsCommitted(R.Outcome))
		{
			Completion(SupabaseMapOutcome(R.Outcome), FMMOCharacterSaveData());
			return;
		}
		int64 Revision = 0;
		int64 Epoch = 0;
		if (!SupabaseParseInt64(R.Json, TEXT("revision"), Revision) || !SupabaseParseInt64(R.Json, TEXT("lease_epoch"), Epoch))
		{
			Completion(SupabaseOut::Unavailable, FMMOCharacterSaveData());
			return;
		}
		FString DataText;
		const TSharedPtr<FJsonValue>* DataValue = R.Json->Values.Find(TEXT("data"));
		if (DataValue && DataValue->IsValid() && (*DataValue)->Type == EJson::Object)
		{
			DataText = SupabaseObjectToString((*DataValue)->AsObject().ToSharedRef());
		}
		else if (DataValue && DataValue->IsValid() && (*DataValue)->Type == EJson::String)
		{
			DataText = (*DataValue)->AsString();
		}
		FMMOCharacterSaveData Data;
		if (DataText.IsEmpty() || !FMMOCharacterSaveJson::FromJson(DataText, Data))
		{
			Completion(SupabaseOut::Invalid, FMMOCharacterSaveData()); // bozuk kayit: ezme/baslangic paketi yok
			return;
		}
		FSlot& S = Slots.FindOrAdd(Key);
		S.bLeased = true;
		S.Epoch = Epoch;
		S.Revision = Revision;
		S.bFrozen = false; // yetkili yeniden yukleme belirsizligi cozer
		S.FrozenOpId.Reset();
		Completion(SupabaseOut::Ok, MoveTemp(Data));
	}

	// ---- CreateEx ----
	void CreateEx(const FString& SlotName, const FMMOCharacterSaveData& Data, FStatusComplete Completion)
	{
		FString Key, Display;
		TSharedPtr<FJsonObject> DataObj;
		if (!FMMOSupabaseCharacterStore::SlotToKey(SlotName, Key, Display) || !SupabaseDataToObject(Data, DataObj))
		{
			Completion(SupabaseOut::Invalid);
			return;
		}
		const FString OpId = FMMOSupabaseRestClient::NewOpId();
		const TSharedRef<FJsonObject> P = NewParams(Key, OpId);
		P->SetStringField(TEXT("p_display_name"), Display);
		P->SetStringField(TEXT("p_holder"), Holder);
		P->SetNumberField(TEXT("p_schema_version"), FMMOCharacterSaveJson::CurrentSchemaVersion);
		P->SetObjectField(TEXT("p_data"), DataObj);
		P->SetField(TEXT("p_race"), MakeShared<FJsonValueNull>());
		Client->Call(TEXT("create_character"), P, OpId, [Completion](FMMORpcResult&& R)
		{
			if (R.Status == EMMORpcStatus::Ok)
			{
				Completion(SupabaseIsCommitted(R.Outcome) ? SupabaseOut::Ok : SupabaseMapOutcome(R.Outcome));
			}
			else
			{
				Completion(R.Status == EMMORpcStatus::Unknown ? SupabaseOut::Unknown : SupabaseOut::Unavailable);
			}
		});
	}

	// ---- SaveEx ----
	void SaveEx(const FString& SlotName, const FMMOCharacterSaveData& Data, FStatusComplete Completion)
	{
		FString Key, Display;
		if (!FMMOSupabaseCharacterStore::SlotToKey(SlotName, Key, Display))
		{
			Completion(SupabaseOut::Invalid);
			return;
		}
		FSlot* S = Slots.Find(Key);
		if (!S || !S->bLeased)
		{
			Completion(SupabaseOut::NotLeased);
			return;
		}
		if (S->bFrozen)
		{
			Completion(SupabaseOut::Frozen); // belirsiz sonuc: kor yeniden yazim yok
			return;
		}
		if (S->bInFlight)
		{
			// Yazim birlestirme: kuyrukta en cok BIR bekleyen; yenisi eskisinin verisini ezer, geri cagrilar birikir.
			S->PendingData = Data;
			S->bHasPending = true;
			S->PendingDone.Add(Completion);
			return;
		}
		TArray<FStatusComplete> Done;
		Done.Add(Completion);
		StartSave(Key, Data, Done);
	}

	void StartSave(const FString& Key, const FMMOCharacterSaveData& Data, const TArray<FStatusComplete>& Done)
	{
		TSharedPtr<FJsonObject> DataObj;
		if (!SupabaseDataToObject(Data, DataObj))
		{
			FinishSave(Key, Done, SupabaseOut::Invalid);
			return;
		}
		FSlot& S = Slots.FindOrAdd(Key);
		S.bInFlight = true;
		const FString OpId = FMMOSupabaseRestClient::NewOpId();
		const TSharedRef<FJsonObject> P = NewParams(Key, OpId);
		P->SetStringField(TEXT("p_holder"), Holder);
		P->SetStringField(TEXT("p_epoch"), SupabaseInt64Text(S.Epoch));
		P->SetStringField(TEXT("p_expected_revision"), SupabaseInt64Text(S.Revision));
		P->SetNumberField(TEXT("p_schema_version"), FMMOCharacterSaveJson::CurrentSchemaVersion);
		P->SetObjectField(TEXT("p_data"), DataObj);
		const TWeakPtr<FImpl> Weak = AsShared();
		Client->Call(TEXT("save_character"), P, OpId, [Weak, Key, OpId, Done](FMMORpcResult&& R)
		{
			if (const TSharedPtr<FImpl> Self = Weak.Pin())
			{
				Self->OnSaveResult(Key, OpId, R, Done);
			}
		});
	}

	void OnSaveResult(const FString& Key, const FString& OpId, const FMMORpcResult& R, const TArray<FStatusComplete>& Done)
	{
		if (R.Status == EMMORpcStatus::Ok)
		{
			if (R.Outcome == TEXT("committed"))
			{
				int64 Revision = 0;
				if (SupabaseParseInt64(R.Json, TEXT("revision"), Revision))
				{
					Slots.FindOrAdd(Key).Revision = Revision;
					FinishSave(Key, Done, SupabaseOut::Ok);
				}
				else
				{
					Freeze(Key, OpId); // "committed" ama revision okunamadi: sonuc belirsiz
					FinishSave(Key, Done, SupabaseOut::Unknown);
				}
				return;
			}
			const SupabaseOut Mapped = SupabaseMapOutcome(R.Outcome);
			if (Mapped == SupabaseOut::OwnershipLost)
			{
				Slots.FindOrAdd(Key).bLeased = false;
			}
			FinishSave(Key, Done, Mapped);
			return;
		}
		if (R.Status == EMMORpcStatus::Failed)
		{
			FinishSave(Key, Done, SupabaseOut::Unavailable); // kesin: uygulanmadi
			return;
		}
		// Belirsiz: ayni op_id'yi get_op ile uzlastir.
		const FString ReconcileOp = FMMOSupabaseRestClient::NewOpId();
		const TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
		P->SetStringField(TEXT("p_op_id"), OpId);
		const TWeakPtr<FImpl> Weak = AsShared();
		Client->Call(TEXT("get_op"), P, ReconcileOp, [Weak, Key, OpId, Done](FMMORpcResult&& G)
		{
			const TSharedPtr<FImpl> Self = Weak.Pin();
			if (!Self)
			{
				return;
			}
			int64 Revision = 0;
			if (SupabaseParseCommittedOp(G, Revision))
			{
				Self->Slots.FindOrAdd(Key).Revision = Revision;
				Self->FinishSave(Key, Done, SupabaseOut::Ok);
			}
			else
			{
				Self->Freeze(Key, OpId);
				Self->FinishSave(Key, Done, SupabaseOut::Unknown);
			}
		});
	}

	void Freeze(const FString& Key, const FString& OpId)
	{
		FSlot& S = Slots.FindOrAdd(Key);
		S.bFrozen = true;
		S.FrozenOpId = OpId;
		UE_LOG(LogMMOSupabase, Warning, TEXT("yuva donduruldu (belirsiz yazim sonucu) op=%s"), *OpId);
	}

	void FinishSave(const FString& Key, const TArray<FStatusComplete>& Done, SupabaseOut Outcome)
	{
		TArray<FStatusComplete> PendingDone;
		FMMOCharacterSaveData PendingData;
		bool bHasPending = false;
		bool bCanContinue = false;
		bool bFrozen = false;
		if (FSlot* S = Slots.Find(Key))
		{
			S->bInFlight = false;
			bFrozen = S->bFrozen;
			bCanContinue = Outcome == SupabaseOut::Ok && S->bLeased && !S->bFrozen;
			if (S->bHasPending)
			{
				bHasPending = true;
				PendingData = MoveTemp(S->PendingData);
				PendingDone = MoveTemp(S->PendingDone);
				S->bHasPending = false;
				S->PendingDone.Reset();
			}
		}
		Deliver(Done, Outcome);
		if (!bHasPending)
		{
			return;
		}
		if (bCanContinue)
		{
			StartSave(Key, PendingData, PendingDone);
		}
		else
		{
			Deliver(PendingDone, bFrozen ? SupabaseOut::Frozen : (Outcome == SupabaseOut::Ok ? SupabaseOut::NotLeased : Outcome));
		}
	}

	// ---- ExistsEx ----
	void ExistsEx(const FString& SlotName, IMMOAsyncCharacterStore::FExistsComplete Completion)
	{
		FString Key, Display;
		if (!FMMOSupabaseCharacterStore::SlotToKey(SlotName, Key, Display))
		{
			Completion(SupabaseOut::Invalid, false);
			return;
		}
		const FString OpId = FMMOSupabaseRestClient::NewOpId();
		Client->Call(TEXT("character_exists"), NewParams(Key, OpId), OpId, [Completion](FMMORpcResult&& R)
		{
			bool bExists = false;
			if (R.Status != EMMORpcStatus::Ok)
			{
				Completion(SupabaseOut::Unavailable, false);
			}
			else if (R.Outcome == TEXT("ok") && R.Json.IsValid() && R.Json->TryGetBoolField(TEXT("exists"), bExists))
			{
				Completion(SupabaseOut::Ok, bExists);
			}
			else
			{
				Completion(R.Outcome == TEXT("ok") ? SupabaseOut::Unavailable : SupabaseMapOutcome(R.Outcome), false);
			}
		});
	}

	// ---- ReconcileEx ----
	void ReconcileEx(const FString& SlotName, FStatusComplete Completion)
	{
		FString Key, Display;
		if (!FMMOSupabaseCharacterStore::SlotToKey(SlotName, Key, Display))
		{
			Completion(SupabaseOut::Invalid);
			return;
		}
		const FSlot* S = Slots.Find(Key);
		if (!S || !S->bFrozen)
		{
			Completion(SupabaseOut::Ok);
			return;
		}
		const FString OpId = S->FrozenOpId;
		const TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
		P->SetStringField(TEXT("p_op_id"), OpId);
		const TWeakPtr<FImpl> Weak = AsShared();
		Client->Call(TEXT("get_op"), P, FMMOSupabaseRestClient::NewOpId(), [Weak, Key, OpId, Completion](FMMORpcResult&& G)
		{
			const TSharedPtr<FImpl> Self = Weak.Pin();
			if (!Self)
			{
				return;
			}
			int64 Revision = 0;
			FSlot* Slot = Self->Slots.Find(Key);
			if (Slot && Slot->bFrozen && Slot->FrozenOpId == OpId && SupabaseParseCommittedOp(G, Revision))
			{
				Slot->Revision = Revision;
				Slot->bFrozen = false;
				Slot->FrozenOpId.Reset();
				Completion(SupabaseOut::Ok);
			}
			else
			{
				Completion(SupabaseOut::Unknown); // bulunamadi/ulasilamadi: DONUK kalir (yetkili yeniden yukleme gerekir)
			}
		});
	}
};

FMMOSupabaseCharacterStore::FMMOSupabaseCharacterStore(const TSharedRef<FMMOSupabaseRestClient>& InClient, const FString& InHolder)
	: Impl(MakeShared<FImpl>(InClient, InHolder))
{
}

FMMOSupabaseCharacterStore::~FMMOSupabaseCharacterStore() = default;

bool FMMOSupabaseCharacterStore::SlotToKey(const FString& SlotName, FString& OutKey, FString& OutDisplayName)
{
	FString Name = SlotName;
	if (Name.StartsWith(TEXT("MMOSave_"), ESearchCase::IgnoreCase))
	{
		Name.RightChopInline(8);
	}
	// Ham ad denetlenir: SlotKey izinsiz karakteri sessizce atip BASKA bir adin anahtarini uretirdi.
	if (Name.IsEmpty() || Name.Len() > SupabaseMaxNameLen || !MMONameRules::HasOnlyAllowedChars(Name) || MMONameRules::IsReservedName(Name))
	{
		return false;
	}
	OutKey = MMONameRules::CanonicalKey(Name);
	OutDisplayName = Name;
	return true;
}

bool FMMOSupabaseCharacterStore::Load(const FString& SlotName, FMMOCharacterSaveData& Out)
{
	UE_LOG(LogMMOSupabase, Error, TEXT("Senkron Load Supabase deposunda KAPALI-BASARISIZ: LoadEx kullanin"));
	return false;
}

bool FMMOSupabaseCharacterStore::Save(const FString& SlotName, const FMMOCharacterSaveData& In)
{
	UE_LOG(LogMMOSupabase, Error, TEXT("Senkron Save Supabase deposunda KAPALI-BASARISIZ: SaveEx kullanin"));
	return false;
}

bool FMMOSupabaseCharacterStore::Exists(const FString& SlotName)
{
	UE_LOG(LogMMOSupabase, Error, TEXT("Senkron Exists Supabase deposunda KAPALI-BASARISIZ: ExistsEx kullanin"));
	return false;
}

void FMMOSupabaseCharacterStore::ListSlotNames(TArray<FString>& Out)
{
	Out.Reset();
	UE_LOG(LogMMOSupabase, Error, TEXT("Senkron ListSlotNames Supabase deposunda KAPALI-BASARISIZ"));
}

void FMMOSupabaseCharacterStore::LoadEx(const FString& SlotName, FLoadExComplete&& Completion)
{
	Impl->LoadEx(SlotName, MoveTemp(Completion));
}

void FMMOSupabaseCharacterStore::CreateEx(const FString& SlotName, const FMMOCharacterSaveData& Data, FStatusComplete&& Completion)
{
	Impl->CreateEx(SlotName, Data, MoveTemp(Completion));
}

void FMMOSupabaseCharacterStore::SaveEx(const FString& SlotName, const FMMOCharacterSaveData& Data, FStatusComplete&& Completion)
{
	Impl->SaveEx(SlotName, Data, MoveTemp(Completion));
}

void FMMOSupabaseCharacterStore::ExistsEx(const FString& SlotName, FExistsComplete&& Completion)
{
	Impl->ExistsEx(SlotName, MoveTemp(Completion));
}

void FMMOSupabaseCharacterStore::ReconcileEx(const FString& SlotName, FStatusComplete&& Completion)
{
	Impl->ReconcileEx(SlotName, MoveTemp(Completion));
}

bool FMMOSupabaseCharacterStore::IsSlotFrozen(const FString& SlotName) const
{
	FString Key, Display;
	if (!SlotToKey(SlotName, Key, Display))
	{
		return false;
	}
	const FImpl::FSlot* S = Impl->Slots.Find(Key);
	return S && S->bFrozen;
}

bool FMMOSupabaseCharacterStore::TryGetSlotInfo(const FString& SlotName, int64& OutEpoch, int64& OutRevision) const
{
	FString Key, Display;
	if (!SlotToKey(SlotName, Key, Display))
	{
		return false;
	}
	const FImpl::FSlot* S = Impl->Slots.Find(Key);
	if (!S || !S->bLeased)
	{
		return false;
	}
	OutEpoch = S->Epoch;
	OutRevision = S->Revision;
	return true;
}

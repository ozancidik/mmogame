// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOSupabaseCharacterStore.h"
#include "Core/MMONameRules.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Save/MMOCharacterSaveJson.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Sozlesme: supabase/tests/vectors.json (contract_version 2) TEK DOGRULUK KAYNAGI. Parametre kumesi rpc_params ile birebir:
// p_op_id YALNIZ create_character / save_character / get_op'ta; bigint alanlar (epoch, revision) JSON SAYI.
namespace
{
	using SupabaseOut = EMMOSupabaseStoreOutcome;

	constexpr int32 SupabaseMaxNameLen = 32;
	constexpr int32 SupabaseMaxHolderLen = 96;
	constexpr int32 SupabaseLeaseTtlSeconds = 120;

	/** int64: 2^53'e kadar ondalik-siz JSON SAYISI (sozlesme: bigint JSON sayidir). Metin ve digerleri RED. */
	bool SupabaseParseInt64(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Name, int64& Out)
	{
		if (!Obj.IsValid())
		{
			return false;
		}
		const TSharedPtr<FJsonValue>* Value = Obj->Values.Find(Name);
		if (!Value || !Value->IsValid() || (*Value)->Type != EJson::Number)
		{
			return false;
		}
		const double Number = (*Value)->AsNumber();
		if (Number < 0.0 || Number > 9007199254740992.0 || Number != FMath::FloorToDouble(Number))
		{
			return false;
		}
		Out = static_cast<int64>(Number);
		return true;
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

	/** vectors.json limits.name_regex ile birebir: 1..32, [A-Za-z0-9 _-], bas ve son karakter bosluk degil. */
	bool SupabaseIsValidName(const FString& Name)
	{
		if (Name.IsEmpty() || Name.Len() > SupabaseMaxNameLen || !MMONameRules::HasOnlyAllowedChars(Name))
		{
			return false;
		}
		return Name[0] != TEXT(' ') && Name[Name.Len() - 1] != TEXT(' ');
	}

	/**
	 * get_op yaniti (vectors.json responses.get_op): found:true + outcome:ok + result nesnesi. Islem turu ve anahtar BEKLENENLE
	 * ayni olmali (baska bir islemin kaydi uzlasti sayilmaz); result.outcome committed ise revision'i verir.
	 */
	bool SupabaseParseCommittedOp(const FMMORpcResult& R, const TCHAR* ExpectedKind, const FString& ExpectedKey, int64& OutRevision)
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
		FString Kind;
		FString Key;
		if (!R.Json->TryGetStringField(TEXT("op_kind"), Kind) || Kind != ExpectedKind
			|| !R.Json->TryGetStringField(TEXT("canonical_key"), Key) || Key != ExpectedKey)
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

	/** Depo silinmis: ham geri cagri GUVENLE cagrilmaz (sahibi gitmis olabilir); sonuc yalniz loglanir (Dilim 4 notu). */
	void SupabaseLogOrphan(const TCHAR* Function, const FMMORpcResult& R)
	{
		UE_LOG(LogMMOSupabase, Warning, TEXT("tamamlanma geri cagrisi atlandi (depo silindi) fn=%s op=%s durum=%d hata=%d http=%d"),
			Function, *R.OpId, static_cast<int32>(R.Status), static_cast<int32>(R.Error), R.HttpCode);
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
		FString FrozenKind; // "save" | "create": get_op op_kind beklentisi
		bool bInFlight = false;
		bool bHasPending = false;
		FMMOCharacterSaveData PendingData;
		TArray<FStatusComplete> PendingDone;
	};

	TSharedRef<FMMOSupabaseRestClient> Client;
	FString Holder;
	bool bHolderOk = false;
	TMap<FString, FSlot> Slots;

	FImpl(const TSharedRef<FMMOSupabaseRestClient>& InClient, const FString& InHolder)
		: Client(InClient), Holder(InHolder), bHolderOk(FMMOSupabaseCharacterStore::IsValidHolder(InHolder))
	{
		if (!bHolderOk)
		{
			UE_LOG(LogMMOSupabase, Warning, TEXT("Supabase deposu: holder kurala uymuyor, hicbir istek gonderilmeyecek"));
		}
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

	/** Yalniz p_key: p_op_id yalniz create/save/get_op'ta vardir (vectors.json rpc_params), cagiran ekler. */
	TSharedRef<FJsonObject> NewParams(const FString& Key) const
	{
		const TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
		P->SetStringField(TEXT("p_key"), Key);
		return P;
	}

	/** Gecerli ad + gecerli holder; aksi halde hicbir istek gonderilmez. */
	bool Resolve(const FString& SlotName, FString& OutKey, FString& OutDisplay) const
	{
		return bHolderOk && FMMOSupabaseCharacterStore::SlotToKey(SlotName, OutKey, OutDisplay);
	}

	// ---- LoadEx ----
	void LoadEx(const FString& SlotName, IMMOAsyncCharacterStore::FLoadExComplete Completion)
	{
		FString Key, Display;
		if (!Resolve(SlotName, Key, Display))
		{
			Completion(SupabaseOut::Invalid, FMMOCharacterSaveData());
			return;
		}
		if (const FSlot* Existing = Slots.Find(Key); Existing && Existing->bInFlight)
		{
			Completion(SupabaseOut::Unavailable, FMMOCharacterSaveData()); // ucustaki yazim varken kira yenilenmez
			return;
		}
		const FString LogOp = FMMOSupabaseRestClient::NewOpId(); // yalniz log icin; acquire_lease'e GONDERILMEZ
		const TSharedRef<FJsonObject> P = NewParams(Key);
		P->SetStringField(TEXT("p_holder"), Holder);
		P->SetNumberField(TEXT("p_ttl_s"), SupabaseLeaseTtlSeconds);
		const TWeakPtr<FImpl> Weak = AsShared();
		Client->Call(TEXT("acquire_lease"), P, LogOp, [Weak, Key, Completion](FMMORpcResult&& R)
		{
			if (const TSharedPtr<FImpl> Self = Weak.Pin())
			{
				Self->OnLoaded(Key, R, Completion);
			}
			else
			{
				SupabaseLogOrphan(TEXT("acquire_lease"), R);
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
		if (R.Outcome != TEXT("ok"))
		{
			Completion(SupabaseMapOutcome(R.Outcome), FMMOCharacterSaveData());
			return;
		}
		int64 Revision = 0;
		int64 Epoch = 0;
		FString RespKey;
		// Sozlesme 2: kira donemi alani `lease_epoch` (eski `epoch` takma adi OKUNMAZ).
		if (!SupabaseParseInt64(R.Json, TEXT("revision"), Revision) || !SupabaseParseInt64(R.Json, TEXT("lease_epoch"), Epoch) || Epoch < 1
			|| !R.Json->TryGetStringField(TEXT("canonical_key"), RespKey) || RespKey != Key)
		{
			Completion(SupabaseOut::Unavailable, FMMOCharacterSaveData());
			return;
		}
		const TSharedPtr<FJsonValue>* DataValue = R.Json->Values.Find(TEXT("data"));
		FString DataText;
		if (DataValue && DataValue->IsValid() && (*DataValue)->Type == EJson::Object)
		{
			DataText = SupabaseObjectToString((*DataValue)->AsObject().ToSharedRef());
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
		S.FrozenKind.Reset();
		Completion(SupabaseOut::Ok, MoveTemp(Data));
	}

	// ---- CreateEx ----
	void CreateEx(const FString& SlotName, const FMMOCharacterSaveData& Data, FStatusComplete Completion)
	{
		FString Key, Display;
		TSharedPtr<FJsonObject> DataObj;
		if (!Resolve(SlotName, Key, Display) || !SupabaseDataToObject(Data, DataObj))
		{
			Completion(SupabaseOut::Invalid);
			return;
		}
		const FString OpId = FMMOSupabaseRestClient::NewOpId();
		const TSharedRef<FJsonObject> P = NewParams(Key);
		P->SetStringField(TEXT("p_display_name"), Display);
		P->SetStringField(TEXT("p_holder"), Holder);
		P->SetStringField(TEXT("p_op_id"), OpId);
		P->SetNumberField(TEXT("p_schema_version"), FMMOCharacterSaveJson::CurrentSchemaVersion);
		P->SetObjectField(TEXT("p_data"), DataObj);
		P->SetField(TEXT("p_race"), MakeShared<FJsonValueNull>());
		const TWeakPtr<FImpl> Weak = AsShared();
		Client->Call(TEXT("create_character"), P, OpId, [Weak, Key, OpId, Completion](FMMORpcResult&& R)
		{
			const TSharedPtr<FImpl> Self = Weak.Pin();
			if (!Self)
			{
				SupabaseLogOrphan(TEXT("create_character"), R);
				return;
			}
			if (R.Status == EMMORpcStatus::Ok)
			{
				Completion(SupabaseIsCommitted(R.Outcome) ? SupabaseOut::Ok : SupabaseMapOutcome(R.Outcome));
			}
			else if (R.Status == EMMORpcStatus::Unknown)
			{
				Self->ReconcileCreate(Key, OpId, Completion); // belirsiz: get_op ile uzlastir
			}
			else
			{
				Completion(SupabaseOut::Unavailable);
			}
		});
	}

	void ReconcileCreate(const FString& Key, const FString& OpId, const FStatusComplete& Completion)
	{
		const TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
		P->SetStringField(TEXT("p_op_id"), OpId);
		const TWeakPtr<FImpl> Weak = AsShared();
		Client->Call(TEXT("get_op"), P, FMMOSupabaseRestClient::NewOpId(), [Weak, Key, OpId, Completion](FMMORpcResult&& G)
		{
			const TSharedPtr<FImpl> Self = Weak.Pin();
			if (!Self)
			{
				SupabaseLogOrphan(TEXT("get_op"), G);
				return;
			}
			int64 Revision = 0;
			if (SupabaseParseCommittedOp(G, TEXT("create"), Key, Revision))
			{
				Completion(SupabaseOut::Ok);
			}
			else
			{
				Self->Freeze(Key, OpId, TEXT("create")); // commit edilmedi mi, gec mi gelecek: bilinmiyor
				Completion(SupabaseOut::Unknown);
			}
		});
	}

	// ---- SaveEx ----
	void SaveEx(const FString& SlotName, const FMMOCharacterSaveData& Data, FStatusComplete Completion)
	{
		FString Key, Display;
		if (!Resolve(SlotName, Key, Display))
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
		const TSharedRef<FJsonObject> P = NewParams(Key);
		P->SetStringField(TEXT("p_holder"), Holder);
		P->SetNumberField(TEXT("p_epoch"), static_cast<double>(S.Epoch));
		P->SetNumberField(TEXT("p_expected_revision"), static_cast<double>(S.Revision));
		P->SetStringField(TEXT("p_op_id"), OpId);
		P->SetNumberField(TEXT("p_schema_version"), FMMOCharacterSaveJson::CurrentSchemaVersion);
		P->SetObjectField(TEXT("p_data"), DataObj);
		const TWeakPtr<FImpl> Weak = AsShared();
		Client->Call(TEXT("save_character"), P, OpId, [Weak, Key, OpId, Done](FMMORpcResult&& R)
		{
			if (const TSharedPtr<FImpl> Self = Weak.Pin())
			{
				Self->OnSaveResult(Key, OpId, R, Done);
			}
			else
			{
				SupabaseLogOrphan(TEXT("save_character"), R);
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
					Freeze(Key, OpId, TEXT("save")); // "committed" ama revision okunamadi: sonuc belirsiz
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
		const TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
		P->SetStringField(TEXT("p_op_id"), OpId);
		const TWeakPtr<FImpl> Weak = AsShared();
		Client->Call(TEXT("get_op"), P, FMMOSupabaseRestClient::NewOpId(), [Weak, Key, OpId, Done](FMMORpcResult&& G)
		{
			const TSharedPtr<FImpl> Self = Weak.Pin();
			if (!Self)
			{
				SupabaseLogOrphan(TEXT("get_op"), G);
				return;
			}
			int64 Revision = 0;
			if (SupabaseParseCommittedOp(G, TEXT("save"), Key, Revision))
			{
				Self->Slots.FindOrAdd(Key).Revision = Revision;
				Self->FinishSave(Key, Done, SupabaseOut::Ok);
			}
			else
			{
				Self->Freeze(Key, OpId, TEXT("save"));
				Self->FinishSave(Key, Done, SupabaseOut::Unknown);
			}
		});
	}

	void Freeze(const FString& Key, const FString& OpId, const TCHAR* Kind)
	{
		FSlot& S = Slots.FindOrAdd(Key);
		S.bFrozen = true;
		S.FrozenOpId = OpId;
		S.FrozenKind = Kind;
		UE_LOG(LogMMOSupabase, Warning, TEXT("yuva donduruldu (belirsiz %s sonucu) op=%s"), Kind, *OpId);
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
		const FString LogOp = FMMOSupabaseRestClient::NewOpId();
		const TWeakPtr<FImpl> Weak = AsShared();
		Client->Call(TEXT("character_exists"), NewParams(Key), LogOp, [Weak, Completion](FMMORpcResult&& R)
		{
			if (!Weak.Pin().IsValid())
			{
				SupabaseLogOrphan(TEXT("character_exists"), R);
				return;
			}
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
		const FString Kind = S->FrozenKind;
		const TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
		P->SetStringField(TEXT("p_op_id"), OpId);
		const TWeakPtr<FImpl> Weak = AsShared();
		Client->Call(TEXT("get_op"), P, FMMOSupabaseRestClient::NewOpId(), [Weak, Key, OpId, Kind, Completion](FMMORpcResult&& G)
		{
			const TSharedPtr<FImpl> Self = Weak.Pin();
			if (!Self)
			{
				SupabaseLogOrphan(TEXT("get_op"), G);
				return;
			}
			int64 Revision = 0;
			FSlot* Slot = Self->Slots.Find(Key);
			if (Slot && Slot->bFrozen && Slot->FrozenOpId == OpId && SupabaseParseCommittedOp(G, *Kind, Key, Revision))
			{
				if (Kind == TEXT("save"))
				{
					Slot->Revision = Revision;
				}
				Slot->bFrozen = false;
				Slot->FrozenOpId.Reset();
				Slot->FrozenKind.Reset();
				Completion(SupabaseOut::Ok);
			}
			else
			{
				Completion(SupabaseOut::Unknown); // bulunamadi/ulasilamadi: DONUK kalir (yetkili yeniden yukleme gerekir)
			}
		});
	}

	// ---- RenewLeaseEx ----
	void RenewLeaseEx(const FString& SlotName, FStatusComplete Completion)
	{
		FString Key, Display;
		if (!Resolve(SlotName, Key, Display))
		{
			Completion(SupabaseOut::Invalid);
			return;
		}
		const FSlot* S = Slots.Find(Key);
		if (!S || !S->bLeased)
		{
			Completion(SupabaseOut::NotLeased);
			return;
		}
		const int64 SentEpoch = S->Epoch;
		const TSharedRef<FJsonObject> P = NewParams(Key);
		P->SetStringField(TEXT("p_holder"), Holder);
		P->SetNumberField(TEXT("p_epoch"), static_cast<double>(SentEpoch));
		P->SetNumberField(TEXT("p_ttl_s"), SupabaseLeaseTtlSeconds);
		const TWeakPtr<FImpl> Weak = AsShared();
		Client->Call(TEXT("renew_lease"), P, FMMOSupabaseRestClient::NewOpId(), [Weak, Key, SentEpoch, Completion](FMMORpcResult&& R)
		{
			const TSharedPtr<FImpl> Self = Weak.Pin();
			if (!Self)
			{
				SupabaseLogOrphan(TEXT("renew_lease"), R);
				return;
			}
			Completion(Self->OnLeaseResult(Key, SentEpoch, R, false));
		});
	}

	// ---- ReleaseLeaseEx ----
	void ReleaseLeaseEx(const FString& SlotName, FStatusComplete Completion)
	{
		FString Key, Display;
		if (!Resolve(SlotName, Key, Display))
		{
			Completion(SupabaseOut::Invalid);
			return;
		}
		const FSlot* S = Slots.Find(Key);
		if (!S || !S->bLeased)
		{
			Completion(SupabaseOut::NotLeased);
			return;
		}
		if (S->bInFlight)
		{
			Completion(SupabaseOut::Unavailable); // ucustaki yazim bitmeden birakilmaz
			return;
		}
		const int64 SentEpoch = S->Epoch;
		const TSharedRef<FJsonObject> P = NewParams(Key);
		P->SetStringField(TEXT("p_holder"), Holder);
		P->SetNumberField(TEXT("p_epoch"), static_cast<double>(SentEpoch));
		const TWeakPtr<FImpl> Weak = AsShared();
		Client->Call(TEXT("release_lease"), P, FMMOSupabaseRestClient::NewOpId(), [Weak, Key, SentEpoch, Completion](FMMORpcResult&& R)
		{
			const TSharedPtr<FImpl> Self = Weak.Pin();
			if (!Self)
			{
				SupabaseLogOrphan(TEXT("release_lease"), R);
				return;
			}
			Completion(Self->OnLeaseResult(Key, SentEpoch, R, true));
		});
	}

	/** renew/release ortak sonuc: kira bilgisi yalniz KESIN sonucla degisir; ulasilamazsa korunur (yeniden denenebilir). */
	SupabaseOut OnLeaseResult(const FString& Key, int64 SentEpoch, const FMMORpcResult& R, bool bRelease)
	{
		if (R.Status != EMMORpcStatus::Ok)
		{
			return SupabaseOut::Unavailable;
		}
		FSlot* S = Slots.Find(Key);
		const bool bSameLease = S && S->bLeased && S->Epoch == SentEpoch;
		if (R.Outcome == TEXT("ok"))
		{
			int64 Epoch = 0;
			if (!SupabaseParseInt64(R.Json, TEXT("lease_epoch"), Epoch) || Epoch != SentEpoch)
			{
				return SupabaseOut::Unavailable; // yanit sozlesmeye uymuyor: durum degistirme
			}
			if (bRelease && bSameLease)
			{
				S->bLeased = false;
			}
			return SupabaseOut::Ok;
		}
		const SupabaseOut Mapped = SupabaseMapOutcome(R.Outcome);
		if ((Mapped == SupabaseOut::OwnershipLost || Mapped == SupabaseOut::NotFound) && bSameLease)
		{
			S->bLeased = false; // kira yitirildi: yazim yok
		}
		return Mapped;
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
		Name.RightChopInline(8); // yalniz bir kez
	}
	// Ham ad denetlenir: SlotKey izinsiz karakteri/bosluklari sessizce atip BASKA bir adin anahtarini uretirdi.
	if (!SupabaseIsValidName(Name) || MMONameRules::IsReservedName(Name))
	{
		return false;
	}
	OutKey = MMONameRules::CanonicalKey(Name);
	OutDisplayName = Name;
	return true;
}

bool FMMOSupabaseCharacterStore::IsValidHolder(const FString& Holder)
{
	if (Holder.IsEmpty() || Holder.Len() > SupabaseMaxHolderLen)
	{
		return false;
	}
	for (const TCHAR Ch : Holder)
	{
		if (!(MMONameRules::IsAsciiAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('.') || Ch == TEXT(':') || Ch == TEXT('-')))
		{
			return false;
		}
	}
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

void FMMOSupabaseCharacterStore::RenewLeaseEx(const FString& SlotName, FStatusComplete&& Completion)
{
	Impl->RenewLeaseEx(SlotName, MoveTemp(Completion));
}

void FMMOSupabaseCharacterStore::ReleaseLeaseEx(const FString& SlotName, FStatusComplete&& Completion)
{
	Impl->ReleaseLeaseEx(SlotName, MoveTemp(Completion));
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

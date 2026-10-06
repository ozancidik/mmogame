// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOCharacterStoreSubsystem.h"
#include "Save/MMOLocalFileCharacterStore.h"
#include "Save/MMOCharacterSaveJson.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/OutputDevice.h"
#include "Misc/CoreMisc.h"

#if !UE_BUILD_SHIPPING
namespace
{
	// Hata ayiklama konsol komutu: "MMOTestStore". Pawn gerektirmez (ExecCmds ile calisir).
	// Diskteki TUM karakter yuvalarini magaza uzerinden listeler/yukler (yalniz okur), ilk gecerli
	// yuvayla gecici bir yuvaya Save -> Load gidis-donus yapar ve gecici dosyayi siler.
	// Cikti: "[MMO_STORE_TEST] ..." satirlari (kayit formati/eski .sav uyumu icin kanit).
	struct FMMOStoreTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestStore")))
			{
				return false;
			}
			UWorld* World = InWorld;
			if (!World && GEngine)
			{
				for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
				{
					if (Ctx.World() && Ctx.World()->IsGameWorld())
					{
						World = Ctx.World();
						break;
					}
				}
			}
			IMMOCharacterStore* Store = UMMOCharacterStoreSubsystem::Get(World);
			if (!Store)
			{
				UE_LOG(LogTemp, Error, TEXT("[MMO_STORE_TEST] FAIL store unavailable"));
				return true;
			}
			TArray<FString> Slots;
			Store->ListSlotNames(Slots);
			int32 Loaded = 0, Invalid = 0, Failed = 0, ExistsMismatch = 0;
			// Gercek .sav -> ToJson -> FromJson -> alan alan esitlik (v2 JSON eski kayitlarla uyumlu mu).
			int32 JsonOk = 0, JsonMismatch = 0, MaxLevel = 0, MaxJsonBytes = 0;
			FMMOCharacterSaveData First;
			FString FirstSlot;
			for (const FString& Slot : Slots)
			{
				FMMOCharacterSaveData D;
				if (!Store->Load(Slot, D))
				{
					++Failed;
				}
				else if (!D.bValid)
				{
					++Invalid;
				}
				else
				{
					++Loaded;
					MaxLevel = FMath::Max(MaxLevel, D.Level);
					FString Json;
					FString JsonError;
					FMMOCharacterSaveData FromJsonData;
					TArray<FString> Differences;
					if (FMMOCharacterSaveJson::ToJson(D, Json) && FMMOCharacterSaveJson::FromJson(Json, FromJsonData, nullptr, &JsonError))
					{
						FMMOCharacterSaveJson::FindDifferences(D, FromJsonData, Differences);
					}
					else
					{
						Differences.Add(JsonError.IsEmpty() ? TEXT("ToJson/FromJson basarisiz") : JsonError);
					}
					MaxJsonBytes = FMath::Max(MaxJsonBytes, Json.Len());
					if (Differences.IsEmpty())
					{
						++JsonOk;
					}
					else
					{
						++JsonMismatch;
						UE_LOG(LogTemp, Error, TEXT("[MMO_STORE_TEST] FAIL json mismatch slot=%s fields=%s"),
							*Slot, *FString::Join(Differences, TEXT(",")));
					}
					// En yuksek seviyeli gecerli kaydi gidis-donus icin sec (en dolu veri).
					if (FirstSlot.IsEmpty() || D.Level > First.Level)
					{
						First = D;
						FirstSlot = Slot;
					}
				}
				if (!Store->Exists(Slot))
				{
					++ExistsMismatch;
				}
			}
			UE_LOG(LogTemp, Display, TEXT("[MMO_STORE_TEST] slots=%d loaded=%d invalid=%d loadFailed=%d existsMismatch=%d"),
				Slots.Num(), Loaded, Invalid, Failed, ExistsMismatch);
			UE_LOG(LogTemp, Display, TEXT("[MMO_STORE_TEST] json ok=%d mismatch=%d maxLevel=%d maxJsonChars=%d"),
				JsonOk, JsonMismatch, MaxLevel, MaxJsonBytes);

			// Yuva basina yazim sirasi: yeni yazim ayni yuvadaki (buyuk/kucuk harf duyarsiz) eski numarayi
			// "eski" yapar, baska yuva etkilenmez, sonradan baslayan yazim en yeni olur.
			{
				const TSharedRef<FMMOSlotWriteTracker>& Tracker = Store->GetWriteTracker();
				const uint64 First1 = Tracker->BeginWrite(TEXT("MMOSave_WriteOrderA"));
				const uint64 Other = Tracker->BeginWrite(TEXT("MMOSave_WriteOrderB"));
				const bool bFreshOk = Tracker->IsNewest(TEXT("MMOSave_WriteOrderA"), First1);
				const uint64 Second = Tracker->BeginWrite(TEXT("mmosave_writeordera"));
				const bool bOrderOk = bFreshOk && !Tracker->IsNewest(TEXT("MMOSave_WriteOrderA"), First1)
					&& Tracker->IsNewest(TEXT("MMOSave_WriteOrderA"), Second) && Second > First1
					&& Tracker->IsNewest(TEXT("MMOSave_WriteOrderB"), Other)
					&& !Tracker->IsNewest(TEXT("MMOSave_WriteOrderNeverWritten"), Second);
				UE_LOG(LogTemp, Display, TEXT("[MMO_STORE_TEST] writeorder ok=%d"), bOrderOk ? 1 : 0);
			}

			if (!FirstSlot.IsEmpty())
			{
				const FString TempSlot = TEXT("MMOSave_ZStoreTestTemp");
				FMMOCharacterSaveData Back;
				const bool bSaved = Store->Save(TempSlot, First);
				const bool bLoaded = bSaved && Store->Load(TempSlot, Back);
				// Tum SaveGame alanlari (Items/Equipment/Warehouse/Skills/Quests/Gold/Cooldown dahil).
				TArray<FString> RoundTripDiffs;
				if (bLoaded)
				{
					FMMOCharacterSaveJson::FindDifferences(First, Back, RoundTripDiffs);
				}
				const bool bEqual = bLoaded && RoundTripDiffs.IsEmpty();
				UE_LOG(LogTemp, Display, TEXT("[MMO_STORE_TEST] roundtrip source=%s level=%d saved=%d loaded=%d equal=%d existsAfterSave=%d"),
					*FirstSlot, First.Level, bSaved ? 1 : 0, bLoaded ? 1 : 0, bEqual ? 1 : 0, Store->Exists(TempSlot) ? 1 : 0);
				IFileManager::Get().Delete(*(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (TempSlot + TEXT(".sav"))));
			}
			return true;
		}
	};
	FMMOStoreTestExec GMMOStoreTestExec;
}
#endif

void UMMOCharacterStoreSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Store = MakeUnique<FMMOLocalFileCharacterStore>();
}

void UMMOCharacterStoreSubsystem::Deinitialize()
{
	Store.Reset();
	Super::Deinitialize();
}

IMMOCharacterStore* UMMOCharacterStoreSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UMMOCharacterStoreSubsystem* Subsystem = GI ? GI->GetSubsystem<UMMOCharacterStoreSubsystem>() : nullptr;
	return Subsystem ? Subsystem->GetStore() : nullptr;
}

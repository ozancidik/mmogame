// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOCharacterStoreSubsystem.h"
#include "Save/MMOLocalFileCharacterStore.h"
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

			if (!FirstSlot.IsEmpty())
			{
				const FString TempSlot = TEXT("MMOSave_ZStoreTestTemp");
				FMMOCharacterSaveData Back;
				const bool bSaved = Store->Save(TempSlot, First);
				const bool bLoaded = bSaved && Store->Load(TempSlot, Back);
				const bool bEqual = bLoaded && Back.bValid == First.bValid && Back.Level == First.Level
					&& Back.Experience == First.Experience && Back.ClanName == First.ClanName
					&& Back.MapName == First.MapName && Back.Location.Equals(First.Location, 0.01)
					&& Back.Friends == First.Friends && Back.CharacterClass == First.CharacterClass;
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

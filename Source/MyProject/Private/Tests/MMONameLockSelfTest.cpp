// Copyright Epic Games, Inc. All Rights Reserved.

// K-1 "guvenlik tur 2" ad kilidi testleri (SECURITY_AUDIT K-1 F1/F5/F3):
//  F1: uzak (yonetici disi) oyuncu, ActiveSaveSlot'u DISKTE kayitli karakterin adini degistiremez; kayitsiz yeni karakter ve
//      yuklemesi bitmemis (bos ActiveSaveSlot) karakter degistirebilir; yerel (host) denetimli pawn muaf (test bayragi kapali).
//  F5: "bu ad kayitli bir karaktere ait" reddi ad sogumasini baslatir (ikinci deneme hemen sogumaya takilir).
//  F3: yuklu karakterde kaydi olmayan canli adla LoadCharacter() hicbir seyi degistirmez (yuva, seri, yukleme bayragi,
//      envanter, basarim durumu).
// Kimlik yalniz GUID'li gecici yuvalarla kurulur; sonunda .sav ve basarim yan dosyalari silinir.

#include "Characters/MMOCharacter.h"

#if !UE_BUILD_SHIPPING
#include "AIController.h"
#include "Components/MMOAchievementComponent.h"
#include "Components/MMOInventoryComponent.h"
#include "Player/MMOPlayerController.h"
#include "Save/MMOCharacterStoreSubsystem.h"
#include "Save/MMOCharacterSaveJson.h"
#include "Save/MMOSaveGame.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/Guid.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
	constexpr int32 NameLockPawnCount = 5;

	bool RunMMONameLockSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
	{
		OutChecks = 0; OutError.Reset();
		if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client
			|| !FParse::Param(FCommandLine::Get(), TEXT("unattended"))
			|| !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
		{ OutError = TEXT("disposable authority headless game world required"); return false; }
		IMMOCharacterStore* Store = UMMOCharacterStoreSubsystem::Get(World);
		if (!Store) { OutError = TEXT("store unavailable"); return false; }

		bool bPassed = true;
		auto Check = [&](bool Condition, const TCHAR* Label)
		{
			++OutChecks;
			UE_LOG(LogTemp, Display, TEXT("[MMO_NAMELOCK_CASE] %s check=%s"), Condition ? TEXT("PASS") : TEXT("FAIL"), Label);
			if (!Condition) { bPassed = false; if (OutError.IsEmpty()) OutError = Label; }
		};

		// Ad en fazla 20 karakter (ServerSetName): "ZNL" + 8 hex + 1-2 harf.
		const FString Tag = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
		auto NameOf = [&Tag](const TCHAR* Suffix) { return FString(TEXT("ZNL")) + Tag + Suffix; };
		auto SlotOf = [](const FString& Name) { return FString(TEXT("MMOSave_")) + Name; };
		const FString NReg = NameOf(TEXT("R"));    // P0: kayitli karakter
		const FString NRegTo = NameOf(TEXT("RT")); // P0'in denedigi yeni ad
		const FString NNew = NameOf(TEXT("N"));    // P1: kayitsiz yeni karakter
		const FString NNewTo = NameOf(TEXT("NT"));
		const FString NEmpty = NameOf(TEXT("E"));  // P2: yuklemesi bitmemis
		const FString NEmptyTo = NameOf(TEXT("ET"));
		const FString NCool = NameOf(TEXT("C"));   // P3: F5 soguma
		const FString NCoolTo = NameOf(TEXT("CT"));
		const FString NTaken = NameOf(TEXT("K"));  // cevrimdisi kayitli ad (hicbir pawn'da degil)
		const FString NLoad = NameOf(TEXT("L"));   // P4: F3 yeniden yukleme
		const FString NLoadX = NameOf(TEXT("LX")); // P4'un kaydi olmayan canli adi
		const FString NHost = NameOf(TEXT("H"));   // yerel (host) denetimli kayitli karakter
		const FString NHostTo = NameOf(TEXT("HT"));
		const TArray<FString> AllNames = { NReg, NRegTo, NNew, NNewTo, NEmpty, NEmptyTo, NCool, NCoolTo, NTaken, NLoad, NLoadX, NHost, NHostTo };
		for (const FString& N : AllNames)
		{
			if (Store->Exists(SlotOf(N))) { OutError = TEXT("temporary slot collision"); return false; }
		}
		const FString AchDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MMOAchievements"));

		TArray<AMMOCharacter*> Pawns;
		TArray<AMMOPlayerController*> Controllers;
		AMMOCharacter* HostPawn = nullptr;
		AAIController* HostAI = nullptr;
		APlayerState* HostPS = nullptr;
		const TCHAR* InitialNames[NameLockPawnCount] = { *NReg, *NNew, *NEmpty, *NCool, *NLoad };
		bool bFixture = true;
		for (int32 I = 0; I < NameLockPawnCount; ++I)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AMMOCharacter* Pawn = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(),
				FVector(2100000. + I * 3000., 2100000., 5000.), FRotator::ZeroRotator, Params);
			AMMOPlayerController* PC = World->SpawnActor<AMMOPlayerController>();
			Pawns.Add(Pawn); Controllers.Add(PC);
			if (!Pawn || !PC) { bFixture = false; break; }
			Pawn->SetActorTickEnabled(false); PC->SetActorTickEnabled(false);
			PC->Possess(Pawn);
			if (!Pawn->GetPlayerState()) { bFixture = false; break; }
			Pawn->GetPlayerState()->SetPlayerName(InitialNames[I]);
		}
		Check(bFixture && Pawns.Num() == NameLockPawnCount, TEXT("fixture_possessed_pawns"));

		auto LiveName = [](const AMMOCharacter* P) { return P->GetPlayerState() ? P->GetPlayerState()->GetPlayerName() : FString(); };
		auto InvSnapshot = [](const AMMOCharacter* P)
		{
			FMMOCharacterSaveData D;
			if (P->GetInventory()) P->GetInventory()->FillSaveData(D);
			return D;
		};
		auto SameInv = [](const FMMOCharacterSaveData& A, const FMMOCharacterSaveData& B)
		{
			TArray<FString> Diff; FMMOCharacterSaveJson::FindDifferences(A, B, Diff); return Diff.IsEmpty();
		};

		if (bFixture)
		{
			// --- F1: kayitli karakter (ActiveSaveSlot diskte var), uzak + yonetici disi: ad degisimi REDDEDILIR ---
			AMMOCharacter* P0 = Pawns[0];
			P0->LoadCharacter(); // kayit yok: yeni karakter dali, ActiveSaveSlot atanir
			const bool bSaved0 = P0->SaveCharacterForTravel();
			Check(bSaved0 && Store->Exists(SlotOf(NReg)) && P0->GetActiveSaveSlot() == SlotOf(NReg), TEXT("f1_fixture_registered_slot_on_disk"));
			P0->SetRemoteNameRulesForTest(true);
			const FMMOCharacterSaveData Inv0 = InvSnapshot(P0);
			const UMMOAchievementComponent* A0 = P0->GetAchievementComponent();
			const TArray<FName> Unlocked0 = A0 ? A0->Unlocked : TArray<FName>();
			const uint32 Serial0 = P0->GetLoadSerial();
			P0->MMOSetName(NRegTo);
			Check(LiveName(P0) == NReg && P0->GetSaveSlotName() == SlotOf(NReg), TEXT("f1_registered_remote_rename_rejected"));
			Check(P0->GetActiveSaveSlot() == SlotOf(NReg) && P0->GetLoadSerial() == Serial0 && P0->IsInitialLoadComplete()
				&& !Store->Exists(SlotOf(NRegTo)), TEXT("f1_registered_reject_keeps_save_identity"));
			Check(SameInv(Inv0, InvSnapshot(P0)) && A0 && A0->Unlocked == Unlocked0, TEXT("f1_registered_reject_keeps_gold_items_unlocked"));

			// Host/yerel denetleyici muafiyeti. UE 5.7'de NetDriver yokken ULocalPlayer'siz (test icin spawn edilen) bir
			// APlayerController IsLocalController()=false doner, yani yukaridaki PC'li pawn'lar bayraksiz da "uzak" sayilir.
			// Yerel denetimi AController tabanli (AI) denetleyici verir (standalone'da IsLocalController=true): ayni kayitli yuva,
			// once bayrak ACIK (uzak benzetimi) -> RED, sonra bayrak KAPALI (yerel/host kurallari) -> izin. F1 reddi soguma baslatmaz.
			FMMOCharacterSaveData HostData;
			const bool bHostSlot = Store->Load(SlotOf(NReg), HostData) && Store->Save(SlotOf(NHost), HostData);
			FActorSpawnParameters HostParams;
			HostParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			HostPawn = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(2100000., 2110000., 5000.), FRotator::ZeroRotator, HostParams);
			HostAI = World->SpawnActor<AAIController>(AAIController::StaticClass(), FVector(2100000., 2110000., 5000.), FRotator::ZeroRotator, HostParams);
			HostPS = World->SpawnActor<APlayerState>(APlayerState::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, HostParams);
			const bool bHostFixture = bHostSlot && HostPawn && HostAI && HostPS;
			if (bHostFixture)
			{
				HostPawn->SetActorTickEnabled(false);
				HostAI->Possess(HostPawn);
				HostPS->SetPlayerName(NHost);
				HostPawn->SetPlayerState(HostPS);
				HostPawn->SetSaveIdentityForTest(true, SlotOf(NHost), 1);
			}
			Check(bHostFixture && HostPawn->IsLocallyControlled() && Store->Exists(SlotOf(NHost)) && LiveName(HostPawn) == NHost,
				TEXT("f1_fixture_local_host_registered"));
			if (bHostFixture)
			{
				HostPawn->SetRemoteNameRulesForTest(true);
				HostPawn->MMOSetName(NHostTo);
				Check(LiveName(HostPawn) == NHost, TEXT("f1_same_slot_remote_flag_rejected"));
				HostPawn->SetRemoteNameRulesForTest(false);
				HostPawn->MMOSetName(NHostTo);
				Check(LiveName(HostPawn) == NHostTo && HostPawn->GetActiveSaveSlot() == SlotOf(NHost), TEXT("f1_host_local_exempt_registered_rename_allowed"));
			}

			// --- F1: kayitsiz yeni karakter (ActiveSaveSlot atanmis ama diskte YOK): ad degisebilir ---
			AMMOCharacter* P1 = Pawns[1];
			P1->LoadCharacter();
			Check(P1->IsInitialLoadComplete() && P1->GetActiveSaveSlot() == SlotOf(NNew) && !Store->Exists(SlotOf(NNew)), TEXT("f1_fixture_unsaved_new_character"));
			P1->SetRemoteNameRulesForTest(true);
			P1->MMOSetName(NNewTo);
			Check(LiveName(P1) == NNewTo && P1->GetSaveSlotName() == SlotOf(NNewTo), TEXT("f1_unsaved_new_character_rename_allowed"));
			Check(P1->GetActiveSaveSlot() == SlotOf(NNew), TEXT("f1_unsaved_rename_does_not_move_active_slot"));

			// --- F1: yukleme bitmemis (ActiveSaveSlot bos): ad degisebilir ---
			AMMOCharacter* P2 = Pawns[2];
			P2->SetSaveIdentityForTest(false, FString(), 0);
			P2->SetRemoteNameRulesForTest(true);
			P2->MMOSetName(NEmptyTo);
			Check(LiveName(P2) == NEmptyTo && P2->GetActiveSaveSlot().IsEmpty(), TEXT("f1_empty_active_slot_rename_allowed"));

			// --- F5: cevrimdisi kayitli ada gecis reddi sogumayi baslatir; hemen ardindan BOS bir ad da reddedilir ---
			FMMOCharacterSaveData TakenData;
			const bool bTaken = Store->Load(SlotOf(NReg), TakenData) && Store->Save(SlotOf(NTaken), TakenData);
			Check(bTaken && Store->Exists(SlotOf(NTaken)), TEXT("f5_fixture_offline_registered_name"));
			AMMOCharacter* P3 = Pawns[3];
			P3->LoadCharacter(); // kayitsiz yeni karakter: F1 kilidi uygulanmaz, ad kurallari soguma/kayitli-ad dalina iner
			P3->SetRemoteNameRulesForTest(true);
			P3->MMOSetName(NTaken);
			Check(LiveName(P3) == NCool && P3->GetSaveSlotName() == SlotOf(NCool), TEXT("f5_registered_name_rejected"));
			P3->MMOSetName(NCoolTo);
			Check(LiveName(P3) == NCool && !Store->Exists(SlotOf(NCoolTo)), TEXT("f5_rejection_starts_cooldown"));

			// --- F3: yuklu karakter, canli adin kaydi yok: LoadCharacter hicbir seyi degistirmez ---
			AMMOCharacter* P4 = Pawns[4];
			P4->LoadCharacter();
			const bool bSaved4 = P4->SaveCharacterForTravel();
			UMMOAchievementComponent* A4 = P4->GetAchievementComponent();
			if (A4) { A4->NotifyMobKilled(FName(TEXT("Goblin"))); } // basarimlar S4 yuvasina baglanir, sayac 1
			Check(bSaved4 && A4 && A4->GetCounters().KillTotal == 1 && P4->GetActiveSaveSlot() == SlotOf(NLoad), TEXT("f3_fixture_loaded_registered"));
			const uint32 Serial4 = P4->GetLoadSerial();
			const FMMOCharacterSaveData Inv4 = InvSnapshot(P4);
			P4->GetPlayerState()->SetPlayerName(NLoadX); // MMOLoad oncesi canli ad degisti (host/yonetici yolu)
			const bool bReload = P4->LoadCharacter();
			Check(!bReload, TEXT("f3_reload_unsaved_name_returns_false"));
			Check(P4->GetActiveSaveSlot() == SlotOf(NLoad), TEXT("f3_reload_keeps_active_slot"));
			Check(P4->GetLoadSerial() == Serial4, TEXT("f3_reload_keeps_load_serial"));
			Check(P4->IsInitialLoadComplete(), TEXT("f3_reload_keeps_initial_load_complete"));
			Check(SameInv(Inv4, InvSnapshot(P4)) && !Store->Exists(SlotOf(NLoadX)), TEXT("f3_reload_no_starter_pack_no_new_slot"));
			if (A4) { A4->NotifyMobKilled(FName(TEXT("Goblin"))); }
			Check(A4 && A4->GetCounters().KillTotal == 2, TEXT("f3_reload_keeps_achievement_state"));
			// Kontrol: kayitli ada donunce yeniden yukleme yine calisir (erken donus yalniz kayitsiz ad icin).
			P4->GetPlayerState()->SetPlayerName(NLoad);
			const bool bReload2 = P4->LoadCharacter();
			Check(bReload2 && P4->GetLoadSerial() == Serial4 + 1 && P4->GetActiveSaveSlot() == SlotOf(NLoad), TEXT("f3_reload_registered_name_still_loads"));
		}

		// Temizlik: once kaliciligi kapat (EndPlay/UnPossess kaydi olmasin), sonra controller/pawn, sonra GUID'li dosyalar.
		for (AMMOCharacter* Pawn : Pawns)
		{
			if (IsValid(Pawn))
			{
				Pawn->SetRemoteNameRulesForTest(false);
				Pawn->SetSaveIdentityForTest(false, Pawn->GetActiveSaveSlot(), Pawn->GetLoadSerial());
				Pawn->SetTravelSaveCommitted(true);
			}
		}
		for (AMMOPlayerController* PC : Controllers)
		{
			if (IsValid(PC)) { PC->UnPossess(); PC->Destroy(); }
		}
		for (AMMOCharacter* Pawn : Pawns) { if (IsValid(Pawn)) Pawn->Destroy(); }
		if (IsValid(HostPawn))
		{
			HostPawn->SetRemoteNameRulesForTest(false);
			HostPawn->SetSaveIdentityForTest(false, HostPawn->GetActiveSaveSlot(), HostPawn->GetLoadSerial());
			HostPawn->SetTravelSaveCommitted(true);
			HostPawn->SetPlayerState(nullptr);
		}
		if (IsValid(HostAI)) { HostAI->UnPossess(); HostAI->Destroy(); }
		if (IsValid(HostPawn)) HostPawn->Destroy();
		if (IsValid(HostPS)) HostPS->Destroy();
		for (const FString& N : AllNames)
		{
			const FString Slot = SlotOf(N);
			if (Store->Exists(Slot) && !UGameplayStatics::DeleteGameInSlot(Slot, 0))
			{
				bPassed = false;
				if (OutError.IsEmpty()) OutError = TEXT("temporary slot cleanup failed");
			}
			for (const TCHAR* Ext : { TEXT(""), TEXT(".tmp"), TEXT(".bad") })
			{
				IFileManager::Get().Delete(*(FPaths::Combine(AchDir, Slot + TEXT(".json")) + Ext), false, true, true);
			}
		}
		return bPassed; // Ayni senkron kare: ad kurallari ve LoadCharacter kapilari dogrulanir; ag/RPC/PIE kaniti degil.
	}
}
// Bu testin KANITLAMADIKLARI: gercek uzak istemci (ServerSetName RPC'si, IsLocallyControlled=false olan ag baglantisi; burada
// SetRemoteNameRulesForTest bayragi uzak + yonetici disi oyuncuyu benzetir), yonetici muafiyetinin kendisi (MMOAdmin::IsServerAdmin
// ULocalPlayer'li host PC ister; burada yerel muafiyet AI denetleyiciyle, yani UsesRemoteNameRules=false dalindan kanitlanir),
// dedicated sunucuda yonetici yoklugu, 30 sn
// sogumanin zamanla bitmesi (tek karede yalniz baslatildigi gorulur), magaza yokken muhafazakar ret, gercek MMOLoad komut
// yetki kapisi, ?Name= ile yeniden baglanma akisi, PIE ve ag replikasyonu.

namespace
{
	class FMMONameLockTestExec final : public FSelfRegisteringExec
	{
		bool Exec(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestNameLock"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = RunMMONameLockSelfTest(World, Error, Checks);
			UE_LOG(LogTemp, Display, TEXT("[MMO_NAMELOCK_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMONameLockTestExec GMMONameLockTestExec;
}
#endif

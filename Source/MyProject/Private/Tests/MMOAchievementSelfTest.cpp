// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMOAchievementComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Characters/MMOMobArchetype.h"
#include "Components/MMOStatsComponent.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOQuestComponent.h"
#include "Save/MMOSaveGame.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpectatorPawn.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CoreMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"

bool UMMOAchievementComponent::RunSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{ OutError = TEXT("authority game world required"); return false; }
	bool bPassed = true;
	auto Check = [&](bool Condition, const TCHAR* Label)
	{
		++OutChecks;
		UE_LOG(LogTemp, Display, TEXT("[MMO_ACH_CASE] %s check=%s"), Condition ? TEXT("PASS") : TEXT("FAIL"), Label);
		if (!Condition) { bPassed = false; if (OutError.IsEmpty()) OutError = Label; }
	};

	// --- Veri tablosu ---
	{
		TSet<FName> Seen;
		bool bUnique = true, bTargetsOk = true, bPositive = true, bText = true;
		for (const FMMOAchievementDef& D : FMMOAchievementDatabase::GetAll())
		{
			bUnique &= !D.Id.IsNone() && !Seen.Contains(D.Id);
			Seen.Add(D.Id);
			bPositive &= D.Target > 0 && D.RewardGold >= 0;
			bText &= !D.Name.IsEmpty() && !D.Description.IsEmpty();
			if (D.Kind == EMMOAchievementKind::KillMob)
			{
				bTargetsOk &= FMMOMobDatabase::Find(D.TargetId) != nullptr;
			}
		}
		Check(FMMOAchievementDatabase::GetAll().Num() > 20, TEXT("table_size"));
		Check(bUnique, TEXT("table_unique_ids"));
		Check(bPositive, TEXT("table_positive_targets"));
		Check(bText, TEXT("table_text"));
		Check(bTargetsOk, TEXT("table_killmob_archetypes_exist"));
	}

	// --- Saf mantik ---
	{
		FMMOAchievementCounters C;
		C.KillTotal = 10; C.PeakGold = 12000; C.Deaths = 3; C.Kills.Add(FName(TEXT("SpiderQueen")), 1);
		const FMMOAchievementDef* Kill10 = FMMOAchievementDatabase::Find(FName(TEXT("Ach_Kill10")));
		const FMMOAchievementDef* Lv20 = FMMOAchievementDatabase::Find(FName(TEXT("Ach_Lv20")));
		const FMMOAchievementDef* Spider = FMMOAchievementDatabase::Find(FName(TEXT("Ach_SpiderQueen")));
		const FMMOAchievementDef* Gold10k = FMMOAchievementDatabase::Find(FName(TEXT("Ach_Gold10k")));
		const FMMOAchievementDef* Quests10 = FMMOAchievementDatabase::Find(FName(TEXT("Ach_Quests10")));
		Check(Kill10 && Lv20 && Spider && Gold10k && Quests10, TEXT("logic_defs_found"));
		if (Kill10 && Lv20 && Spider && Gold10k && Quests10)
		{
			Check(IsSatisfied(*Kill10, C, 1, 0), TEXT("logic_kill_total_at_target"));
			C.KillTotal = 9;
			Check(!IsSatisfied(*Kill10, C, 1, 0), TEXT("logic_kill_total_below_target"));
			Check(!IsSatisfied(*Lv20, C, 19, 0) && IsSatisfied(*Lv20, C, 20, 0), TEXT("logic_level_boundary"));
			Check(IsSatisfied(*Spider, C, 1, 0), TEXT("logic_killmob"));
			Check(IsSatisfied(*Gold10k, C, 1, 0), TEXT("logic_peak_gold"));
			Check(!IsSatisfied(*Quests10, C, 1, 9) && IsSatisfied(*Quests10, C, 1, 10), TEXT("logic_quests_boundary"));
			Check(ProgressOf(*Kill10, C, 1, 0) == 9, TEXT("logic_progress"));
		}
	}

	// --- JSON dogrulama (kotu niyetli dosya) ---
	{
		FMMOAchievementCounters C; TArray<FName> U; FString T;
		Check(!FromJson(TEXT("not json"), C, U, T), TEXT("json_rejects_garbage"));
		const FString Hostile = TEXT("{\"killTotal\":-5,\"deaths\":99999999999,\"peakGold\":12,\"kills\":{\"Goblin\":-3,\"Orc\":4},"
			"\"unlocked\":[\"Ach_Kill10\",\"Nope\",\"Ach_Kill10\"],\"title\":\"Avci\"}");
		Check(FromJson(Hostile, C, U, T), TEXT("json_accepts_hostile_shape"));
		Check(C.KillTotal == 0 && C.Deaths == MaxCounter && C.PeakGold == 12, TEXT("json_clamps_counters"));
		Check(C.Kills.FindRef(FName(TEXT("Goblin"))) == 0 && C.Kills.FindRef(FName(TEXT("Orc"))) == 4, TEXT("json_clamps_kill_map"));
		Check(U.Num() == 1 && U[0] == FName(TEXT("Ach_Kill10")), TEXT("json_drops_unknown_and_duplicate_ids"));
		Check(T.IsEmpty(), TEXT("json_drops_unowned_title"));
		FMMOAchievementCounters C2; TArray<FName> U2; FString T2;
		const FString Owned = TEXT("{\"unlocked\":[\"Ach_Kill10\"],\"title\":\"Acemi\"}");
		Check(FromJson(Owned, C2, U2, T2) && T2 == TEXT("Acemi"), TEXT("json_keeps_owned_title"));
	}

	// --- Canli bilesen ---
	const FVector Location(1600000, 1600000, 5000);
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TStrongObjectPtr<AMMOCharacter> Char(World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params));
	UMMOAchievementComponent* A = Char.IsValid() ? Char->FindComponentByClass<UMMOAchievementComponent>() : nullptr;
	UMMOInventoryComponent* Inv = Char.IsValid() ? Char->GetInventory() : nullptr;
	UMMOStatsComponent* St = Char.IsValid() ? Char->GetStats() : nullptr;
	Check(A && Inv && St, TEXT("live_fixture"));
	if (!(A && Inv && St))
	{
		if (Char.IsValid()) Char->Destroy();
		return false;
	}

	// PlayerState/disk yok: yuklenmeden sayac artmamali.
	A->NotifyMobKilled(FName(TEXT("Goblin")));
	Check(A->GetCounters().KillTotal == 0, TEXT("live_unloaded_ignores_events"));

	A->InitForTest();
	{
		FMMOCharacterSaveData Seed; Seed.Gold = 123; Seed.NextInstanceId = 50;
		Inv->ApplySaveData(Seed); // baslangic paketi/altin degiskenligini ele
	}
	const int32 G0 = Inv->Gold;
	A->NotifyMobKilled(NAME_None);
	Check(A->GetCounters().KillTotal == 0, TEXT("live_ignores_none_archetype"));
	for (int32 i = 0; i < 9; ++i) A->NotifyMobKilled(FName(TEXT("Goblin")));
	Check(!A->IsUnlocked(FName(TEXT("Ach_Kill10"))) && Inv->Gold == G0, TEXT("live_below_target_no_unlock"));
	A->NotifyMobKilled(FName(TEXT("Goblin")));
	Check(A->IsUnlocked(FName(TEXT("Ach_Kill10"))) && Inv->Gold == G0 + 50, TEXT("live_unlock_grants_gold"));
	Check(A->GetCounters().KillTotal == 10 && A->GetCounters().Kills.FindRef(FName(TEXT("Goblin"))) == 10, TEXT("live_counters"));
	const int32 UnlockedBefore = A->Unlocked.Num();
	for (int32 i = 0; i < 5; ++i) A->NotifyMobKilled(FName(TEXT("Goblin")));
	A->EvaluateAll();
	Check(A->Unlocked.Num() == UnlockedBefore && Inv->Gold == G0 + 50, TEXT("live_no_regrant"));

	St->Level = 10;
	A->EvaluateAll();
	Check(A->IsUnlocked(FName(TEXT("Ach_Lv10"))) && Inv->Gold == G0 + 150, TEXT("live_level_unlock"));

	A->NotifyMobKilled(FName(TEXT("SpiderQueen")));
	Check(A->IsUnlocked(FName(TEXT("Ach_SpiderQueen"))) && Inv->Gold == G0 + 450, TEXT("live_boss_unlock"));

	const int32 GoldBeforeDeaths = Inv->Gold;
	for (int32 i = 0; i < 10; ++i) A->OnOwnerDied();
	Check(A->GetCounters().Deaths == 10 && A->IsUnlocked(FName(TEXT("Ach_Death10"))) && Inv->Gold == GoldBeforeDeaths, TEXT("live_deaths_unlock_no_gold"));

	// Unvan secimi
	A->CmdTitle(TEXT("Avci"));
	Check(A->ActiveTitle.IsEmpty(), TEXT("live_title_unowned_rejected"));
	A->CmdTitle(TEXT("acemi"));
	Check(A->ActiveTitle == TEXT("Acemi"), TEXT("live_title_owned_case_insensitive"));
	A->CmdTitle(TEXT(""));
	Check(A->ActiveTitle.IsEmpty(), TEXT("live_title_cleared"));
	A->CmdTitle(TEXT("Acemi"));

	// Gidis-donus
	{
		FString Json; FMMOAchievementCounters C; TArray<FName> U; FString T;
		Check(ToJson(A->GetCounters(), A->Unlocked, A->ActiveTitle, Json) && FromJson(Json, C, U, T), TEXT("live_json_roundtrip_ok"));
		Check(C.KillTotal == A->GetCounters().KillTotal && C.Deaths == A->GetCounters().Deaths && U == A->Unlocked && T == A->ActiveTitle
			&& C.Kills.FindRef(FName(TEXT("Goblin"))) == 15, TEXT("live_json_roundtrip_equal"));
	}

	Char->Destroy();

	// --- Kalicilik (gercek disk, GUID'li yuvalar): A6 erken olay, O1 sabit yol/Default yok, C3, gidis-donus, B1, LoadSerial, A5, D3 ---
	{
		const FString Tag = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
		const FString NameA = TEXT("ZAchA_") + Tag; // ilk canli ad = ilk kayit yuvasi
		const FString NameB = TEXT("ZAchB_") + Tag; // ad degisimi sonrasi canli ad
		const FString SlotA = TEXT("MMOSave_") + NameA;
		const FString SlotB = TEXT("MMOSave_") + NameB;
		const FString SlotC = TEXT("MMOSave_ZAchC_") + Tag;
		const FString SlotD = TEXT("MMOSave_ZAchD_") + Tag;
		const FString AchDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MMOAchievements"));
		IFileManager& FM = IFileManager::Get();
		auto PathOf = [&AchDir](const FString& Slot) { return FPaths::Combine(AchDir, Slot + TEXT(".json")); };
		auto ReadSlot = [&PathOf](const FString& Slot, FMMOAchievementCounters& OutC, TArray<FName>& OutU, FString& OutT)
		{
			FString Raw;
			return FFileHelper::LoadFileToString(Raw, *PathOf(Slot)) && FromJson(Raw, OutC, OutU, OutT);
		};
		auto WriteSlotFile = [&FM, &AchDir](const FString& FilePath, int32 Kills, const TArray<FName>& U, const FString& Title)
		{
			FMMOAchievementCounters Seed; Seed.KillTotal = Kills;
			FString Json;
			FM.MakeDirectory(*AchDir, true);
			return ToJson(Seed, U, Title, Json) && FFileHelper::SaveStringToFile(Json, *FilePath);
		};
		const FString DefaultPath = PathOf(TEXT("MMOSave_Default"));
		const int64 DefaultSize0 = FM.FileSize(*DefaultPath);
		const FDateTime DefaultStamp0 = FM.GetTimeStamp(*DefaultPath);
		const FName Kill10(TEXT("Ach_Kill10"));
		const FName Goblin(TEXT("Goblin"));

		TStrongObjectPtr<AMMOCharacter> P1(World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location + FVector(0, 5000, 0), FRotator::ZeroRotator, Params));
		TStrongObjectPtr<APlayerState> PS1(World->SpawnActor<APlayerState>(APlayerState::StaticClass(), Location, FRotator::ZeroRotator, Params));
		TStrongObjectPtr<AMMOCharacter> P2(World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location + FVector(0, 10000, 0), FRotator::ZeroRotator, Params));
		TStrongObjectPtr<APlayerState> PS2(World->SpawnActor<APlayerState>(APlayerState::StaticClass(), Location, FRotator::ZeroRotator, Params));
		UMMOAchievementComponent* A1 = P1.IsValid() ? P1->GetAchievementComponent() : nullptr;
		UMMOAchievementComponent* A2 = P2.IsValid() ? P2->GetAchievementComponent() : nullptr;
		UMMOInventoryComponent* Inv1 = P1.IsValid() ? P1->GetInventory() : nullptr;
		UMMOInventoryComponent* Inv2 = P2.IsValid() ? P2->GetInventory() : nullptr;
		const bool bFixture = A1 && A2 && Inv1 && Inv2 && PS1.IsValid() && PS2.IsValid();
		Check(bFixture, TEXT("persist_fixture"));
		if (bFixture)
		{
			FMMOCharacterSaveData Seed; Seed.Gold = 123; Seed.NextInstanceId = 50;
			Inv1->ApplySaveData(Seed);
			Inv2->ApplySaveData(Seed);
			const int32 G1 = Inv1->Gold;
			const int32 G2 = Inv2->Gold;
			PS1->SetPlayerName(NameA);
			P1->SetPlayerState(PS1.Get());

			// A6/O1: karakter kaydi yuklenmeden gelen olaylar SAYILMAZ (kuyruga alinmaz) ve hicbir dosya yazilmaz.
			A1->NotifyMobKilled(Goblin); A1->OnOwnerDied(); A1->EvaluateAll(); A1->CmdTitle(TEXT("Acemi"));
			Check(A1->GetCounters().KillTotal == 0 && A1->GetCounters().Deaths == 0 && A1->GetSaveCallsForTest() == 0 && !FM.FileExists(*PathOf(SlotA)), TEXT("a6_unloaded_events_dropped_no_write"));
			P1->SetSaveIdentityForTest(false, SlotA, 1);
			A1->NotifyMobKilled(Goblin);
			Check(A1->GetCounters().KillTotal == 0 && !FM.FileExists(*PathOf(SlotA)), TEXT("a6_slot_without_initial_load_ignored"));
			P1->SetSaveIdentityForTest(true, TEXT("MMOSave_Default"), 1);
			A1->NotifyMobKilled(Goblin);
			Check(A1->GetCounters().KillTotal == 0 && A1->GetSaveCallsForTest() == 0, TEXT("o1_default_slot_never_bound"));
			P1->SetSaveIdentityForTest(true, FString(), 1);
			A1->NotifyMobKilled(Goblin);
			Check(A1->GetCounters().KillTotal == 0 && A1->GetSaveCallsForTest() == 0, TEXT("o1_empty_slot_never_bound"));

			// Yukleme tamamlaninca ilk olay yuklemeyi tetikler ve SAYILIR (zamanlayicidan once gelen oldurme kaybolmaz).
			P1->SetSaveIdentityForTest(true, SlotA, 1);
			A1->NotifyMobKilled(Goblin);
			Check(A1->GetCounters().KillTotal == 1, TEXT("a6_first_event_after_load_counts"));
			for (int32 i = 0; i < 9; ++i) A1->NotifyMobKilled(Goblin);
			{
				FMMOAchievementCounters C; TArray<FName> U; FString T;
				Check(A1->IsUnlocked(Kill10) && Inv1->Gold == G1 + 50 && ReadSlot(SlotA, C, U, T) && U.Contains(Kill10) && C.KillTotal == 10, TEXT("persist_unlock_on_disk_with_reward"));
			}

			// C3: ayni unvan tekrar secilince / bos unvan tekrar temizlenince diske yazilmaz.
			const int32 Saves0 = A1->GetSaveCallsForTest();
			A1->CmdTitle(TEXT("Acemi"));
			const int32 Saves1 = A1->GetSaveCallsForTest();
			A1->CmdTitle(TEXT("acemi"));
			A1->CmdTitle(TEXT("Acemi"));
			Check(A1->ActiveTitle == TEXT("Acemi") && Saves1 == Saves0 + 1 && A1->GetSaveCallsForTest() == Saves1, TEXT("c3_same_title_not_rewritten"));
			A1->CmdTitle(TEXT(""));
			const int32 Saves2 = A1->GetSaveCallsForTest();
			A1->CmdTitle(TEXT(""));
			Check(A1->ActiveTitle.IsEmpty() && Saves2 == Saves1 + 1 && A1->GetSaveCallsForTest() == Saves2, TEXT("c3_clear_twice_written_once"));
			A1->CmdTitle(TEXT("Acemi"));

			// O1: kirli (30 sn araligi nedeniyle henuz yazilmamis) durum, PlayerState koptuktan sonra EndPlay'de yukleme aninda
			// sabitlenen yola yazilir; MMOSave_Default.json'a dokunulmaz.
			A1->NotifyMobKilled(Goblin); A1->NotifyMobKilled(Goblin); A1->OnOwnerDied();
			{
				FMMOAchievementCounters C; TArray<FName> U; FString T;
				Check(ReadSlot(SlotA, C, U, T) && C.KillTotal == 10 && A1->GetCounters().KillTotal == 12, TEXT("persist_dirty_pending_before_endplay"));
			}
			P1->SetPlayerState(nullptr);
			PS1->Destroy();
			P1->Destroy();
			{
				FMMOAchievementCounters C; TArray<FName> U; FString T;
				Check(ReadSlot(SlotA, C, U, T) && C.KillTotal == 12 && C.Deaths == 1 && U.Contains(Kill10) && T == TEXT("Acemi"), TEXT("o1_endplay_saves_to_pinned_path"));
			}
			Check(FM.FileSize(*DefaultPath) == DefaultSize0 && FM.GetTimeStamp(*DefaultPath) == DefaultStamp0, TEXT("o1_default_file_untouched"));

			// Gidis-donus: ayni yuvayla yeni bilesen diskten yukler; sayaclar/acilmalar/unvan ayni, odul TEKRAR verilmez.
			PS2->SetPlayerName(NameA);
			P2->SetPlayerState(PS2.Get());
			P2->SetSaveIdentityForTest(true, SlotA, 1);
			A2->NotifyMobKilled(Goblin);
			Check(A2->GetCounters().KillTotal == 13 && A2->GetCounters().Deaths == 1 && A2->GetCounters().Kills.FindRef(Goblin) == 13
				&& A2->IsUnlocked(Kill10) && A2->ActiveTitle == TEXT("Acemi"), TEXT("persist_roundtrip_fresh_component"));
			Check(Inv2->Gold == G2, TEXT("persist_reload_no_regrant"));

			// B1: canli ad degisti ama karakter yeni ada yazilmadi (ActiveSaveSlot ayni): durum sifirlanmaz, odul tekrar yok,
			// canli adin yoluna dosya acilmaz (yuva canli ada DEGIL ActiveSaveSlot'a bagli).
			PS2->SetPlayerName(NameB);
			A2->NotifyMobKilled(Goblin);
			A2->EvaluateAll();
			Check(A2->GetCounters().KillTotal == 14 && A2->IsUnlocked(Kill10) && Inv2->Gold == G2, TEXT("b1_rename_keeps_state_no_regrant"));
			Check(!FM.FileExists(*PathOf(SlotB)), TEXT("b1_rename_does_not_bind_live_name"));

			// Karakter yeni ada basariyla yazildi (ActiveSaveSlot degisti): durum yeni yuvaya TASINIR, sifirlanmaz.
			P2->SetSaveIdentityForTest(true, SlotB, 1);
			A2->NotifyMobKilled(Goblin);
			{
				FMMOAchievementCounters C; TArray<FName> U; FString T;
				Check(ReadSlot(SlotB, C, U, T) && C.KillTotal >= 14 && U.Contains(Kill10) && A2->GetCounters().KillTotal == 15 && Inv2->Gold == G2, TEXT("b1_slot_move_carries_state"));
			}

			// LoadSerial (MMOLoad): kirli durum ESKI yola yazilir, sonra yeni yuklenen karakterin yuvasi yuklenir.
			Check(WriteSlotFile(PathOf(SlotC), 3, TArray<FName>(), FString()), TEXT("serial_seed_written"));
			A2->NotifyMobKilled(Goblin); // 16, 30 sn araligi nedeniyle kirli
			P2->SetSaveIdentityForTest(true, SlotC, 2);
			A2->NotifyMobKilled(Goblin);
			{
				FMMOAchievementCounters C; TArray<FName> U; FString T;
				Check(ReadSlot(SlotB, C, U, T) && C.KillTotal == 16, TEXT("serial_reload_flushes_old_state_to_old_path"));
			}
			Check(A2->GetCounters().KillTotal == 4 && !A2->IsUnlocked(Kill10) && A2->ActiveTitle.IsEmpty() && Inv2->Gold == G2, TEXT("serial_reload_loads_new_slot"));

			// A5: yazma basarisizken acilma geri alinir ve odul verilmez; yazma duzelince tekrar denenir ve TEK kez odenir.
			A2->SetSaveFailForTest(true);
			for (int32 i = 0; i < 6; ++i) A2->NotifyMobKilled(Goblin);
			Check(A2->GetCounters().KillTotal == 10 && !A2->IsUnlocked(Kill10) && Inv2->Gold == G2, TEXT("a5_failed_write_no_unlock_no_reward"));
			A2->EvaluateAll();
			Check(!A2->IsUnlocked(Kill10) && Inv2->Gold == G2, TEXT("a5_still_deferred_while_failing"));
			A2->SetSaveFailForTest(false);
			A2->EvaluateAll();
			Check(A2->IsUnlocked(Kill10) && Inv2->Gold == G2 + 50, TEXT("a5_retry_after_fix_pays"));
			A2->EvaluateAll();
			A2->NotifyMobKilled(Goblin);
			{
				FMMOAchievementCounters C; TArray<FName> U; FString T;
				Check(Inv2->Gold == G2 + 50 && ReadSlot(SlotC, C, U, T) && U.Contains(Kill10), TEXT("a5_paid_once_and_on_disk"));
			}

			// D3: yarim kalan yazim (yalniz .tmp) yuklemede geri konur.
			Check(WriteSlotFile(PathOf(SlotD) + TEXT(".tmp"), 7, TArray<FName>{ Kill10 }, TEXT("Acemi")), TEXT("tmp_seed_written"));
			P2->SetSaveIdentityForTest(true, SlotD, 3);
			A2->NotifyMobKilled(Goblin);
			Check(A2->GetCounters().KillTotal == 8 && A2->IsUnlocked(Kill10) && A2->ActiveTitle == TEXT("Acemi") && Inv2->Gold == G2 + 50
				&& FM.FileExists(*PathOf(SlotD)) && !FM.FileExists(*(PathOf(SlotD) + TEXT(".tmp"))), TEXT("d3_tmp_recovered_on_load"));
		}
		if (::IsValid(P2.Get())) { P2->SetPlayerState(nullptr); P2->Destroy(); } // EndPlay son kirli durumu SlotD'ye yazar (asagida silinir)
		if (::IsValid(P1.Get())) { P1->SetPlayerState(nullptr); P1->Destroy(); }
		if (::IsValid(PS2.Get())) PS2->Destroy();
		if (::IsValid(PS1.Get())) PS1->Destroy();
		for (const FString& Slot : { SlotA, SlotB, SlotC, SlotD })
		{
			for (const TCHAR* Ext : { TEXT(""), TEXT(".tmp"), TEXT(".bad") })
			{
				FM.Delete(*(PathOf(Slot) + Ext), false, true, true);
			}
		}
	}

	// --- Guvenlik tur 2 (K-1 F2 + okunamayan dosya): EndPlay'de yuva tasimasi, dosyasiz/bozuk dosyali Lv>1 karakter,
	// kilitli ana dosya ve kurtarilamayan .tmp (yukleme ertelenir, odul verilmez, dosya ezilmez). GUID'li yuvalar. ---
	{
		const FString Tag2 = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
		const FString SlotE = TEXT("MMOSave_ZAchE_") + Tag2;        // F2: eski yuva
		const FString SlotF = TEXT("MMOSave_ZAchF_") + Tag2;        // F2: ad degisimi + kayit sonrasi yeni yuva
		const FString SlotL = TEXT("MMOSave_ZAchL_") + Tag2;        // F2 seri: MMOLoad oncesi yuva
		const FString SlotM = TEXT("MMOSave_ZAchM_") + Tag2;        // F2 seri: MMOLoad ile yuklenen baska karakter
		const FString SlotG = TEXT("MMOSave_ZAchG_") + Tag2;        // dosyasiz Lv20
		const FString SlotH = TEXT("MMOSave_ZAchH_") + Tag2;        // bozuk dosyali Lv20
		const FString SlotI = TEXT("MMOSave_ZAchI_") + Tag2;        // dosyasiz Lv1 (kontrol)
		const FString SlotJ = TEXT("MMOSave_ZAchLockMain_") + Tag2; // kilitli ana dosya
		const FString SlotK = TEXT("MMOSave_ZAchLockTmp_") + Tag2;  // ana dosya yok, kilitli .tmp (calistirici bu adi bekler)
		const FString AchDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MMOAchievements"));
		IFileManager& FM = IFileManager::Get();
		IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
		auto PathOf = [&AchDir](const FString& Slot) { return FPaths::Combine(AchDir, Slot + TEXT(".json")); };
		auto ReadSlot = [&PathOf](const FString& Slot, FMMOAchievementCounters& OutC, TArray<FName>& OutU)
		{
			FString Raw, T;
			return FFileHelper::LoadFileToString(Raw, *PathOf(Slot)) && FromJson(Raw, OutC, OutU, T);
		};
		auto ReadRaw = [](const FString& FilePath) { FString Raw; FFileHelper::LoadFileToString(Raw, *FilePath); return Raw; };
		auto SeedJson = [](int32 Kills, const TArray<FName>& U)
		{
			FMMOAchievementCounters Seed; Seed.KillTotal = Kills;
			FString Json; ToJson(Seed, U, FString(), Json);
			return Json;
		};
		const FName Goblin(TEXT("Goblin"));
		const FName Kill10(TEXT("Ach_Kill10"));
		auto Spawn = [&](float Dy)
		{
			return TStrongObjectPtr<AMMOCharacter>(World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location + FVector(9000, Dy, 0), FRotator::ZeroRotator, Params));
		};
		TStrongObjectPtr<AMMOCharacter> Q1 = Spawn(0.f), Q2 = Spawn(4000.f), Q3 = Spawn(8000.f), Q4 = Spawn(12000.f), Q5 = Spawn(16000.f);
		UMMOAchievementComponent* B1 = Q1.IsValid() ? Q1->GetAchievementComponent() : nullptr;
		UMMOAchievementComponent* B2 = Q2.IsValid() ? Q2->GetAchievementComponent() : nullptr;
		UMMOAchievementComponent* B3 = Q3.IsValid() ? Q3->GetAchievementComponent() : nullptr;
		UMMOAchievementComponent* B4 = Q4.IsValid() ? Q4->GetAchievementComponent() : nullptr;
		UMMOAchievementComponent* B5 = Q5.IsValid() ? Q5->GetAchievementComponent() : nullptr;
		const bool bFixture2 = B1 && B2 && B3 && B4 && B5 && Q2->GetInventory() && Q3->GetInventory() && Q4->GetInventory()
			&& Q3->GetStats() && Q3->GetQuestComponent() && Q4->GetQuestComponent();
		Check(bFixture2, TEXT("r2_fixture"));
		if (bFixture2)
		{
			FMMOCharacterSaveData Seed; Seed.Gold = 123; Seed.NextInstanceId = 50;
			Q2->GetInventory()->ApplySaveData(Seed);
			Q4->GetInventory()->ApplySaveData(Seed);

			// F2: kirli durum (30 sn araligi nedeniyle yazilmamis) varken karakter yeni yuvaya yazildi (ActiveSaveSlot degisti,
			// ayni LoadSerial) ve hemen yok edildi: EndPlay once TryLoad ile durumu YENI yuvaya tasir; eski yuvaya yazmaz.
			Q1->SetSaveIdentityForTest(true, SlotE, 1);
			B1->NotifyMobKilled(Goblin); // yukler (dosya yok, Lv1), ilk kayit hemen
			B1->NotifyMobKilled(Goblin); B1->NotifyMobKilled(Goblin);
			{
				FMMOAchievementCounters C; TArray<FName> U;
				Check(ReadSlot(SlotE, C, U) && C.KillTotal == 1 && B1->GetCounters().KillTotal == 3, TEXT("f2_fixture_dirty_pending"));
			}
			Q1->SetSaveIdentityForTest(true, SlotF, 1);
			Q1->Destroy();
			{
				FMMOAchievementCounters C; TArray<FName> U;
				Check(ReadSlot(SlotF, C, U) && C.KillTotal == 3, TEXT("f2_endplay_moves_dirty_state_to_new_slot"));
			}
			// Tur 3 (Y7): eski yuva yazilmaz; yeni yuvaya basarili yazimdan sonra eski yuva dosyasi silinir (catal kalmaz).
			Check(!FM.FileExists(*PathOf(SlotE)) && !FM.FileExists(*(PathOf(SlotE) + TEXT(".tmp"))), TEXT("f2_endplay_old_slot_not_written"));
			const int32 G2 = Q2->GetInventory()->Gold;
			Q2->SetSaveIdentityForTest(true, SlotF, 1);
			B2->NotifyMobKilled(Goblin);
			Check(B2->GetCounters().KillTotal == 4 && Q2->GetInventory()->Gold == G2, TEXT("f2_new_slot_reload_continues"));

			// F2 seri (MMOLoad: baska karakter yuklendi, LoadSerial artti): kirli durum ESKI yuvaya yazilir, yeni yuvaya TASINMAZ.
			Q5->SetSaveIdentityForTest(true, SlotL, 1);
			B5->NotifyMobKilled(Goblin);
			B5->NotifyMobKilled(Goblin); // kirli
			Q5->SetSaveIdentityForTest(true, SlotM, 2);
			Q5->Destroy();
			{
				FMMOAchievementCounters CL, CM; TArray<FName> UL, UM;
				Check(ReadSlot(SlotL, CL, UL) && CL.KillTotal == 2 && !(ReadSlot(SlotM, CM, UM) && CM.KillTotal == 2), TEXT("f2_serial_change_flushes_old_slot_only"));
			}

			// Dosyasiz Lv20 karakter: seviye/gorev/altin basarimlari ODULSUZ acik isaretlenir ve hemen diske yazilir.
			UMMOInventoryComponent* Inv3 = Q3->GetInventory();
			{
				FMMOCharacterSaveData Rich; Rich.Gold = 12000; Rich.NextInstanceId = 50;
				Inv3->ApplySaveData(Rich);
			}
			Q3->GetStats()->Level = 20;
			for (int32 i = 0; i < 10; ++i) Q3->GetQuestComponent()->CompletedQuests.Add(FName(*FString::Printf(TEXT("Q_TestAchSnap%d"), i)));
			const int32 G3 = Inv3->Gold;
			Q3->SetSaveIdentityForTest(true, SlotG, 1);
			B3->EvaluateAll();
			Check(B3->IsUnlocked(FName(TEXT("Ach_Lv10"))) && B3->IsUnlocked(FName(TEXT("Ach_Lv20"))) && !B3->IsUnlocked(FName(TEXT("Ach_Lv30")))
				&& Inv3->Gold == G3, TEXT("fileless_lv20_level_kinds_marked_without_gold"));
			Check(B3->IsUnlocked(FName(TEXT("Ach_Quests10"))) && !B3->IsUnlocked(FName(TEXT("Ach_Quests40"))) && B3->IsUnlocked(FName(TEXT("Ach_Gold10k"))),
				TEXT("fileless_lv20_quest_and_gold_kinds_marked"));
			Check(!B3->IsUnlocked(Kill10) && !B3->IsUnlocked(FName(TEXT("Ach_Death10"))), TEXT("fileless_lv20_event_kinds_not_marked"));
			{
				FMMOAchievementCounters C; TArray<FName> U;
				Check(ReadSlot(SlotG, C, U) && U.Contains(FName(TEXT("Ach_Lv20"))) && U.Contains(FName(TEXT("Ach_Quests10"))), TEXT("fileless_lv20_marked_state_on_disk"));
			}
			for (int32 i = 0; i < 10; ++i) B3->NotifyMobKilled(Goblin);
			B3->EvaluateAll();
			Check(B3->IsUnlocked(Kill10) && Inv3->Gold == G3 + 50, TEXT("fileless_lv20_later_event_unlock_pays_once"));

			// Bozuk dosyali Lv20 (yeniden yukleme ile ayni bilesen): dosya .bad'a ayrilir, ayni basarimlar yine odulsuz isaretlenir.
			FM.MakeDirectory(*AchDir, true);
			FFileHelper::SaveStringToFile(TEXT("{{{ bozuk"), *PathOf(SlotH));
			const int32 G3b = Inv3->Gold;
			Q3->SetSaveIdentityForTest(true, SlotH, 2);
			B3->EvaluateAll();
			{
				FMMOAchievementCounters C; TArray<FName> U;
				Check(FM.FileExists(*(PathOf(SlotH) + TEXT(".bad"))) && B3->IsUnlocked(FName(TEXT("Ach_Lv20"))) && Inv3->Gold == G3b
					&& ReadSlot(SlotH, C, U) && U.Contains(FName(TEXT("Ach_Lv20"))), TEXT("corrupt_file_lv20_quarantined_marked_no_gold"));
			}

			// Kontrol: dosyasiz Lv1 karakter isaretlenmez, gorev basarimi normal odul verir (savunma Lv>1 ile sinirli).
			UMMOInventoryComponent* Inv4 = Q4->GetInventory();
			for (int32 i = 0; i < 10; ++i) Q4->GetQuestComponent()->CompletedQuests.Add(FName(*FString::Printf(TEXT("Q_TestAchLv1_%d"), i)));
			const int32 G4 = Inv4->Gold;
			Q4->SetSaveIdentityForTest(true, SlotI, 1);
			B4->EvaluateAll();
			Check(B4->IsUnlocked(FName(TEXT("Ach_Quests10"))) && Inv4->Gold == G4 + 200, TEXT("fileless_lv1_not_marked_rewards_paid"));

			// Ana dosya VAR ama okunamiyor (baska surec kilitli): yukleme ertelenir, olay sayilmaz, odul yok, dosya ezilmez.
			const FString SeedMain = SeedJson(12, TArray<FName>{ Kill10 });
			FFileHelper::SaveStringToFile(SeedMain, *PathOf(SlotJ));
			TUniquePtr<IFileHandle> MainLock(PF.OpenWrite(*PathOf(SlotJ), true, false));
			Check(MainLock.IsValid() && FM.FileExists(*PathOf(SlotJ)), TEXT("locked_main_fixture"));
			const int32 G2b = Q2->GetInventory()->Gold;
			Q2->SetSaveIdentityForTest(true, SlotJ, 2);
			B2->NotifyMobKilled(Goblin);
			Check(B2->GetCounters().KillTotal == 0 && !B2->IsUnlocked(Kill10) && Q2->GetInventory()->Gold == G2b, TEXT("locked_main_defers_load"));
			MainLock.Reset();
			Check(ReadRaw(PathOf(SlotJ)) == SeedMain && !FM.FileExists(*(PathOf(SlotJ) + TEXT(".tmp"))), TEXT("locked_main_not_overwritten"));
			B2->NotifyMobKilled(Goblin);
			Check(B2->GetCounters().KillTotal == 13 && B2->IsUnlocked(Kill10) && Q2->GetInventory()->Gold == G2b, TEXT("locked_main_loads_after_release_no_regrant"));

			// Ana dosya yok, .tmp var ama kilitli (geri konamiyor): yukleme ertelenir, ana dosya olusturulmaz, .tmp bozulmaz.
			// Tur 3 (Y1): kurtarma Move'u tekrar denemesiz (motor "Error moving file" satiri ve 5 sn bekleme YOK; calistirici
			// 0 satir bekler) ve basarisizlikta 10 sn (tur 4) geri cekilir; kilit kalkinca test geri cekilmeyi temizleyip yeniden dener.
			const FString SeedTmp = SeedJson(5, TArray<FName>{ Kill10 });
			FFileHelper::SaveStringToFile(SeedTmp, *(PathOf(SlotK) + TEXT(".tmp")));
			TUniquePtr<IFileHandle> TmpLock(PF.OpenWrite(*(PathOf(SlotK) + TEXT(".tmp")), true, false));
			Check(TmpLock.IsValid() && !FM.FileExists(*PathOf(SlotK)), TEXT("locked_tmp_fixture"));
			Q2->SetSaveIdentityForTest(true, SlotK, 3);
			B2->NotifyMobKilled(Goblin);
			Check(B2->GetCounters().KillTotal == 0 && !FM.FileExists(*PathOf(SlotK)) && Q2->GetInventory()->Gold == G2b, TEXT("locked_tmp_defers_load"));
			TmpLock.Reset();
			Check(ReadRaw(PathOf(SlotK) + TEXT(".tmp")) == SeedTmp, TEXT("locked_tmp_kept_intact"));
			// Tur 4: geri cekilme 10 sn (60 degil); kilit kalksa da pencere icinde yeniden denenmez (olay sayilmaz, ana dosya acilmaz).
			const double BackoffLeft = B2->RecoveryRetryAt - FPlatformTime::Seconds();
			UE_LOG(LogTemp, Display, TEXT("[MMO_ACH_NOTE] recovery_backoff_left=%.3f"), BackoffLeft);
			Check(BackoffLeft > 8.0 && BackoffLeft <= 10.01, TEXT("y1_ach_backoff_is_10s"));
			B2->NotifyMobKilled(Goblin);
			Check(B2->GetCounters().KillTotal == 0 && !FM.FileExists(*PathOf(SlotK)) && FM.FileExists(*(PathOf(SlotK) + TEXT(".tmp"))),
				TEXT("y1_ach_backoff_blocks_retry_after_unlock"));
			B2->ResetRecoveryBackoffForTest();
			B2->NotifyMobKilled(Goblin);
			Check(B2->GetCounters().KillTotal == 6 && B2->IsUnlocked(Kill10) && Q2->GetInventory()->Gold == G2b
				&& FM.FileExists(*PathOf(SlotK)) && !FM.FileExists(*(PathOf(SlotK) + TEXT(".tmp"))), TEXT("locked_tmp_recovered_after_release"));

			// Tur 4 (Y-NEW-2): ana dosya paylasimli okuyucuda (silinemez): SaveNow tekrar denemesiz basarisiz ve hizli, bDirty kalir,
			// disk degismez; kilit kalkinca sonraki SaveNow yazar.
			{
				const FString Before = ReadRaw(PathOf(SlotK));
				TUniquePtr<IFileHandle> MainRead(PF.OpenRead(*PathOf(SlotK), false));
				const bool bLocked = MainRead.IsValid();
				B2->bDirty = true;
				const double T0 = FPlatformTime::Seconds();
				const bool bSaved = bLocked && B2->SaveNow();
				const double Dt = FPlatformTime::Seconds() - T0;
				const bool bDirtyAfter = B2->bDirty;
				MainRead.Reset();
				Check(bLocked && !bSaved && bDirtyAfter, TEXT("y4_ach_locked_main_save_fails_stays_dirty"));
				Check(Dt < 1.0, TEXT("y4_ach_locked_main_save_fast_under_1s"));
				Check(ReadRaw(PathOf(SlotK)).Equals(Before, ESearchCase::CaseSensitive), TEXT("y4_ach_locked_main_disk_unchanged"));
				FMMOAchievementCounters C; TArray<FName> U;
				Check(B2->SaveNow() && !B2->bDirty && ReadSlot(SlotK, C, U) && C.KillTotal == B2->GetCounters().KillTotal
					&& !FM.FileExists(*(PathOf(SlotK) + TEXT(".tmp"))), TEXT("y4_ach_save_after_unlock_writes"));
			}
		}
		for (TStrongObjectPtr<AMMOCharacter>* Q : { &Q1, &Q2, &Q3, &Q4, &Q5 })
		{
			if (::IsValid(Q->Get())) (*Q)->Destroy(); // EndPlay kirli durumu kendi yuvasina yazar (asagida silinir)
		}
		for (const FString& Slot : { SlotE, SlotF, SlotL, SlotM, SlotG, SlotH, SlotI, SlotJ, SlotK })
		{
			for (const TCHAR* Ext : { TEXT(""), TEXT(".tmp"), TEXT(".bad") })
			{
				FM.Delete(*(PathOf(Slot) + Ext), false, true, true);
			}
		}
	}
	// --- Tur 3 (Y7) + tur 4 (Y-NEW-3/4): yuva tasimasinda eski yuva dosyasinin silinme kurallari (GUID'li yuvalar) ---
	{
		const FString Tag3 = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
		const FString AchDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MMOAchievements"));
		IFileManager& FM = IFileManager::Get();
		auto PathOf = [&AchDir](const FString& Slot) { return FPaths::Combine(AchDir, Slot + TEXT(".json")); };
		auto Has = [&FM, &PathOf](const FString& Slot) { return FM.FileExists(*PathOf(Slot)); };
		const FString SlotP = TEXT("MMOSave_ZAchFP_") + Tag3, SlotQ = TEXT("MMOSave_ZAchFQ_") + Tag3, SlotR = TEXT("MMOSave_ZAchFR_") + Tag3;
		const FString NameS = TEXT("ZAchFS_") + Tag3; // canli adi bu yuvayi veren cevrimici oyuncu senaryosu
		const FString SlotS = TEXT("MMOSave_") + NameS, SlotT = TEXT("MMOSave_ZAchFT_") + Tag3, SlotU = TEXT("MMOSave_ZAchFU_") + Tag3;
		const FString SlotY = TEXT("MMOSave_ZAchFY_") + Tag3; // diger oyuncunun ilgisiz yuvasi
		// Test turu 5: pawn'siz / izleyici pawn'li bagli oyuncu (GameState->PlayerArray) senaryolarinin yuvalari.
		const FString SlotV1 = TEXT("MMOSave_ZAchFV1_") + Tag3, SlotV2 = TEXT("MMOSave_ZAchFV2_") + Tag3, SlotV3 = TEXT("MMOSave_ZAchFV3_") + Tag3;
		const FString SlotV4 = TEXT("MMOSave_ZAchFV4_") + Tag3, SlotV5 = TEXT("MMOSave_ZAchFV5_") + Tag3;
		const FName Goblin(TEXT("Goblin"));
		TStrongObjectPtr<AMMOCharacter> X(World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location + FVector(20000, 0, 0), FRotator::ZeroRotator, Params));
		TStrongObjectPtr<AMMOCharacter> Y(World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location + FVector(20000, 5000, 0), FRotator::ZeroRotator, Params));
		TStrongObjectPtr<APlayerState> PSY(World->SpawnActor<APlayerState>(APlayerState::StaticClass(), Location, FRotator::ZeroRotator, Params));
		TStrongObjectPtr<APlayerState> PSV(World->SpawnActor<APlayerState>(APlayerState::StaticClass(), Location, FRotator::ZeroRotator, Params)); // pawn'siz bagli oyuncu
		TStrongObjectPtr<APlayerState> PSX(World->SpawnActor<APlayerState>(APlayerState::StaticClass(), Location, FRotator::ZeroRotator, Params)); // X'in kendi PS'i (son adim)
		TStrongObjectPtr<ASpectatorPawn> Spec(World->SpawnActor<ASpectatorPawn>(ASpectatorPawn::StaticClass(), Location + FVector(20000, 10000, 0), FRotator::ZeroRotator, Params));
		UMMOAchievementComponent* BX = X.IsValid() ? X->GetAchievementComponent() : nullptr;
		const bool bFixture3 = BX && Y.IsValid() && PSY.IsValid();
		if (bFixture3)
		{
			PSY->SetPlayerName(TEXT("ZAchFOwner_") + Tag3);
			Y->SetPlayerState(PSY.Get()); // IsPlayerControlled: eski yuvanin "cevrimici baska oyuncu" sahibi olabilir
			Y->SetSaveIdentityForTest(true, SlotY, 1);
			X->SetSaveIdentityForTest(true, SlotP, 1);
			BX->NotifyMobKilled(Goblin); // yukler, ilk kayit hemen
		}
		Check(bFixture3 && Y->IsPlayerControlled() && Has(SlotP), TEXT("f_fixture_old_slot_written"));
		if (bFixture3)
		{
			// Basarili yazimdan sonra eski yuva (ve .tmp'si) silinir.
			X->SetSaveIdentityForTest(true, SlotQ, 1);
			BX->NotifyMobKilled(Goblin);
			Check(Has(SlotQ) && !Has(SlotP) && !FM.FileExists(*(PathOf(SlotP) + TEXT(".tmp"))), TEXT("f_move_success_deletes_old_slot"));
			// Yeni yuvaya yazim basarisizsa eski yuva silinmez; sonraki basarili yazimda silinir.
			BX->SetSaveFailForTest(true);
			X->SetSaveIdentityForTest(true, SlotR, 1);
			BX->NotifyMobKilled(Goblin);
			Check(Has(SlotQ) && !Has(SlotR) && !BX->PendingOldStoragePath.IsEmpty(), TEXT("f_move_write_fail_keeps_old_slot"));
			BX->SetSaveFailForTest(false);
			Check(BX->SaveNow() && Has(SlotR) && !Has(SlotQ) && BX->PendingOldStoragePath.IsEmpty(), TEXT("f_move_retry_success_deletes_old_slot"));
			// Eski yuva su an cevrimici BASKA bir oyuncunun yukledigi yuva (ActiveSaveSlot): silinmez.
			Y->SetSaveIdentityForTest(true, SlotR, 1);
			X->SetSaveIdentityForTest(true, SlotS, 1);
			BX->NotifyMobKilled(Goblin);
			Check(Has(SlotS) && Has(SlotR), TEXT("f_old_slot_of_online_player_active_kept"));
			// Eski yuva cevrimici baska oyuncunun CANLI adinin yuvasi (GetSaveSlotName): silinmez.
			Y->SetSaveIdentityForTest(true, SlotY, 1);
			PSY->SetPlayerName(NameS);
			X->SetSaveIdentityForTest(true, SlotT, 1);
			BX->NotifyMobKilled(Goblin);
			Check(Has(SlotT) && Has(SlotS), TEXT("f_old_slot_of_online_player_live_name_kept"));
			PSY->SetPlayerName(TEXT("ZAchFOwner_") + Tag3);
			// A -> B -> A (B'ye yazim basarisiz): geri donuste ayni dosya; basarili yazim onu silmez, B hic olusmaz.
			BX->SetSaveFailForTest(true);
			X->SetSaveIdentityForTest(true, SlotU, 1);
			BX->NotifyMobKilled(Goblin);
			X->SetSaveIdentityForTest(true, SlotT, 1);
			BX->NotifyMobKilled(Goblin);
			BX->SetSaveFailForTest(false);
			Check(BX->PendingOldStoragePath.IsEmpty() && BX->SaveNow() && Has(SlotT) && !Has(SlotU), TEXT("f_round_trip_back_keeps_slot"));
			// Eski yol yeni yolla kanonik olarak ayni dosya (NTFS harf duyarsiz, ornek MMOSave_Ali -> MMOSave_ali): yeni yazilan dosya
			// silinmez. TryLoad bu durumu tasima saymadigi icin bekleyen eski yol dogrudan verilir (ic koruma tek basina sinanir).
			BX->PendingOldStoragePath = FPaths::Combine(AchDir, SlotT.ToUpper() + TEXT(".json"));
			BX->bDirty = true;
			Check(BX->SaveNow() && Has(SlotT) && BX->PendingOldStoragePath.IsEmpty(), TEXT("f_canonical_same_file_not_deleted"));

			// --- Test turu 5: eski yuva, pawn'i OLMAYAN (ya da AMMOCharacter olmayan izleyici pawn'li) bagli bir oyuncunun canli adinin
			// yuvasiysa silinmez. Yukaridaki pawn dongusu (TActorIterator<AMMOCharacter>) onu gormez; yalniz GameState->PlayerArray gorur. ---
			const AGameStateBase* GS = World->GetGameState();
			const bool bPawnlessFixture = GS && PSV.IsValid() && PSX.IsValid() && Spec.IsValid() && GS->PlayerArray.Contains(PSV.Get())
				&& GS->PlayerArray.Contains(PSX.Get()) && !PSV->GetPawn() && !X->GetPlayerState();
			Check(bPawnlessFixture, TEXT("y5_fixture_pawnless_player_state_listed"));
			if (bPawnlessFixture)
			{
				// X su an SlotT'de; pawn'siz PSV'nin canli adi SlotT'nin adi: T -> V1 tasimasi T'yi silmemeli.
				PSV->SetPlayerName(TEXT("ZAchFT_") + Tag3);
				X->SetSaveIdentityForTest(true, SlotV1, 1);
				BX->NotifyMobKilled(Goblin);
				Check(Has(SlotV1) && Has(SlotT), TEXT("y5_old_slot_of_pawnless_player_kept"));
				// Kanonik esdeger canli ad (kucuk harf, '_' yerine bosluk) ayni yuvadir: V1 -> V2 tasimasi V1'i silmemeli.
				PSV->SetPlayerName(TEXT("zachfv1 ") + Tag3.ToLower());
				X->SetSaveIdentityForTest(true, SlotV2, 1);
				BX->NotifyMobKilled(Goblin);
				Check(Has(SlotV2) && Has(SlotV1), TEXT("y5_old_slot_of_pawnless_canonical_variant_kept"));
				// Kontrol: pawn'siz oyuncunun adi ilgisizse eski yuva silinir (koruma asiri genis degil).
				PSV->SetPlayerName(TEXT("ZAchFNone_") + Tag3);
				X->SetSaveIdentityForTest(true, SlotV3, 1);
				BX->NotifyMobKilled(Goblin);
				Check(Has(SlotV3) && !Has(SlotV2), TEXT("y5_old_slot_pawnless_unrelated_name_deleted"));
				// Pawn VAR ama AMMOCharacter degil (izleyici): yine yalniz PlayerArray gorur. V3 -> V4 tasimasi V3'u silmemeli.
				Spec->SetPlayerState(PSV.Get());
				PSV->SetPlayerName(TEXT("ZAchFV3_") + Tag3);
				X->SetSaveIdentityForTest(true, SlotV4, 1);
				BX->NotifyMobKilled(Goblin);
				Check(PSV->GetPawn() == Spec.Get() && Has(SlotV4) && Has(SlotV3), TEXT("y5_old_slot_of_spectator_pawn_player_kept"));
				Spec->SetPlayerState(nullptr);
				PSV->SetPlayerName(TEXT("ZAchFNone_") + Tag3);
				// Tasinan karakterin KENDI PlayerState'i eski adi tasiyorsa koruma sayilmaz (SelfPS haric tutulur): V4 silinir.
				PSX->SetPlayerName(TEXT("ZAchFV4_") + Tag3);
				X->SetPlayerState(PSX.Get());
				X->SetSaveIdentityForTest(true, SlotV5, 1);
				BX->NotifyMobKilled(Goblin);
				Check(Has(SlotV5) && !Has(SlotV4), TEXT("y5_own_player_state_old_name_not_a_guard"));
				X->SetPlayerState(nullptr);
			}
		}
		if (::IsValid(Y.Get())) { Y->SetSaveIdentityForTest(false, SlotY, 1); Y->SetTravelSaveCommitted(true); Y->SetPlayerState(nullptr); Y->Destroy(); }
		if (::IsValid(X.Get())) { X->SetPlayerState(nullptr); X->Destroy(); } // EndPlay son durumu son yuvaya yazar (asagida silinir)
		if (::IsValid(Spec.Get())) { Spec->SetPlayerState(nullptr); Spec->Destroy(); }
		if (::IsValid(PSY.Get())) PSY->Destroy();
		if (::IsValid(PSV.Get())) PSV->Destroy();
		if (::IsValid(PSX.Get())) PSX->Destroy();
		for (const FString& Slot : { SlotP, SlotQ, SlotR, SlotS, SlotT, SlotU, SlotY, SlotV1, SlotV2, SlotV3, SlotV4, SlotV5 })
		{
			for (const TCHAR* Ext : { TEXT(""), TEXT(".tmp"), TEXT(".bad") })
			{
				FM.Delete(*(PathOf(Slot) + Ext), false, true, true);
			}
		}
	}
	return bPassed; // Ayni senkron kare: ag/timer kaniti degil; mantik, tablo, JSON ve (GUID'li yuvalarla) disk kaliciligi dogrulanir.
}
// Bu testin KANITLAMADIKLARI: gercek LoadCharacter/MMOSetName/MMOLoad akisi (kimlik SetSaveIdentityForTest ile verilir), 5 sn
// zamanlayici ve 30 sn kayit araligi zamanlamasi, gercek cikista PlayerState'in kopma ani (burada SetPlayerState(nullptr) ile benzetilir), ag replikasyonu, PIE.
// F2 bolumu EndPlay'in TryLoad cagrisini ve tasima mantigini dogrular; motorun gercek yok etme sirasini (UnPossessed kaydi ->
// ActiveSaveSlot guncellemesi -> bilesen EndPlay) KANITLAMAZ: burada yuva degisimi Destroy'dan once elle verilir. Dosya kilidi
// ayni surecteki ozel (paylasimsiz) bir tutamakla benzetilir; antivirus/ag surucusu gibi gercek dis kilitler denenmedi.
// Tur 4 bolumu: "cevrimici baska oyuncu" PlayerState'li ama denetleyicisiz bir pawn ile benzetilir; kanonik-ayni-dosya korumasi TryLoad
// yolundan ulasilamadigi icin (ASCII yuvalarda harf farki tasima sayilmaz) bekleyen eski yol test icinden verilir. 10 sn geri cekilme
// RecoveryRetryAt'tan okunur; gercek 10 sn bekleme kosulmaz.
// Test turu 5 bolumu: "pawn'siz bagli oyuncu" GameState->PlayerArray'e kendiliginden eklenen, denetleyicisiz bir APlayerState ile,
// "izleyici" ASpectatorPawn'a baglanan ayni PlayerState ile benzetilir; gercek olum/izleyici gecisi, ag baglantisi ve PIE kosulmaz.

namespace
{
	class FMMOAchievementTestExec final : public FSelfRegisteringExec
	{
		bool Exec(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestAchievements"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = UMMOAchievementComponent::RunSelfTest(World, Error, Checks);
			UE_LOG(LogTemp, Display, TEXT("[MMO_ACH_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOAchievementTestExec GMMOAchievementTestExec;
}
#endif

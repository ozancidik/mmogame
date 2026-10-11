// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMODailyComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOStatsComponent.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOQuestComponent.h"
#include "Save/MMOSaveGame.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/CoreMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"

bool UMMODailyComponent::RunSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{ OutError = TEXT("authority game world required"); return false; }
	bool bPassed = true;
	auto Check = [&](bool Condition, const TCHAR* Label)
	{
		++OutChecks;
		UE_LOG(LogTemp, Display, TEXT("[MMO_DAILY_CASE] %s check=%s"), Condition ? TEXT("PASS") : TEXT("FAIL"), Label);
		if (!Condition) { bPassed = false; if (OutError.IsEmpty()) OutError = Label; }
	};

	// --- Saf mantik ---
	Check(DayKeyOf(FDateTime(2026, 10, 11)) == 20261011 && DayKeyOf(FDateTime(2027, 1, 1)) == 20270101, TEXT("logic_day_key"));
	{
		TArray<FMMODailyBounty> B;
		BuildBounties(1, 100, B);
		Check(B.Num() == BountyCount && B[0].Kind == EMMODailyKind::KillAny && B[1].Kind == EMMODailyKind::KillBand && B[2].Kind == EMMODailyKind::ClaimQuests, TEXT("logic_build_kinds"));
		Check(B[0].Target == 15 && B[1].Target == 10 && B[2].Target == 1, TEXT("logic_build_targets_lv1"));
		Check(B[0].RewardGold == 15 && B[2].RewardGold == 30 && B[0].RewardXP == 2 && B[2].RewardXP == 4, TEXT("logic_build_rewards_lv1"));
		BuildBounties(80, 1000000, B);
		Check(B[0].Target == 55 && B[1].Target == 30 && B[0].RewardGold == 1200 && B[2].RewardGold == 2400 && B[0].RewardXP == 20000 && B[2].RewardXP == 40000, TEXT("logic_build_lv80"));
		BuildBounties(-5, -5, B);
		Check(B.Num() == BountyCount && B[0].Target == 15 && B[0].RewardXP >= 1 && B[0].RewardGold == 15, TEXT("logic_build_clamps_bad_input"));
		Check(IsKillBandMatch(20, 12) && !IsKillBandMatch(20, 11) && IsKillBandMatch(20, 23) && !IsKillBandMatch(20, 24), TEXT("logic_band_boundaries"));
	}

	// --- JSON dogrulama ---
	{
		TArray<FMMODailyBounty> B; BuildBounties(10, 1000, B);
		B[0].Progress = 7; B[1].bDone = true;
		FString Json; int32 Day = 0, Base = 0; TArray<FMMODailyBounty> R;
		Check(ToJson(20261011, 3, B, Json) && FromJson(Json, Day, Base, R), TEXT("json_roundtrip_ok"));
		Check(Day == 20261011 && Base == 3 && R.Num() == 3 && R[0].Progress == 7 && R[1].bDone && R[2].Kind == EMMODailyKind::ClaimQuests
			&& R[0].Target == B[0].Target && R[2].RewardGold == B[2].RewardGold, TEXT("json_roundtrip_equal"));
		Check(!FromJson(TEXT("not json"), Day, Base, R) && R.IsEmpty(), TEXT("json_rejects_garbage"));
		const FString TwoItems = TEXT("{\"day\":20261011,\"bounties\":[{\"kind\":0,\"target\":5},{\"kind\":1,\"target\":5}]}");
		Check(!FromJson(TwoItems, Day, Base, R), TEXT("json_rejects_wrong_count"));
		const auto Three = [](const FString& Item) { return FString::Printf(TEXT("{\"day\":20261011,\"bounties\":[%s,%s,%s]}"), *Item, *Item, *Item); };
		Check(!FromJson(Three(TEXT("{\"kind\":7,\"target\":5}")), Day, Base, R), TEXT("json_rejects_bad_kind"));
		Check(!FromJson(Three(TEXT("{\"kind\":0,\"target\":0}")), Day, Base, R), TEXT("json_rejects_zero_target"));
		Check(!FromJson(Three(TEXT("{\"kind\":0,\"target\":10001}")), Day, Base, R), TEXT("json_rejects_huge_target"));
		Check(!FromJson(TEXT("{\"day\":5,\"bounties\":[{\"kind\":0,\"target\":5},{\"kind\":0,\"target\":5},{\"kind\":0,\"target\":5}]}"), Day, Base, R), TEXT("json_rejects_bad_day"));
		const FString Clamp = Three(TEXT("{\"kind\":0,\"target\":5,\"progress\":99,\"gold\":-4,\"xp\":99999999999}"));
		Check(FromJson(Clamp, Day, Base, R) && R[0].Progress == 5 && R[0].RewardGold == 0 && R[0].RewardXP == 10000000, TEXT("json_clamps_values"));
	}

	// --- Canli bilesen ---
	const FVector Location(1700000, 1700000, 5000);
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TStrongObjectPtr<AMMOCharacter> Char(World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params));
	TStrongObjectPtr<AMMOCharacter> Other(World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location + FVector(5000, 0, 0), FRotator::ZeroRotator, Params));
	UMMODailyComponent* D = Char.IsValid() ? Char->GetDailyComponent() : nullptr;
	UMMODailyComponent* DOther = Other.IsValid() ? Other->GetDailyComponent() : nullptr;
	UMMOInventoryComponent* Inv = Char.IsValid() ? Char->GetInventory() : nullptr;
	UMMOStatsComponent* St = Char.IsValid() ? Char->GetStats() : nullptr;
	UMMOQuestComponent* Q = Char.IsValid() ? Char->GetQuestComponent() : nullptr;
	Check(D && DOther && Inv && St && Q, TEXT("live_fixture"));
	if (!(D && DOther && Inv && St && Q))
	{
		if (Char.IsValid()) Char->Destroy();
		if (Other.IsValid()) Other->Destroy();
		return false;
	}

	// Kayit yuklenmeden (PlayerState/yuklenme yok) olaylar yok sayilmali ve gorev uretilmemeli.
	DOther->NotifyMobKilled(FName(TEXT("Orc")));
	DOther->EvaluateAll();
	Check(DOther->GetBounties().IsEmpty(), TEXT("live_unloaded_ignores_events"));

	{
		FMMOCharacterSaveData Seed; Seed.Gold = 123; Seed.NextInstanceId = 50;
		Inv->ApplySaveData(Seed);
	}
	St->Level = 10;
	St->ExperienceToNextLevel = 1000;
	St->Experience = 0;
	D->InitForTest(20261011);
	D->EvaluateAll();
	Check(D->GetBounties().Num() == 3 && D->GetDayKey() == 20261011, TEXT("live_first_roll"));
	Check(D->GetBounties()[0].Target == 20 && D->GetBounties()[1].Target == 12 && D->GetBounties()[2].Target == 1, TEXT("live_targets_lv10"));
	const int32 G0 = Inv->Gold;

	for (int32 i = 0; i < 11; ++i) D->NotifyMobKilled(FName(TEXT("Orc"))); // Orc Lv3: [2,13] penceresinde
	Check(!D->GetBounties()[1].bDone && D->GetBounties()[1].Progress == 11 && Inv->Gold == G0, TEXT("live_below_target"));
	D->NotifyMobKilled(FName(TEXT("Orc")));
	Check(D->GetBounties()[1].bDone && Inv->Gold == G0 + 150 && St->Experience > 0, TEXT("live_band_bounty_rewards"));
	for (int32 i = 0; i < 8; ++i) D->NotifyMobKilled(FName(TEXT("Orc")));
	Check(D->GetBounties()[0].bDone && D->GetBounties()[0].Progress == 20 && Inv->Gold == G0 + 300, TEXT("live_any_bounty_rewards"));
	for (int32 i = 0; i < 5; ++i) D->NotifyMobKilled(FName(TEXT("Orc")));
	D->EvaluateAll();
	Check(Inv->Gold == G0 + 300 && D->GetBounties()[0].Progress == 20, TEXT("live_no_double_reward"));
	const int32 XPBefore = St->Experience;
	Q->CompletedQuests.Add(FName(TEXT("Q_TestDaily")));
	D->EvaluateAll();
	Check(D->GetBounties()[2].bDone && Inv->Gold == G0 + 600 && St->Experience > XPBefore, TEXT("live_quest_bounty_double_reward"));

	// Gun ici seviye atlama hedefleri degistirmez.
	St->Level = 40;
	D->NotifyMobKilled(FName(TEXT("Orc")));
	Check(D->GetBounties()[0].Target == 20 && D->GetDayKey() == 20261011, TEXT("live_targets_frozen_within_day"));

	// Yeni gun: yenilenir, ilerleme/tamam sifirlanir, referans gorev sayisi yeni gune tasinir.
	D->SetDayForTest(20261012);
	St->Level = 10;
	D->NotifyMobKilled(FName(TEXT("Goblin"))); // Lv1: KillAny sayar, KillBand [2,13] disinda
	Check(D->GetDayKey() == 20261012 && !D->GetBounties()[0].bDone && !D->GetBounties()[2].bDone, TEXT("live_new_day_resets"));
	Check(D->GetBounties()[0].Progress == 1 && D->GetBounties()[1].Progress == 0, TEXT("live_band_excludes_out_of_window_mob"));
	Check(D->GetBounties()[2].Progress == 0 && Q->CompletedQuests.Num() == 1, TEXT("live_quest_baseline_ignores_old_quests"));
	const int32 GoldNewDay = Inv->Gold;
	D->EvaluateAll();
	Check(Inv->Gold == GoldNewDay, TEXT("live_no_free_reward_on_new_day"));

	Char->Destroy();
	Other->Destroy();

	// --- Kalicilik (gercek disk, GUID'li yuvalar): D5 erken olay, O1 sabit yol/Default yok, A5, gidis-donus, B1, LoadSerial, D3 ---
	{
		const FString Tag = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
		const FString NameA = TEXT("ZDayA_") + Tag;
		const FString NameB = TEXT("ZDayB_") + Tag;
		const FString SlotA = TEXT("MMOSave_") + NameA;
		const FString SlotB = TEXT("MMOSave_") + NameB;
		const FString SlotC = TEXT("MMOSave_ZDayC_") + Tag;
		const FString SlotD = TEXT("MMOSave_ZDayD_") + Tag;
		const int32 Day = 20261011;
		const FString DayDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MMODaily"));
		IFileManager& FM = IFileManager::Get();
		auto PathOf = [&DayDir](const FString& Slot) { return FPaths::Combine(DayDir, Slot + TEXT(".json")); };
		auto ReadSlot = [&PathOf](const FString& Slot, TArray<FMMODailyBounty>& OutB)
		{
			FString Raw; int32 OutDay = 0, OutBase = 0;
			return FFileHelper::LoadFileToString(Raw, *PathOf(Slot)) && FromJson(Raw, OutDay, OutBase, OutB);
		};
		auto WriteFresh = [&FM, &DayDir, Day](const FString& FilePath, int32 KillAnyProgress)
		{
			TArray<FMMODailyBounty> Fresh;
			BuildBounties(10, 1000, Fresh);
			Fresh[0].Progress = KillAnyProgress;
			FString Json;
			FM.MakeDirectory(*DayDir, true);
			return ToJson(Day, 0, Fresh, Json) && FFileHelper::SaveStringToFile(Json, *FilePath);
		};
		const FString DefaultPath = PathOf(TEXT("MMOSave_Default"));
		const int64 DefaultSize0 = FM.FileSize(*DefaultPath);
		const FDateTime DefaultStamp0 = FM.GetTimeStamp(*DefaultPath);
		const FName Orc(TEXT("Orc")); // Lv3: Lv10 icin uygun-seviye penceresinde [2,13]

		TStrongObjectPtr<AMMOCharacter> P1(World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location + FVector(0, 5000, 0), FRotator::ZeroRotator, Params));
		TStrongObjectPtr<APlayerState> PS1(World->SpawnActor<APlayerState>(APlayerState::StaticClass(), Location, FRotator::ZeroRotator, Params));
		TStrongObjectPtr<AMMOCharacter> P2(World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location + FVector(0, 10000, 0), FRotator::ZeroRotator, Params));
		TStrongObjectPtr<APlayerState> PS2(World->SpawnActor<APlayerState>(APlayerState::StaticClass(), Location, FRotator::ZeroRotator, Params));
		UMMODailyComponent* D1 = P1.IsValid() ? P1->GetDailyComponent() : nullptr;
		UMMODailyComponent* D2 = P2.IsValid() ? P2->GetDailyComponent() : nullptr;
		UMMOInventoryComponent* Inv1 = P1.IsValid() ? P1->GetInventory() : nullptr;
		UMMOInventoryComponent* Inv2 = P2.IsValid() ? P2->GetInventory() : nullptr;
		UMMOStatsComponent* St1 = P1.IsValid() ? P1->GetStats() : nullptr;
		UMMOStatsComponent* St2 = P2.IsValid() ? P2->GetStats() : nullptr;
		UMMOQuestComponent* Q2 = P2.IsValid() ? P2->GetQuestComponent() : nullptr;
		const bool bFixture = D1 && D2 && Inv1 && Inv2 && St1 && St2 && Q2 && PS1.IsValid() && PS2.IsValid();
		Check(bFixture, TEXT("persist_fixture"));
		if (bFixture)
		{
			FMMOCharacterSaveData Seed; Seed.Gold = 123; Seed.NextInstanceId = 50;
			Inv1->ApplySaveData(Seed);
			Inv2->ApplySaveData(Seed);
			for (UMMOStatsComponent* S : { St1, St2 }) { S->Level = 10; S->ExperienceToNextLevel = 1000; S->Experience = 0; }
			const int32 G1 = Inv1->Gold;
			const int32 G2 = Inv2->Gold;
			D1->SetDayForTest(Day);
			D2->SetDayForTest(Day);
			PS1->SetPlayerName(NameA);
			P1->SetPlayerState(PS1.Get());

			// D5/O1: karakter kaydi yuklenmeden gelen olaylar SAYILMAZ (kuyruga alinmaz), gorev uretilmez, dosya yazilmaz.
			D1->NotifyMobKilled(Orc); D1->EvaluateAll(); D1->CmdList();
			Check(D1->GetBounties().IsEmpty() && !FM.FileExists(*PathOf(SlotA)), TEXT("d5_unloaded_events_dropped_no_write"));
			P1->SetSaveIdentityForTest(false, SlotA, 1);
			D1->NotifyMobKilled(Orc);
			Check(D1->GetBounties().IsEmpty() && !FM.FileExists(*PathOf(SlotA)), TEXT("d5_slot_without_initial_load_ignored"));
			P1->SetSaveIdentityForTest(true, TEXT("MMOSave_Default"), 1);
			D1->NotifyMobKilled(Orc);
			P1->SetSaveIdentityForTest(true, FString(), 1);
			D1->NotifyMobKilled(Orc);
			Check(D1->GetBounties().IsEmpty(), TEXT("o1_default_or_empty_slot_never_bound"));

			// Yukleme tamamlaninca ilk olay yuklemeyi tetikler, gunu kurar ve SAYILIR.
			P1->SetSaveIdentityForTest(true, SlotA, 1);
			D1->NotifyMobKilled(Orc);
			Check(D1->GetBounties().Num() == BountyCount && D1->GetBounties()[0].Progress == 1 && D1->GetBounties()[1].Progress == 1
				&& D1->GetDayKey() == Day && FM.FileExists(*PathOf(SlotA)), TEXT("d5_first_event_after_load_rolls_and_counts"));

			// A5: yazma basarisizken bDone geri alinir, odul verilmez; yazma duzelince TEK kez odenir.
			D1->SetSaveFailForTest(true);
			for (int32 i = 0; i < 11; ++i) D1->NotifyMobKilled(Orc); // uygun-seviye 12/12
			Check(D1->GetBounties()[1].Progress == 12 && !D1->GetBounties()[1].bDone && Inv1->Gold == G1, TEXT("a5_failed_write_no_reward"));
			D1->EvaluateAll();
			Check(!D1->GetBounties()[1].bDone && Inv1->Gold == G1, TEXT("a5_still_deferred_while_failing"));
			D1->SetSaveFailForTest(false);
			D1->EvaluateAll();
			Check(D1->GetBounties()[1].bDone && Inv1->Gold == G1 + 150, TEXT("a5_retry_after_fix_pays"));
			D1->EvaluateAll();
			D1->NotifyMobKilled(Orc); // her tur 13
			{
				TArray<FMMODailyBounty> R;
				Check(Inv1->Gold == G1 + 150 && ReadSlot(SlotA, R) && R[1].bDone && R[0].Progress == 13, TEXT("a5_paid_once_and_on_disk"));
			}

			// O1: kirli durum (yazma hatasiyla bekletilmis) PlayerState koptuktan sonra EndPlay'de sabit yola yazilir; Default'a dokunulmaz.
			D1->SetSaveFailForTest(true);
			D1->NotifyMobKilled(Orc); // 14, diske gitmedi
			D1->SetSaveFailForTest(false);
			{
				TArray<FMMODailyBounty> R;
				Check(ReadSlot(SlotA, R) && R[0].Progress == 13 && D1->GetBounties()[0].Progress == 14, TEXT("persist_dirty_pending_before_endplay"));
			}
			P1->SetPlayerState(nullptr);
			PS1->Destroy();
			P1->Destroy();
			{
				TArray<FMMODailyBounty> R;
				Check(ReadSlot(SlotA, R) && R[0].Progress == 14 && R[1].bDone, TEXT("o1_endplay_saves_to_pinned_path"));
			}
			Check(FM.FileSize(*DefaultPath) == DefaultSize0 && FM.GetTimeStamp(*DefaultPath) == DefaultStamp0, TEXT("o1_default_file_untouched"));

			// Gidis-donus: ayni yuvayla yeni bilesen diskten yukler; ilerleme/tamam/hedef ayni, odul TEKRAR verilmez.
			PS2->SetPlayerName(NameA);
			P2->SetPlayerState(PS2.Get());
			P2->SetSaveIdentityForTest(true, SlotA, 1);
			D2->EvaluateAll();
			Check(D2->GetDayKey() == Day && D2->GetBounties().Num() == BountyCount && D2->GetBounties()[0].Progress == 14 && D2->GetBounties()[0].Target == 20
				&& D2->GetBounties()[1].bDone && !D2->GetBounties()[2].bDone, TEXT("persist_roundtrip_fresh_component"));
			Check(Inv2->Gold == G2, TEXT("persist_reload_no_regrant"));

			// B1: canli ad degisti, ActiveSaveSlot ayni: durum sifirlanmaz, odul tekrar yok, canli adin yoluna dosya acilmaz.
			PS2->SetPlayerName(NameB);
			D2->NotifyMobKilled(Orc);
			Check(D2->GetBounties()[0].Progress == 15 && D2->GetBounties()[1].bDone && Inv2->Gold == G2, TEXT("b1_rename_keeps_state_no_regrant"));
			Check(!FM.FileExists(*PathOf(SlotB)), TEXT("b1_rename_does_not_bind_live_name"));

			// Karakter yeni ada yazildi (ActiveSaveSlot degisti): durum yeni yuvaya TASINIR.
			P2->SetSaveIdentityForTest(true, SlotB, 1);
			D2->NotifyMobKilled(Orc);
			{
				TArray<FMMODailyBounty> R;
				Check(ReadSlot(SlotB, R) && R[0].Progress == 16 && R[1].bDone && D2->GetBounties()[0].Progress == 16 && Inv2->Gold == G2, TEXT("b1_slot_move_carries_state"));
			}

			// LoadSerial (MMOLoad): kirli durum ESKI yola yazilir; yeni yuva yuklenir; gorev referansi simdiki sayaca gore yeniden alinir.
			Check(WriteFresh(PathOf(SlotC), 2), TEXT("serial_seed_written"));
			D2->SetSaveFailForTest(true);
			D2->NotifyMobKilled(Orc); // 17, diske gitmedi
			D2->SetSaveFailForTest(false);
			P2->SetSaveIdentityForTest(true, SlotC, 2);
			Q2->CompletedQuests.Add(FName(TEXT("Q_TestDailyPersistA")));
			Q2->CompletedQuests.Add(FName(TEXT("Q_TestDailyPersistB")));
			D2->EvaluateAll();
			{
				TArray<FMMODailyBounty> R;
				Check(ReadSlot(SlotB, R) && R[0].Progress == 17, TEXT("serial_reload_flushes_old_state_to_old_path"));
			}
			Check(D2->GetBounties()[0].Progress == 2 && !D2->GetBounties()[1].bDone && !D2->GetBounties()[2].bDone && D2->GetBounties()[2].Progress == 0
				&& Inv2->Gold == G2, TEXT("serial_reload_loads_new_slot_rebases_quests"));
			Q2->CompletedQuests.Add(FName(TEXT("Q_TestDailyPersistC")));
			D2->EvaluateAll();
			Check(D2->GetBounties()[2].bDone && Inv2->Gold == G2 + 300, TEXT("serial_reload_new_quest_counts_once"));

			// D3: yarim kalan yazim (yalniz .tmp) yuklemede geri konur.
			Check(WriteFresh(PathOf(SlotD) + TEXT(".tmp"), 5), TEXT("tmp_seed_written"));
			P2->SetSaveIdentityForTest(true, SlotD, 3);
			D2->EvaluateAll();
			Check(D2->GetBounties()[0].Progress == 5 && FM.FileExists(*PathOf(SlotD)) && !FM.FileExists(*(PathOf(SlotD) + TEXT(".tmp"))) && Inv2->Gold == G2 + 300,
				TEXT("d3_tmp_recovered_on_load"));
		}
		if (::IsValid(P2.Get())) { P2->SetPlayerState(nullptr); P2->Destroy(); }
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

	// --- Guvenlik tur 2 (K-1 F2 + okunamayan dosya): EndPlay'de yuva tasimasi, MMOLoad serisi, kilitli ana dosya ve
	// kurtarilamayan .tmp (yukleme ertelenir, gorev sifirdan acilmaz, odul yok, dosya ezilmez). GUID'li yuvalar. ---
	{
		const FString Tag2 = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
		const FString SlotE = TEXT("MMOSave_ZDayE_") + Tag2;        // F2: eski yuva
		const FString SlotF = TEXT("MMOSave_ZDayF_") + Tag2;        // F2: ad degisimi + kayit sonrasi yeni yuva
		const FString SlotL = TEXT("MMOSave_ZDayL_") + Tag2;        // F2 seri: MMOLoad oncesi yuva
		const FString SlotM = TEXT("MMOSave_ZDayM_") + Tag2;        // F2 seri: MMOLoad ile yuklenen baska karakter
		const FString SlotJ = TEXT("MMOSave_ZDayLockMain_") + Tag2; // kilitli ana dosya
		const FString SlotK = TEXT("MMOSave_ZDayLockTmp_") + Tag2;  // ana dosya yok, kilitli .tmp (calistirici bu adi bekler)
		const int32 Day = 20261011;
		const FString DayDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MMODaily"));
		IFileManager& FM = IFileManager::Get();
		IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
		auto PathOf = [&DayDir](const FString& Slot) { return FPaths::Combine(DayDir, Slot + TEXT(".json")); };
		auto ReadSlot = [&PathOf](const FString& Slot, TArray<FMMODailyBounty>& OutB)
		{
			FString Raw; int32 OutDay = 0, OutBase = 0;
			return FFileHelper::LoadFileToString(Raw, *PathOf(Slot)) && FromJson(Raw, OutDay, OutBase, OutB) && OutB.Num() == BountyCount;
		};
		auto ReadRaw = [](const FString& FilePath) { FString Raw; FFileHelper::LoadFileToString(Raw, *FilePath); return Raw; };
		auto FreshJson = [Day](int32 KillAnyProgress)
		{
			TArray<FMMODailyBounty> Fresh;
			BuildBounties(10, 1000, Fresh);
			Fresh[0].Progress = KillAnyProgress;
			FString Json; ToJson(Day, 0, Fresh, Json);
			return Json;
		};
		const FName Orc(TEXT("Orc"));
		auto Spawn = [&](float Dy)
		{
			return TStrongObjectPtr<AMMOCharacter>(World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location + FVector(9000, Dy, 0), FRotator::ZeroRotator, Params));
		};
		TStrongObjectPtr<AMMOCharacter> R1 = Spawn(0.f), R2 = Spawn(4000.f), R3 = Spawn(8000.f);
		UMMODailyComponent* E1 = R1.IsValid() ? R1->GetDailyComponent() : nullptr;
		UMMODailyComponent* E2 = R2.IsValid() ? R2->GetDailyComponent() : nullptr;
		UMMODailyComponent* E3 = R3.IsValid() ? R3->GetDailyComponent() : nullptr;
		const bool bFixture2 = E1 && E2 && E3 && R2->GetInventory() && R1->GetStats() && R2->GetStats() && R3->GetStats();
		Check(bFixture2, TEXT("r2_fixture"));
		if (bFixture2)
		{
			FMMOCharacterSaveData Seed; Seed.Gold = 123; Seed.NextInstanceId = 50;
			R2->GetInventory()->ApplySaveData(Seed);
			for (AMMOCharacter* R : { R1.Get(), R2.Get(), R3.Get() })
			{
				R->GetStats()->Level = 10; R->GetStats()->ExperienceToNextLevel = 1000; R->GetStats()->Experience = 0;
				R->GetDailyComponent()->SetDayForTest(Day);
			}

			// F2: kirli durum (yazma hatasiyla bekletilmis) varken karakter yeni yuvaya yazildi (ayni LoadSerial) ve yok edildi:
			// EndPlay once TryLoad ile durumu YENI yuvaya tasir; eski yuvaya yazmaz (yeni adla gorevler sifirdan acilmaz).
			R1->SetSaveIdentityForTest(true, SlotE, 1);
			E1->NotifyMobKilled(Orc); // yukler, gunu kurar, 1, diske
			E1->SetSaveFailForTest(true);
			E1->NotifyMobKilled(Orc); // 2, diske gitmedi
			E1->SetSaveFailForTest(false);
			{
				TArray<FMMODailyBounty> B;
				Check(ReadSlot(SlotE, B) && B[0].Progress == 1 && E1->GetBounties()[0].Progress == 2, TEXT("f2_fixture_dirty_pending"));
			}
			R1->SetSaveIdentityForTest(true, SlotF, 1);
			R1->Destroy();
			{
				TArray<FMMODailyBounty> B;
				Check(ReadSlot(SlotF, B) && B[0].Progress == 2, TEXT("f2_endplay_moves_dirty_state_to_new_slot"));
			}
			// Tur 3 (Y7): eski yuva yazilmaz; yeni yuvaya basarili yazimdan sonra eski yuva dosyasi silinir (catal kalmaz).
			Check(!FM.FileExists(*PathOf(SlotE)) && !FM.FileExists(*(PathOf(SlotE) + TEXT(".tmp"))), TEXT("f2_endplay_old_slot_not_written"));
			const int32 G2 = R2->GetInventory()->Gold;
			R2->SetSaveIdentityForTest(true, SlotF, 1);
			E2->EvaluateAll();
			Check(E2->GetBounties().Num() == BountyCount && E2->GetBounties()[0].Progress == 2 && E2->GetDayKey() == Day && R2->GetInventory()->Gold == G2,
				TEXT("f2_new_slot_reload_continues"));

			// F2 seri (MMOLoad: baska karakter yuklendi, LoadSerial artti): kirli durum ESKI yuvaya yazilir, yeni yuvaya TASINMAZ.
			R3->SetSaveIdentityForTest(true, SlotL, 1);
			E3->NotifyMobKilled(Orc);
			E3->SetSaveFailForTest(true);
			E3->NotifyMobKilled(Orc); // 2, kirli
			E3->SetSaveFailForTest(false);
			R3->SetSaveIdentityForTest(true, SlotM, 2);
			R3->Destroy();
			{
				TArray<FMMODailyBounty> BL, BM;
				const bool bNewCarries = ReadSlot(SlotM, BM) && BM[0].Progress == 2;
				Check(ReadSlot(SlotL, BL) && BL[0].Progress == 2 && !bNewCarries, TEXT("f2_serial_change_flushes_old_slot_only"));
				// Gozlem (kontrol degil): EndPlay'de yeni yuva dosyasizken gorev referansi yeniden alinip bos gorev listesi yazilabilir.
				UE_LOG(LogTemp, Display, TEXT("[MMO_DAILY_NOTE] serial_endplay_new_slot_file_exists=%d parses=%d"),
					FM.FileExists(*PathOf(SlotM)) ? 1 : 0, ReadSlot(SlotM, BM) ? 1 : 0);
			}

			// Ana dosya VAR ama okunamiyor (ozel kilit): yukleme ertelenir, gorev uretilmez, odul yok, dosya ezilmez.
			const FString SeedMain = FreshJson(9);
			FM.MakeDirectory(*DayDir, true);
			FFileHelper::SaveStringToFile(SeedMain, *PathOf(SlotJ));
			TUniquePtr<IFileHandle> MainLock(PF.OpenWrite(*PathOf(SlotJ), true, false));
			Check(MainLock.IsValid() && FM.FileExists(*PathOf(SlotJ)), TEXT("locked_main_fixture"));
			const int32 G2b = R2->GetInventory()->Gold;
			R2->SetSaveIdentityForTest(true, SlotJ, 2);
			E2->NotifyMobKilled(Orc);
			Check(E2->GetBounties().IsEmpty() && R2->GetInventory()->Gold == G2b, TEXT("locked_main_defers_load"));
			MainLock.Reset();
			Check(ReadRaw(PathOf(SlotJ)) == SeedMain && !FM.FileExists(*(PathOf(SlotJ) + TEXT(".tmp"))), TEXT("locked_main_not_overwritten"));
			E2->NotifyMobKilled(Orc);
			Check(E2->GetBounties().Num() == BountyCount && E2->GetBounties()[0].Progress == 10 && R2->GetInventory()->Gold == G2b,
				TEXT("locked_main_loads_after_release"));

			// Ana dosya yok, .tmp var ama kilitli (geri konamiyor): yukleme ertelenir, ana dosya olusturulmaz, .tmp bozulmaz.
			// Tur 3 (Y1): kurtarma Move'u tekrar denemesiz (motor "Error moving file" satiri ve 5 sn bekleme YOK; calistirici
			// 0 satir bekler) ve basarisizlikta 10 sn (tur 4) geri cekilir; kilit kalkinca test geri cekilmeyi temizleyip yeniden dener.
			const FString SeedTmp = FreshJson(4);
			FFileHelper::SaveStringToFile(SeedTmp, *(PathOf(SlotK) + TEXT(".tmp")));
			TUniquePtr<IFileHandle> TmpLock(PF.OpenWrite(*(PathOf(SlotK) + TEXT(".tmp")), true, false));
			Check(TmpLock.IsValid() && !FM.FileExists(*PathOf(SlotK)), TEXT("locked_tmp_fixture"));
			R2->SetSaveIdentityForTest(true, SlotK, 3);
			E2->EvaluateAll();
			Check(E2->GetBounties().IsEmpty() && !FM.FileExists(*PathOf(SlotK)) && R2->GetInventory()->Gold == G2b, TEXT("locked_tmp_defers_load"));
			TmpLock.Reset();
			Check(ReadRaw(PathOf(SlotK) + TEXT(".tmp")) == SeedTmp, TEXT("locked_tmp_kept_intact"));
			E2->ResetRecoveryBackoffForTest();
			E2->EvaluateAll();
			Check(E2->GetBounties().Num() == BountyCount && E2->GetBounties()[0].Progress == 4 && R2->GetInventory()->Gold == G2b
				&& FM.FileExists(*PathOf(SlotK)) && !FM.FileExists(*(PathOf(SlotK) + TEXT(".tmp"))), TEXT("locked_tmp_recovered_after_release"));
		}
		for (TStrongObjectPtr<AMMOCharacter>* R : { &R1, &R2, &R3 })
		{
			if (::IsValid(R->Get())) (*R)->Destroy();
		}
		for (const FString& Slot : { SlotE, SlotF, SlotL, SlotM, SlotJ, SlotK })
		{
			for (const TCHAR* Ext : { TEXT(""), TEXT(".tmp"), TEXT(".bad") })
			{
				FM.Delete(*(PathOf(Slot) + Ext), false, true, true);
			}
		}
	}
	return bPassed; // Ayni senkron kare: gercek saat/ag/zamanlayici kaniti degil; gun anahtari zorlanir, disk kaliciligi GUID'li yuvalarla dogrulanir.
}
// Bu testin KANITLAMADIKLARI: gercek LoadCharacter/MMOSetName/MMOLoad akisi (kimlik SetSaveIdentityForTest ile verilir), gercek UTC gun
// donumu ve 5 sn zamanlayici, gercek cikista PlayerState'in kopma ani (SetPlayerState(nullptr) ile benzetilir), ag ve PIE.
// F2 bolumu EndPlay'in TryLoad cagrisini ve tasima mantigini dogrular; motorun gercek yok etme sirasini (UnPossessed kaydi ->
// ActiveSaveSlot guncellemesi -> bilesen EndPlay) KANITLAMAZ: yuva degisimi Destroy'dan once elle verilir. Dosya kilidi ayni
// surecteki ozel (paylasimsiz) bir tutamakla benzetilir; antivirus/ag surucusu gibi gercek dis kilitler denenmedi.

namespace
{
	class FMMODailyTestExec final : public FSelfRegisteringExec
	{
		bool Exec(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestDaily"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = UMMODailyComponent::RunSelfTest(World, Error, Checks);
			UE_LOG(LogTemp, Display, TEXT("[MMO_DAILY_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMODailyTestExec GMMODailyTestExec;
}
#endif

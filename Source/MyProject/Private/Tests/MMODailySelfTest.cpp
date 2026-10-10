// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMODailyComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOStatsComponent.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOQuestComponent.h"
#include "Save/MMOSaveGame.h"
#include "Engine/World.h"
#include "Misc/CoreMisc.h"
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
	return bPassed; // Ayni senkron kare: gercek saat/disk/PlayerState/ag/zamanlayici kaniti degil; gun anahtari ve kayit zorlanarak mantik dogrulanir.
}

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

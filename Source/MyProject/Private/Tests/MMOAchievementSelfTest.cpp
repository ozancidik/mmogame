// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMOAchievementComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Characters/MMOMobArchetype.h"
#include "Components/MMOStatsComponent.h"
#include "Components/MMOInventoryComponent.h"
#include "Save/MMOSaveGame.h"
#include "Engine/World.h"
#include "Misc/CoreMisc.h"
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
	return bPassed; // Ayni senkron kare: PlayerState/diske yazma/ag/timer kaniti degil; yalniz mantik, tablo ve JSON dogrulamasi.
}

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

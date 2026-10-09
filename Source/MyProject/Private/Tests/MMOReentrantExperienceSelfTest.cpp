#include "Components/MMOStatsComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Save/MMOSaveGame.h"
#include "Tests/MMOFirstLevelUpCallbackProbe.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	// Fifteen state/save checks of twelve assertions each, plus 26 callback checks.
	// Keep Scripts/TestReentrantExperience.ps1's exact gate in sync.
	constexpr int32 MMOReentrantExperienceExpectedChecks = 206;
}

bool UMMOStatsComponent::RunReentrantExperienceSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0;
	OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client
		|| !FParse::Param(FCommandLine::Get(), TEXT("unattended"))
		|| !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
	{
		OutError = TEXT("disposable authority headless game world required");
		return false;
	}

	TArray<AMMOCharacter*> Fixtures;
	int32 Failures = 0;
	FString FirstFailure;
	const auto Check = [&](const TCHAR* Case, const TCHAR* Label, bool bOK,
		const FString& Expected, const FString& Actual)
	{
		++OutChecks;
		UE_LOG(LogTemp, Display, TEXT("[MMO_REENTRANT_EXPERIENCE_CASE] %s case=%s check=%s expected={%s} actual={%s}"),
			bOK ? TEXT("PASS") : TEXT("FAIL"), Case, Label, *Expected, *Actual);
		if (!bOK)
		{
			++Failures;
			if (FirstFailure.IsEmpty()) FirstFailure = FString::Printf(TEXT("%s/%s"), Case, Label);
		}
	};
	const auto Number = [&](const TCHAR* Case, const TCHAR* Label, int64 Expected, int64 Actual)
	{
		Check(Case, Label, Expected == Actual,
			FString::Printf(TEXT("%lld"), static_cast<long long>(Expected)),
			FString::Printf(TEXT("%lld"), static_cast<long long>(Actual)));
	};
	const auto Unbind = [](UMMOStatsComponent* S, UMMOFirstLevelUpCallbackProbe* Probe)
	{
		S->OnHealthChanged.RemoveDynamic(Probe, &UMMOFirstLevelUpCallbackProbe::OnRefill);
		S->OnManaChanged.RemoveDynamic(Probe, &UMMOFirstLevelUpCallbackProbe::OnRefill);
		S->OnLevelUp.RemoveDynamic(Probe, &UMMOFirstLevelUpCallbackProbe::OnLevel);
		S->OnExperienceChanged.RemoveDynamic(Probe, &UMMOFirstLevelUpCallbackProbe::OnExperience);
		Probe->Callback = nullptr; // Release stack captures before assertions and destruction.
	};
	const auto VerifyState = [&](const TCHAR* Case, UMMOStatsComponent* S, int32 ExpectedLevel, int32 ExpectedXP)
	{
		const int32 ExpectedStat = (ExpectedLevel - 1) * 3;
		const int32 ExpectedSkill = (ExpectedLevel - 1) * 2;
		Number(Case, TEXT("level"), ExpectedLevel, S->Level);
		Number(Case, TEXT("experience"), ExpectedXP, S->Experience);
		Number(Case, TEXT("threshold"), S->ComputeExperienceForLevel(ExpectedLevel), S->ExperienceToNextLevel);
		Number(Case, TEXT("stat_budget"), ExpectedStat, S->UnspentStatPoints);
		Number(Case, TEXT("skill_budget"), ExpectedSkill, S->UnspentSkillPoints);
		Number(Case, TEXT("guard_reset"), 0, S->bGainExperienceInProgress);
		Number(Case, TEXT("bank_reset"), 0, S->PendingExperienceBalance);
		Check(Case, TEXT("refilled_vitals"), S->Health == S->MaxHealth && S->Mana == S->MaxMana,
			TEXT("health=max mana=max"), FString::Printf(TEXT("health=%g/%g mana=%g/%g"), S->Health, S->MaxHealth, S->Mana, S->MaxMana));
		FMMOCharacterSaveData Saved;
		S->FillSaveData(Saved);
		Number(Case, TEXT("saved_level"), ExpectedLevel, Saved.Level);
		Number(Case, TEXT("saved_experience"), ExpectedXP, Saved.Experience);
		Check(Case, TEXT("saved_budgets"), Saved.UnspentStatPoints == ExpectedStat && Saved.UnspentSkillPoints == ExpectedSkill,
			FString::Printf(TEXT("stat=%d skill=%d"), ExpectedStat, ExpectedSkill),
			FString::Printf(TEXT("stat=%d skill=%d"), Saved.UnspentStatPoints, Saved.UnspentSkillPoints));
		// In-memory save roundtrip only; no slots, controllers or persistence timers.
		S->ApplySaveData(Saved);
		const bool bRoundtrip = S->Level == Saved.Level && S->Experience == Saved.Experience
			&& S->UnspentStatPoints == Saved.UnspentStatPoints && S->UnspentSkillPoints == Saved.UnspentSkillPoints
			&& S->Strength == Saved.Strength && S->Dexterity == Saved.Dexterity
			&& S->Intelligence == Saved.Intelligence && S->Vitality == Saved.Vitality
			&& !S->bGainExperienceInProgress && S->PendingExperienceBalance == 0;
		Check(Case, TEXT("save_roundtrip"), bRoundtrip, TEXT("progression/points/attributes preserved; guard=0 bank=0"),
			FString::Printf(TEXT("preserved=%d guard=%d bank=%lld"), bRoundtrip, S->bGainExperienceInProgress,
				static_cast<long long>(S->PendingExperienceBalance)));
	};

	const bool bCompleted = [&]()
	{
		UMMOStatsComponent* Stats[13] = {};
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		bool bAuthorityNPCs = true;
		for (int32 I = 0; I < 13; ++I)
		{
			AMMOCharacter* Pawn = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(),
				FVector(1000000 + I * 5000, 1000000, 5000), FRotator::ZeroRotator, Params);
			if (Pawn) Fixtures.Add(Pawn);
			if (!Pawn || !Pawn->GetStats())
			{
				OutError = FString::Printf(TEXT("pawn fixture %d unavailable"), I);
				return false; // No probes are bound until every fixture is available.
			}
			Stats[I] = Pawn->GetStats();
			bAuthorityNPCs &= Pawn->HasAuthority() && !Pawn->GetController() && !Pawn->IsPlayerControlled();
			Stats[I]->MaxLevel = 80;
			Stats[I]->StatPointsPerLevel = 3;
			Stats[I]->SkillPointsPerLevel = 2;
			FMMOCharacterSaveData Seed;
			Seed.Experience = I == 1 ? 90 : 0;
			Stats[I]->ApplySaveData(Seed);
		}
		Number(TEXT("fixtures"), TEXT("authority_NPCs_no_controller"), 1, bAuthorityNPCs);

		// Five partial awards use the real health, mana and level delegates. Each
		// probe is strongly held through unbind, and its one-shot gate bounds recursion
		// even when the old local-balance algorithm is used for a baseline run.
		const TCHAR* PartialCases[] = { TEXT("health_zero"), TEXT("health_existing90"),
			TEXT("health_multiple_invalid"), TEXT("mana_partial"), TEXT("level_partial") };
		const int32 ExpectedXP[] = { 10, 35, 30, 10, 10 };
		for (int32 I = 0; I < 5; ++I)
		{
			UMMOStatsComponent* S = Stats[I];
			TStrongObjectPtr<UMMOFirstLevelUpCallbackProbe> Probe(NewObject<UMMOFirstLevelUpCallbackProbe>());
			int32 SeenLevel = -1;
			int64 SeenBank = -1;
			bool bSeenGuard = false;
			Probe->Callback = [&]()
			{
				S->GainExperience(10);
				if (I == 2)
				{
					S->GainExperience(20);
					S->GainExperience(0);
					S->GainExperience(-20);
				}
				SeenLevel = S->Level;
				SeenBank = S->PendingExperienceBalance;
				bSeenGuard = S->bGainExperienceInProgress;
			};
			if (I == 3) S->OnManaChanged.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
			else if (I == 4) S->OnLevelUp.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnLevel);
			else S->OnHealthChanged.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
			S->GainExperience(I == 1 ? 35 : 100);
			Unbind(S, Probe.Get());
			Number(PartialCases[I], TEXT("callback_fired"), 1, Probe->bFired);
			Check(PartialCases[I], TEXT("accepted_into_active_bank"), SeenLevel == 2 && bSeenGuard && SeenBank == ExpectedXP[I],
				FString::Printf(TEXT("level=2 guard=1 bank=%d"), ExpectedXP[I]),
				FString::Printf(TEXT("level=%d guard=%d bank=%lld"), SeenLevel, bSeenGuard, static_cast<long long>(SeenBank)));
			VerifyState(PartialCases[I], S, 2, ExpectedXP[I]);
		}

		{
			UMMOStatsComponent* S = Stats[5];
			TStrongObjectPtr<UMMOFirstLevelUpCallbackProbe> Probe(NewObject<UMMOFirstLevelUpCallbackProbe>());
			int32 BeforeLevel = -1, AfterLevel = -1, Calls = 0;
			int64 SeenBank = -1;
			bool bSeenGuard = false;
			Probe->Callback = [&]()
			{
				++Calls;
				BeforeLevel = S->Level;
				S->GainExperience(125);
				AfterLevel = S->Level;
				SeenBank = S->PendingExperienceBalance;
				bSeenGuard = S->bGainExperienceInProgress;
			};
			S->OnHealthChanged.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
			S->OnLevelUp.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnLevel);
			S->GainExperience(100);
			Unbind(S, Probe.Get());
			Number(TEXT("nested_threshold"), TEXT("shared_one_shot_calls"), 1, Calls);
			Number(TEXT("nested_threshold"), TEXT("level_before_nested"), 2, BeforeLevel);
			// A nested accepted award is queued: its caller still sees Lv2. Only
			// the outer drain commits Lv3 after this refill/level callback returns.
			Number(TEXT("nested_threshold"), TEXT("level_after_nested_queued"), 2, AfterLevel);
			Check(TEXT("nested_threshold"), TEXT("queue_state"), bSeenGuard && SeenBank == 125,
				TEXT("guard=1 bank=125"), FString::Printf(TEXT("guard=%d bank=%lld"), bSeenGuard, static_cast<long long>(SeenBank)));
			VerifyState(TEXT("nested_threshold"), S, 3, 0);
		}

		{
			UMMOStatsComponent* S = Stats[6];
			TStrongObjectPtr<UMMOFirstLevelUpCallbackProbe> Probe(NewObject<UMMOFirstLevelUpCallbackProbe>());
			int64 SeenBank = -1;
			int32 SeenLevel = -1;
			bool bSeenGuard = false;
			Probe->Callback = [&]()
			{
				S->GainExperience(MAX_int32);
				SeenBank = S->PendingExperienceBalance;
				SeenLevel = S->Level;
				bSeenGuard = S->bGainExperienceInProgress;
			};
			S->OnHealthChanged.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
			S->GainExperience(MAX_int32);
			Unbind(S, Probe.Get());
			Number(TEXT("max_int32_cap"), TEXT("callback_fired"), 1, Probe->bFired);
			const int64 ExpectedBank = static_cast<int64>(MAX_int32) * 2 - 100;
			Check(TEXT("max_int32_cap"), TEXT("64bit_active_bank"), SeenBank == ExpectedBank && SeenLevel == 2 && bSeenGuard,
				FString::Printf(TEXT("level=2 guard=1 bank=%lld"), static_cast<long long>(ExpectedBank)),
				FString::Printf(TEXT("level=%d guard=%d bank=%lld"), SeenLevel, bSeenGuard, static_cast<long long>(SeenBank)));
			VerifyState(TEXT("max_int32_cap"), S, 80, 0); // Exactly 237 stat / 158 skill points.
			Probe->bFired = false;
			Probe->Callback = []() {};
			S->OnHealthChanged.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
			S->OnManaChanged.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
			S->OnLevelUp.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnLevel);
			S->OnExperienceChanged.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnExperience);
			S->GainExperience(MAX_int32);
			Unbind(S, Probe.Get());
			Number(TEXT("cap_noop"), TEXT("any_callback_fired"), 0, Probe->bFired);
			VerifyState(TEXT("cap_noop"), S, 80, 0);
		}

		{
			UMMOStatsComponent* S = Stats[7];
			TStrongObjectPtr<UMMOFirstLevelUpCallbackProbe> Probe(NewObject<UMMOFirstLevelUpCallbackProbe>());
			bool bRejected = false;
			FString Observed;
			Probe->Callback = [&]()
			{
				const int64 BeforeBank = S->PendingExperienceBalance;
				const int32 BeforeXP = S->Experience;
				const float BeforeHealth = S->Health;
				S->bIsDead = true;
				S->Health = 0.f;
				S->GainExperience(10);
				bRejected = S->PendingExperienceBalance == BeforeBank && S->Experience == BeforeXP && S->Level == 2;
				Observed = FString::Printf(TEXT("bank=%lld->%lld xp=%d->%d level=%d"),
					static_cast<long long>(BeforeBank), static_cast<long long>(S->PendingExperienceBalance), BeforeXP, S->Experience, S->Level);
				S->bIsDead = false;
				S->Health = BeforeHealth;
			};
			S->OnHealthChanged.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
			S->GainExperience(100);
			Unbind(S, Probe.Get());
			Number(TEXT("dead_nested"), TEXT("callback_fired"), 1, Probe->bFired);
			Check(TEXT("dead_nested"), TEXT("award_rejected"), bRejected, TEXT("bank and XP unchanged; level=2"), Observed);
			VerifyState(TEXT("dead_nested"), S, 2, 0);
		}

		{
			UMMOStatsComponent* A = Stats[8];
			UMMOStatsComponent* B = Stats[9];
			TStrongObjectPtr<UMMOFirstLevelUpCallbackProbe> ProbeA(NewObject<UMMOFirstLevelUpCallbackProbe>());
			TStrongObjectPtr<UMMOFirstLevelUpCallbackProbe> ProbeB(NewObject<UMMOFirstLevelUpCallbackProbe>());
			bool bIndependent = false;
			FString Observed;
			ProbeB->Callback = [&]() { A->GainExperience(10); };
			ProbeA->Callback = [&]()
			{
				B->GainExperience(100);
				bIndependent = B->Level == 2 && B->Experience == 0 && !B->bGainExperienceInProgress
					&& B->PendingExperienceBalance == 0 && A->Level == 2 && A->bGainExperienceInProgress && A->PendingExperienceBalance == 10;
				Observed = FString::Printf(TEXT("A:level=%d guard=%d bank=%lld B:level=%d xp=%d guard=%d bank=%lld"),
					A->Level, A->bGainExperienceInProgress, static_cast<long long>(A->PendingExperienceBalance),
					B->Level, B->Experience, B->bGainExperienceInProgress, static_cast<long long>(B->PendingExperienceBalance));
			};
			A->OnHealthChanged.AddDynamic(ProbeA.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
			B->OnHealthChanged.AddDynamic(ProbeB.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
			A->GainExperience(100);
			Unbind(A, ProbeA.Get());
			Unbind(B, ProbeB.Get());
			Number(TEXT("independent"), TEXT("A_callback_fired"), 1, ProbeA->bFired);
			Number(TEXT("independent"), TEXT("B_callback_fired"), 1, ProbeB->bFired);
			Check(TEXT("independent"), TEXT("component_transactions"), bIndependent,
				TEXT("A:level=2 guard=1 bank=10 B:level=2 xp=0 guard=0 bank=0"), Observed);
			VerifyState(TEXT("independent_A"), A, 2, 10);
			VerifyState(TEXT("independent_B"), B, 2, 0);
		}

		Stats[10]->GainExperience(35);
		VerifyState(TEXT("baseline_no_threshold"), Stats[10], 1, 35);
		Stats[11]->GainExperience(100 + 125 + 156 + 17);
		VerifyState(TEXT("baseline_multi_level"), Stats[11], 4, 17);

		{
			UMMOStatsComponent* S = Stats[12];
			TStrongObjectPtr<UMMOFirstLevelUpCallbackProbe> Probe(NewObject<UMMOFirstLevelUpCallbackProbe>());
			bool bGuardBefore = true;
			int64 BankBefore = -1;
			int32 XPBefore = -1, XPAfter = -1;
			Probe->Callback = [&]()
			{
				bGuardBefore = S->bGainExperienceInProgress;
				BankBefore = S->PendingExperienceBalance;
				XPBefore = S->Experience;
				S->GainExperience(7);
				XPAfter = S->Experience;
			};
			S->OnExperienceChanged.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnExperience);
			S->GainExperience(100);
			Unbind(S, Probe.Get());
			Number(TEXT("final_experience"), TEXT("callback_fired"), 1, Probe->bFired);
			Check(TEXT("final_experience"), TEXT("cleared_before_broadcast"), !bGuardBefore && BankBefore == 0 && XPBefore == 0,
				TEXT("guard=0 bank=0 xp=0"), FString::Printf(TEXT("guard=%d bank=%lld xp=%d"), bGuardBefore, static_cast<long long>(BankBefore), XPBefore));
			Number(TEXT("final_experience"), TEXT("nested7_immediately_accepted"), 7, XPAfter);
			VerifyState(TEXT("final_experience"), S, 2, 7);
			S->GainExperience(13);
			VerifyState(TEXT("subsequent_normal_award"), S, 2, 20);
		}
		return true;
	}();

	// Every callback was removed before any assertion/roundtrip. No tick, timer
	// or player controller was introduced; destroy all actors even on spawn failure.
	for (AMMOCharacter* Fixture : Fixtures)
	{
		if (IsValid(Fixture)) Fixture->Destroy();
	}
	if (!bCompleted) return false;
	if (OutChecks != MMOReentrantExperienceExpectedChecks)
	{
		++Failures;
		if (FirstFailure.IsEmpty()) FirstFailure = TEXT("check_count_mismatch");
	}
	if (Failures > 0)
	{
		OutError = FString::Printf(TEXT("failures=%d first=%s expected_checks=%d"), Failures, *FirstFailure, MMOReentrantExperienceExpectedChecks);
	}
	return Failures == 0;
}

namespace
{
	struct FMMOReentrantExperienceTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestReentrantExperience"))) return false;
			FString Error;
			int32 Checks = 0;
			const bool bPassed = UMMOStatsComponent::RunReentrantExperienceSelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_REENTRANT_EXPERIENCE_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOReentrantExperienceTestExec GMMOReentrantExperienceTestExec;
}
#endif

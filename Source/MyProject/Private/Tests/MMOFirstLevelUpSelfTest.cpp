#include "Player/MMOPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOStatsComponent.h"
#include "Save/MMOSaveGame.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Tests/MMOFirstLevelUpCallbackProbe.h"
#include "UObject/StrongObjectPtr.h"

bool AMMOPlayerController::RunFirstLevelUpSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client
		|| !FParse::Param(FCommandLine::Get(), TEXT("unattended"))
		|| !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
	{
		OutError = TEXT("disposable authority headless game world required"); return false;
	}
	TArray<AActor*> Fixtures;
	const bool bPassed = [&]()
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AMMOPlayerController* PCs[5] = {};
		AMMOCharacter* Pawns[6] = {};
		for (int32 I = 0; I < 6; ++I)
		{
			const FVector Location(1000000 + I * 5000, 1000000, 5000);
			Pawns[I] = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
			if (Pawns[I]) Fixtures.Add(Pawns[I]);
			if (!Pawns[I] || !Pawns[I]->GetStats()) { OutError = TEXT("pawn fixture unavailable"); return false; }
			if (I < 5)
			{
				PCs[I] = World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(), Location, FRotator::ZeroRotator, Params);
				if (PCs[I]) Fixtures.Add(PCs[I]);
				if (!PCs[I]) { OutError = TEXT("controller fixture unavailable"); return false; }
			}
		}
		const auto Check = [&](bool Condition, const TCHAR* Label)
		{
			++OutChecks;
			if (!Condition) { OutError = Label; return false; }
			return true;
		};
		const FName Action(TEXT("levelup"));
		if (!Check(!PCs[0]->RecordFirstLevelUpMetric(1), TEXT("unpossessed controller emitted"))) return false;
		PCs[0]->Possess(Pawns[0]);
		UMMOStatsComponent* S = Pawns[0]->GetStats();
		if (!Check(!PCs[0]->RecordFirstLevelUpMetric(0) && !PCs[0]->RecordFirstLevelUpMetric(-1)
			&& !PCs[0]->RecordFirstLevelUpMetric(80) && !PCs[0]->RecordFirstLevelUpMetric(MAX_int32)
			&& !PCs[0]->RecordFirstLevelUpMetric(1), TEXT("invalid transition consumed first action"))) return false;
		S->GainExperience(0); S->GainExperience(-100); S->GainExperience(S->ExperienceToNextLevel - 1);
		if (!Check(S->Level == 1 && !PCs[0]->RecordedFirstActionMetrics.Contains(Action), TEXT("no-threshold XP emitted"))) return false;
		const int32 XPBefore = S->Experience;
		S->bIsDead = true; S->Health = 0.f; S->GainExperience(1);
		if (!Check(S->Level == 1 && S->Experience == XPBefore && !PCs[0]->RecordedFirstActionMetrics.Contains(Action), TEXT("dead XP emitted"))) return false;
		S->bIsDead = false; S->Health = S->MaxHealth;
		S->GainExperience(1);
		if (!Check(S->Level == 2 && S->Experience == 0 && S->UnspentStatPoints == S->StatPointsPerLevel
			&& S->UnspentSkillPoints == S->SkillPointsPerLevel && PCs[0]->RecordedFirstActionMetrics.Contains(Action),
			TEXT("real threshold hook absent or gameplay budget changed"))) return false;
		S->GainExperience(S->ExperienceToNextLevel);
		if (!Check(S->Level == 3 && PCs[0]->RecordedFirstActionMetrics.Num() == 1
			&& !PCs[0]->RecordFirstLevelUpMetric(2), TEXT("later level emitted again"))) return false;
		PCs[0]->Possess(Pawns[1]);
		Pawns[1]->GetStats()->GainExperience(Pawns[1]->GetStats()->ExperienceToNextLevel);
		if (!Check(Pawns[1]->GetStats()->Level == 2 && PCs[0]->RecordedFirstActionMetrics.Num() == 1, TEXT("pawn replacement reset first"))) return false;
		const FGuid OriginalSession = PCs[0]->FirstActionMetricsSession;
		const double OriginalStart = PCs[0]->FirstActionMetricsStart;
		PCs[0]->SeamlessTravelTo(PCs[1]);
		PCs[1]->Possess(Pawns[1]);
		Pawns[1]->GetStats()->GainExperience(Pawns[1]->GetStats()->ExperienceToNextLevel);
		if (!Check(PCs[1]->FirstActionMetricsSession == OriginalSession && PCs[1]->FirstActionMetricsStart == OriginalStart
			&& PCs[1]->RecordedFirstActionMetrics.Contains(Action) && Pawns[1]->GetStats()->Level == 3,
			TEXT("controller handoff reset first"))) return false;
		PCs[2]->Possess(Pawns[2]);
		UMMOStatsComponent* Multi = Pawns[2]->GetStats();
		Multi->GainExperience(3 * Multi->ExperienceToNextLevel);
		if (!Check(Multi->Level > 2 && PCs[2]->RecordedFirstActionMetrics.Num() == 1
			&& Multi->UnspentStatPoints == (Multi->Level - 1) * Multi->StatPointsPerLevel
			&& Multi->UnspentSkillPoints == (Multi->Level - 1) * Multi->SkillPointsPerLevel,
			TEXT("multi-level award first transition or points changed"))) return false;
		PCs[3]->Possess(Pawns[3]);
		FMMOCharacterSaveData Restore;
		Pawns[3]->GetStats()->FillSaveData(Restore); Restore.Level = 5; Restore.Experience = 0;
		Pawns[3]->GetStats()->ApplySaveData(Restore);
		if (!Check(Pawns[3]->GetStats()->Level == 5 && !PCs[3]->RecordedFirstActionMetrics.Contains(Action), TEXT("save apply emitted levelup"))) return false;
		Pawns[3]->GetStats()->GainExperience(Pawns[3]->GetStats()->ExperienceToNextLevel);
		if (!Check(Pawns[3]->GetStats()->Level == 6 && PCs[3]->RecordedFirstActionMetrics.Contains(Action), TEXT("restored character real XP hook absent"))) return false;
		PCs[4]->Possess(Pawns[4]);
		Pawns[4]->GetStats()->FillSaveData(Restore); Restore.Level = 79; Restore.Experience = 0;
		Pawns[4]->GetStats()->ApplySaveData(Restore);
		if (!Check(Pawns[4]->GetStats()->Level == 79 && !PCs[4]->RecordedFirstActionMetrics.Contains(Action), TEXT("high-level save emitted"))) return false;
		Pawns[4]->GetStats()->GainExperience(Pawns[4]->GetStats()->ExperienceToNextLevel);
		if (!Check(Pawns[4]->GetStats()->Level == 80 && Pawns[4]->GetStats()->Experience == 0
			&& PCs[4]->RecordedFirstActionMetrics.Contains(Action), TEXT("max adjacent transition absent"))) return false;
		Pawns[4]->GetStats()->GainExperience(MAX_int32);
		if (!Check(Pawns[4]->GetStats()->Level == 80 && Pawns[4]->GetStats()->Experience == 0
			&& PCs[4]->RecordedFirstActionMetrics.Num() == 1, TEXT("capped XP emitted or changed"))) return false;
		Pawns[5]->GetStats()->GainExperience(Pawns[5]->GetStats()->ExperienceToNextLevel);
		if (!Check(!Pawns[5]->IsPlayerControlled() && Pawns[5]->GetStats()->Level == 2, TEXT("NPC fixture failed"))) return false;
		// Reuse the already spawned fixtures with a fresh disposable controller
		// session. Bind the real dynamic health delegate, not a simulated call.
		PCs[4]->RecordedFirstActionMetrics.Reset();
		PCs[4]->FirstActionMetricsSession = FGuid::NewGuid();
		Restore.Level = 1; Restore.Experience = 0;
		UMMOStatsComponent* Reentrant = Pawns[4]->GetStats();
		Reentrant->ApplySaveData(Restore);
		TStrongObjectPtr<UMMOFirstLevelUpCallbackProbe> Probe(NewObject<UMMOFirstLevelUpCallbackProbe>());
		bool bFirstRecordedBeforeRefill = false;
		Probe->Callback = [&]()
		{
			bFirstRecordedBeforeRefill = PCs[4]->RecordedFirstActionMetrics.Contains(Action);
			Reentrant->GainExperience(Reentrant->ExperienceToNextLevel);
		};
		Reentrant->OnHealthChanged.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
		Reentrant->GainExperience(Reentrant->ExperienceToNextLevel);
		Reentrant->OnHealthChanged.RemoveDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
		if (!Check(Probe->bFired && bFirstRecordedBeforeRefill && Reentrant->Level == 3
			&& PCs[4]->RecordedFirstActionMetrics.Num() == 1, TEXT("refill callback stole first transition"))) return false;
		return true;
	}();
	for (AActor* Fixture : Fixtures) { if (IsValid(Fixture)) Fixture->Destroy(); }
	return bPassed; // Same frame: no fixture load/save timers reach a tick.
}

namespace
{
	struct FMMOFirstLevelUpTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestFirstLevelUp"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = AMMOPlayerController::RunFirstLevelUpSelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_FIRST_LEVELUP_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOFirstLevelUpTestExec GMMOFirstLevelUpTestExec;
}
#endif

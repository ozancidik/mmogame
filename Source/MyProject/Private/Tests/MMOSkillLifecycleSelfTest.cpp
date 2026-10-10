#include "Components/MMOSkillComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Characters/MMOMob.h"
#include "Components/MMOStatsComponent.h"
#include "Skills/MMOSkillDatabase.h"
#include "Tests/MMOFirstLevelUpCallbackProbe.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"

bool UMMOSkillComponent::RunCasterLifecycleSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{ OutError = TEXT("authority game world required"); return false; }
	bool bPassed = true;
	for (int32 Kind = 0; Kind < 2; ++Kind)
	for (int32 Route = 0; Route < 2; ++Route)
	for (int32 Phase = 0; Phase < 2; ++Phase)
	for (int32 Mode = 0; Mode < 3; ++Mode)
	{
		const FString Case = FString::Printf(TEXT("kind%d_route%d_phase%d_mode%d"), Kind, Route, Phase, Mode);
		auto Check = [&](bool Condition, const TCHAR* Label)
		{
			++OutChecks;
			UE_LOG(LogTemp, Display, TEXT("[MMO_SKILL_LIFECYCLE_CASE] %s case=%s check=%s"), Condition ? TEXT("PASS") : TEXT("FAIL"), *Case, Label);
			if (!Condition) { bPassed = false; if (OutError.IsEmpty()) OutError = Case + TEXT("/") + Label; }
		};
		const FVector Location(1400000 + OutChecks * 1000, 1400000, 5000);
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		TStrongObjectPtr<AMMOCharacter> Caster(World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params));
		const FTransform TM(FRotator::ZeroRotator, Location + FVector(100, 0, 0));
		AMMOMob* Mob = World->SpawnActorDeferred<AMMOMob>(AMMOMob::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Mob)
		{
			Mob->AutoPossessAI = EAutoPossessAI::Disabled; Mob->ArchetypeId = TEXT("Goblin"); Mob->SetActorEnableCollision(false);
			UGameplayStatics::FinishSpawningActor(Mob, TM); Mob->SetActorTickEnabled(false);
			Mob->bHasHub = false; Mob->ItemDropChance = 0.f; Mob->EquipDropChance = 0.f;
			Mob->GetStats()->MaxHealth = 10000.f; Mob->GetStats()->Health = 1000.f; Mob->GetStats()->Defense = 0.f;
		}
		UMMOSkillComponent* K = Caster.Get() ? Caster->GetSkillComponent() : nullptr;
		UMMOStatsComponent* S = Caster.Get() ? Caster->GetStats() : nullptr;
		const FName Id = Kind == 0 ? TEXT("War_PowerStrike") : TEXT("Pri_HolyLight");
		const FMMOSkillDef* Def = FMMOSkillDatabase::Find(Id);
		Check(Caster.Get() && K && S && Mob && Def, TEXT("fixture"));
		if (!Caster.Get() || !K || !S || !Mob || !Def)
		{ if (IsValid(Caster.Get())) Caster->Destroy(); if (IsValid(Mob)) Mob->Destroy(); continue; }
		Caster->SetActorEnableCollision(false); Caster->SetActorTickEnabled(false);
		K->SetupForClass(Def->RequiredClass); K->LearnedSkills.AddUnique(Id);
		S->Level = 30; S->MaxMana = 10000.f; S->Mana = 1000.f; S->MaxHealth = 10000.f; S->Health = 1000.f;
		auto Terminate = [&]()
		{
			if (Mode == 1) Caster->RouteEndPlay(EEndPlayReason::LevelTransition);
			if (Mode == 2) Caster->Destroy();
		};
		if (Phase == 0) Terminate();
		TStrongObjectPtr<UMMOFirstLevelUpCallbackProbe> Probe(NewObject<UMMOFirstLevelUpCallbackProbe>());
		int32 Callouts = 0;
		Probe->Callback = [&]() { ++Callouts; if (Phase == 1) Terminate(); };
		S->OnManaChanged.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
		const float Now = World->GetTimeSeconds();
		bool bOuter = false;
		if (Route == 0) K->ServerUseSkill_Implementation(Id, Mob); // Native entry only, no RPC transport assertion.
		else { K->RequestedTargetForCast = Mob; bOuter = K->UseSkill(Id); }
		S->OnManaChanged.RemoveDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill); Probe->Callback = nullptr;
		const bool bExpectedAccepted = Mode == 0 || Phase == 1;
		Check(Mode == 0 ? (IsValid(Caster.Get()) && Caster->HasActorBegunPlay() && !Caster->HasStartedEndPlay())
			: (Caster->HasStartedEndPlay() && !Caster->HasActorBegunPlay()), TEXT("termination"));
		Check(Callouts == (bExpectedAccepted ? 1 : 0) && Probe->bFired == bExpectedAccepted, TEXT("callout"));
		Check(FMath::IsNearlyEqual(S->Mana, 1000.f - (bExpectedAccepted ? Def->ManaCost : 0.f)), TEXT("mana"));
		const float* End = K->CooldownEndTimes.Find(Id);
		Check(bExpectedAccepted ? End && FMath::IsNearlyEqual(*End, Now + Def->Cooldown) : !End, TEXT("cooldown"));
		Check(Route == 0 || bOuter == bExpectedAccepted, TEXT("acceptance"));
		Check(FMath::IsNearlyEqual(Mob->GetStats()->Health, 1000.f - (Kind == 0 && Mode == 0 ? Def->Power : 0.f))
			&& FMath::IsNearlyEqual(S->Health, 1000.f + (Kind == 1 && Mode == 0 ? Def->Power : 0.f)), TEXT("effect"));
		Check(Route == 0 ? !K->RequestedTargetForCast.IsValid() : K->RequestedTargetForCast.Get() == Mob, TEXT("context"));
		K->RequestedTargetForCast.Reset(); K->CooldownEndTimes.Remove(Id);
		const float BeforeMana = S->Mana, BeforeHealth = S->Health, BeforeMob = Mob->GetStats()->Health;
		bool bRetry = false;
		if (Route == 0) K->ServerUseSkill_Implementation(Id, Mob);
		else { K->RequestedTargetForCast = Mob; bRetry = K->UseSkill(Id); K->RequestedTargetForCast.Reset(); }
		Check((Route == 0 || bRetry == (Mode == 0)) && (K->CooldownEndTimes.Contains(Id) == (Mode == 0)), TEXT("retry"));
		Check(FMath::IsNearlyEqual(S->Mana, BeforeMana - (Mode == 0 ? Def->ManaCost : 0.f))
			&& FMath::IsNearlyEqual(S->Health, BeforeHealth + (Mode == 0 && Kind == 1 ? Def->Power : 0.f))
			&& FMath::IsNearlyEqual(Mob->GetStats()->Health, BeforeMob - (Mode == 0 && Kind == 0 ? Def->Power : 0.f)), TEXT("retry_resources"));
		if (IsValid(Caster.Get())) Caster->Destroy(); if (IsValid(Mob)) Mob->Destroy();
	}
	return bPassed; // Same synchronous frame, no controller/save timers/GC/network proof.
}

namespace
{
	class FMMOSkillLifecycleExec final : public FSelfRegisteringExec
	{
		bool Exec(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestSkillLifecycle"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = UMMOSkillComponent::RunCasterLifecycleSelfTest(World, Error, Checks);
			UE_LOG(LogTemp, Display, TEXT("[MMO_SKILL_LIFECYCLE_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOSkillLifecycleExec GMMOSkillLifecycleExec;
}
#endif

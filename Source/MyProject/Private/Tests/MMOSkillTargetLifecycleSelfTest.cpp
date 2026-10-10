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

bool UMMOSkillComponent::RunTargetLifecycleSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{ OutError = TEXT("authority game world required"); return false; }
	bool bPassed = true;
	for (int32 Kind = 0; Kind < 4; ++Kind)
	for (int32 Route = 0; Route < 2; ++Route)
	for (int32 Phase = 0; Phase < 2; ++Phase)
	for (int32 Mode = 0; Mode < 3; ++Mode)
	{
		const FString Case = FString::Printf(TEXT("kind%d_route%d_phase%d_mode%d"), Kind, Route, Phase, Mode);
		auto Check = [&](bool Condition, const TCHAR* Label)
		{
			++OutChecks;
			UE_LOG(LogTemp, Display, TEXT("[MMO_SKILL_TARGET_LIFECYCLE_CASE] %s case=%s check=%s"), Condition ? TEXT("PASS") : TEXT("FAIL"), *Case, Label);
			if (!Condition) { bPassed = false; if (OutError.IsEmpty()) OutError = Case + TEXT("/") + Label; }
		};
		const FVector Location(1500000 + OutChecks * 1000, 1500000, 5000);
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto Spawn = [&](FVector At, bool bMob) -> AMMOCharacter*
		{
			AMMOCharacter* C = nullptr;
			if (bMob)
			{
				const FTransform TM(FRotator::ZeroRotator, At);
				AMMOMob* M = World->SpawnActorDeferred<AMMOMob>(AMMOMob::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (M)
				{
					M->AutoPossessAI = EAutoPossessAI::Disabled; M->ArchetypeId = TEXT("Goblin"); M->SetActorEnableCollision(false);
					UGameplayStatics::FinishSpawningActor(M, TM); M->bHasHub = false; M->ItemDropChance = 0.f; M->EquipDropChance = 0.f;
				}
				C = M;
			}
			else C = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), At, FRotator::ZeroRotator, Params);
			if (C)
			{
				C->SetActorEnableCollision(false); C->SetActorTickEnabled(false); C->EndSpawnProtection();
				C->GetStats()->MaxHealth = 10000.f; C->GetStats()->Health = 1000.f; C->GetStats()->Defense = 0.f;
			}
			return C;
		};
		TStrongObjectPtr<AMMOCharacter> Caster(Spawn(Location, false));
		TStrongObjectPtr<AMMOCharacter> B(Spawn(Location + FVector(Kind == 1 ? 50 : 100, 0, 0), Kind != 2));
		TStrongObjectPtr<AMMOCharacter> A(Spawn(Location + FVector(Kind == 1 ? 100 : 50, 0, 0), Kind != 2));
		UMMOSkillComponent* K = Caster.Get() ? Caster->GetSkillComponent() : nullptr;
		UMMOStatsComponent* S = Caster.Get() ? Caster->GetStats() : nullptr;
		const FName Id = Kind == 2 ? TEXT("Pri_GreaterHeal") : (Kind == 3 ? TEXT("War_Cleave") : TEXT("War_PowerStrike"));
		const FMMOSkillDef* Def = FMMOSkillDatabase::Find(Id);
		Check(Caster.Get() && K && S && B.Get() && A.Get() && Def, TEXT("fixture"));
		if (!Caster.Get() || !K || !S || !B.Get() || !A.Get() || !Def)
		{ if (IsValid(Caster.Get())) Caster->Destroy(); if (IsValid(B.Get())) B->Destroy(); if (IsValid(A.Get())) A->Destroy(); continue; }
		K->SetupForClass(Def->RequiredClass); K->LearnedSkills.AddUnique(Id);
		S->Level = 30; S->MaxMana = 10000.f; S->Mana = 1000.f;
		auto Terminate = [&]() { if (Mode == 1) B->RouteEndPlay(EEndPlayReason::LevelTransition); if (Mode == 2) B->Destroy(); };
		if (Phase == 0) Terminate();
		TStrongObjectPtr<UMMOFirstLevelUpCallbackProbe> Probe(NewObject<UMMOFirstLevelUpCallbackProbe>());
		int32 Callouts = 0;
		Probe->Callback = [&]() { ++Callouts; if (Phase == 1) Terminate(); };
		S->OnManaChanged.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
		AMMOCharacter* Requested = Kind == 0 || Kind == 2 ? B.Get() : nullptr;
		const float Now = World->GetTimeSeconds();
		bool bAccepted = true;
		if (Route == 0) K->ServerUseSkill_Implementation(Id, Requested); // Native entry, no RPC transport.
		else { K->RequestedTargetForCast = Requested; bAccepted = K->UseSkill(Id); }
		S->OnManaChanged.RemoveDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill); Probe->Callback = nullptr;
		const float BDelta = Mode == 0 ? (Kind == 2 ? Def->Power : -Def->Power) : 0.f;
		const float ADelta = Kind == 3 || (Kind < 2 && Mode != 0) ? -Def->Power : 0.f;
		const float CDelta = Kind == 2 && Mode != 0 ? Def->Power : 0.f;
		Check(Mode == 0 ? (B->HasActorBegunPlay() && !B->HasStartedEndPlay()) : (B->HasStartedEndPlay() && !B->HasActorBegunPlay()), TEXT("termination"));
		Check(Callouts == 1 && Probe->bFired, TEXT("callout"));
		Check(bAccepted, TEXT("acceptance")); // RPC has no bool return; resources assert its acceptance.
		Check(FMath::IsNearlyEqual(S->Mana, 1000.f - Def->ManaCost), TEXT("mana"));
		const float* End = K->CooldownEndTimes.Find(Id);
		Check(End && FMath::IsNearlyEqual(*End, Now + Def->Cooldown), TEXT("cooldown"));
		Check(FMath::IsNearlyEqual(B->GetStats()->Health, 1000.f + BDelta), TEXT("target_effect"));
		Check(FMath::IsNearlyEqual(A->GetStats()->Health, 1000.f + ADelta) && FMath::IsNearlyEqual(S->Health, 1000.f + CDelta), TEXT("other_effect"));
		Check(Route == 0 ? !K->RequestedTargetForCast.IsValid() : K->RequestedTargetForCast.Get() == (IsValid(Requested) ? Requested : nullptr), TEXT("context"));
		K->RequestedTargetForCast.Reset(); K->CooldownEndTimes.Remove(Id);
		K->ServerUseSkill_Implementation(Id, Requested);
		Check(FMath::IsNearlyEqual(S->Mana, 1000.f - 2.f * Def->ManaCost) && K->GetCooldownRemaining(Id) > 0.f
			&& FMath::IsNearlyEqual(B->GetStats()->Health, 1000.f + 2.f * BDelta)
			&& FMath::IsNearlyEqual(A->GetStats()->Health, 1000.f + 2.f * ADelta)
			&& FMath::IsNearlyEqual(S->Health, 1000.f + 2.f * CDelta), TEXT("retry"));
		if (IsValid(Caster.Get())) Caster->Destroy(); if (IsValid(B.Get())) B->Destroy(); if (IsValid(A.Get())) A->Destroy();
	}
	return bPassed; // Strong-owned actors, synchronous frame, no GC/save/FX/network proof.
}

namespace
{
	class FMMOSkillTargetLifecycleExec final : public FSelfRegisteringExec
	{
		bool Exec(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestSkillTargetLifecycle"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = UMMOSkillComponent::RunTargetLifecycleSelfTest(World, Error, Checks);
			UE_LOG(LogTemp, Display, TEXT("[MMO_SKILL_TARGET_LIFECYCLE_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOSkillTargetLifecycleExec GMMOSkillTargetLifecycleExec;
}
#endif

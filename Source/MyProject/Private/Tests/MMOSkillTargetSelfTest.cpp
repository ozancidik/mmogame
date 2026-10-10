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

bool UMMOSkillComponent::RunTargetContextSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{ OutError = TEXT("authority game world required"); return false; }
	bool bPassed = true;
	for (int32 Kind = 0; Kind < 2; ++Kind)
	for (int32 Route = 0; Route < 2; ++Route)
	for (int32 Mode = 0; Mode < 4; ++Mode)
	{
		const FString Case = FString::Printf(TEXT("kind%d_route%d_mode%d"), Kind, Route, Mode);
		auto Check = [&](bool Condition, const TCHAR* Label)
		{
			++OutChecks;
			UE_LOG(LogTemp, Display, TEXT("[MMO_SKILL_TARGET_CASE] %s case=%s check=%s"), Condition ? TEXT("PASS") : TEXT("FAIL"), *Case, Label);
			if (!Condition) { bPassed = false; if (OutError.IsEmpty()) OutError = Case + TEXT("/") + Label; }
		};
		TArray<AActor*> Fixtures;
		const FVector Location(1300000 + OutChecks * 1000, 1300000, 5000);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto SpawnCharacter = [&](FVector At) -> AMMOCharacter*
		{
			AMMOCharacter* C = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), At, FRotator::ZeroRotator, Params);
			if (C) { Fixtures.Add(C); C->SetActorEnableCollision(false); C->SetActorTickEnabled(false); }
			return C;
		};
		AMMOCharacter* Caster = SpawnCharacter(Location);
		AMMOCharacter* Targets[3] = {};
		for (int32 i = 0; i < 3; ++i)
		{
			const FTransform TM(FRotator::ZeroRotator, Location + FVector(50 + 50 * i, 0, 0));
			if (Kind == 0)
			{
				AMMOMob* Mob = World->SpawnActorDeferred<AMMOMob>(AMMOMob::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (Mob)
				{
					Fixtures.Add(Mob); Mob->AutoPossessAI = EAutoPossessAI::Disabled; Mob->ArchetypeId = TEXT("Goblin");
					Mob->SetActorEnableCollision(false); UGameplayStatics::FinishSpawningActor(Mob, TM); Mob->SetActorTickEnabled(false);
					Mob->bHasHub = false; Mob->ItemDropChance = 0.f; Mob->EquipDropChance = 0.f;
				}
				Targets[i] = Mob;
			}
			else Targets[i] = SpawnCharacter(TM.GetLocation());
			if (Targets[i] && Targets[i]->GetStats())
			{
				Targets[i]->EndSpawnProtection();
				Targets[i]->GetStats()->MaxHealth = 10000.f; Targets[i]->GetStats()->Health = 1000.f;
				Targets[i]->GetStats()->Defense = 0.f;
			}
		}
		UMMOSkillComponent* K = Caster ? Caster->GetSkillComponent() : nullptr;
		UMMOStatsComponent* S = Caster ? Caster->GetStats() : nullptr;
		const FName OuterId = Kind == 0 ? TEXT("War_PowerStrike") : TEXT("Pri_GreaterHeal");
		const FName NestedId = Kind == 0 ? TEXT("War_ShieldBash") : TEXT("Pri_DivineRestoration");
		const FMMOSkillDef* OuterDef = FMMOSkillDatabase::Find(OuterId);
		const FMMOSkillDef* NestedDef = FMMOSkillDatabase::Find(NestedId);
		Check(Caster && K && S && OuterDef && NestedDef && Targets[0] && Targets[1] && Targets[2], TEXT("fixture"));
		if (!Caster || !K || !S || !OuterDef || !NestedDef || !Targets[0] || !Targets[1] || !Targets[2])
		{ for (AActor* A : Fixtures) if (IsValid(A)) A->Destroy(); continue; }
		K->SetupForClass(OuterDef->RequiredClass); K->LearnedSkills.AddUnique(OuterId); K->LearnedSkills.AddUnique(NestedId);
		S->Level = 30; S->MaxMana = 10000.f; S->Mana = 1000.f; S->MaxHealth = 10000.f; S->Health = 1000.f;
		const float InitialCasterHealth = S->Health;
		if (Mode == 2) K->CooldownEndTimes.Add(NestedId, World->GetTimeSeconds() + 60.f);
		TStrongObjectPtr<UMMOFirstLevelUpCallbackProbe> Probe(NewObject<UMMOFirstLevelUpCallbackProbe>());
		int32 Callouts = 0; bool bRestored = false, bNestedEffect = false, bNestedMana = false;
		Probe->Callback = [&]()
		{
			++Callouts;
			const float BeforeMana = S->Mana;
			if (Mode == 3) S->Mana = 0.f;
			// Native RPC implementation only: does not simulate transport or actor ownership.
			K->ServerUseSkill_Implementation(Mode == 1 ? FName(TEXT("UnknownSkill")) : NestedId, Targets[2]);
			bRestored = K->RequestedTargetForCast.Get() == Targets[1];
			bNestedMana = FMath::IsNearlyEqual(S->Mana, Mode == 3 ? 0.f : BeforeMana - (Mode == 0 ? NestedDef->ManaCost : 0.f));
			if (Mode == 3) S->Mana = BeforeMana;
			const float NestedHealth = Targets[2]->GetStats()->Health;
			bNestedEffect = FMath::IsNearlyEqual(NestedHealth, 1000.f + (Mode == 0 ? (Kind == 0 ? -NestedDef->Power : NestedDef->Power) : 0.f))
				&& FMath::IsNearlyEqual(K->GetCooldownRemaining(NestedId), Mode == 0 ? NestedDef->Cooldown : (Mode == 2 ? 60.f : 0.f));
		};
		S->OnManaChanged.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
		bool bOuter = true;
		if (Route == 0) K->ServerUseSkill_Implementation(OuterId, Targets[1]);
		else { K->RequestedTargetForCast = Targets[1]; bOuter = K->UseSkill(OuterId); }
		S->OnManaChanged.RemoveDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill); Probe->Callback = nullptr;
		Check(Callouts == 1 && Probe->bFired, TEXT("real_callout"));
		Check(bNestedEffect, TEXT("nested_effect"));
		Check(bNestedMana, TEXT("nested_mana"));
		Check(bRestored, TEXT("restored_context"));
		const float OuterHealth = Targets[1]->GetStats()->Health;
		Check(bOuter && FMath::IsNearlyEqual(OuterHealth, 1000.f + (Kind == 0 ? -OuterDef->Power : OuterDef->Power)), TEXT("outer_target"));
		Check(Targets[0]->GetStats()->Health == 1000.f && S->Health == InitialCasterHealth, TEXT("fallback_target"));
		Check(FMath::IsNearlyEqual(Targets[2]->GetStats()->Health, 1000.f + (Mode == 0 ? (Kind == 0 ? -NestedDef->Power : NestedDef->Power) : 0.f)), TEXT("nested_target"));
		Check(FMath::IsNearlyEqual(S->Mana, 1000.f - OuterDef->ManaCost - (Mode == 0 ? NestedDef->ManaCost : 0.f)), TEXT("outer_mana"));
		Check(FMath::IsNearlyEqual(K->GetCooldownRemaining(OuterId), OuterDef->Cooldown), TEXT("outer_cooldown"));
		Check(Route == 0 ? !K->RequestedTargetForCast.IsValid() : K->RequestedTargetForCast.Get() == Targets[1], TEXT("top_level_context"));
		K->RequestedTargetForCast.Reset(); K->CooldownEndTimes.Remove(OuterId);
		const float BeforeB = Targets[1]->GetStats()->Health, BeforeC = Targets[2]->GetStats()->Health;
		K->ServerUseSkill_Implementation(OuterId, Targets[0]);
		Check(FMath::IsNearlyEqual(Targets[0]->GetStats()->Health, 1000.f + (Kind == 0 ? -OuterDef->Power : OuterDef->Power))
			&& Targets[1]->GetStats()->Health == BeforeB && Targets[2]->GetStats()->Health == BeforeC && !K->RequestedTargetForCast.IsValid(), TEXT("subsequent_target"));
		for (AActor* A : Fixtures) if (IsValid(A)) A->Destroy();
	}
	return bPassed; // Same synchronous frame; no player controller, save timers or network proof.
}

namespace
{
	class FMMOSkillTargetExec final : public FSelfRegisteringExec
	{
		bool Exec(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestSkillTargets"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = UMMOSkillComponent::RunTargetContextSelfTest(World, Error, Checks);
			UE_LOG(LogTemp, Display, TEXT("[MMO_SKILL_TARGET_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOSkillTargetExec GMMOSkillTargetExec;
}
#endif

#include "Components/MMOSkillComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOStatsComponent.h"
#include "Skills/MMOSkillDatabase.h"
#include "Tests/MMOFirstLevelUpCallbackProbe.h"
#include "Engine/World.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"

bool UMMOSkillComponent::RunCastCallbackSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{ OutError = TEXT("authority game world required"); return false; }
	bool bPassed = true;
	for (int32 Kind = 0; Kind < 2; ++Kind)
	for (int32 Outer = 0; Outer < 2; ++Outer)
	for (int32 Nested = 0; Nested < 2; ++Nested)
	for (int32 Phase = 0; Phase < 2; ++Phase)
	for (int32 Different = 0; Different < 2; ++Different)
	{
		const FString Case = FString::Printf(TEXT("kind%d_outer%d_nested%d_phase%d_different%d"), Kind, Outer, Nested, Phase, Different);
		auto Check = [&](bool Condition, const TCHAR* Label)
		{
			++OutChecks;
			UE_LOG(LogTemp, Display, TEXT("[MMO_SKILL_CALLBACK_CASE] %s case=%s check=%s"), Condition ? TEXT("PASS") : TEXT("FAIL"), *Case, Label);
			if (!Condition) { bPassed = false; if (OutError.IsEmpty()) OutError = Case + TEXT("/") + Label; }
		};
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AMMOCharacter* C = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(1200000, 1200000, 5000), FRotator::ZeroRotator, Params);
		UMMOSkillComponent* K = C ? C->GetSkillComponent() : nullptr;
		UMMOStatsComponent* S = C ? C->GetStats() : nullptr;
		const FName Id = Kind == 0 ? TEXT("War_BloodDrain") : TEXT("Pri_HolyLight");
		const FName OtherId = Kind == 0 ? TEXT("War_LifeReap") : TEXT("Pri_GreaterHeal");
		const FMMOSkillDef* Def = FMMOSkillDatabase::Find(Id);
		const FMMOSkillDef* OtherDef = FMMOSkillDatabase::Find(OtherId);
		Check(C && K && S && Def && OtherDef, TEXT("fixture"));
		if (!C || !K || !S || !Def || !OtherDef) { if (C) C->Destroy(); continue; }
		C->SetActorEnableCollision(false); C->SetActorTickEnabled(false);
		K->SetupForClass(Def->RequiredClass); K->LearnedSkills.AddUnique(Id); K->LearnedSkills.AddUnique(OtherId);
		S->Level = 30; S->MaxMana = 10000.f; S->Mana = 1000.f; S->MaxHealth = 10000.f; S->Health = 1000.f;
		TStrongObjectPtr<UMMOFirstLevelUpCallbackProbe> Probe(NewObject<UMMOFirstLevelUpCallbackProbe>());
		int32 Callouts = 0; bool bNestedState = true; bool bObservedCooldown = false;
		Probe->Callback = [&]()
		{
			++Callouts;
			bObservedCooldown = FMath::IsNearlyEqual(K->GetCooldownRemaining(Id), Def->Cooldown);
			const float BeforeMana = S->Mana, BeforeHealth = S->Health;
			const FName NestedId = Different ? OtherId : Id;
			int32 Accepted = 0;
			for (int32 Attempt = 0; Attempt < 3; ++Attempt)
			{
				if (Nested == 0) { if (K->UseSkill(NestedId)) ++Accepted; }
				else K->ServerUseSkill_Implementation(NestedId, nullptr); // Native entry, no network claim.
			}
			const float ExpectedCost = Different ? OtherDef->ManaCost : 0.f;
			const float ExpectedHeal = Different ? OtherDef->Power : 0.f;
			bNestedState = FMath::IsNearlyEqual(S->Mana, BeforeMana - ExpectedCost)
				&& FMath::IsNearlyEqual(S->Health, BeforeHealth + ExpectedHeal)
				&& (Nested != 0 || Accepted == Different);
		};
		if (Phase == 0) S->OnManaChanged.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
		else S->OnHealthChanged.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
		bool bOuter = true;
		if (Outer == 0) bOuter = K->UseSkill(Id);
		else K->ServerUseSkill_Implementation(Id, nullptr);
		S->OnManaChanged.RemoveDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
		S->OnHealthChanged.RemoveDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
		Probe->Callback = nullptr;
		Check(Callouts == 1 && Probe->bFired, TEXT("real_callout"));
		Check(bObservedCooldown, TEXT("callback_cooldown"));
		Check(bNestedState, TEXT("nested_state"));
		Check(bOuter && FMath::IsNearlyEqual(S->Mana, 1000.f - Def->ManaCost - (Different ? OtherDef->ManaCost : 0.f)), TEXT("outer_mana"));
		Check(FMath::IsNearlyEqual(S->Health, 1000.f + Def->Power + (Different ? OtherDef->Power : 0.f)), TEXT("outer_health"));
		Check(FMath::IsNearlyEqual(K->GetCooldownRemaining(Id), Def->Cooldown)
			&& (!Different || FMath::IsNearlyEqual(K->GetCooldownRemaining(OtherId), OtherDef->Cooldown)), TEXT("outer_cooldown"));
		float Mana = S->Mana, Health = S->Health;
		const bool bRejected = !K->UseSkill(Id); K->ServerUseSkill_Implementation(Id, nullptr);
		Check(bRejected && S->Mana == Mana && S->Health == Health, TEXT("cooldown_reject"));
		K->CooldownEndTimes.Reset(); S->Mana = 0.f;
		const bool bNoMana = !K->UseSkill(Id); K->ServerUseSkill_Implementation(Id, nullptr);
		Check(bNoMana && S->Mana == 0.f && S->Health == Health && K->GetCooldownRemaining(Id) == 0.f, TEXT("no_mana_reject"));
		S->Mana = 1000.f;
		Check(K->UseSkill(Id) && FMath::IsNearlyEqual(S->Mana, 1000.f - Def->ManaCost)
			&& FMath::IsNearlyEqual(S->Health, Health + Def->Power), TEXT("retry_after_mana"));
		Mana = S->Mana; Health = S->Health;
		K->CooldownEndTimes.Add(Id, World->GetTimeSeconds() + 0.05f);
		K->ServerUseSkill_Implementation(Id, nullptr);
		Check(FMath::IsNearlyEqual(S->Mana, Mana - Def->ManaCost) && FMath::IsNearlyEqual(S->Health, Health + Def->Power)
			&& FMath::IsNearlyEqual(K->GetCooldownRemaining(Id), Def->Cooldown), TEXT("rpc_grace"));
		Mana = S->Mana; Health = S->Health;
		K->CooldownEndTimes.Add(Id, World->GetTimeSeconds() - 0.01f);
		Check(K->UseSkill(Id) && FMath::IsNearlyEqual(S->Mana, Mana - Def->ManaCost)
			&& FMath::IsNearlyEqual(S->Health, Health + Def->Power), TEXT("expired_retry"));
		C->Destroy(); // Controllerless synchronous fixtures: no player disk load/write.
	}
	return bPassed;
}

namespace
{
	struct FMMOCastCallbackTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestSkillCallbacks"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = UMMOSkillComponent::RunCastCallbackSelfTest(World, Error, Checks);
			Ar.Logf(TEXT("[MMO_SKILL_CALLBACK_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOCastCallbackTestExec GMMOCastCallbackTestExec;
}
#endif

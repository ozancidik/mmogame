#include "Components/MMOSkillComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Characters/MMOMob.h"
#include "Components/MMOStatsComponent.h"
#include "Skills/MMOSkillDatabase.h"
#include "Tests/MMOFirstLevelUpCallbackProbe.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"

bool UMMOSkillComponent::RunEffectContinuationSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
    OutChecks = 0; OutError.Reset();
    if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
    { OutError = TEXT("authority game world required"); return false; }
    bool bPassed = true;
    for (int32 Kind = 0; Kind < 3; ++Kind)
    for (int32 Route = 0; Route < 2; ++Route)
    for (int32 Who = 0; Who < 2; ++Who)
    for (int32 Mode = 0; Mode < 3; ++Mode)
    {
        const FString Case = FString::Printf(TEXT("kind%d_route%d_who%d_mode%d"), Kind, Route, Who, Mode);
        auto Check = [&](bool Condition, const TCHAR* Label)
        {
            ++OutChecks;
            UE_LOG(LogTemp, Display, TEXT("[MMO_SKILL_EFFECT_CONTINUATION_CASE] %s case=%s check=%s"), Condition ? TEXT("PASS") : TEXT("FAIL"), *Case, Label);
            if (!Condition) { bPassed = false; if (OutError.IsEmpty()) OutError = Case + TEXT("/") + Label; }
        };
        const FVector Location(2000000 + OutChecks * 1000, 2000000, 5000);
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
        TStrongObjectPtr<AMMOCharacter> B(Spawn(Location + FVector(100, 0, 0), Kind != 1));
        TStrongObjectPtr<AMMOCharacter> A(Spawn(Location + FVector(50, 0, 0), Kind != 1));
        UMMOSkillComponent* K = Caster.Get() ? Caster->GetSkillComponent() : nullptr;
        UMMOStatsComponent* S = Caster.Get() ? Caster->GetStats() : nullptr;
        const FName Id = Kind == 0 ? TEXT("Arc_PoisonArrow") : (Kind == 1 ? TEXT("Pri_DivineRestoration") : TEXT("Mag_FrostNova"));
        const FMMOSkillDef* Def = FMMOSkillDatabase::Find(Id);
        AMMOCharacter* First = B.Get(); AMMOCharacter* Second = A.Get();
        if (Kind == 2)
        {
            for (TActorIterator<AMMOMob> It(World); It; ++It)
            {
                if (*It == B.Get() || *It == A.Get()) { First = *It; Second = First == B.Get() ? A.Get() : B.Get(); break; }
            }
        }
        Check(Caster.Get() && K && S && B.Get() && A.Get() && Def && !Def->AppliedEffectId.IsNone(), TEXT("fixture"));
        if (!Caster.Get() || !K || !S || !B.Get() || !A.Get() || !Def)
        { if (IsValid(Caster.Get())) Caster->Destroy(); if (IsValid(B.Get())) B->Destroy(); if (IsValid(A.Get())) A->Destroy(); continue; }
        K->SetupForClass(Def->RequiredClass); K->LearnedSkills.AddUnique(Id);
        S->Level = 30; S->MaxMana = 10000.f; S->Mana = 1000.f;
        AMMOCharacter* Victim = Who == 0 ? Caster.Get() : First;
        TStrongObjectPtr<UMMOFirstLevelUpCallbackProbe> Probe(NewObject<UMMOFirstLevelUpCallbackProbe>());
        int32 Callouts = 0; bool bReservedAtCallout = false; bool bDestroyReturned = false;
        Probe->Callback = [&]()
        {
            ++Callouts; bReservedAtCallout = FMath::IsNearlyEqual(S->Mana, 1000.f - Def->ManaCost) && K->GetCooldownRemaining(Id) > 0.f;
            if (Mode == 1) Victim->RouteEndPlay(EEndPlayReason::LevelTransition);
            if (Mode == 2) bDestroyReturned = Victim->Destroy();
        };
        First->GetStats()->OnHealthChanged.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
        AMMOCharacter* Requested = Kind < 2 ? B.Get() : nullptr;
        const float Now = World->GetTimeSeconds(); bool bAccepted = true;
        if (Route == 0) K->ServerUseSkill_Implementation(Id, Requested); // Native entry only.
        else { K->RequestedTargetForCast = Requested; bAccepted = K->UseSkill(Id); }
        First->GetStats()->OnHealthChanged.RemoveDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill); Probe->Callback = nullptr;
        auto HasEffect = [&](AMMOCharacter* C, bool bExpected)
        {
            const TArray<FMMOActiveEffect>& Effects = C->GetStats()->ActiveEffects;
            return bExpected ? (Effects.Num() == 1 && Effects[0].EffectId == Def->AppliedEffectId && Effects[0].Instigator.Get() == Caster.Get()) : Effects.IsEmpty();
        };
        const bool bSecondHit = Kind == 2 && (Mode == 0 || Who == 1);
        Check(Callouts == 1 && Probe->bFired && bReservedAtCallout, TEXT("callout"));
        Check(Mode == 0 ? (IsValid(Victim) && Victim->HasActorBegunPlay() && !Victim->HasStartedEndPlay())
            : (Victim->HasStartedEndPlay() && !Victim->HasActorBegunPlay()
                && (Mode == 1 ? (IsValid(Victim) && !Victim->IsActorBeingDestroyed())
                    : (bDestroyReturned && !IsValid(Victim) && Victim->IsActorBeingDestroyed()))), TEXT("termination"));
        Check(bAccepted, TEXT("acceptance")); // RPC bool is a no-op; resources prove acceptance.
        Check(FMath::IsNearlyEqual(S->Mana, 1000.f - Def->ManaCost), TEXT("mana"));
        const float* End = K->CooldownEndTimes.Find(Id);
        Check(End && FMath::IsNearlyEqual(*End, Now + Def->Cooldown), TEXT("cooldown"));
        Check(FMath::IsNearlyEqual(First->GetStats()->Health, 1000.f + (Kind == 1 ? Def->Power : -Def->Power)), TEXT("primary"));
        Check(HasEffect(First, Mode == 0) && HasEffect(Second, bSecondHit) && S->ActiveEffects.IsEmpty(), TEXT("effects"));
        Check(FMath::IsNearlyEqual(Second->GetStats()->Health, 1000.f - (bSecondHit ? Def->Power : 0.f)) && S->Health == 1000.f, TEXT("remaining"));
        Check(Route == 0 ? !K->RequestedTargetForCast.IsValid() : K->RequestedTargetForCast.Get() == (IsValid(Requested) ? Requested : nullptr), TEXT("context"));
        K->RequestedTargetForCast.Reset();
        if (IsValid(Caster.Get())) Caster->Destroy(); if (IsValid(B.Get())) B->Destroy(); if (IsValid(A.Get())) A->Destroy();
    }
    return bPassed; // Synchronous strong-owned actors: no GC/save/real RPC or visual FX proof.
}
namespace
{
    class FMMOSkillEffectContinuationExec final : public FSelfRegisteringExec
    {
        bool Exec(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
        {
            if (!FParse::Command(&Cmd, TEXT("MMOTestSkillEffectContinuation"))) return false;
            FString Error; int32 Checks = 0;
            const bool bPassed = UMMOSkillComponent::RunEffectContinuationSelfTest(World, Error, Checks);
            UE_LOG(LogTemp, Display, TEXT("[MMO_SKILL_EFFECT_CONTINUATION_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
            return true;
        }
    };
    FMMOSkillEffectContinuationExec GMMOSkillEffectContinuationExec;
}
#endif

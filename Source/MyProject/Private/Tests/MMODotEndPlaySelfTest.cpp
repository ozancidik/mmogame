#include "Components/MMOStatsComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Characters/MMOMob.h"
#include "Effects/MMOEffectDatabase.h"
#include "Tests/MMOFirstLevelUpCallbackProbe.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"

bool UMMOStatsComponent::RunDotEndPlaySelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
    OutError.Reset(); OutChecks = 0;
    if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
    { OutError = TEXT("authority game world required"); return false; }
    bool bPassed = true;
    for (int32 Timing = 0; Timing < 2; ++Timing)
    for (int32 Mode = 0; Mode < 3; ++Mode)
    for (int32 Refresh = 0; Refresh < 3; ++Refresh)
    {
        const FString Case = FString::Printf(TEXT("timing%d_mode%d_refresh%d"), Timing, Mode, Refresh);
        auto Check = [&](bool Condition, const TCHAR* Label)
        {
            ++OutChecks;
            UE_LOG(LogTemp, Display, TEXT("[MMO_DOT_ENDPLAY_CASE] %s case=%s check=%s"), Condition ? TEXT("PASS") : TEXT("FAIL"), *Case, Label);
            if (!Condition) { bPassed = false; if (OutError.IsEmpty()) OutError = Case + TEXT("/") + Label; }
        };
        const FVector Location(2200000 + OutChecks * 1000, 2200000, 5000);
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        TStrongObjectPtr<AMMOCharacter> Source(World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params));
        TStrongObjectPtr<AMMOCharacter> Incoming(World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location + FVector(100, 0, 0), FRotator::ZeroRotator, Params));
        const FTransform TM(FRotator::ZeroRotator, Location + FVector(200, 0, 0));
        TStrongObjectPtr<AMMOMob> Target(World->SpawnActorDeferred<AMMOMob>(AMMOMob::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn));
        if (Target.Get())
        {
            Target->AutoPossessAI = EAutoPossessAI::Disabled; Target->ArchetypeId = TEXT("Goblin"); Target->SetActorEnableCollision(false);
            UGameplayStatics::FinishSpawningActor(Target.Get(), TM); Target->bHasHub = false; Target->ItemDropChance = 0.f; Target->EquipDropChance = 0.f;
        }
        const FMMOEffectDef* Def = FMMOEffectDatabase::Find(TEXT("Poison"));
        UMMOStatsComponent* S = Target.Get() ? Target->GetStats() : nullptr;
        Check(Source.Get() && Incoming.Get() && Target.Get() && S && Def && Def->TickInterval == 1.f && Def->Duration > 4.f, TEXT("fixture"));
        if (!Source.Get() || !Incoming.Get() || !Target.Get() || !S || !Def)
        { if (IsValid(Source.Get())) Source->Destroy(); if (IsValid(Incoming.Get())) Incoming->Destroy(); if (IsValid(Target.Get())) Target->Destroy(); continue; }
        for (AMMOCharacter* C : { Source.Get(), Incoming.Get(), static_cast<AMMOCharacter*>(Target.Get()) })
        { C->SetActorEnableCollision(false); C->SetActorTickEnabled(false); C->GetStats()->SetComponentTickEnabled(false); C->EndSpawnProtection(); }
        Source->GetStats()->AttackPower = 100.f; Incoming->GetStats()->AttackPower = Refresh == 1 ? 20.f : 200.f;
        S->MaxHealth = 10000.f; S->Health = 1000.f; S->Defense = 0.f;
        S->ApplyEffect(TEXT("Poison"), Source.Get());
        Check(S->ActiveEffects.Num() == 1 && S->ActiveEffects[0].bPlayerSource && S->ActiveEffects[0].Instigator.Get() == Source.Get(), TEXT("initial_effect"));
        TStrongObjectPtr<UMMOFirstLevelUpCallbackProbe> Probe(NewObject<UMMOFirstLevelUpCallbackProbe>());
        int32 Callouts = 0; bool bDestroyReturned = false;
        auto Terminate = [&]()
        {
            ++Callouts;
            if (Mode == 1) Source->RouteEndPlay(EEndPlayReason::LevelTransition);
            if (Mode == 2) bDestroyReturned = Source->Destroy();
        };
        Probe->Callback = Terminate;
        if (Timing == 0) Terminate();
        else S->OnHealthChanged.AddDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill);
        S->ProcessEffects(3.f); // Three pending real TakeDamage ticks; first callback can end the source.
        S->OnHealthChanged.RemoveDynamic(Probe.Get(), &UMMOFirstLevelUpCallbackProbe::OnRefill); Probe->Callback = nullptr;
        Check(Callouts == 1 && (Timing == 0 || Probe->bFired), TEXT("callback"));
        Check(Mode == 0 ? (IsValid(Source.Get()) && Source->HasActorBegunPlay() && !Source->HasStartedEndPlay())
            : (Source->HasStartedEndPlay() && !Source->HasActorBegunPlay()
                && (Mode == 1 ? (IsValid(Source.Get()) && !Source->IsActorBeingDestroyed())
                    : (bDestroyReturned && !IsValid(Source.Get()) && Source->IsActorBeingDestroyed()))), TEXT("lifetime"));
        const float OldMagnitude = Def->Magnitude + 10.f; // Existing 0.10 attack scaling, asserted against actual damage.
        const float BatchDamage = Mode == 0 ? 3.f * OldMagnitude : (Timing == 0 ? 0.f : OldMagnitude);
        Check(FMath::IsNearlyEqual(S->Health, 1000.f - BatchDamage), TEXT("batch"));
        if (Refresh != 0) S->ApplyEffect(TEXT("Poison"), Incoming.Get());
        const bool bRetain = Refresh == 0 || (Mode == 0 && Refresh == 1);
        const float Magnitude = bRetain ? OldMagnitude : Def->Magnitude + (Refresh == 1 ? 2.f : 20.f);
        const AActor* ExpectedSource = bRetain ? (IsValid(Source.Get()) ? Source.Get() : nullptr) : Incoming.Get();
        Check(S->ActiveEffects.Num() == 1 && S->ActiveEffects[0].Instigator.Get() == ExpectedSource && S->ActiveEffects[0].bPlayerSource, TEXT("refresh_source"));
        Check(S->ActiveEffects.Num() == 1 && FMath::IsNearlyEqual(S->ActiveEffects[0].Magnitude, Magnitude), TEXT("refresh_magnitude"));
        S->ProcessEffects(1.f);
        const float NextDamage = Refresh == 0 && Mode != 0 ? 0.f : Magnitude;
        Check(FMath::IsNearlyEqual(S->Health, 1000.f - BatchDamage - NextDamage), TEXT("followup"));
        Check(S->ActiveEffects.Num() == 1 && FMath::IsNearlyEqual(S->ActiveEffects[0].RemainingTime, Def->Duration - (Refresh == 0 ? 4.f : 1.f))
            && FMath::IsNearlyZero(S->ActiveEffects[0].TimeSinceLastTick), TEXT("clock"));
        Check(IsValid(Target.Get()) && Target->HasActorBegunPlay() && !Target->HasStartedEndPlay() && !S->bIsDead
            && IsValid(Incoming.Get()) && Incoming->HasActorBegunPlay() && !Incoming->HasStartedEndPlay(), TEXT("context"));
        if (IsValid(Source.Get())) Source->Destroy(); if (IsValid(Incoming.Get())) Incoming->Destroy(); if (IsValid(Target.Get())) Target->Destroy();
    }
    return bPassed; // Controllerless/strong-owned synchronous fixture, no save/GC/real RPC proof.
}
namespace
{
    class FMMODotEndPlayExec final : public FSelfRegisteringExec
    {
        bool Exec(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
        {
            if (!FParse::Command(&Cmd, TEXT("MMOTestDotEndPlay"))) return false;
            FString Error; int32 Checks = 0;
            const bool bPassed = UMMOStatsComponent::RunDotEndPlaySelfTest(World, Error, Checks);
            UE_LOG(LogTemp, Display, TEXT("[MMO_DOT_ENDPLAY_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
            return true;
        }
    };
    FMMODotEndPlayExec GMMODotEndPlayExec;
}
#endif

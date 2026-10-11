#include "Components/MMOStatsComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Player/MMOPlayerController.h"
#include "Tests/MMODamageDeathCallbackProbe.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"

bool UMMOStatsComponent::RunReentrantDeathSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
    OutError.Reset(); OutChecks = 0;
    if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
    { OutError = TEXT("authority game world required"); return false; }
    bool bPassed = true;
    for (int32 Outer = 0; Outer < 3; ++Outer)
    for (int32 Inner = 0; Inner < 3; ++Inner)
    for (int32 Mode = 0; Mode < 7; ++Mode)
    for (int32 Controlled = 0; Controlled < 2; ++Controlled)
    {
        const FString Case = FString::Printf(TEXT("outer%d_inner%d_mode%d_controlled%d"), Outer, Inner, Mode, Controlled);
        auto Check = [&](bool Condition, const TCHAR* Label)
        {
            ++OutChecks;
            UE_LOG(LogTemp, Display, TEXT("[MMO_REENTRANT_DEATH_CASE] %s case=%s check=%s"), Condition ? TEXT("PASS") : TEXT("FAIL"), *Case, Label);
            if (!Condition) { bPassed = false; if (OutError.IsEmpty()) OutError = Case + TEXT("/") + Label; }
        };
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        const FVector Location(2400000 + OutChecks * 100, 2400000, 5000);
        TStrongObjectPtr<AMMOCharacter> Pawn(World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params));
        TStrongObjectPtr<AMMOPlayerController> PC(Controlled ? World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(), Location, FRotator::ZeroRotator, Params) : nullptr);
        UMMOStatsComponent* S = Pawn.Get() ? Pawn->GetStats() : nullptr;
        if (PC.Get() && Pawn.Get()) PC->Possess(Pawn.Get());
        Check(IsValid(Pawn.Get()) && S && Pawn->HasAuthority() && Pawn->HasActorBegunPlay()
            && (Controlled ? (PC.Get() && PC->GetPawn() == Pawn.Get() && Pawn->IsPlayerControlled()) : !Pawn->IsPlayerControlled()), TEXT("fixture"));
        if (!Pawn.Get() || !S || (Controlled && (!PC.Get() || PC->GetPawn() != Pawn.Get())))
        { if (IsValid(PC.Get())) { PC->UnPossess(); PC->Destroy(); } if (IsValid(Pawn.Get())) Pawn->Destroy(); continue; }
        Pawn->SetTravelSaveCommitted(true); // Never initialize player load/save; cleanup cannot save this fixture.
        Pawn->SetActorEnableCollision(false); Pawn->SetActorTickEnabled(false); Pawn->EndSpawnProtection(); S->SetComponentTickEnabled(false);
        // Keep the derived MaxHealth: death effect cleanup recalculates it and must not
        // manufacture a health delta from an artificial fixture maximum.
        S->Health = 100.f; S->Defense = 0.f; S->Experience = 80; S->ExperienceToNextLevel = 100; S->DeathXPPenaltyPct = 0.1f;
        S->ApplyEffect(TEXT("Regen"), nullptr);
        TStrongObjectPtr<UMMODamageDeathCallbackProbe> Probe(NewObject<UMMODamageDeathCallbackProbe>());
        auto Damage = [&](int32 Route, float Amount)
        {
            if (Route == 0) return S->ApplyDamage(Amount, nullptr);
            if (Route == 1) return S->ApplyDamageIgnoringDefense(Amount, nullptr);
            return Pawn->TakeDamage(Amount, FDamageEvent(), nullptr, nullptr);
        };
        const bool bNested = Mode == 1 || Mode == 2 || Mode == 4 || Mode == 5 || Mode == 6;
        const bool bDead = Mode == 2 || Mode == 3 || Mode == 4;
        const int32 ExpectedDeaths = bDead || Mode == 6 ? 1 : 0;
        const float OuterAmount = Mode == 3 || Mode == 4 || Mode == 5 ? 100.f : 20.f;
        float NestedApplied = -1.f;
        bool bCallbackReviveRefilled = true;
        if (bNested) Probe->HealthCallback = [&]()
        {
            if (Mode == 5) { S->Heal(40.f); NestedApplied = -2.f; return; }
            NestedApplied = Damage(Inner, Mode == 1 ? 10.f : (Mode == 4 ? 100.f : 80.f));
            if (Mode == 6)
            {
                S->Revive();
                bCallbackReviveRefilled = !S->bIsDead && S->Health == S->MaxHealth;
                S->Health = 100.f; S->Defense = 0.f;
            }
        };
        S->OnHealthChanged.AddDynamic(Probe.Get(), &UMMODamageDeathCallbackProbe::OnHealth);
        S->OnDeath.AddDynamic(Probe.Get(), &UMMODamageDeathCallbackProbe::OnDeath);
        const float Applied = Damage(Outer, OuterAmount);
        // Clearing Regen on death emits the existing derived-vitals health notification;
        // Mode6 additionally refills on Revive. These are not extra damage callbacks.
        Check(Probe->HealthCalls == (bNested ? 2 : 1) + ExpectedDeaths + (Mode == 6 ? 1 : 0) && Probe->bFired, TEXT("callback"));
        Check(Applied == OuterAmount, TEXT("outer_damage"));
        Check(NestedApplied == (Mode == 5 ? -2.f : (bNested ? (Mode == 1 ? 10.f : (Mode == 4 ? 0.f : 80.f)) : -1.f)), TEXT("nested_damage"));
        Check(bCallbackReviveRefilled && S->Health == (bDead ? 0.f : (Mode == 1 ? 70.f : (Mode == 5 ? 40.f : (Mode == 6 ? 100.f : 80.f)))), TEXT("health"));
        Check(Probe->DeathCalls == ExpectedDeaths, TEXT("death_count"));
        const int32 XP = 80 - (Controlled ? 10 * ExpectedDeaths : 0);
        Check(S->Experience == XP, TEXT("penalty"));
        Check(S->bIsDead == bDead, TEXT("life_state"));
        const int32 HealthCalls = Probe->HealthCalls, DeathCalls = Probe->DeathCalls;
        const float RepeatApplied = bDead ? Damage(Outer, 100.f) : 0.f;
        Check(RepeatApplied == 0.f && Probe->DeathCalls == DeathCalls && Probe->HealthCalls == HealthCalls && S->Experience == XP, TEXT("dead_reentry"));
        Check(S->ActiveEffects.Num() == (ExpectedDeaths ? 0 : 1), TEXT("effects"));
        S->OnHealthChanged.RemoveDynamic(Probe.Get(), &UMMODamageDeathCallbackProbe::OnHealth); Probe->HealthCallback = nullptr;
        S->Revive();
        const bool bNextReviveRefilled = !S->bIsDead && S->Health == S->MaxHealth;
        S->Health = 100.f; S->Defense = 0.f;
        const float NextApplied = S->ApplyDamage(100.f, nullptr);
        Check(bNextReviveRefilled && NextApplied == 100.f && S->Health == 0.f && S->bIsDead && Probe->DeathCalls == ExpectedDeaths + 1, TEXT("next_life_death"));
        Check(S->Experience == XP - (Controlled ? 10 : 0), TEXT("next_life_penalty"));
        S->OnDeath.RemoveDynamic(Probe.Get(), &UMMODamageDeathCallbackProbe::OnDeath);
        if (IsValid(PC.Get())) { PC->UnPossess(); PC->Destroy(); }
        if (IsValid(Pawn.Get())) Pawn->Destroy();
    }
    return bPassed; // Synchronous first-frame world, no network transport/GC/timer expiry proof.
}
namespace
{
    class FMMOReentrantDeathExec final : public FSelfRegisteringExec
    {
        bool Exec(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
        {
            if (!FParse::Command(&Cmd, TEXT("MMOTestReentrantDeath"))) return false;
            FString Error; int32 Checks = 0;
            const bool bPassed = UMMOStatsComponent::RunReentrantDeathSelfTest(World, Error, Checks);
            UE_LOG(LogTemp, Display, TEXT("[MMO_REENTRANT_DEATH_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
            return true;
        }
    };
    FMMOReentrantDeathExec GMMOReentrantDeathExec;
}
#endif

#include "Player/MMOPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOStatsComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include <limits>

bool AMMOPlayerController::RunFirstMoveSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
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
		AMMOCharacter* Pawns[6] = {};
		AMMOPlayerController* PCs[6] = {};
		for (int32 I = 0; I < 6; ++I)
		{
			const FVector Location(1000000 + I * 5000, 1000000, 5000);
			Pawns[I] = World->SpawnActor<AMMOCharacter>(Location, FRotator::ZeroRotator, Params);
			PCs[I] = World->SpawnActor<AMMOPlayerController>(Location, FRotator::ZeroRotator, Params);
			if (Pawns[I]) Fixtures.Add(Pawns[I]);
			if (PCs[I]) Fixtures.Add(PCs[I]);
			if (!Pawns[I] || !PCs[I] || !Pawns[I]->GetStats()) { OutError = TEXT("fixture unavailable"); return false; }
			// Spawned disposable controllers have no ULocalPlayer. Mark them local
			// for the real input-driven CMC tick; no viewport or network connection.
			PCs[I]->SetAsLocalPlayerController();
			Pawns[I]->GetCharacterMovement()->AirControl = 1.f;
			Pawns[I]->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		}
		const auto Check = [&](bool Condition, const TCHAR* Label)
		{
			++OutChecks;
			if (!Condition) { OutError = Label; return false; }
			return true;
		};
		const FName Action(TEXT("move"));
		const auto Seen = [&](AMMOPlayerController* PC) { return PC->RecordedFirstActionMetrics.Contains(Action); };
		const auto Step = [&](AMMOCharacter* TargetPawn, const FVector& Accel)
		{
			TargetPawn->AddMovementInput(Accel.GetSafeNormal(), Accel.IsNearlyZero() ? 0.f : 1.f);
			TargetPawn->GetCharacterMovement()->TickComponent(0.1f, LEVELTICK_All, nullptr);
		};
		const FVector Input(2048, 0, 0);
		AMMOCharacter* P = Pawns[0];
		UCharacterMovementComponent* Move = P->GetCharacterMovement();
		UMMOStatsComponent* Stats = P->GetStats();
		if (!Check(!PCs[0]->RecordFirstMoveMetric(TEXT("walking"), 2.0), TEXT("unpossessed payload emitted"))) return false;
		PCs[0]->Possess(P);
		const int32 OriginalLevel = Stats->Level, OriginalXP = Stats->Experience;
		const float OriginalHealth = Stats->Health, OriginalMana = Stats->Mana;
		if (!Check(!PCs[0]->RecordFirstMoveMetric(TEXT("swimming"), 2.0)
			&& !PCs[0]->RecordFirstMoveMetric(TEXT("falling"), 0.99)
			&& !PCs[0]->RecordFirstMoveMetric(TEXT("walking"), std::numeric_limits<double>::infinity())
			&& !PCs[0]->RecordFirstMoveMetric(TEXT("walking"), std::numeric_limits<double>::quiet_NaN()), TEXT("bad payload consumed first"))) return false;
		// Prime actual engine acceleration while the life-state gate rejects emission.
		Stats->bIsDead = true;
		Step(P, Input);
		if (!Check(!Seen(PCs[0]) && Move->GetCurrentAcceleration().SizeSquared2D() > 0,
			TEXT("dead movement emitted or acceleration fixture failed"))) return false;
		Stats->bIsDead = false;
		Move->bJustTeleported = false; Move->bTeleportedSinceLastUpdate = false;
		const FVector Here = P->GetActorLocation();
		const FVector Old = Here - FVector(2, 0, 0);
		const auto Observe = [&](float Delta, const FVector& Initial, const FVector& Velocity = FVector::ZeroVector)
		{
			P->OnCharacterMovementUpdated.Broadcast(Delta, Initial, Velocity);
			return !Seen(PCs[0]);
		};
		// Synthetic delegate observations isolate rejection gates. Positive first
		// events below come from real input -> TickComponent -> PerformMovement updates.
		if (!Check(Observe(0.f, Old) && Observe(-1.f, Old)
			&& Observe(std::numeric_limits<float>::quiet_NaN(), Old)
			&& Observe(std::numeric_limits<float>::infinity(), Old), TEXT("invalid delta emitted"))) return false;
		if (!Check(Observe(0.1f, FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0))
			&& Observe(0.1f, Old, FVector(std::numeric_limits<double>::infinity(), 0, 0)), TEXT("nonfinite observation emitted"))) return false;
		if (!Check(Observe(0.1f, Here) && Observe(0.1f, Here - FVector(0, 0, 50))
			&& Observe(0.1f, Here - FVector(0.99, 0, 0)), TEXT("blocked/vertical/subthreshold emitted"))) return false;
		Move->bJustTeleported = true;
		if (!Check(Observe(0.1f, Old), TEXT("just teleported emitted"))) return false;
		Move->bJustTeleported = false; Move->bTeleportedSinceLastUpdate = true;
		if (!Check(Observe(0.1f, Old), TEXT("external reposition emitted"))) return false;
		Move->bTeleportedSinceLastUpdate = false; Move->bForceMaxAccel = true;
		if (!Check(Observe(0.1f, Old), TEXT("forced acceleration emitted"))) return false;
		Move->bForceMaxAccel = false;
		Move->Deactivate();
		if (!Check(Observe(0.1f, Old), TEXT("inactive movement emitted"))) return false;
		Move->Activate();
		Move->SetUpdatedComponent(nullptr);
		if (!Check(Observe(0.1f, Old), TEXT("missing updated component emitted"))) return false;
		Move->SetUpdatedComponent(P->GetCapsuleComponent());
		Move->SetMovementMode(MOVE_Falling);
		Stats->bIsDead = true; Step(P, Input); Stats->bIsDead = false;
		Move->bJustTeleported = false; Move->bTeleportedSinceLastUpdate = false;
		for (EMovementMode Mode : {MOVE_None, MOVE_Flying, MOVE_Swimming, MOVE_Custom})
		{
			// Synthetic mode observation without physics side effects: SetMovementMode(None)
			// would clear the acceleration and mask a missing mode rejection.
			Move->MovementMode = Mode;
			if (!Check(Move->GetCurrentAcceleration().SizeSquared2D() > 0 && Observe(0.1f, P->GetActorLocation() - FVector(2, 0, 0)),
				TEXT("unsupported mode emitted or its input precondition absent"))) return false;
		}
		Move->MovementMode = MOVE_Falling;
		Move->SetMovementMode(MOVE_Falling);
		// MOVE_None clears acceleration. Re-prime before the independent life,
		// role, base and root-source gates so zero input cannot hide a broken gate.
		Stats->bIsDead = true; Step(P, Input); Stats->bIsDead = false;
		Move->bJustTeleported = false; Move->bTeleportedSinceLastUpdate = false;
		if (!Check(Move->GetCurrentAcceleration().SizeSquared2D() > 0 && !Seen(PCs[0]), TEXT("gate acceleration reprime failed"))) return false;
		Stats->Health = 0.f;
		if (!Check(Observe(0.1f, Old), TEXT("zero health emitted"))) return false;
		Stats->Health = std::numeric_limits<float>::quiet_NaN();
		if (!Check(Observe(0.1f, Old), TEXT("invalid health emitted"))) return false;
		Stats->Health = Stats->MaxHealth;
		P->SetRole(ROLE_SimulatedProxy);
		const bool bPawnRejected = Observe(0.1f, Old);
		P->SetRole(ROLE_Authority);
		if (!Check(bPawnRejected, TEXT("client pawn observation emitted"))) return false;
		PCs[0]->SetRole(ROLE_SimulatedProxy);
		const bool bPCRejected = Observe(0.1f, Old);
		PCs[0]->SetRole(ROLE_Authority);
		if (!Check(bPCRejected, TEXT("client controller observation emitted"))) return false;
		AActor* Platform = World->SpawnActor<AActor>(Here - FVector(0, 0, 200), FRotator::ZeroRotator, Params);
		if (!Platform) { OutError = TEXT("platform fixture unavailable"); return false; }
		Fixtures.Add(Platform);
		UBoxComponent* Base = NewObject<UBoxComponent>(Platform);
		Platform->SetRootComponent(Base); Platform->AddInstanceComponent(Base);
		Base->SetMobility(EComponentMobility::Movable); Base->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Base->SetWorldLocation(Here - FVector(0, 0, 200));
		Base->RegisterComponent();
		P->SetBase(Base);
		if (!Check(MovementBaseUtility::IsDynamicBase(Base) && Observe(0.1f, Old), TEXT("dynamic base emitted"))) return false;
		P->SetBase(nullptr);
		TSharedPtr<FRootMotionSource_ConstantForce> Root = MakeShared<FRootMotionSource_ConstantForce>();
		Root->InstanceName = TEXT("FirstMoveFixtureRoot"); Root->Duration = 10.f; Root->Force = FVector(100, 0, 0);
		Move->ApplyRootMotionSource(Root);
		if (!Check(Move->HasRootMotionSources() && Observe(0.1f, Old), TEXT("root-motion source emitted"))) return false;
		Move->CurrentRootMotion.Clear();
		Step(P, FVector::ZeroVector);
		if (!Check(Move->GetCurrentAcceleration().IsNearlyZero() && !Seen(PCs[0]), TEXT("idle/coasting physics emitted"))) return false;
		// Teleport after a normal move; engine's outside-update flag must survive
		// even if the falling physics clears bJustTeleported before the callback.
		P->SetActorLocation(P->GetActorLocation() + FVector(100, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
		Step(P, Input);
		if (!Check(Move->bTeleportedSinceLastUpdate && !Seen(PCs[0]), TEXT("real external teleport emitted"))) return false;
		Step(P, Input);
		if (!Check(Seen(PCs[0]) && PCs[0]->RecordedFirstActionMetrics.Num() == 1, TEXT("real falling update hook absent"))) return false;
		Step(P, Input);
		if (!Check(PCs[0]->RecordedFirstActionMetrics.Num() == 1, TEXT("later movement repeated first"))) return false;
		PCs[0]->Possess(Pawns[2]);
		Step(Pawns[2], Input); Step(Pawns[2], Input);
		if (!Check(Seen(PCs[0]) && PCs[0]->RecordedFirstActionMetrics.Num() == 1, TEXT("pawn replacement reset first"))) return false;
		const FGuid OriginalSID = PCs[0]->FirstActionMetricsSession;
		const double OriginalStart = PCs[0]->FirstActionMetricsStart;
		PCs[0]->SeamlessTravelTo(PCs[2]); PCs[2]->Possess(Pawns[2]);
		Step(Pawns[2], Input);
		if (!Check(PCs[2]->FirstActionMetricsSession == OriginalSID && PCs[2]->FirstActionMetricsStart == OriginalStart
			&& Seen(PCs[2]) && PCs[2]->RecordedFirstActionMetrics.Num() == 1, TEXT("handoff reset first"))) return false;
		// Real walking on a registered static floor; no navmesh or world timer tick.
		AMMOCharacter* Walker = Pawns[1];
		AActor* FloorActor = World->SpawnActor<AActor>(FVector(1005000, 1000000, 4000), FRotator::ZeroRotator, Params);
		if (!FloorActor) { OutError = TEXT("floor fixture unavailable"); return false; }
		Fixtures.Add(FloorActor);
		UBoxComponent* Floor = NewObject<UBoxComponent>(FloorActor);
		FloorActor->SetRootComponent(Floor); FloorActor->AddInstanceComponent(Floor);
		Floor->SetMobility(EComponentMobility::Static); Floor->SetBoxExtent(FVector(1500, 1500, 50));
		Floor->SetCollisionProfileName(TEXT("BlockAll"));
		Floor->SetWorldLocation(FVector(1005000, 1000000, 4000));
		Floor->RegisterComponent();
		Walker->SetActorLocation(FVector(1005000, 1000000, 4052 + Walker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
		PCs[1]->Possess(Walker);
		Step(Walker, FVector::ZeroVector); Step(Walker, FVector::ZeroVector);
		if (!Check(Walker->GetCharacterMovement()->IsMovingOnGround() && !Seen(PCs[1]), TEXT("static-floor/idle fixture failed"))) return false;
		Step(Walker, Input);
		if (!Check(Seen(PCs[1]) && Walker->GetCharacterMovement()->MovementMode == MOVE_Walking,
			TEXT("real walking update hook absent"))) return false;
		PCs[3]->Possess(Walker); Step(Walker, Input);
		if (!Check(Seen(PCs[3]) && PCs[3]->FirstActionMetricsSession != PCs[1]->FirstActionMetricsSession,
			TEXT("new controller did not get independent first"))) return false;
		PCs[4]->Possess(Pawns[4]);
		const FGuid PendingSID = PCs[4]->FirstActionMetricsSession;
		const double PendingStart = PCs[4]->FirstActionMetricsStart;
		PCs[4]->SeamlessTravelTo(PCs[5]); PCs[5]->Possess(Pawns[4]);
		Step(Pawns[4], Input); Step(Pawns[4], Input);
		if (!Check(Seen(PCs[5]) && PCs[5]->FirstActionMetricsSession == PendingSID
			&& PCs[5]->FirstActionMetricsStart == PendingStart && !Seen(PCs[4]), TEXT("handoff before first lost lifetime"))) return false;
		Pawns[5]->GetCharacterMovement()->bRunPhysicsWithNoController = true;
		const FVector NPCStart = Pawns[5]->GetActorLocation();
		Step(Pawns[5], Input); Step(Pawns[5], Input);
		if (!Check(!Pawns[5]->IsPlayerControlled() && !Seen(PCs[4])
			&& (Pawns[5]->GetActorLocation() - NPCStart).SizeSquared2D() >= 1.0,
			TEXT("NPC movement emitted or physical fixture did not move"))) return false;
		if (!Check(Stats->Level == OriginalLevel && Stats->Experience == OriginalXP
			&& Stats->Health == OriginalHealth && Stats->Mana == OriginalMana,
			TEXT("movement observation changed gameplay vitals/progression"))) return false;
		// EndPlay removes the dynamic binding; direct delegate observation afterwards
		// cannot feed the owning controller (not a map-loading/ServerTravel test).
		PCs[0]->RecordedFirstActionMetrics.Reset();
		PCs[0]->Possess(P);
		if (!Check(P->OnCharacterMovementUpdated.IsBound(), TEXT("native movement binding unavailable"))) return false;
		P->RouteEndPlay(EEndPlayReason::LevelTransition);
		P->OnCharacterMovementUpdated.Broadcast(0.1f, P->GetActorLocation() - FVector(2, 0, 0), FVector::ZeroVector);
		if (!Check(!P->OnCharacterMovementUpdated.IsBound() && !Seen(PCs[0]), TEXT("ended pawn observation emitted"))) return false;
		return true;
	}();
	for (AActor* Fixture : Fixtures) { if (IsValid(Fixture)) Fixture->Destroy(); }
	return bPassed; // Same frame: original slots and fixture persistence timers are untouched.
}

namespace
{
	struct FMMOFirstMoveTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestFirstMove"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = AMMOPlayerController::RunFirstMoveSelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_FIRST_MOVE_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOFirstMoveTestExec GMMOFirstMoveTestExec;
}
#endif

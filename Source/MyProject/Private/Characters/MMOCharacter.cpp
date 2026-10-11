// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/MMOCharacter.h"
#include "Core/MMONotify.h"
#include "Characters/MMOMob.h"
#include "Effects/MMODotDamageType.h"
#include "Components/MMOStatsComponent.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOSkillComponent.h"
#include "Components/MMOQuestComponent.h"
#include "Components/MMOPartyComponent.h"
#include "Components/MMOTradeComponent.h"
#include "Components/MMOClanComponent.h"
#include "Components/MMOFriendsComponent.h"
#include "Components/MMOAchievementComponent.h"
#include "Components/MMODailyComponent.h"
#include "Components/MMOMailComponent.h"
#include "Core/MMOTargeting.h"
#include "Core/MMOMetrics.h"
#include "Player/MMOPlayerController.h"
#include "Core/MMOGameMode.h"
#include "Core/MMOGameState.h"
#include "Core/MMOAdmin.h"
#include "Items/MMOConsumableTypes.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/DamageEvents.h"
#include "Components/InputComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "UI/MMOHUD.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AI/MMODialogueSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Save/MMOSaveGame.h"
#include "Save/MMOCharacterStoreSubsystem.h"
#include "Save/MMOCharacterSaveJson.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogMMOIdentityAudit, Log, All);

#if !UE_BUILD_SHIPPING
#include <limits>
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"

bool AMMOCharacter::RunSaveLifecycleSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || World->GetNetMode() == NM_Client
		|| !FParse::Param(FCommandLine::Get(), TEXT("unattended"))
		|| !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
	{
		OutError = TEXT("disposable authority headless world required"); return false;
	}
	IMMOCharacterStore* Store = UMMOCharacterStoreSubsystem::Get(World);
	if (!Store) { OutError = TEXT("store unavailable"); return false; }
	// Only new GUID slots are written/deleted; existing Default is read for comparison.
	const FString Prefix = TEXT("ZLife") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(20);
	TArray<AMMOCharacter*> Pawns;
	TArray<AMMOPlayerController*> Controllers;
	TArray<FString> Slots;
	for (int32 I = 0; I < 7; ++I)
	{
		const FString Name = Prefix + FString::FromInt(I);
		const FString Slot = TEXT("MMOSave_") + Name;
		if (Store->Exists(Slot)) { OutError = TEXT("temporary slot collision"); return false; }
		Slots.Add(Slot);
	}
	const auto Check = [&](bool bOk, const TCHAR* Why)
	{
		++OutChecks;
		if (!bOk && OutError.IsEmpty()) OutError = Why;
		return bOk;
	};
	const bool bDefaultBefore = Store->Exists(TEXT("MMOSave_Default"));
	FMMOCharacterSaveData DefaultBefore;
	if (bDefaultBefore && !Store->Load(TEXT("MMOSave_Default"), DefaultBefore))
	{
		OutError = TEXT("cannot compare existing Default"); return false;
	}
	for (int32 I = 0; I < 7; ++I)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AMMOCharacter* Pawn = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(),
			FVector(0., 0., 5000. + I * 200.), FRotator::ZeroRotator, Params);
		AMMOPlayerController* PC = World->SpawnActor<AMMOPlayerController>();
		Pawns.Add(Pawn); Controllers.Add(PC);
		if (!Pawn || !PC) { OutError = TEXT("fixture spawn failed"); break; }
		Pawn->SetActorTickEnabled(false); PC->SetActorTickEnabled(false);
		PC->Possess(Pawn);
		if (!Pawn->GetPlayerState()) { OutError = TEXT("fixture player state missing"); break; }
		Pawn->GetPlayerState()->SetPlayerName(Prefix + FString::FromInt(I));
	}
	const auto Run = [&]() -> bool
	{
		if (!OutError.IsEmpty()) return false;
		FMMOCharacterSaveData Back;
		AMMOCharacter* P = Pawns[0];
		if (!Check(P->IsPlayerControlled() && P->GetSaveSlotName() == Slots[0], TEXT("identity fixture invalid"))) return false;
		P->LoadCharacter(); // No record: false return, but initial load complete.
		if (!Check(P->bInitialLoadComplete && !Store->Exists(Slots[0]), TEXT("new character initialization invalid"))) return false;
		P->Inventory->AddGold(123);
		if (!Check(P->Destroy(), TEXT("possessed Destroy rejected"))) return false;
		if (!Check(Store->Load(Slots[0], Back) && Back.bValid && Back.Gold == 123,
			TEXT("possessed Destroy lost latest character save"))) return false;
		if (!Check(Controllers[0]->GetPawn() == nullptr, TEXT("Destroy retained controller pawn"))) return false;

		P = Pawns[1]; P->LoadCharacter(); P->Inventory->AddGold(11);
		// Arm the real callback explicitly; this checks cancellation, not nav readiness.
		World->GetTimerManager().SetTimer(P->SavedLocationRetryTimerHandle, P,
			&AMMOCharacter::RetrySavedLocationRestore, 30.f, false);
		if (!Check(P->SaveCharacterForTravel()
			&& World->GetTimerManager().IsTimerActive(P->AutoLoadTimerHandle)
			&& World->GetTimerManager().IsTimerActive(P->AutoSaveTimerHandle)
			&& World->GetTimerManager().IsTimerActive(P->SavedLocationRetryTimerHandle), TEXT("initial save/timer fixture failed"))) return false;
		P->Inventory->AddGold(29);
		Controllers[1]->UnPossess();
		if (!Check(!P->GetPlayerState() && P->GetSaveSlotName() == TEXT("MMOSave_Default"), TEXT("identity did not detach"))) return false;
		if (!Check(Store->Load(Slots[1], Back) && Back.Gold == 40, TEXT("UnPossess lost latest gold"))) return false;
		if (!Check(!P->bInitialLoadComplete, TEXT("detached pawn saving remained enabled"))) return false;
		if (!Check(!World->GetTimerManager().IsTimerActive(P->AutoLoadTimerHandle)
			&& !World->GetTimerManager().IsTimerActive(P->AutoSaveTimerHandle)
			&& !World->GetTimerManager().IsTimerActive(P->SavedLocationRetryTimerHandle), TEXT("detached persistence timer remained active"))) return false;
		if (!Check(!P->SaveCharacterForTravel(true), TEXT("detached pawn accepted Default save"))) return false;
		if (!Check(P->Destroy() && Store->Load(Slots[1], Back) && Back.Gold == 40, TEXT("post-UnPossess Destroy rewrote save"))) return false;

		P = Pawns[2]; P->Inventory->AddGold(222);
		if (!Check(!P->bInitialLoadComplete && P->Destroy() && !Store->Exists(Slots[2]), TEXT("uninitialized pawn wrote save"))) return false;

		P = Pawns[3]; P->LoadCharacter(); P->Inventory->AddGold(31);
		if (!Check(P->SaveCharacterForTravel(), TEXT("travel fixture save failed"))) return false;
		P->SetTravelSaveCommitted(true); P->Inventory->AddGold(99);
		if (!Check(P->Destroy() && Store->Load(Slots[3], Back) && Back.Gold == 31, TEXT("committed travel snapshot overwritten"))) return false;

		P = Pawns[4]; P->LoadCharacter(); P->Inventory->AddGold(45);
		const auto Tracker = Store->GetWriteTracker();
		const uint64 Before = Tracker->BeginWrite(Slots[4]);
		P->RouteEndPlay(EEndPlayReason::LevelTransition); // Native route, NOT ServerTravel.
		const uint64 After = Tracker->BeginWrite(Slots[4]);
		if (!Check(Store->Load(Slots[4], Back) && Back.Gold == 45, TEXT("transition EndPlay lost snapshot"))) return false;
		if (!Check(After == Before + 2, TEXT("transition EndPlay wrote more than once"))) return false;
		if (!Check(!P->GetPlayerState() && !P->bInitialLoadComplete, TEXT("transition persistence remained enabled"))) return false;

		P = Pawns[5]; P->LoadCharacter(); P->Inventory->AddGold(56);
		if (!Check(Controllers[5]->Destroy(), TEXT("controller Destroy rejected"))) return false;
		if (!Check(Store->Load(Slots[5], Back) && Back.Gold == 56, TEXT("controller teardown lost character save"))) return false;

		P = Pawns[6]; P->LoadCharacter(); P->Inventory->AddGold(61);
		if (!Check(P->SaveCharacterForTravel(), TEXT("rename source save failed"))) return false;
		// Existing target is another disposable fixture, never a user slot.
		P->Inventory->AddGold(99);
		P->GetPlayerState()->SetPlayerName(Prefix + TEXT("0"));
		Controllers[6]->UnPossess();
		if (!Check(Store->Load(Slots[0], Back) && Back.Gold == 123, TEXT("rename overwrote existing target"))) return false;
		if (!Check(Store->Load(Slots[6], Back) && Back.Gold == 61, TEXT("rejected rename silently saved old identity"))) return false;
		if (!Check(!P->bInitialLoadComplete && !P->SaveCharacterForTravel(true), TEXT("rejected rename left saving enabled"))) return false;

		if (!Check(Store->Exists(TEXT("MMOSave_Default")) == bDefaultBefore, TEXT("Default slot existence changed"))) return false;
		TArray<FString> Diff;
		if (bDefaultBefore)
		{
			if (!Store->Load(TEXT("MMOSave_Default"), Back)) { OutError = TEXT("Default no longer readable"); return false; }
			FMMOCharacterSaveJson::FindDifferences(DefaultBefore, Back, Diff);
		}
		return Check(Diff.IsEmpty(), TEXT("Default slot data changed"));
	};
	const bool bPassed = Run();
	// Disable persistence before fixture cleanup even after an early failed assertion.
	for (AMMOCharacter* Pawn : Pawns)
	{
		if (IsValid(Pawn)) { Pawn->bInitialLoadComplete = false; Pawn->SetTravelSaveCommitted(true); }
	}
	for (AMMOPlayerController* PC : Controllers)
	{
		if (IsValid(PC)) { PC->UnPossess(); PC->Destroy(); }
	}
	for (AMMOCharacter* Pawn : Pawns) { if (IsValid(Pawn)) Pawn->Destroy(); }
	for (const FString& Slot : Slots)
	{
		if (Store->Exists(Slot) && !UGameplayStatics::DeleteGameInSlot(Slot, 0))
		{
			OutError = TEXT("temporary slot cleanup failed");
		}
	}
	return bPassed && OutError.IsEmpty();
}

bool AMMOCharacter::RunMeleeCooldownSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	// Synthetic timestamps can reach cosmetic HUD entries. Only an explicitly disposable
	// automation process may run this fixture; restoring the clock alone cannot undo a HUD.
	if (!FParse::Param(FCommandLine::Get(), TEXT("MMOAllowSyntheticWorldTime")))
	{ OutError = TEXT("requires -MMOAllowSyntheticWorldTime in disposable automation process"); return false; }
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{ OutError = TEXT("server game world required"); return false; }
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Location(1200000, 1000000, 5000);
	AMMOCharacter* C = World->SpawnActor<AMMOCharacter>(StaticClass(), Location, FRotator::ZeroRotator, Params);
	const FTransform TM(FRotator::ZeroRotator, Location + FVector(100, 0, 0));
	AMMOMob* M = World->SpawnActorDeferred<AMMOMob>(AMMOMob::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (M)
	{
		M->AutoPossessAI = EAutoPossessAI::Disabled; M->ArchetypeId = TEXT("Goblin");
		UGameplayStatics::FinishSpawningActor(M, TM);
		M->bHasHub = false; M->XPReward = 0;
	}
	const double OriginalTime = World->TimeSeconds;
	const bool bPassed = [&]()
	{
		if (!C || !M) { OutError = TEXT("fixture spawn failed"); return false; }
		C->SetActorEnableCollision(false); C->SetActorTickEnabled(false); C->EndSpawnProtection();
		M->SetActorEnableCollision(false); M->SetActorTickEnabled(false);
		C->Stats->AttackPower = 10.f;
		M->Stats->MaxHealth = 10000.f; M->Stats->Defense = 0.f;
		auto Check = [&](bool Condition, const TCHAR* Label, double Now)
		{
			if (!Condition) { OutError = FString::Printf(TEXT("%s uptime=%.0f"), Label, Now); return false; }
			++OutChecks; return true;
		};
		// Same frame only: no world tick, timers, save/load or player controllers.
		for (double Now : {100., 600., 1000., 3600., 86400., 604800., 10000000.})
		{
			World->TimeSeconds = Now; M->Stats->Health = 10000.f;
			C->LastMeleeTime = Now - 2.; C->ServerAttackTarget_Implementation(M);
			if (!Check(M->Stats->Health == 9990.f && C->LastMeleeTime == Now, TEXT("expired RPC rejected or timestamp changed"), Now)) return false;
			C->ServerAttackTarget_Implementation(M);
			if (!Check(M->Stats->Health == 9990.f && C->LastMeleeTime == Now, TEXT("duplicate RPC damaged"), Now)) return false;
			C->LastMeleeTime = Now - .5; const double EarlyTime = C->LastMeleeTime;
			C->ServerAttackTarget_Implementation(M);
			if (!Check(M->Stats->Health == 9990.f && C->LastMeleeTime == EarlyTime, TEXT("early RPC damaged or advanced clock"), Now)) return false;
			C->LastMeleeTime = Now - .55; C->ServerAttackTarget_Implementation(M);
			if (!Check(M->Stats->Health == 9980.f && C->LastMeleeTime == Now, TEXT("RPC jitter tolerance rejected"), Now)) return false;
			C->LastMeleeTime = Now - .55; const double LocalTime = C->LastMeleeTime;
			C->AttackTarget(M);
			if (!Check(M->Stats->Health == 9980.f && C->LastMeleeTime == LocalTime, TEXT("direct attack gained RPC tolerance"), Now)) return false;
			C->LastMeleeTime = Now - .61; C->AttackTarget(M);
			if (!Check(M->Stats->Health == 9970.f && C->LastMeleeTime == Now, TEXT("expired direct attack rejected"), Now)) return false;
			for (bool bRPC : {false, true})
			{
				const double Threshold = bRPC ? C->MeleeCooldown * 0.9f : C->MeleeCooldown;
				for (double Offset : {-0.000001, 0., 0.000001})
				{
					M->Stats->Health = 10000.f;
					C->LastMeleeTime = Now - (Threshold + Offset);
					const double Before = C->LastMeleeTime;
					if (bRPC) C->ServerAttackTarget_Implementation(M); else C->AttackTarget(M);
					const bool bExpected = Offset >= 0.;
					if (!Check(M->Stats->Health == (bExpected ? 9990.f : 10000.f)
						&& C->LastMeleeTime == (bExpected ? Now : Before),
						bRPC ? TEXT("RPC threshold boundary changed") : TEXT("direct threshold boundary changed"), Now)) return false;
				}
			}
		}
		const double Now = 100.; World->TimeSeconds = Now;
		C->Stats->bIsDead = true; C->LastMeleeTime = Now - 2.;
		C->ServerAttackTarget_Implementation(M);
		if (!Check(C->LastMeleeTime == Now - 2., TEXT("dead RPC advanced clock"), Now)) return false;
		C->Stats->bIsDead = false;
		TActorIterator<APlayerStart> Starts(World);
		if (!Starts) { OutError = TEXT("hub fixture needs PlayerStart"); return false; }
		C->SetActorLocation(Starts->GetActorLocation());
		C->ServerAttackTarget_Implementation(M);
		if (!Check(C->LastMeleeTime == Now - 2., TEXT("hub RPC advanced clock"), Now)) return false;
		C->SetActorLocation(Location);
		C->ServerAttackTarget_Implementation(C);
		if (!Check(C->LastMeleeTime == Now, TEXT("self target did not consume attempt cooldown"), Now)) return false;
		C->LastMeleeTime = Now - 2.; M->SetActorLocation(Location + FVector(1000, 0, 0));
		C->ServerAttackTarget_Implementation(M);
		if (!Check(C->LastMeleeTime == Now, TEXT("far target did not consume attempt cooldown"), Now)) return false;
		C->LastMeleeTime = Now - 2.; C->ServerAttackTarget_Implementation(nullptr);
		if (!Check(C->LastMeleeTime == Now, TEXT("missing target did not consume attempt cooldown"), Now)) return false;
		return true;
	}();
	World->TimeSeconds = OriginalTime; // Restore even on a failed assertion before destroying fixtures.
	if (IsValid(C)) C->Destroy();
	if (IsValid(M)) M->Destroy();
	return bPassed;
}

bool AMMOCharacter::RunFirstDeathSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{ OutError = TEXT("server game world required"); return false; }
	TArray<AActor*> Fixtures; TArray<AMMOPlayerController*> Controllers;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Location(1900000, 1900000, 5000);
	const bool bPassed = [&]()
	{
		auto Check = [&](bool Condition, const TCHAR* Label)
		{ if (!Condition) { OutError = Label; return false; } ++OutChecks; return true; };
		auto MakePawn = [&](int32 Index) -> AMMOCharacter*
		{
			AMMOCharacter* C = World->SpawnActor<AMMOCharacter>(StaticClass(), Location + FVector(Index * 5000, 0, 0), FRotator::ZeroRotator, Params);
			if (C) { Fixtures.Add(C); C->SetActorEnableCollision(false); C->SetActorTickEnabled(false); C->EndSpawnProtection(); }
			return C;
		};
		AMMOPlayerController* PCs[3] = {}; AMMOCharacter* Pawns[3] = {};
		for (int32 i = 0; i < 3; ++i)
		{
			PCs[i] = World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(), Location, FRotator::ZeroRotator, Params);
			Pawns[i] = MakePawn(i);
			if (PCs[i]) { Fixtures.Add(PCs[i]); Controllers.Add(PCs[i]); }
			if (!PCs[i] || !Pawns[i]) { OutError = TEXT("fixture spawn failed"); return false; }
			PCs[i]->Possess(Pawns[i]);
			Pawns[i]->EndSpawnProtection();
			Pawns[i]->Stats->Health = 100.f; Pawns[i]->Stats->MaxHealth = 100.f; Pawns[i]->Stats->Defense = 0.f;
			Pawns[i]->Stats->Experience = i == 0 ? 20 : (i == 1 ? 0 : 2);
			Pawns[i]->Stats->ExperienceToNextLevel = 100; Pawns[i]->Stats->DeathXPPenaltyPct = .05f;
		}
		const FTransform TM(FRotator::ZeroRotator, Location + FVector(100, 0, 0));
		AMMOMob* Mob = World->SpawnActorDeferred<AMMOMob>(AMMOMob::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Mob) { OutError = TEXT("mob spawn failed"); return false; }
		Fixtures.Add(Mob); Mob->AutoPossessAI = EAutoPossessAI::Disabled; Mob->ArchetypeId = TEXT("Goblin");
		UGameplayStatics::FinishSpawningActor(Mob, TM); Mob->SetActorTickEnabled(false); Mob->SetActorEnableCollision(false);
		// Emission gates must not consume the later real death. These edits are local fixture state only.
		if (!Check(!PCs[0]->RecordFirstDeathMetric(TEXT("Goblin"), 5), TEXT("living pawn emitted"))) return false;
		Pawns[0]->Stats->bIsDead = true;
		if (!Check(!PCs[0]->RecordFirstDeathMetric(TEXT("Goblin"), 5), TEXT("positive health emitted"))) return false;
		Pawns[0]->Stats->Health = 0.f;
		if (!Check(!PCs[0]->RecordFirstDeathMetric(TEXT("Goblin"), -1), TEXT("negative loss emitted"))) return false;
		Pawns[0]->Stats->Health = std::numeric_limits<float>::quiet_NaN();
		if (!Check(!PCs[0]->RecordFirstDeathMetric(NAME_None, 0), TEXT("nonfinite health emitted"))) return false;
		Pawns[0]->Stats->Health = 100.f; Pawns[0]->Stats->bIsDead = false;
		FDamageEvent Event;
		Pawns[0]->StartSpawnProtection();
		if (!Check(Pawns[0]->TakeDamage(1000.f, Event, nullptr, Mob) == 0.f && !Pawns[0]->Stats->bIsDead, TEXT("protected death changed"))) return false;
		Pawns[0]->EndSpawnProtection();
		if (!Check(Pawns[0]->TakeDamage(10.f, Event, nullptr, Mob) == 10.f && Pawns[0]->Stats->Health == 90.f
			&& Pawns[0]->Stats->Experience == 20 && !Pawns[0]->Stats->bIsDead, TEXT("nonlethal damage changed"))) return false;
		UE_LOG(LogTemp, Display, TEXT("[MMO_FIRST_DEATH_GATES] rejected=6"));
		if (!Check(Pawns[0]->TakeDamage(1000.f, Event, nullptr, Mob) == 90.f && Pawns[0]->Stats->bIsDead
			&& Pawns[0]->Stats->Health == 0.f && Pawns[0]->Stats->Experience == 15 && Pawns[0]->Stats->Level == 1
			&& Pawns[0]->GetCharacterMovement()->MovementMode == MOVE_None && Pawns[0]->GetRespawnTimeRemaining() > 0.f
			&& !PCs[0]->RecordFirstDeathMetric(TEXT("Goblin"), 5), TEXT("first mob death hook/penalty/respawn changed"))) return false;
		if (!Check(Pawns[0]->TakeDamage(1000.f, Event, nullptr, Mob) == 0.f && Pawns[0]->Stats->Experience == 15, TEXT("dead damage repeated penalty"))) return false;
		Pawns[0]->Stats->Revive();
		if (!Check(!Pawns[0]->Stats->bIsDead && Pawns[0]->Stats->Health > 0.f && Pawns[0]->GetRespawnTimeRemaining() == 0.f
			&& !PCs[0]->RecordFirstDeathMetric(NAME_None, 0), TEXT("revive reset marker or emitted while living"))) return false;
		Pawns[0]->EndSpawnProtection();
		Pawns[0]->TakeDamage(1000.f, Event, nullptr, nullptr);
		if (!Check(Pawns[0]->Stats->bIsDead && Pawns[0]->Stats->Experience == 10 && !PCs[0]->RecordFirstDeathMetric(NAME_None, 5), TEXT("repeat death changed"))) return false;
		AMMOCharacter* Replacement = MakePawn(4);
		if (!Replacement) { OutError = TEXT("replacement spawn failed"); return false; }
		PCs[0]->Possess(Replacement); Replacement->EndSpawnProtection(); Replacement->Stats->Defense = 0.f;
		Replacement->Stats->Experience = 0;
		Replacement->TakeDamage(100000.f, Event, nullptr, Mob);
		if (!Check(Replacement->Stats->bIsDead && Replacement->Stats->Experience == 0 && !PCs[0]->RecordFirstDeathMetric(TEXT("Goblin"), 0), TEXT("pawn replacement reset death"))) return false;
		Pawns[1]->TakeDamage(1000.f, Event, nullptr, nullptr);
		if (!Check(Pawns[1]->Stats->bIsDead && Pawns[1]->Stats->Experience == 0 && !PCs[1]->RecordFirstDeathMetric(NAME_None, 0), TEXT("zero loss death missing"))) return false;
		// Non-mob cause stays anonymous/unknown; this is not a PvP attribution fixture.
		AActor* OtherCause = World->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator, Params);
		if (!OtherCause) { OutError = TEXT("unknown source spawn failed"); return false; } Fixtures.Add(OtherCause);
		Pawns[2]->Stats->Level = 3;
		Pawns[2]->TakeDamage(1000.f, Event, nullptr, OtherCause);
		if (!Check(Pawns[2]->Stats->bIsDead && Pawns[2]->Stats->Experience == 0 && Pawns[2]->Stats->Level == 3
			&& !PCs[2]->RecordFirstDeathMetric(NAME_None, 2), TEXT("clamped XP/non-mob death changed"))) return false;
		AMMOCharacter* NPC = MakePawn(5);
		if (!NPC) { OutError = TEXT("NPC spawn failed"); return false; }
		NPC->Stats->Experience = 20; NPC->Stats->Defense = 0.f;
		NPC->TakeDamage(100000.f, Event, nullptr, Mob);
		return Check(NPC->Stats->bIsDead && NPC->Stats->Experience == 20 && !NPC->IsPlayerControlled(), TEXT("controllerless death changed"));
	}();
	for (AMMOPlayerController* PC : Controllers) if (IsValid(PC)) PC->UnPossess();
	for (AActor* A : Fixtures) if (IsValid(A)) A->Destroy();
	return bPassed; // Same frame only: no timers run, save/load, remote delivery or PIE input.
}

bool AMMOCharacter::RunFirstBasicAttackSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	AMMOGameMode* GM = World ? World->GetAuthGameMode<AMMOGameMode>() : nullptr;
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client || !GM)
	{ OutError = TEXT("MMO server game world required"); return false; }
	const bool bOriginalPvP = GM->bAllowPvP;
	TArray<AActor*> Fixtures; TArray<AMMOPlayerController*> Controllers;
	const bool bPassed = [&]()
	{
		auto Check = [&](bool Condition, const TCHAR* Label)
		{ if (!Condition) { OutError = Label; return false; } ++OutChecks; return true; };
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto MakeCharacter = [&](const FVector& Location, bool bPlayer) -> AMMOCharacter*
		{
			AMMOCharacter* C = World->SpawnActor<AMMOCharacter>(StaticClass(), Location, FRotator::ZeroRotator, Params);
			if (!C) return nullptr;
			Fixtures.Add(C); C->SetActorEnableCollision(false); C->SetActorTickEnabled(false);
			C->Stats->MaxHealth = 10000.f; C->Stats->Health = 10000.f; C->Stats->Defense = 0.f;
			C->Stats->AttackPower = 10.f; C->EndSpawnProtection();
			if (bPlayer)
			{
				AMMOPlayerController* PC = World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(), Location, FRotator::ZeroRotator, Params);
				if (!PC) return nullptr;
				Fixtures.Add(PC); Controllers.Add(PC); PC->Possess(C);
			}
			C->Stats->AttackPower = 10.f; // Possession can refresh equipment bonuses.
			return C;
		};
		auto MakeMob = [&](const FVector& Location) -> AMMOMob*
		{
			const FTransform TM(FRotator::ZeroRotator, Location);
			AMMOMob* M = World->SpawnActorDeferred<AMMOMob>(AMMOMob::StaticClass(), TM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!M) return nullptr;
			Fixtures.Add(M); M->AutoPossessAI = EAutoPossessAI::Disabled; M->ArchetypeId = TEXT("Goblin");
			M->SetActorEnableCollision(false); UGameplayStatics::FinishSpawningActor(M, TM); M->SetActorTickEnabled(false);
			M->bHasHub = false; M->ItemDropChance = 0.f; M->EquipDropChance = 0.f;
			M->MinGold = 0; M->MaxGold = 0; M->XPReward = 0;
			M->Stats->MaxHealth = 10000.f; M->Stats->Health = 10000.f; M->Stats->Defense = 0.f;
			return M;
		};
		const EMMOClass Classes[] = {EMMOClass::Warrior, EMMOClass::Archer, EMMOClass::Mage, EMMOClass::Priest};
		for (int32 i = 0; i < UE_ARRAY_COUNT(Classes); ++i)
		{
			const FVector Location(1000000 + i * 5000, 1000000, 5000);
			AMMOCharacter* C = MakeCharacter(Location, true);
			AMMOMob* M = MakeMob(Location + FVector(100, 0, 0));
			if (!C || !M) { OutError = TEXT("fixture spawn failed"); return false; }
			C->SkillComponent->SetupForClass(Classes[i]);
			C->Stats->AttackPower = 10.f; // Class setup recalculates derived stats; fixed damage fixture only.
			if (i == 0)
			{
				AMMOCharacter* Other = MakeCharacter(Location + FVector(100, 0, 0), false);
				if (!Other) { OutError = TEXT("player target spawn failed"); return false; }
				auto Rejected = [&](AMMOCharacter* Target, const TCHAR* Label)
				{
					const float MobHealth = M->Stats->Health, OtherHealth = Other->Stats->Health;
					C->AttackTarget(Target);
					return Check(M->Stats->Health == MobHealth && Other->Stats->Health == OtherHealth, Label);
				};
				C->Stats->bIsDead = true;
				if (!Rejected(M, TEXT("dead attacker damaged"))) return false;
				C->Stats->bIsDead = false; C->LastMeleeTime = World->GetTimeSeconds();
				if (!Rejected(M, TEXT("cooldown damaged"))) return false;
				C->LastMeleeTime = -100.f; M->SetActorLocation(Location + FVector(1000, 0, 0));
				if (!Rejected(nullptr, TEXT("missing target damaged"))) return false;
				C->LastMeleeTime = -100.f;
				if (!Rejected(C, TEXT("self target damaged"))) return false;
				C->LastMeleeTime = -100.f;
				if (!Rejected(M, TEXT("far target damaged"))) return false;
				GM->SetPvPAllowed(false); C->LastMeleeTime = -100.f;
				if (!Rejected(Other, TEXT("PvP off damaged"))) return false;
				GM->SetPvPAllowed(true); C->PartyComponent->Members.Add(Other); C->LastMeleeTime = -100.f;
				if (!Rejected(Other, TEXT("party damaged"))) return false;
				C->PartyComponent->Members.Reset(); C->ClanComponent->ClanName = TEXT("Fixture"); Other->ClanComponent->ClanName = TEXT("Fixture"); C->LastMeleeTime = -100.f;
				if (!Rejected(Other, TEXT("clan damaged"))) return false;
				C->ClanComponent->ClanName.Reset(); Other->ClanComponent->ClanName.Reset(); Other->StartSpawnProtection(); C->LastMeleeTime = -100.f;
				if (!Rejected(Other, TEXT("protected target damaged"))) return false;
				Other->EndSpawnProtection();
				TActorIterator<APlayerStart> Starts(World);
				if (!Starts) { OutError = TEXT("hub fixture needs existing PlayerStart"); return false; }
				C->SetActorLocation(Starts->GetActorLocation()); C->LastMeleeTime = -100.f;
				if (!Rejected(M, TEXT("hub attack damaged"))) return false;
				C->SetActorLocation(Location); M->SetActorLocation(Location + FVector(100, 0, 0));
				UE_LOG(LogTemp, Display, TEXT("[MMO_FIRST_BASIC_ATTACK_GATES] rejected=10"));
			}
			C->LastMeleeTime = -100.f;
			if (i == 1) C->ServerAttackTarget_Implementation(M); // First accepted native RPC entry, not network transport.
			else C->AttackTarget(M);
			if (!Check(M->Stats->Health == 9990.f && C->Stats->Level == 1, TEXT("class basic attack changed"))) return false;
			if (i == 0)
			{
				C->ServerAttackTarget_Implementation(M); // Native call; not network delivery proof.
				if (!Check(M->Stats->Health == 9990.f, TEXT("server cooldown damaged twice"))) return false;
				C->LastMeleeTime = -100.f; C->AttackTarget(M);
				if (!Check(M->Stats->Health == 9980.f && !Cast<AMMOPlayerController>(C->GetController())->RecordFirstBasicAttackMetric(TEXT("mob"), TEXT("Goblin"), 1), TEXT("repeat attack/session changed"))) return false;
			}
		}
		GM->SetPvPAllowed(true);
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector Location(1040000 + i * 5000, 1000000, 5000);
			AMMOCharacter* C = MakeCharacter(Location, true);
			AMMOCharacter* Target = i == 0 ? MakeCharacter(Location + FVector(100, 0, 0), false) : MakeMob(Location + FVector(100, 0, 0));
			if (!C || !Target) { OutError = TEXT("alternate fixture spawn failed"); return false; }
			if (i == 0) Target->Stats->Level = 7;
			if (i == 2) { Target->Stats->Health = 1.f; Cast<AMMOMob>(Target)->XPReward = 200; }
			if (i == 3) C->Stats->AttackPower = 0.f; // Accepted attack can deal zero damage.
			C->AttackTarget(i == 1 ? nullptr : Target);
			const bool bCorrect = i == 2 ? Target->Stats->bIsDead && C->Stats->Level == 2 && C->Stats->Experience == 100
				: Target->Stats->Health == (i == 3 ? 10000.f : 9990.f);
			if (!Check(bCorrect, i == 0 ? TEXT("accepted PvP changed") : (i == 1 ? TEXT("nearest fallback changed")
				: (i == 2 ? TEXT("lethal XP path changed") : TEXT("zero damage attack changed"))))) return false;
		}
		const FVector Location(1100000, 1000000, 5000);
		AMMOCharacter* NPC = MakeCharacter(Location, false); AMMOMob* Mob = MakeMob(Location + FVector(100, 0, 0));
		if (!NPC || !Mob) { OutError = TEXT("controllerless fixture spawn failed"); return false; }
		NPC->AttackTarget(Mob);
		return Check(Mob->Stats->Health == 9990.f, TEXT("controllerless attack changed"));
	}();
	GM->SetPvPAllowed(bOriginalPvP);
	for (AMMOPlayerController* PC : Controllers) if (IsValid(PC)) PC->UnPossess();
	for (AActor* A : Fixtures) if (IsValid(A)) A->Destroy();
	return bPassed; // Synchronous frame; no save/load timers or real remote RPC/PIE assertion.
}

bool AMMOCharacter::RunConsumableBenefitSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0;
	OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{
		OutError = TEXT("server game world required");
		return false;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AMMOCharacter* Character = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(1000000, 1000000, 5000), FRotator::ZeroRotator, Params);
	if (!Character) { OutError = TEXT("fixture spawn failed"); return false; }
	const bool bPassed = [&]()
	{
		UMMOStatsComponent* S = Character->GetStats();
		UMMOInventoryComponent* I = Character->GetInventory();
		if (!S || !I) { OutError = TEXT("fixture components missing"); return false; }
		const FName HP(TEXT("GreaterHealthPotion")), MP(TEXT("ManaPotion")), GMP(TEXT("GreaterManaPotion")), Scroll(TEXT("ScrollRage"));
		I->AddItem(HP, 3); I->AddItem(MP, 3); I->AddItem(GMP, 3); I->AddItem(Scroll, 2);
		I->AddItem(TEXT("HealthPotion"), 3);
		auto Check = [&](bool Condition, const TCHAR* Label)
		{
			if (!Condition) { OutError = Label; return false; }
			++OutChecks;
			return true;
		};
		auto ResetCooldown = [&]() { Character->LastConsumableUseTime = World->GetTimeSeconds() - 2.f; };
		S->Health = S->MaxHealth; S->Mana = S->MaxMana;
		ResetCooldown();
		const float BeforeTime = Character->LastConsumableUseTime;
		Character->ServerUseConsumable_Implementation(HP);
		if (!Check(I->GetItemCount(HP) == 3 && Character->LastConsumableUseTime == BeforeTime, TEXT("full health consumed potion or cooldown"))) return false;
		Character->UseManaPotion();
		if (!Check(I->GetItemCount(MP) == 3 && Character->LastConsumableUseTime == BeforeTime, TEXT("full mana consumed potion or cooldown"))) return false;
		Character->ServerUseConsumable_Implementation(GMP);
		if (!Check(I->GetItemCount(GMP) == 3 && Character->LastConsumableUseTime == BeforeTime, TEXT("full mana consumed greater potion"))) return false;
		const float OldMaxMana = S->MaxMana;
		S->MaxMana = 0.f; S->Mana = 0.f;
		Character->UseManaPotion();
		if (!Check(I->GetItemCount(MP) == 3 && Character->LastConsumableUseTime == BeforeTime, TEXT("zero mana pool consumed potion"))) return false;
		S->MaxMana = OldMaxMana; S->Mana = 0.f;
		S->Health = S->MaxHealth - 50.f;
		Character->ServerUseConsumable_Implementation(HP);
		if (!Check(I->GetItemCount(HP) == 2 && S->Health == S->MaxHealth, TEXT("needed health potion failed"))) return false;
		Character->UseManaPotion();
		if (!Check(I->GetItemCount(MP) == 3 && S->Mana == 0.f, TEXT("shared cooldown bypassed"))) return false;
		ResetCooldown();
		Character->UseManaPotion();
		if (!Check(I->GetItemCount(MP) == 2 && S->Mana == FMath::Min(60.f, S->MaxMana), TEXT("needed mana potion failed"))) return false;
		ResetCooldown(); S->bIsDead = true;
		S->Health = 0.f;
		Character->UseConsumable(HP);
		if (!Check(I->GetItemCount(HP) == 2 && S->Health == 0.f, TEXT("dead potion consumed"))) return false;
		S->Revive(); ResetCooldown();
		Character->UseConsumable(Scroll);
		if (!Check(I->GetItemCount(Scroll) == 1 && S->ActiveEffects.ContainsByPredicate([](const FMMOActiveEffect& E) { return E.EffectId == FName(TEXT("Rage")); }), TEXT("full vitals blocked buff scroll"))) return false;
		ResetCooldown();
		const float AfterReset = Character->LastConsumableUseTime;
		Character->UseConsumable(TEXT("UnknownConsumableFixture"));
		if (!Check(Character->LastConsumableUseTime == AfterReset, TEXT("invalid item consumed cooldown"))) return false;
		const int32 BaseHP = I->GetItemCount(TEXT("HealthPotion"));
		Character->UseHealthPotion();
		return Check(I->GetItemCount(TEXT("HealthPotion")) == BaseHP && Character->LastConsumableUseTime == AfterReset, TEXT("full base health potion consumed"));
	}();
	Character->Destroy(); // Controllersiz fixture: kayit yuklenmez/yazilmaz.
	return bPassed;
}

namespace
{
	struct FMMOSaveLifecycleTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestSaveLifecycle"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = AMMOCharacter::RunSaveLifecycleSelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_SAVE_LIFECYCLE_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOSaveLifecycleTestExec GMMOSaveLifecycleTestExec;

	struct FMMOMeleeCooldownTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestMeleeCooldown"))) return false;
			FString Error; int32 Checks = 0;
			const double Before = InWorld ? InWorld->GetTimeSeconds() : 0.;
			const bool bPassed = AMMOCharacter::RunMeleeCooldownSelfTest(InWorld, Error, Checks)
				&& InWorld && InWorld->GetTimeSeconds() == Before;
			Ar.Logf(TEXT("[MMO_MELEE_COOLDOWN_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOMeleeCooldownTestExec GMMOMeleeCooldownTestExec;

	struct FMMOFirstDeathTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestFirstDeath"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = AMMOCharacter::RunFirstDeathSelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_FIRST_DEATH_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOFirstDeathTestExec GMMOFirstDeathTestExec;

	struct FMMOFirstBasicAttackTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestFirstBasicAttack"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = AMMOCharacter::RunFirstBasicAttackSelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_FIRST_BASIC_ATTACK_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOFirstBasicAttackTestExec GMMOFirstBasicAttackTestExec;

	struct FMMOConsumableBenefitTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestConsumableBenefit"))) return false;
			FString Error;
			int32 Checks = 0;
			const bool bPassed = AMMOCharacter::RunConsumableBenefitSelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_CONSUMABLE_BENEFIT_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOConsumableBenefitTestExec GMMOConsumableBenefitTestExec;
}
#endif

namespace
{
	// Hile/debug komutlari (MMOGiveItem, MMOGainXP, MMOSpawnMob vb.) Shipping derlemede
	// calismaz (SECURITY_AUDIT Y-6). Gelistirme ve editor derlemelerinde davranis ayni.
	bool MMODebugCommandsEnabled()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		return true;
#endif
	}
}

namespace
{
	// PvP kurallarinda "oyuncu" = mob olmayan AMMOCharacter. IsPlayerControlled() kontrolcusu
	// o an olmayan (travel/yeniden sahiplenme) oyuncu pawn'ini mob sanip korumayi atlardi.
	bool IsPlayerPawn(const AMMOCharacter* C)
	{
		return C && !Cast<AMMOMob>(C);
	}
}

AMMOCharacter::AMMOCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	Stats = CreateDefaultSubobject<UMMOStatsComponent>(TEXT("Stats"));
	Inventory = CreateDefaultSubobject<UMMOInventoryComponent>(TEXT("Inventory"));
	SkillComponent = CreateDefaultSubobject<UMMOSkillComponent>(TEXT("SkillComponent"));
	QuestComponent = CreateDefaultSubobject<UMMOQuestComponent>(TEXT("QuestComponent"));
	PartyComponent = CreateDefaultSubobject<UMMOPartyComponent>(TEXT("PartyComponent"));
	TradeComponent = CreateDefaultSubobject<UMMOTradeComponent>(TEXT("TradeComponent"));
	ClanComponent = CreateDefaultSubobject<UMMOClanComponent>(TEXT("ClanComponent"));
	FriendsComponent = CreateDefaultSubobject<UMMOFriendsComponent>(TEXT("FriendsComponent"));
	AchievementComponent = CreateDefaultSubobject<UMMOAchievementComponent>(TEXT("AchievementComponent"));
	DailyComponent = CreateDefaultSubobject<UMMODailyComponent>(TEXT("DailyComponent"));
	MailComponent = CreateDefaultSubobject<UMMOMailComponent>(TEXT("MailComponent"));

	// NETWORK_AUDIT Y5: oyuncu aktoru icin makul replikasyon sikligi (motor varsayilani ~100 Hz
	// gereksiz; 60 Hz akici hareket icin yeterli, hareketsizken 20 Hz tabanina duser). Cull
	// mesafesi motor varsayilaninda birakildi (oyuncular/parti uyeleri uzaktan da gorunmeli).
	// DOGRULANMADI: degerler PIE/Network Profiler ile olculmedi. AMMOMob kurucusu bunu ezer.
	SetNetUpdateFrequency(60.f);
	SetMinNetUpdateFrequency(20.f);

	// Saldiri animasyonlari. Sira onemli: 0..2 = yakin dovus varyasyonlari, 3 = buyu okuma.
	// Kilic savurma Mixamo'dan alinip Manny iskeletine retarget edildi (RT_ onekli).
	{
		static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Sword(TEXT("/Game/RT_Great_Sword_Slash_Anim.RT_Great_Sword_Slash_Anim"));
		static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Atk1(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"));
		static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Atk2(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02.MM_Attack_02"));
		static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Charged(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_ChargedAttack.MM_ChargedAttack"));

		UAnimSequenceBase* SwordAnim = nullptr;
		if (Sword.Succeeded()) { SwordAnim = Sword.Object; }

		UAnimSequenceBase* Fallback1 = nullptr;
		if (Atk1.Succeeded()) { Fallback1 = Atk1.Object; }

		UAnimSequenceBase* Fallback2 = nullptr;
		if (Atk2.Succeeded()) { Fallback2 = Atk2.Object; }

		// Kilic animasyonu varsa yakin dovus slotlarini o doldurur; yoksa eski Unarmed setine duser.
		AttackAnims.Add(SwordAnim ? SwordAnim : Fallback1);
		AttackAnims.Add(SwordAnim ? SwordAnim : Fallback2);
		AttackAnims.Add(SwordAnim ? SwordAnim : Fallback1);

		// 3 = buyu okuma / buyuk vurus (henuz ozel bir cast animasyonu yok).
		UAnimSequenceBase* ChargedAnim = nullptr;
		if (Charged.Succeeded()) { ChargedAnim = Charged.Object; }
		AttackAnims.Add(ChargedAnim);
	}
}

void AMMOCharacter::PlayAttackAnimLocal(int32 AnimIndex)
{
	if (!AttackAnims.IsValidIndex(AnimIndex) || !AttackAnims[AnimIndex])
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInst = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!AnimInst)
	{
		return;
	}

	UAnimSequenceBase* Anim = AttackAnims[AnimIndex];
	if (UAnimMontage* AsMontage = Cast<UAnimMontage>(Anim))
	{
		AnimInst->Montage_Play(AsMontage);
	}
	else
	{
		// Duz AnimSequence: ABP_Unarmed'daki "DefaultSlot" uzerinde dinamik montaj olarak calinir
		// (yurume/kosma alt katmani bozulmadan ustune biner).
		AnimInst->PlaySlotAnimationAsDynamicMontage(Anim, TEXT("DefaultSlot"), 0.1f, 0.15f, 1.f, 1);
	}
}

void AMMOCharacter::BeginPlay()
{
	bEndPlayStarted = false;
	Super::BeginPlay();

#if !UE_BUILD_SHIPPING
	if (HasAuthority())
	{
		OnCharacterMovementUpdated.AddDynamic(this, &AMMOCharacter::HandleFirstMovementMetric);
	}
#endif

	if (Stats)
	{
		Stats->OnDeath.AddDynamic(this, &AMMOCharacter::HandleDeath);
		Stats->OnRevived.AddDynamic(this, &AMMOCharacter::HandleRevived);
		Stats->OnLevelUp.AddDynamic(this, &AMMOCharacter::OnLevelUpFeedback);
	}

	ApplyCameraSettings();
}

void AMMOCharacter::HandleFirstMovementMetric(float DeltaSeconds, FVector InitialLocation, FVector InitialVelocity)
{
#if !UE_BUILD_SHIPPING
	AMMOPlayerController* PC = Cast<AMMOPlayerController>(GetController());
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (bEndPlayStarted || !HasAuthority() || !IsValid(PC) || !PC->HasAuthority() || PC->GetPawn() != this
		|| !IsValid(Stats) || Stats->GetOwner() != this || Stats->bIsDead
		|| !FMath::IsFinite(Stats->Health) || Stats->Health <= 0.f
		|| !IsValid(Move) || !Move->IsActive() || Move->UpdatedComponent != GetCapsuleComponent()
		|| GetCapsuleComponent()->IsSimulatingPhysics() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f
		|| InitialLocation.ContainsNaN() || InitialVelocity.ContainsNaN()
		|| Move->bForceMaxAccel || Move->bJustTeleported || Move->bTeleportedSinceLastUpdate
		|| Move->HasRootMotionSources() || MovementBaseUtility::IsDynamicBase(GetMovementBase())) return;
	const EMovementMode Mode = Move->MovementMode;
	if (Mode != MOVE_Walking && Mode != MOVE_NavWalking && Mode != MOVE_Falling) return;
	const FVector Location = Move->UpdatedComponent->GetComponentLocation();
	const FVector Acceleration = Move->GetCurrentAcceleration();
	if (Location.ContainsNaN() || Acceleration.ContainsNaN()) return;
	const double DistanceSquared = (Location - InitialLocation).SizeSquared2D();
	const double AccelerationSquared = Acceleration.SizeSquared2D();
	if (!FMath::IsFinite(DistanceSquared) || DistanceSquared < 1.0
		|| !FMath::IsFinite(AccelerationSquared) || AccelerationSquared <= 0.0) return;
	const FName ModeName = Mode == MOVE_Walking ? TEXT("walking") : (Mode == MOVE_NavWalking ? TEXT("navwalking") : TEXT("falling"));
	// Observable state is a filter, not proof that input exclusively caused the displacement.
	PC->RecordFirstMoveMetric(ModeName, FMath::Sqrt(DistanceSquared));
#else
	static_cast<void>(DeltaSeconds); static_cast<void>(InitialLocation); static_cast<void>(InitialVelocity);
#endif
}

void AMMOCharacter::ApplyCameraSettings()
{
	// BP'deki Camera Boom'u (spring arm) bul ve uzaklik ayarla (yerel kameraya etki eder).
	if (USpringArmComponent* Boom = FindComponentByClass<USpringArmComponent>())
	{
		Boom->TargetArmLength = CameraDistance;
	}

	// Baslangic bakis acisini biraz yukaridan yap (sadece yerel oyuncu; kamera control rotation'i takip eder).
	if (IsLocallyControlled())
	{
		if (AController* C = GetController())
		{
			FRotator R = C->GetControlRotation();
			R.Pitch = CameraStartPitch;
			C->SetControlRotation(R);
		}
	}
}

void AMMOCharacter::OnLevelUpFeedback(int32 NewLevel)
{
	// OnLevelUp hem sunucuda (broadcast) hem istemcide (OnRep_Level) tetiklenir; sadece yerel oyuncuya goster.
	if (IsLocallyControlled() && IsPlayerControlled() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor(255, 230, 90),
			FString::Printf(TEXT("*** LEVEL UP!  Seviye %d ***"), NewLevel));
	}
}

void AMMOCharacter::HandleRevived()
{
	// Hem sunucuda hem sahibi istemcide calisir: kilitlenen hareketi geri ac.
	if (IsPlayerControlled())
	{
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}

	// Bu diriltme sifaci NPC'den (erken, bedelli) geldiyse HandleDeath'in kurdugu
	// respawn zamanlayicisi hala bekliyor olabilir; temizlenmezse birkac saniye sonra
	// zaten dirilmis/oynayan karakteri zorla baslangic noktasina isinlar. Zamanlayici
	// zaten dogal respawn'dan geldiyse (RespawnAtStart calismis) bu cagri zaten bosa
	// giden bir temizlik (handle gecersiz), zararsiz. Sadece sunucuda anlamli, ama
	// istemcide bos bir handle'i temizlemek de zararsiz.
	GetWorldTimerManager().ClearTimer(RespawnTimerHandle);

	// Canlanma korumasi: dogal respawn (RespawnAtStart -> Revive) ve sifaci diriltmesi
	// ikisi de buradan gecer. Dogus noktasinin yakininda agresif mob varsa oyuncu hemen
	// tekrar olup olum dongusune girmesin diye kisa sure hasar/aggro'dan muaf tut.
	if (HasAuthority() && IsPlayerControlled())
	{
		StartSpawnProtection();
	}
}

void AMMOCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMMOCharacter, bSpawnProtected);
	DOREPLIFETIME_CONDITION(AMMOCharacter, bClassChoicePending, COND_OwnerOnly);
}

void AMMOCharacter::StartSpawnProtection()
{
	if (!HasAuthority() || SpawnProtectionDuration <= 0.f)
	{
		return;
	}

	bSpawnProtected = true;
	GetWorldTimerManager().SetTimer(SpawnProtectionTimerHandle, this,
		&AMMOCharacter::EndSpawnProtection, SpawnProtectionDuration, false);
	ForceNetUpdate();
	OnRep_SpawnProtected(); // OnRep sunucuda calismaz; host kendi geri bildirimini burada alir
}

void AMMOCharacter::EndSpawnProtection()
{
	if (!HasAuthority() || !bSpawnProtected)
	{
		return;
	}

	bSpawnProtected = false;
	GetWorldTimerManager().ClearTimer(SpawnProtectionTimerHandle);
	ForceNetUpdate();
	OnRep_SpawnProtected();
}

void AMMOCharacter::OnRep_SpawnProtected()
{
	// HUD etiketi bayragi her karede okur; burada sadece korumanin bittigini yerel oyuncuya bildir.
	if (!bSpawnProtected && IsLocallyControlled() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor(150, 200, 255), TEXT("Canlanma korumasi bitti"));
	}
}

void AMMOCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Sunucu tarafi: oyuncu possess ettiginde sinif kurulumu (starter skill + stat dagilimi).
	// BeginPlay'de degil burada — possession BeginPlay'den sonra gelebilir (ozellikle MP'de).
	if (Cast<APlayerController>(NewController) && SkillComponent)
	{
		SkillComponent->SetupForClass(SkillComponent->CharacterClass);
	}

	// Otorite + oyuncu: kaydi (varsa) guvenli zamanlamada yukle + periyodik otomatik kaydet.
	if (HasAuthority() && Cast<APlayerController>(NewController))
	{
		GetWorldTimerManager().SetTimer(AutoLoadTimerHandle, this, &AMMOCharacter::AutoLoadDeferred, 0.6f, false);
		GetWorldTimerManager().SetTimer(AutoSaveTimerHandle, this, &AMMOCharacter::SaveCharacter, 60.f, true);

		// Ilk giris de bir "dogus": kisa canlanma korumasi ver.
		StartSpawnProtection();
	}
}

void AMMOCharacter::UnPossessed()
{
	if (HasAuthority())
	{
		// Publish while the old PlayerState is still available, even if loading
		// or saving never completed. A travel/EndPlay route keeps online continuity.
		if (!bEndPlayStarted && IsPlayerControlled() && !bTravelSaveCommitted && FriendsComponent)
		{
			FriendsComponent->AnnouncePresence(false);
		}
		// APawn::Destroyed detaches before Character EndPlay. Save BEFORE Super
		// removes PlayerState; never derive a Default identity for a detached pawn.
		if (!bEndPlayStarted && IsPlayerControlled() && bInitialLoadComplete && !bTravelSaveCommitted)
		{
			if (!SaveCharacterForTravel(true))
			{
				UE_LOG(LogTemp, Warning, TEXT("Character detach save not confirmed: %s"), *GetSaveSlotName());
			}
		}
		// The pawn can remain alive after a controller switch. Its old autoload
		// must not load Default, and autosave/nav retry must not reopen saving.
		GetWorldTimerManager().ClearTimer(AutoLoadTimerHandle);
		GetWorldTimerManager().ClearTimer(AutoSaveTimerHandle);
		GetWorldTimerManager().ClearTimer(SavedLocationRetryTimerHandle);
		bInitialLoadComplete = false;
	}
	Super::UnPossessed();
}

void AMMOCharacter::AutoLoadDeferred()
{
	// bInitialLoadComplete artik LoadCharacter() icinde (her cagiran yoldan) set edilir.
	LoadCharacter();

	// Giris tamam: listesinde bu oyuncu olan cevrimici arkadaslara haber ver.
	if (HasAuthority() && IsPlayerControlled() && FriendsComponent)
	{
		FriendsComponent->AnnouncePresence(true);
	}
}

void AMMOCharacter::MoveToPlayerStart()
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			SetActorLocation(It->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
			SetActorRotation(It->GetActorRotation());
			return;
		}
	}
}

bool AMMOCharacter::TryRestoreSavedLocation(const FVector& SavedLocation, const FRotator& SavedRotation, bool& bNavPending)
{
	bNavPending = false;
	if (SavedLocation.ContainsNaN())
	{
		return false;
	}
	UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!World || !Capsule)
	{
		return false;
	}
	const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Nav || !Nav->GetMainNavData())
	{
		// Harita yuklenirken navmesh henuz hazir olmayabilir. PlayerStart'a donup
		// autosave ile eski koordinati ezmek yerine kisa araliklarla yeniden dene.
		bNavPending = true;
		return false;
	}

	FHitResult GroundHit;
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(MMOCharacterLoadGround), false, this);
	// Baslangici kapsul merkezinde tut: ustteki cati/kopru, ayak altindaki
	// destek zeminden once vurulup gecerli kaydi yanlis reddetmesin.
	const FVector TraceEnd(SavedLocation.X, SavedLocation.Y, SavedLocation.Z - 500.f);
	if (!World->LineTraceSingleByChannel(GroundHit, SavedLocation, TraceEnd, ECC_Visibility, TraceParams)
		|| GroundHit.ImpactNormal.Z < 0.7f
		|| FMath::Abs((SavedLocation.Z - GroundHit.ImpactPoint.Z) - Capsule->GetScaledCapsuleHalfHeight()) > 120.f)
	{
		return false;
	}
	FNavLocation NavLocation;
	if (!Nav->ProjectPointToNavigation(GroundHit.ImpactPoint + FVector(0.f, 0.f, 20.f),
			NavLocation, FVector(75.f, 75.f, 80.f))
		|| FVector::Dist2D(NavLocation.Location, GroundHit.ImpactPoint) > 75.f
		|| FMath::Abs(NavLocation.Location.Z - GroundHit.ImpactPoint.Z) > 35.f)
	{
		return false;
	}
	const FVector Target = NavLocation.Location + FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + 2.f);
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
	if (World->OverlapBlockingTestByChannel(Target, FQuat::Identity, ECC_Pawn, CapsuleShape, TraceParams))
	{
		return false;
	}
	SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
	if (!SavedRotation.ContainsNaN())
	{
		SetActorRotation(SavedRotation);
	}
	return true;
}

void AMMOCharacter::RetrySavedLocationRestore()
{
	if (!HasAuthority())
	{
		return;
	}
	bool bNavPending = false;
	if (TryRestoreSavedLocation(PendingSavedLocation, PendingSavedRotation, bNavPending))
	{
		bInitialLoadComplete = true;
	}
	else if (bNavPending && ++SavedLocationRetryCount < MaxSavedLocationRetries)
	{
		GetWorldTimerManager().SetTimer(SavedLocationRetryTimerHandle, this,
			&AMMOCharacter::RetrySavedLocationRestore, 1.f, false);
		return;
	}
	else
	{
		if (bNavPending)
		{
			UE_LOG(LogTemp, Warning, TEXT("Character navigation unavailable after %d retries; using PlayerStart and enabling saves"),
				SavedLocationRetryCount);
		}
		MoveToPlayerStart();
		bInitialLoadComplete = true;
	}
	GetWorldTimerManager().ClearTimer(SavedLocationRetryTimerHandle);
}

void AMMOCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndPlayStarted = true;
#if !UE_BUILD_SHIPPING
	OnCharacterMovementUpdated.RemoveDynamic(this, &AMMOCharacter::HandleFirstMovementMetric);
#endif
	// Oyundan cikis/level bitiminde otomatik kaydet (otorite + oyuncu).
	if (HasAuthority() && IsPlayerControlled())
	{
		// bInitialLoadComplete SART: yukleme (0.6sn gecikmeli AutoLoadDeferred) henuz
		// calismadiysa component'ler hala varsayilan/bos durumda — bu haliyle kaydetmek
		// gercek kayit dosyasini bos bir karakterle EZER (bkz. bInitialLoadComplete yorumu,
		// bug-hunt bulgusu: possess sonrasi ~0.6sn icinde disconnect = ilerlemenin silinmesi).
		if (bInitialLoadComplete && !bTravelSaveCommitted)
		{
			SaveCharacterForTravel(true); // karakter yok oluyor: gecis penceresinde bile ertelenmez
		}

		// Gercek cikis (Destroyed; bolge gecisi/kapanis degil): arkadaslara cevrimdisi haberi.
		if (FriendsComponent && EndPlayReason == EEndPlayReason::Destroyed && !bTravelSaveCommitted)
		{
			FriendsComponent->AnnouncePresence(false);
		}
	}
	// Destroyed() detaches the controller before EndPlay; party cleanup must not require PlayerState.
	// Components are still alive until Super::EndPlay. Authority dispatch is local even without ownership.
	if (HasAuthority() && PartyComponent)
	{
		PartyComponent->RequestLeave();
	}
	Super::EndPlay(EndPlayReason);
}

FString AMMOCharacter::GetSaveSlotName() const
{
	FString Name = TEXT("Default");
	if (const APlayerState* PS = GetPlayerState())
	{
		const FString PlayerName = PS->GetPlayerName();
		if (!PlayerName.IsEmpty())
		{
			Name = PlayerName;
		}
	}
	// PlayerState adi bir platformdan da gelebilir. Yol ayirici/nokta gibi karakterleri
	// save dosya adina tasima; bosluklarin eski kayitlardaki '_' eslemesini koru.
	FString SafeName;
	SafeName.Reserve(FMath::Min(Name.Len(), 32));
	for (const TCHAR Ch : Name)
	{
		if (SafeName.Len() >= 32)
		{
			break;
		}
		if (FChar::IsAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('-'))
		{
			SafeName.AppendChar(Ch);
		}
		else if (Ch == TEXT(' '))
		{
			SafeName.AppendChar(TEXT('_'));
		}
	}
	return FString(TEXT("MMOSave_")) + (SafeName.IsEmpty() ? TEXT("Default") : SafeName);
}

void AMMOCharacter::SaveCharacter()
{
	SaveCharacterForTravel();
}

void AMMOCharacter::SetTravelSaveCommitted(bool bCommitted)
{
	if (HasAuthority())
	{
		bTravelSaveCommitted = bCommitted;
	}
}

bool AMMOCharacter::FlushDeferredSave(bool bRefreshSnapshot)
{
	// Ilk snapshot sonrasi pasif XP/ganimet, SaveCharacter cagirmadan degisebilir.
	// Tum travel onaylari geldikten sonra son durumu yaz; hata/iptal yolunda da yeniden denensin.
	if (bRefreshSnapshot && HasAuthority())
	{
		bSaveDeferredByTravel = true;
	}
	if (!bSaveDeferredByTravel)
	{
		return true;
	}
#if !UE_BUILD_SHIPPING
	if (TestDeferredSaveFailures > 0)
	{
		--TestDeferredSaveFailures;
		UE_LOG(LogTemp, Display, TEXT("[MMO_ZONE_TRAVEL_DEFERRED_TEST] deferred write intentionally failed"));
		return false;
	}
#endif
	const bool bSaved = SaveCharacterForTravel();
	UE_LOG(LogTemp, Display, TEXT("[MMOSave] Gecis penceresinde ertelenen kayit denendi (%s) saved=%d"),
		*GetSaveSlotName(), bSaved ? 1 : 0);
	return bSaved;
}

bool AMMOCharacter::SaveCharacterForTravel(bool bForce)
{
	// Bolge gecisi kaydim ucustaysa (bulut magaza) yeni bir yazim numarasi onu "eski" yapip gecisi iptal ettirir:
	// ertele, gecis bitince tek yazimda birlestirilir. Yerel magazada callback ayni karede doner, pencere yok.
	if (!bForce)
	{
		const UWorld* World = GetWorld();
		const AMMOGameMode* GameMode = World ? World->GetAuthGameMode<AMMOGameMode>() : nullptr;
		if (GameMode && GameMode->IsZoneTravelSaveInFlight(this))
		{
			bSaveDeferredByTravel = true;
			return false;
		}
	}

	IMMOCharacterStore* Store = nullptr;
	FString TargetSlot;
	FMMOCharacterSaveData D;
	if (!BuildCharacterSaveRequest(Store, TargetSlot, D))
	{
		return false;
	}

	// Yazim sirasi: bu yazim, ayni yuvada ucustaki daha eski asenkron yazimlari "eski" yapar (bkz. IMMOCharacterStore).
	Store->GetWriteTracker()->BeginWrite(TargetSlot);
	const bool bOk = Store->Save(TargetSlot, D);
	if (bOk)
	{
		ActiveSaveSlot = TargetSlot;
		// Elle/otomatik/EndPlay kaydi da onceki ertelenen anlik durumu basariyla kalici hale getirir.
		bSaveDeferredByTravel = false;
	}

	// "Kaydedildi" mesaji burada DEGIL: bu yol otomatik kayitta (60 sn), klan/ticaret/seyahat
	// kaydinda da calisir ve oyuncuya her seferinde mesaj yagmamali. Yalniz elle kayit
	// (MMOSave / ServerSaveCharacter) ToOwner ile bildirir.
	return bOk;
}

bool AMMOCharacter::BuildCharacterSaveRequest(IMMOCharacterStore*& OutStore, FString& OutSlot,
	FMMOCharacterSaveData& OutData)
{
	OutStore = nullptr;
	OutSlot.Reset();
	OutData = FMMOCharacterSaveData();
	if (!HasAuthority() || !bInitialLoadComplete)
	{
		return false;
	}

	OutStore = UMMOCharacterStoreSubsystem::Get(this);
	if (!OutStore)
	{
		return false;
	}

	// SECURITY_AUDIT K-1 (kayit ezme): yuklenen/olusturulan yuva ile simdiki yuva (ad degisti) farkliysa
	// ve hedef yuvada baska birine ait bir kayit zaten varsa YAZMA.
	OutSlot = GetSaveSlotName();
	if (!ActiveSaveSlot.IsEmpty() && !ActiveSaveSlot.Equals(OutSlot, ESearchCase::IgnoreCase)
		&& OutStore->Exists(OutSlot))
	{
		UE_LOG(LogTemp, Warning, TEXT("Save refused: %s -> %s already exists (name changed since load)"), *ActiveSaveSlot, *OutSlot);
		MMONotify::ToOwner(this,
			TEXT("Bu ad baska bir kayda ait: kaydedilmedi. Once MMOLoad ile o kaydi yukle ya da farkli bir ad sec."),
			FColor::Orange, 5.f);
		return false;
	}

	OutData.bValid = true;
	if (Stats)
	{
		Stats->FillSaveData(OutData);
	}
	if (Inventory)
	{
		Inventory->FillSaveData(OutData);
	}
	if (SkillComponent)
	{
		SkillComponent->FillSaveData(OutData);
	}
	if (QuestComponent)
	{
		QuestComponent->FillSaveData(OutData);
	}
	if (ClanComponent)
	{
		ClanComponent->FillSaveData(OutData);
	}
	if (FriendsComponent)
	{
		FriendsComponent->FillSaveData(OutData);
	}
	OutData.MapName = UGameplayStatics::GetCurrentLevelName(this, true);
	OutData.Location = GetActorLocation();
	OutData.Rotation = GetActorRotation();
	return true;
}

void AMMOCharacter::SaveCharacterForTravelAsync(TFunction<void(bool bSuccess)>&& Completion)
{
	IMMOCharacterStore* Store = nullptr;
	FString TargetSlot;
	FMMOCharacterSaveData Data;
	if (!BuildCharacterSaveRequest(Store, TargetSlot, Data))
	{
		if (Completion)
		{
			Completion(false);
		}
		return;
	}

	const TWeakObjectPtr<AMMOCharacter> WeakThis(this);
	// Yazim sirasi korumasi: callback geldiginde ayni yuvaya daha YENI bir yazim (otomatik kayit, ticaret,
	// yeni gecis istegi) baslamissa bu eski anlik goruntunun sonucu yok sayilir (basari degil) — eski
	// yazim yeni veriyi geri sarmis gibi islem gormesin. Tracker paylasimli: magaza yok olsa da guvenli.
	const TSharedRef<FMMOSlotWriteTracker> Tracker = Store->GetWriteTracker();
	const uint64 WriteTicket = Tracker->BeginWrite(TargetSlot);
	Store->SaveAsync(TargetSlot, Data,
		[WeakThis, TargetSlot, Tracker, WriteTicket, Completion = MoveTemp(Completion)](bool bSuccess) mutable
		{
			AMMOCharacter* Character = WeakThis.Get();
			if (bSuccess && !Tracker->IsNewest(TargetSlot, WriteTicket))
			{
				UE_LOG(LogTemp, Warning, TEXT("Save superseded: %s write #%llu finished after a newer write started; result ignored"),
					*TargetSlot, WriteTicket);
				bSuccess = false;
			}
			else if (!Character)
			{
				bSuccess = false;
			}
			else if (bSuccess)
			{
				Character->ActiveSaveSlot = TargetSlot;
			}
			if (Completion)
			{
				Completion(bSuccess);
			}
		});
}

void AMMOCharacter::GrantStarterPack()
{
	// Yalniz sunucu + oyuncu. Ayni pawn icin bir kez (MMOLoad vb. ile yeni-karakter dali tekrar
	// calisirsa cift vermesin). Kayit formatina alan eklenmez: "kayit dosyasi yoktu" bilgisi yeterli.
	if (!HasAuthority() || bStarterPackGranted || !Inventory || !IsPlayerControlled())
	{
		return;
	}
	bStarterPackGranted = true;

	// Ilk silah + ilk zirh (VERTICAL_SLICE_LV1_30 madde 2) cantaya girip kusanilir; 3 Can Iksiri.
	static const FName StarterEquipment[] = { FName(TEXT("ShortSword")), FName(TEXT("LeatherArmor")) };
	for (const FName& ItemId : StarterEquipment)
	{
		const int32 BagBefore = Inventory->EquipmentBag.Num();
		Inventory->AddEquipmentInstance(ItemId);
		if (Inventory->EquipmentBag.Num() > BagBefore)
		{
			Inventory->EquipStarterEquipment(Inventory->EquipmentBag.Last().InstanceId);
		}
	}
	Inventory->AddItem(FName(TEXT("HealthPotion")), 3);

	UE_LOG(LogTemp, Display, TEXT("[MMODBG] Starter pack granted to %s weapon=%d armor=%d bag=%d potions=%d"),
		*MMOMetrics::WhoName(this), Inventory->GetEquipped(EMMOEquipSlot::Weapon).IsValidInstance() ? 1 : 0,
		Inventory->GetEquipped(EMMOEquipSlot::Armor).IsValidInstance() ? 1 : 0, Inventory->EquipmentBag.Num(),
		Inventory->GetItemCount(FName(TEXT("HealthPotion"))));
}

bool AMMOCharacter::LoadCharacter()
{
	if (!HasAuthority())
	{
		return false;
	}

	// SECURITY_AUDIT K-1 F3: karakter zaten yukluyken (MMOLoad, yalniz host/yonetici) canli adin kaydi yoksa HICBIR SEY
	// degistirme. Eskiden "yeni karakter" dalina dusup yuklu karakteri bellekte tutarak ActiveSaveSlot'u bos yeni yuvaya
	// ceviriyor, LoadSerial'i artirip basarim/gunluk durumunu sifirliyordu: sonraki kayit catallama + odul tekrari demekti.
	// Bu donus LoadSerial/bInitialLoadComplete'e dokunmadan once olmali. Kosul yalniz ActiveSaveSlot (Y3): bInitialLoadComplete
	// false iken de (navmesh bekleyen yukleme, basarisiz yeniden yukleme, sahip degisimi) bellekte bir karakter vardir ve
	// "yeni karakter" dali onu yeni yuvaya baslangic paketiyle catallardi.
	if (!ActiveSaveSlot.IsEmpty())
	{
		IMMOCharacterStore* ReloadStore = UMMOCharacterStoreSubsystem::Get(this);
		if (ReloadStore && !ReloadStore->Exists(GetSaveSlotName()))
		{
			MMONotify::ToOwner(this, TEXT("Bu adin kaydi yok: yuklu karakter degismedi."), FColor::Orange, 4.f);
			return false;
		}
	}

	// Manuel MMOLoad ve otomatik yukleme ayni guvenlik kapisindan gecer. Onceki bir
	// yukleme basarili olsa bile simdiki dosya okunamazsa autosave onu ezmesin.
	bInitialLoadComplete = false;
	++LoadSerial; // basarim/gunluk: karakter verisi degisti, yan dosyalari yeniden yukle
	GetWorldTimerManager().ClearTimer(SavedLocationRetryTimerHandle);
	SavedLocationRetryCount = 0;

	// Magaza yoksa yeni karakter dali YOK sayilmaz (orada kayit uzerine yazma riski): yukleme basarisiz,
	// bInitialLoadComplete false kalir, kayit kapali.
	IMMOCharacterStore* Store = UMMOCharacterStoreSubsystem::Get(this);
	if (!Store)
	{
		UE_LOG(LogTemp, Error, TEXT("Character store unavailable; load failed, saving disabled"));
		return false;
	}

	const FString Slot = GetSaveSlotName();
	if (!Store->Exists(Slot))
	{
		// Yeni karakter: hicbir kayit olmadigi icin PossessedBy'daki SetupForClass
		// varsayilan (Warrior) sinifi sessizce uygulamis durumda. Oyuncunun bunu
		// degistirmenin tek yolunun gizli bir konsol komutu oldugunu bilmesinin
		// hicbir yolu yoktu (icerik-erisilebilirligi eksigi, "PVP acik/kapali" gibi
		// diger konsol-yonlendirmeli bildirimlerle ayni desen kullanildi).
		// Eskiden 8 sn'lik bir ekran mesajiydi (kacirilabiliyordu, istemcide hic cikmiyordu);
		// artik replike bayrakla HUD'da sinif secilene kadar kalici bir ipucu paneli cizilir.
		bClassChoicePending = true;
		bInitialLoadComplete = true;
		ActiveSaveSlot = Slot;
		// Baslangic paketi YALNIZ bu dalda (kayit dosyasi yok); kayit yuklenen karakterlere hic verilmez.
		GrantStarterPack();
		return false; // yeni karakter
	}
	bClassChoicePending = false;

	FMMOCharacterSaveData D;
	if (!Store->Load(Slot, D) || !D.bValid)
	{
		UE_LOG(LogTemp, Error, TEXT("Character save %s exists but could not be loaded; saving disabled"), *Slot);
		return false;
	}

	// Once sinif/skiller (stat reset yapmadan), sonra statlar, sonra envanter (ekipman bonusu).
	if (SkillComponent)
	{
		SkillComponent->ApplySaveData(D);
	}
	if (Stats)
	{
		Stats->ApplySaveData(D);
	}
	if (Inventory)
	{
		Inventory->ApplySaveData(D);
	}
	if (QuestComponent)
	{
		QuestComponent->ApplySaveData(D);
	}
	if (ClanComponent)
	{
		ClanComponent->ApplySaveData(D);
	}
	if (FriendsComponent)
	{
		FriendsComponent->ApplySaveData(D);
	}

	// Kayit verisi uygulandi: yuvayi hemen simdi sahiplen. Konum-bekleme (navmesh) dalindan ONCE olmali;
	// aksi halde ActiveSaveSlot bos/bayat kalir ve K-1 korumasi pasif olur ya da mesru kayitlar reddedilir.
	ActiveSaveSlot = Slot;

	// Eski/bozuk kayit koordinati harita disinda, boslukta veya navmesh disinda olabilir.
	// Boyle bir konuma isiklanmak oyuncuyu kalici olarak kilitler. Ilk dogus konumu
	// guvenli geri donus noktasidir; yeni kayitlarda harita adi da eslesmelidir.
	bool bNavPending = false;
	const FString CurrentMap = UGameplayStatics::GetCurrentLevelName(this, true);
	// Harita adi olmayan eski kaydin hangi map'e ait oldugu bilinmiyor. Konumunu
	// yalniz mevcut ana harita Summer_scene'de dene; baska map'te ayni XY'yi kullanma.
	const bool bMatchingMap = D.MapName.IsEmpty()
		? CurrentMap == TEXT("Summer_scene") : D.MapName == CurrentMap;
	const bool bRestored = bMatchingMap && TryRestoreSavedLocation(D.Location, D.Rotation, bNavPending);
	if (!bRestored && bNavPending)
	{
		PendingSavedLocation = D.Location;
		PendingSavedRotation = D.Rotation;
		GetWorldTimerManager().SetTimer(SavedLocationRetryTimerHandle, this,
			&AMMOCharacter::RetrySavedLocationRestore, 1.f, false);
		UE_LOG(LogTemp, Warning, TEXT("Character navigation data unavailable; saved location restore deferred"));
		return true;
	}
	if (!bRestored)
	{
		MoveToPlayerStart();
		UE_LOG(LogTemp, Warning, TEXT("Saved character position rejected for map %s; moved to PlayerStart"),
			*UGameplayStatics::GetCurrentLevelName(this, true));
	}
	bInitialLoadComplete = true;

	// LoadCharacter yalniz sunucuda calisir: uzak istemciye de ToOwner ile (B5).
	MMONotify::ToOwner(this, TEXT("Karakter yuklendi"), FColor(120, 200, 255), 2.5f);
	return true;
}

void AMMOCharacter::MMOSave()
{
	if (HasAuthority())
	{
		if (SaveCharacterForTravel())
		{
			MMONotify::ToOwner(this, TEXT("Kaydedildi"), FColor(120, 220, 120), 2.f);
		}
		else
		{
			const UWorld* World = GetWorld();
			const AMMOGameMode* GameMode = World ? World->GetAuthGameMode<AMMOGameMode>() : nullptr;
			const bool bWaitingForTravel = GameMode && GameMode->IsZoneTravelSaveInFlight(this);
			MMONotify::ToOwner(this, bWaitingForTravel
				? TEXT("Bolge gecisi kaydi suruyor; kayit hemen ardindan yapilacak")
				: TEXT("Karakter kaydedilemedi; tekrar deneyin"), FColor::Orange, 3.f);
		}
	}
	else
	{
		ServerSaveCharacter();
	}
}

void AMMOCharacter::MMOLoad()
{
	if (HasAuthority())
	{
		LoadCharacter();
	}
	else
	{
		ServerLoadCharacter();
	}
}

void AMMOCharacter::MMOTestSaveJson()
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	FString Error;
	int32 Utf8Bytes = 0;
	int32 NegativeCases = 0;
	const bool bPassed = FMMOCharacterSaveJson::RunRoundTripSelfTest(Error, Utf8Bytes, &NegativeCases);
	if (bPassed)
	{
		UE_LOG(LogTemp, Display, TEXT("[MMO_SAVE_JSON_TEST] PASS schema=%d bytes=%d negative=%d"),
			FMMOCharacterSaveJson::CurrentSchemaVersion, Utf8Bytes, NegativeCases);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[MMO_SAVE_JSON_TEST] FAIL %s"), *Error);
	}
}

void AMMOCharacter::ServerSaveCharacter_Implementation()
{
	// Reliable RPC spam'i sunucuyu surekli diske yazmaya zorlamasin.
	const UWorld* World = GetWorld();
	if (!bInitialLoadComplete || !World
		|| World->GetTimeSeconds() - LastRemoteManualSaveTime < RemoteManualSaveCooldown)
	{
		return;
	}
	LastRemoteManualSaveTime = World->GetTimeSeconds();
	// Elle kayit: uzak sahibine de bildir (otomatik kayit sessiz kalir; bkz. SaveCharacterForTravel).
	if (SaveCharacterForTravel())
	{
		MMONotify::ToOwner(this, TEXT("Kaydedildi"), FColor(120, 220, 120), 2.f);
	}
	else
	{
		const AMMOGameMode* GameMode = World->GetAuthGameMode<AMMOGameMode>();
		const bool bWaitingForTravel = GameMode && GameMode->IsZoneTravelSaveInFlight(this);
		MMONotify::ToOwner(this, bWaitingForTravel
			? TEXT("Bolge gecisi kaydi suruyor; kayit hemen ardindan yapilacak")
			: TEXT("Karakter kaydedilemedi; tekrar deneyin"), FColor::Orange, 3.f);
	}
}

void AMMOCharacter::ServerLoadCharacter_Implementation()
{
	// Manuel yukleme debug islemidir. Uzak oyuncu eski disk durumuna donerek
	// tuketim, olum cezasi veya gorev sonuclarini geri alamaz.
	if (!MMOAdmin::IsServerAdmin(Cast<APlayerController>(GetController())))
	{
		return;
	}

	LoadCharacter();
}

void AMMOCharacter::MMOSetName(const FString& NewName)
{
	if (HasAuthority())
	{
		ServerSetName_Implementation(NewName);
	}
	else
	{
		ServerSetName(NewName);
	}
}

void AMMOCharacter::ServerSetName_Implementation(const FString& NewName)
{
	// Reddedilen istekler uzak sahibine gecici ipucu olarak bildirilir (Unreliable, sohbeti doldurmaz;
	// ayni metin 0.5 sn icinde tekrarlanmaz). Eskiden sessizce atiliyordu.
	const auto Reject = [this](const FString& Reason)
	{
		MMONotify::ToOwner(this, Reason, FColor::Orange, 4.f, MMONotify::EMMONotifyKind::Hint);
	};

	// En fazla 20 karakter: motor ?Name= degerini 20'ye keser; daha uzun bir ad bir daha ?Name= ile secilemezdi.
	const FString Trimmed = NewName.TrimStartAndEnd();
	if (Trimmed.IsEmpty() || Trimmed.Len() > 20)
	{
		Reject(TEXT("Ad 1-20 karakter olmali"));
		return;
	}
	for (const TCHAR Ch : Trimmed)
	{
		if (!FChar::IsAlnum(Ch) && Ch != TEXT('_') && Ch != TEXT('-') && Ch != TEXT(' '))
		{
			Reject(TEXT("Adda yalniz harf, rakam, bosluk, '_' ve '-' olabilir"));
			return;
		}
	}
	// SECURITY_AUDIT K-1 yeni yuz (F1): uzak istemci yalniz HIC kaydedilmemis karakterin adini degistirebilir. Kayitli karakter
	// yeni ada gecip cikinca yeni yuvaya tam envanterle yazilir, eski yuva diskte kalir (catallama, altin/esya ikiye katlanir).
	// Kosul diskteki yuva: ActiveSaveSlot.IsEmpty() YETMEZ, yeni karakter dali yuvayi dosya yokken de atar. Bos ActiveSaveSlot
	// (yukleme henuz bitmedi) catallama degil: hicbir sey yuklenmedi, yeni ad kendi kaydini yukler. Host/yerel ve yonetici muaf.
	const bool bRemoteNameRules = UsesRemoteNameRules();
	bool bNameAdmin = MMOAdmin::IsServerAdmin(Cast<APlayerController>(GetController()));
#if !UE_BUILD_SHIPPING
	// Headless testte denetleyici yereldir (= yonetici sayilir); test bayragi uzak ve yonetici DISI oyuncuyu benzetir.
	if (bTestRemoteNameRules)
	{
		bNameAdmin = false;
	}
#endif
	// Soguma (SECURITY_AUDIT Y-7): uzak istemci 30 sn, host/yerel konsol 5 sn (test akisi bozulmasin ama
	// arkadaslara bildirim yagmasi da olmasin). Reddedilince sahibe ipucu gider (Reject).
	// Tur 3 (Y8): soguma diske bakan her Exists kontrolunden ONCE: reddedilen istekler de diske sinirsiz hizda dokunmasin.
	const UWorld* CooldownWorld = GetWorld();
	if (CooldownWorld)
	{
		const float CooldownDuration = bRemoteNameRules ? SetNameCooldown : SetNameCooldownLocal;
		const float CooldownLeft = CooldownDuration - (CooldownWorld->GetTimeSeconds() - LastSetNameServerTime);
		if (CooldownLeft > 0.f)
		{
			Reject(FString::Printf(TEXT("Ad degistirmek icin %.0f sn beklemelisin"), FMath::CeilToFloat(CooldownLeft)));
			return;
		}
	}
	if (bRemoteNameRules && !bNameAdmin && !ActiveSaveSlot.IsEmpty())
	{
		// Magaza yoksa yuvanin durumu dogrulanamaz: reddet (muhafazakar).
		IMMOCharacterStore* OwnStore = UMMOCharacterStoreSubsystem::Get(this);
		if (!OwnStore || OwnStore->Exists(ActiveSaveSlot))
		{
			// Y8: bu ret de soguma baslatir (diske bakan sorgu tekrarlanamaz).
			if (CooldownWorld)
			{
				LastSetNameServerTime = CooldownWorld->GetTimeSeconds();
			}
			MMONotify::ToOwner(this, TEXT("Ad kilitli: karakter ilk kez kaydedildikten sonra (MMOSave, otomatik kayit ya da takas/klan/arkadas/posta islemi kaydi) adi degistirilemez. Baska ad = yeni karakter: cikip ?Name=YeniAd ile baglan (bu karakterin ilerlemesi tasinmaz)."),
				FColor::Orange, 8.f);
			return;
		}
	}
	// Ayni oturumdaki iki karakter ayni save dosyasina yazmasin. Windows dosya
	// adlari buyuk/kucuk harf ayirt etmez; bosluk ve '_' da ayni slotu uretir.
	const FString CandidateSlot = FString(TEXT("MMOSave_")) + Trimmed.Replace(TEXT(" "), TEXT("_"));
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AMMOCharacter> It(World); It; ++It)
		{
			const AMMOCharacter* Other = *It;
			if (Other != this && Other->IsPlayerControlled()
				&& Other->GetSaveSlotName().Equals(CandidateSlot, ESearchCase::IgnoreCase))
			{
				Reject(TEXT("Bu ad su an baska bir oyuncuda"));
				return;
			}
		}
	}
	// Uzak istemci diskte kaydi olan (baska birine ait) bir adi alamaz: aksi halde SaveCharacterForTravel
	// bu karakter icin surekli reddedilir ve host'un bolge gecisi kilitlenir (QA O-3). Kendi yuvasina donus serbest.
	if (bRemoteNameRules && !ActiveSaveSlot.Equals(CandidateSlot, ESearchCase::IgnoreCase))
	{
		// Magaza yoksa varlik dogrulanamaz: ad degisikligi reddedilir (muhafazakar).
		IMMOCharacterStore* NameStore = UMMOCharacterStoreSubsystem::Get(this);
		if (!NameStore || NameStore->Exists(CandidateSlot))
		{
			// F5: bu ret de soguma baslatir; aksi halde cevrimdisi kayitli adlar bedava ve sinirsiz hizda sorgulanirdi.
			if (CooldownWorld)
			{
				LastSetNameServerTime = CooldownWorld->GetTimeSeconds();
			}
			MMONotify::ToOwner(this, TEXT("Bu ad kayitli bir karaktere ait: degistirilemedi. Kendi karakterin icin ?Name= ile yeniden baglan."), FColor::Orange, 6.f);
			return;
		}
	}
	if (CooldownWorld)
	{
		LastSetNameServerTime = CooldownWorld->GetTimeSeconds();
	}
	if (APlayerState* PS = GetPlayerState())
	{
		const FString OldName = PS->GetPlayerName();
		const FString OldSlot = GetSaveSlotName();
		PS->SetPlayerName(Trimmed);
		if (!OldName.Equals(Trimmed, ESearchCase::CaseSensitive))
		{
			UE_LOG(LogMMOIdentityAudit, Display,
				TEXT("event=name_change old_name=\"%s\" new_name=\"%s\" old_slot=\"%s\" new_slot=\"%s\" active_slot=\"%s\""),
				*OldName, *Trimmed, *OldSlot, *GetSaveSlotName(), *ActiveSaveSlot);
		}
		// Ad degisince arkadaslar eski adi cevrimdisi, yeni adi cevrimici gorsun (QA ORTA-2).
		if (FriendsComponent && IsPlayerControlled() && !OldName.Equals(Trimmed, ESearchCase::IgnoreCase))
		{
			FriendsComponent->AnnouncePresence(false, OldName);
			FriendsComponent->AnnouncePresence(true);
		}
	}
	MMONotify::ToOwner(this, bRemoteNameRules && !bNameAdmin
		? FString::Printf(TEXT("Adin '%s' olarak ayarlandi. Ilk kayittan (MMOSave ya da 60 sn otomatik kayit) sonra ad degistirilemez."), *Trimmed)
		: FString::Printf(TEXT("Adin '%s' olarak ayarlandi. Kalici kayit icin simdi MMOSave kullan."), *Trimmed),
		FColor(120, 200, 255), 3.f);
}

bool AMMOCharacter::UsesRemoteNameRules() const
{
#if !UE_BUILD_SHIPPING
	if (bTestRemoteNameRules)
	{
		return true;
	}
#endif
	return !IsLocallyControlled();
}

void AMMOCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	// Sahibi istemci: BP'nin ekledigi Enhanced Input mapping'lerini temizle.
	// (Tikla-git kontrol semasi AMMOPlayerController'da; eski WASD/fare-bak cakismasin.)
	ClearBPInputMappings();

	// Kamera uzakligini/acisini bu istemcide de uygula (controller artik hazir).
	ApplyCameraSettings();

	// BP'nin BeginPlay'i mapping'i BIZDEN SONRA tekrar ekleyebilir (net/possess sirasina gore);
	// bu yuzden ilk ~1.5 sn boyunca periyodik temizle. Aksi halde fare-bak RMB olmadan kamerayi cevirir.
	ClearMappingsAttempts = 0;
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(ClearMappingsTimerHandle, this,
			&AMMOCharacter::ClearBPInputMappings, 0.2f, true);
	}
}

void AMMOCharacter::ClearBPInputMappings()
{
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Subsystem->ClearAllMappings();
			}
		}
	}

	// ~1.5 sn (0.2 x 8) sonra timer'i durdur; BP BeginPlay'i coktan calismis olur.
	if (++ClearMappingsAttempts >= 8)
	{
		if (UWorld* W = GetWorld())
		{
			W->GetTimerManager().ClearTimer(ClearMappingsTimerHandle);
		}
	}
}

float AMMOCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	// Canlanma korumasi (sunucu-otoriter): koruma suresince hicbir kaynaktan (mob, PvP,
	// DoT) hasar islenmez. Moblarda bayrak hic set edilmez, onlari etkilemez.
	if (bSpawnProtected && HasAuthority())
	{
		return 0.f;
	}

	// Tum hasar yollari buradan gecer. Yeni bir PvP skill'i AttackTarget'in on
	// kontrolunu atlayip ApplyDamage cagirsa bile parti/klan/hub kurallari korunur.
	if (HasAuthority() && IsPlayerPawn(this))
	{
		AMMOCharacter* Attacker = EventInstigator
			? Cast<AMMOCharacter>(EventInstigator->GetPawn()) : nullptr;
		if (!Attacker)
		{
			Attacker = Cast<AMMOCharacter>(DamageCauser);
		}
		if (Attacker && Attacker != this && IsPlayerPawn(Attacker))
		{
			const TCHAR* BlockReason = nullptr;
			if (!Attacker->IsPvPAllowed() || !Attacker->CanHarmPlayer(this, BlockReason))
			{
				return 0.f;
			}
		}
	}

	const float BaseHandled = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (!HasAuthority() || !Stats)
	{
		return BaseHandled;
	}

	LastDamageInstigator = DamageCauser;
	// Oyuncunun mob uzerindeki DoT'u savunmayi atlar (UMMODotDamageType; DOT_SCALING_DESIGN.md A). Yalniz mob hedefte:
	// oyuncu -> oyuncu (PvP) ve mob -> oyuncu DoT'u eskisi gibi savunmadan dusulur.
	const bool bDotIgnoresDefense = Cast<AMMOMob>(this) != nullptr && DamageEvent.DamageTypeClass
		&& DamageEvent.DamageTypeClass->IsChildOf(UMMODotDamageType::StaticClass());
	const float ActualDamage = bDotIgnoresDefense
		? Stats->ApplyDamageIgnoringDefense(DamageAmount, DamageCauser)
		: Stats->ApplyDamage(DamageAmount, DamageCauser);

	if (ActualDamage > 0.f)
	{
		MulticastCombatText(GetActorLocation() + FVector(0.f, 0.f, 90.f), ActualDamage, false);
	}

	return ActualDamage;
}

void AMMOCharacter::MulticastCombatText_Implementation(FVector_NetQuantize WorldLocation, float Amount, bool bIsHeal)
{
	if (Amount <= 0.f || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UWorld* World = GetWorld();
	APlayerController* LocalPC = GEngine && World ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
	if (AMMOHUD* HUD = LocalPC ? Cast<AMMOHUD>(LocalPC->GetHUD()) : nullptr)
	{
		HUD->AddDamageNumber(WorldLocation, Amount, bIsHeal);
	}
}

void AMMOCharacter::HandleDeath()
{
	// Ortak ölüm mantığı (hareketi durdur, çarpışmayı kapat) — sunucuda çalışır,
	// görsel/animasyon için Blueprint olayını tetikleriz.
	OnCharacterDied();

	// Oyuncu: hareketi kilitle ve respawn geri sayimini baslat (moblar kendi HandleDeath'inde yok olur).
	if (IsPlayerControlled())
	{
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->StopMovementImmediately();
			Move->DisableMovement();
		}

		if (HasAuthority())
		{
			// Olum cezasi: bir miktar exp kaybi (KO tarzi).
			if (Stats)
			{
				const int32 Lost = Stats->ApplyDeathPenalty();

				// Olcum gunlugu: oyuncu olumu (by = son hasar veren mob arketipi, yoksa none).
				const AMMOMob* KillerMob = Cast<AMMOMob>(LastDamageInstigator.Get());
				if (AMMOPlayerController* PC = Cast<AMMOPlayerController>(GetController()); PC && PC->GetPawn() == this)
				{
					PC->RecordFirstDeathMetric(KillerMob ? KillerMob->ArchetypeId : NAME_None, Lost);
				}
				const FVector DeathLoc = GetActorLocation();
				UE_LOG(LogTemp, Display, TEXT("[MMOMETRIC] t=%.1f ev=death who=%s lv=%d x=%.0f y=%.0f z=%.0f by=%s xploss=%d"),
					MMOMetrics::Seconds(), *MMOMetrics::WhoName(this), Stats->Level, DeathLoc.X, DeathLoc.Y, DeathLoc.Z,
					KillerMob ? *KillerMob->ArchetypeId.ToString() : TEXT("none"), Lost);

				if (Lost > 0)
				{
					// Sunucu dali: uzak oyuncu da XP kaybini gorsun (B5).
					MMONotify::ToOwner(this, FString::Printf(TEXT("Oldun! -%d EXP"), Lost), FColor(255, 120, 120), 3.f);
				}
			}

			GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &AMMOCharacter::RespawnAtStart, RespawnDelay, false);
		}
	}
}

void AMMOCharacter::RespawnAtStart()
{
	// Baslangic noktasini bul (yoksa oldugu yerde diril).
	FVector SpawnLoc = GetActorLocation();
	FRotator SpawnRot = GetActorRotation();
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			SpawnLoc = It->GetActorLocation();
			SpawnRot = It->GetActorRotation();
			break;
		}
	}

	SetActorLocation(SpawnLoc, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(SpawnRot);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
	}

	if (Stats)
	{
		Stats->Revive();
	}
}

float AMMOCharacter::GetRespawnTimeRemaining() const
{
	if (!Stats || !Stats->bIsDead)
	{
		return 0.f;
	}
	return FMath::Max(0.f, GetWorldTimerManager().GetTimerRemaining(RespawnTimerHandle));
}

void AMMOCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (PlayerInputComponent)
	{
		// Enhanced Input hareket bindingi BP'de; bunlar sadece test tuslari (eklemeli).
		// Not: sol tik AMMOPlayerController'da (tikla-git/tikla-saldir); skill kitabi (K) da orada.
		// Hotbar: 1..9,0 tuslari -> slot 0..9. (Debug: MMODamage/MMOHeal/MMOGainXP konsol komutlari.)
		PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AMMOCharacter::UseHotbar0);
		PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AMMOCharacter::UseHotbar1);
		PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AMMOCharacter::UseHotbar2);
		PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AMMOCharacter::UseHotbar3);
		PlayerInputComponent->BindKey(EKeys::Five, IE_Pressed, this, &AMMOCharacter::UseHotbar4);
		PlayerInputComponent->BindKey(EKeys::Six, IE_Pressed, this, &AMMOCharacter::UseHotbar5);
		PlayerInputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &AMMOCharacter::UseHotbar6);
		PlayerInputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &AMMOCharacter::UseHotbar7);
		PlayerInputComponent->BindKey(EKeys::Nine, IE_Pressed, this, &AMMOCharacter::UseHotbar8);
		PlayerInputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &AMMOCharacter::UseHotbar9);

		// F = can iksiri kullan.
		PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AMMOCharacter::UseHealthPotion);
		PlayerInputComponent->BindKey(EKeys::G, IE_Pressed, this, &AMMOCharacter::UseManaPotion);
	}
}

void AMMOCharacter::MMOAttack()
{
	// En yakin dusmana vur (hedef null -> otomatik en yakin mob).
	AttackTarget(nullptr);
}

void AMMOCharacter::AttackTarget(AMMOCharacter* Target)
{
	AttackTargetInternal(Target, MeleeCooldown);
}

void AMMOCharacter::AttackTargetInternal(AMMOCharacter* Target, float RequiredCooldown)
{
	UWorld* World = GetWorld();
	if (!World || !Stats || Stats->bIsDead)
	{
		return;
	}
	if (MMOTargeting::IsInsideHubSafeZone(this, GetActorLocation()))
	{
		// Bu kontrol istemcide de calisir; uzak oyuncu sunucu yanitini beklemeden
		// neden saldiramadigini gorur. Tikla-saldir dongusu her karede cagirdigi icin
		// sabit pozitif anahtar kullanilir (ayni satir yenilenir, yigilmaz).
		if (GEngine && IsLocallyControlled())
		{
			GEngine->AddOnScreenDebugMessage(4101, 1.f, FColor::Silver, TEXT("Guvenli bolgede saldiri yok"));
		}
		// Oyuncu hedefi hub'da kovalanmaz (mob hedefi etkilenmez: cikinca saldirisi surer).
		if (Target && IsPlayerPawn(Target))
		{
			ReleaseAttackTargetOnOwner(Target);
		}
		return;
	}

	// Yerel cooldown kapisi (istemcide RPC spamini, sunucuda cift vurusu engeller).
	const double Now = World->GetTimeSeconds();
	if (Now - LastMeleeTime < RequiredCooldown)
	{
		return;
	}
	LastMeleeTime = Now;

	// Istemci: istegi hedefle sunucuya gonder (hasar sadece otoritede islenir).
	if (!HasAuthority())
	{
		// Istemci on kontrolu (yalniz gereksiz RPC'yi azaltir, karar yine sunucuda): PvP kapaliyken ya da
		// parti/klan/hub kurali yuzunden zaten reddedilecek oyuncu hedefi icin sunucuya hic gitme.
		// PvP durumu GameState'ten (replike ayna), parti/klan replike bilesenlerden okunur; yeni acilan PvP
		// birkac yuz ms gec gorunebilir. Reddedilen hedef zaten ReleaseAttackTarget ile birakilir.
		if (Target && IsPlayerPawn(Target))
		{
			const TCHAR* BlockReason = nullptr;
			const bool bPvPOff = !IsPvPAllowed();
			if (bPvPOff || !CanHarmPlayer(Target, BlockReason))
			{
				if (GEngine && IsLocallyControlled())
				{
					GEngine->AddOnScreenDebugMessage(4102, 1.f, FColor::Silver,
						bPvPOff ? TEXT("PvP kapali") : (BlockReason ? BlockReason : TEXT("Bu hedefe saldiramazsin")));
				}
				ReleaseAttackTargetOnOwner(Target);
				return;
			}
		}
		ServerAttackTarget(Target);
		return;
	}

	const bool bLocalFeedback = IsLocallyControlled();
	auto SendAttackFeedback = [this, bLocalFeedback](const FString& Text)
	{
		if (bLocalFeedback)
		{
			if (GEngine)
			{
				// Sabit anahtar: tikla-saldir dongusunde satir yigilmaz, yenilenir.
				GEngine->AddOnScreenDebugMessage(4102, 1.f, FColor::Silver, Text);
			}
			return;
		}

		// Listen-server'daki uzak pawn sunucuda locally controlled degildir.
		// Ret sebebi gecici ipucu olarak (karakter ustunde kisa balon) saldirana iletilir;
		// kalici sohbet satiri birakmaz. Ayni metin 3 sn icinde tekrar gonderilmez.
		const float FeedbackNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		if (Text == LastAttackFeedbackText && FeedbackNow - LastAttackFeedbackTime < 3.f)
		{
			return;
		}
		if (Cast<AMMOPlayerController>(GetController()))
		{
			LastAttackFeedbackText = Text;
			LastAttackFeedbackTime = FeedbackNow;
			ClientShowHint(Text);
		}
	};

	// Hedef gecersizse (yok/olu) en yakin canli mob'a dus.
	AMMOCharacter* Victim = Target;
	if (!Victim || !Victim->GetStats() || Victim->GetStats()->bIsDead)
	{
		Victim = MMOTargeting::FindNearestMob(this, GetActorLocation(), MeleeRange);
	}

	if (!Victim || Victim == this)
	{
		SendAttackFeedback(TEXT("Menzilde dusman yok - yaklas"));
		return;
	}

	// Menzil dogrulama (anti-cheat: istemci yalan soylemesin).
	if (FVector::Dist(GetActorLocation(), Victim->GetActorLocation()) > MeleeRange * 1.3f)
	{
		return;
	}

	// PvP kontrolu: hedef oyuncuysa PvP acik olmali.
	if (IsPlayerPawn(Victim) && !IsPvPAllowed())
	{
		SendAttackFeedback(TEXT("PvP kapali"));
		ReleaseAttackTargetOnOwner(Victim);
		return;
	}
	// Dostu-ates ve guvenli bolge kurallari (parti / klan / hub).
	if (const TCHAR* BlockReason = nullptr; IsPlayerPawn(Victim) && !CanHarmPlayer(Victim, BlockReason))
	{
		SendAttackFeedback(BlockReason ? FString(BlockReason) : TEXT("Bu hedefe saldiramazsin"));
		ReleaseAttackTargetOnOwner(Victim);
		return;
	}
	// Dogum korumali oyuncuya vurulamaz: vurus reddedilmeden saldirinin yan etkileri
	// (kendi korumasini bitirme / savas durumuna girme) UYGULANMAZ.
	if (IsPlayerPawn(Victim) && Victim->IsSpawnProtected())
	{
		SendAttackFeedback(TEXT("Hedef korumali"));
		ReleaseAttackTargetOnOwner(Victim);
		return;
	}

	// Saldiran oyuncu canlanma korumasini kaybeder (dokunulmazken vurmasin).
	EndSpawnProtection();
	// Uzak menzilden hasar almadan saldiran oyuncu da savastadir; sinif degistiremez.
	Stats->MarkCombatActivity();

	// All authoritative attack gates have passed. Capture level before a lethal hit can grant XP.
#if !UE_BUILD_SHIPPING
	if (AMMOPlayerController* PC = Cast<AMMOPlayerController>(GetController()))
	{
		const AMMOMob* MetricMob = Cast<AMMOMob>(Victim);
		PC->RecordFirstBasicAttackMetric(MetricMob ? FName(TEXT("mob")) : FName(TEXT("player")),
			MetricMob ? MetricMob->ArchetypeId : FName(TEXT("Player")),
			MetricMob ? MetricMob->GetMobLevel() : Victim->GetStats()->Level);
	}
#endif

	// Hedefe don ve vur.
	const FVector Dir = (Victim->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (!Dir.IsNearlyZero())
	{
		SetActorRotation(Dir.Rotation());
	}

	const float ActualDamage = UGameplayStatics::ApplyDamage(Victim, Stats->AttackPower, GetController(), this, nullptr);
	// Vurus animasyonu varyasyonunu sunucuda sec ki tum makineler ayni hareketi gorsun.
	MulticastMeleeFX(Victim->GetActorLocation() + FVector(0.f, 0.f, 40.f), FMath::RandRange(0, 2));

	if (Victim->GetStats())
	{
		const bool bKilled = Victim->GetStats()->bIsDead;
		const FString HitText = FString::Printf(TEXT("VURDUN: %.0f hasar | hedef HP %.0f/%.0f%s"),
			ActualDamage,
			Victim->GetStats()->Health, Victim->GetStats()->MaxHealth,
			bKilled ? TEXT("  >>> OLDU!") : TEXT(""));
		if (GEngine && bLocalFeedback)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, bKilled ? FColor::Red : FColor::Yellow, HitText);
		}
		else if (!bLocalFeedback && bKilled)
		{
			// Uzak istemciye yalniz OLDURUCU vurus (ToOwner sohbet satiri birakir; her vurusta
			// gondermek sohbeti doldururdu). Hasar sayilari/kritik geri bildirimi ayri yoldan gider.
			MMONotify::ToOwner(this, HitText, FColor::Red, 1.5f);
		}
	}
}

void AMMOCharacter::ServerAttackTarget_Implementation(AMMOCharacter* Target)
{
	if (!HasAuthority()) return;
	// Server-owned threshold; retain 10% jitter tolerance with one gate and actual time.
	// Rewriting Now-cooldown and checking again could reject an expired request by rounding.
	AttackTargetInternal(Target, MeleeCooldown * 0.9f);
}

bool AMMOCharacter::CanHarmPlayer(const AMMOCharacter* Victim, const TCHAR*& OutReason) const
{
	if (!IsPlayerPawn(Victim))
	{
		return true; // mob / oyuncu olmayan hedef: kurallar uygulanmaz
	}

	// Parti uyesine vurulmaz.
	if (PartyComponent)
	{
		for (const TObjectPtr<AMMOCharacter>& M : PartyComponent->Members)
		{
			if (M.Get() == Victim)
			{
				OutReason = TEXT("Parti uyesine vuramazsin");
				return false;
			}
		}
	}

	// Ayni klan uyesine vurulmaz.
	if (ClanComponent && Victim->GetClanComponent()
		&& ClanComponent->IsInClan()
		&& ClanComponent->ClanName.Equals(Victim->GetClanComponent()->ClanName, ESearchCase::IgnoreCase))
	{
		OutReason = TEXT("Klan uyesine vuramazsin");
		return false;
	}

	// Hub guvenli bolgesi: saldiran ya da hedef PlayerStart'a 700u icindeyse PvP yok.
	constexpr float SafeRadius = 700.f;
	if (MMOTargeting::IsInsideHubSafeZone(this, GetActorLocation(), SafeRadius)
		|| MMOTargeting::IsInsideHubSafeZone(this, Victim->GetActorLocation(), SafeRadius))
	{
		OutReason = TEXT("Guvenli bolgede PvP yok");
		return false;
	}
	return true;
}

bool AMMOCharacter::IsPvPAllowed() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AMMOGameMode* GM = World->GetAuthGameMode<AMMOGameMode>())
		{
			return GM->bAllowPvP; // sunucu/host: tek kaynak
		}
		// Istemcide GameMode yoktur: sunucunun replike ettigi aynayi oku (yalniz on kontrol/ipucu icin;
		// karari yine sunucu verir).
		if (const AMMOGameState* GS = World->GetGameState<AMMOGameState>())
		{
			return GS->IsPvPEnabled();
		}
	}
	return false;
}

void AMMOCharacter::MMOPvP(int32 bOn)
{
	if (!HasAuthority())
	{
		ServerSetPvP(bOn != 0);
		return;
	}

	if (AMMOGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMMOGameMode>() : nullptr)
	{
		GM->SetPvPAllowed(bOn != 0);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow,
				FString::Printf(TEXT("PvP %s"), GM->bAllowPvP ? TEXT("ACIK") : TEXT("KAPALI")));
		}
	}
}

void AMMOCharacter::ServerSetPvP_Implementation(bool bOn)
{
	// GUVENLIK: bAllowPvP TUM sunucu icin gecerli paylasilan bir kuraldir, kisisel
	// bir ayar degil. Bu kontrol olmadan HERHANGI BIR uzak istemci kendi konsoluna
	// "MMOPvP 1" yazip ev sahibinin onayi olmadan butun oyuncular icin PvP'yi
	// acabiliyordu (PvP'nin kasitli olarak kapali tutulmasi tasarim kararini bypass
	// eden ciddi bir acik). Sadece HOST'un (listen server'i baslatan oyuncunun)
	// karakteri bu RPC'yi kendi yerel controller'i uzerinden cagirabilir; uzak bir
	// istemcinin karakterinin sunucudaki controller'i asla "local" degildir.
	// Karar MMOAdmin::IsServerAdmin'de (dedicated'da simdilik yonetici yok).
	if (!MMOAdmin::IsServerAdmin(Cast<APlayerController>(GetController())))
	{
		return;
	}

	if (AMMOGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMMOGameMode>() : nullptr)
	{
		GM->SetPvPAllowed(bOn);
	}
}

void AMMOCharacter::MMOBuff(const FString& EffectId)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	const FName Id(*EffectId);
	if (HasAuthority())
	{
		if (Stats)
		{
			Stats->ApplyEffect(Id, this);
		}
	}
	else
	{
		ServerApplyBuff(Id);
	}
}

void AMMOCharacter::ServerApplyBuff_Implementation(FName EffectId)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	// Hata ayiklama komutu: uzak istemci kendi RPC'sini dogrudan cagirip buff alamaz.
	if (!MMOAdmin::IsServerAdmin(Cast<APlayerController>(GetController())))
	{
		return;
	}

	if (Stats)
	{
		Stats->ApplyEffect(EffectId, this);
	}
}

void AMMOCharacter::MMOSpawnMob(const FString& ArchetypeId)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	const FName Id(*ArchetypeId);
	if (HasAuthority())
	{
		ServerSpawnMob_Implementation(Id);
	}
	else
	{
		ServerSpawnMob(Id);
	}
}

void AMMOCharacter::MMOTeleport(float X, float Y, float Z)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	const FVector Target(X, Y, Z);
	if (HasAuthority())
	{
		ServerTeleport_Implementation(Target);
	}
	else
	{
		ServerTeleport(Target);
	}
}

void AMMOCharacter::ServerTeleport_Implementation(FVector Location)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	// Yalniz sunucu yoneticisi isinlanabilir (MMOSpawnMob ile ayni kapi); olu karakter isinlanmaz.
	if (!MMOAdmin::IsServerAdmin(Cast<APlayerController>(GetController())) || !Stats || Stats->bIsDead)
	{
		return;
	}
	// Makul harita sinirlari (NaN/devasa deger dunya disina atmasin); Winter navmesh en fazla +-8500.
	if (Location.ContainsNaN() || FMath::Abs(Location.X) > 20000.f || FMath::Abs(Location.Y) > 20000.f
		|| Location.Z < -5000.f || Location.Z > 20000.f)
	{
		return;
	}
	const bool bMoved = TeleportTo(Location, GetActorRotation(), /*bIsATest=*/false, /*bNoCheck=*/true);
	UE_LOG(LogTemp, Display, TEXT("[MMOTELEPORT] %s -> (%.0f,%.0f,%.0f) moved=%d now=(%.0f,%.0f,%.0f)"), *GetName(),
		Location.X, Location.Y, Location.Z, bMoved ? 1 : 0, GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);
}

void AMMOCharacter::MMOInvite()
{
	if (PartyComponent)
	{
		PartyComponent->RequestInviteNearest();
	}
}

void AMMOCharacter::MMOPartyAccept()
{
	if (PartyComponent)
	{
		PartyComponent->RequestAccept();
	}
}

void AMMOCharacter::MMOPartyDecline()
{
	if (PartyComponent)
	{
		PartyComponent->RequestDecline();
	}
}

void AMMOCharacter::MMOPartyLeave()
{
	if (PartyComponent)
	{
		PartyComponent->RequestLeave();
	}
}

void AMMOCharacter::MMOPartyKick()
{
	if (!PartyComponent)
	{
		return;
	}

	// Hedef = oyuncunun secili tuttugu karakter (Z / sol tik ile secilen).
	AMMOPlayerController* MPC = Cast<AMMOPlayerController>(GetController());
	AMMOCharacter* Target = MPC ? MPC->GetSelectedTarget() : nullptr;
	if (!Target || Target == this)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Once cikarilacak parti uyesini sec"));
		}
		return;
	}

	// Yetki/uyelik dogrulamasini sunucu yapar; buradaki kontrol sadece erken geri bildirim.
	if (!PartyComponent->Members.Contains(Target))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Secili oyuncu partinde degil"));
		}
		return;
	}

	PartyComponent->RequestKick(Target);
}

void AMMOCharacter::MMOTrade()
{
	if (TradeComponent)
	{
		TradeComponent->RequestInviteNearest();
	}
}

void AMMOCharacter::MMOTradeAccept()
{
	if (TradeComponent)
	{
		TradeComponent->RequestAccept();
	}
}

void AMMOCharacter::MMOTradeCancel()
{
	if (TradeComponent)
	{
		TradeComponent->RequestCancel();
	}
}

void AMMOCharacter::MMOTradeGold(int32 Amount)
{
	if (TradeComponent)
	{
		TradeComponent->RequestSetGold(Amount);
	}
}

void AMMOCharacter::MMOTradeConfirm()
{
	if (TradeComponent)
	{
		TradeComponent->RequestConfirm();
	}
}

namespace
{
	// "STR"/"str"/"strength" -> "Strength" (SpendStatPoint'in bekledigi ad).
	FName MapMMOStatName(FName In)
	{
		const FString L = In.ToString().ToLower();
		if (L == TEXT("str") || L == TEXT("strength"))     { return FName(TEXT("Strength")); }
		if (L == TEXT("dex") || L == TEXT("dexterity"))    { return FName(TEXT("Dexterity")); }
		if (L == TEXT("int") || L == TEXT("intelligence")) { return FName(TEXT("Intelligence")); }
		if (L == TEXT("vit") || L == TEXT("vitality"))     { return FName(TEXT("Vitality")); }
		return NAME_None;
	}
}

void AMMOCharacter::RequestSpendStat(FName StatName)
{
	const FName Mapped = MapMMOStatName(StatName);
	if (Mapped.IsNone())
	{
		return;
	}
	if (HasAuthority())
	{
		if (Stats)
		{
			Stats->SpendStatPoint(Mapped, 1);
		}
	}
	else
	{
		ServerSpendStat(Mapped);
	}
}

void AMMOCharacter::ServerSpendStat_Implementation(FName StatName)
{
	if (Stats)
	{
		Stats->SpendStatPoint(StatName, 1); // zaten eslendi
	}
}

void AMMOCharacter::MMOStat(const FString& StatName)
{
	RequestSpendStat(FName(*StatName));
}

void AMMOCharacter::ClientNotifyXPGain_Implementation(int32 Amount, int32 Percent)
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	AMMOHUD* HUD = PC ? Cast<AMMOHUD>(PC->GetHUD()) : nullptr;
	if (!HUD || Amount <= 0)
	{
		return;
	}
	const FString Label = Percent < 100
		? FString::Printf(TEXT("+%d EXP (%%%d)"), Amount, Percent)
		: FString::Printf(TEXT("+%d EXP"), Amount);
	HUD->AddFloatingLabel(GetActorLocation() + FVector(0.f, 0.f, 130.f), Label);
}

void AMMOCharacter::ReleaseAttackTargetOnOwner(AMMOCharacter* Rejected)
{
	AMMOPlayerController* PC = Cast<AMMOPlayerController>(GetController());
	if (!PC || !Rejected)
	{
		return;
	}
	if (IsLocallyControlled())
	{
		PC->ReleaseAttackTarget(Rejected);
	}
	else if (HasAuthority())
	{
		// Uzak istemci: tikla-kovala dongusu sunucuya her 0,6 sn RPC atmasin diye hedefi biraktir.
		// Unreliable: kaybolursa istemci bir sonraki vurusta yine reddedilir ve yeniden gonderilir.
		ClientReleaseAttackTarget(Rejected);
	}
}

void AMMOCharacter::ClientReleaseAttackTarget_Implementation(AMMOCharacter* Rejected)
{
	if (AMMOPlayerController* PC = Cast<AMMOPlayerController>(GetController()))
	{
		PC->ReleaseAttackTarget(Rejected);
	}
}

void AMMOCharacter::ClientShowHint_Implementation(const FString& Text)
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	AMMOHUD* HUD = PC ? Cast<AMMOHUD>(PC->GetHUD()) : nullptr;
	if (!HUD || Text.IsEmpty())
	{
		return;
	}
	HUD->AddFloatingLabel(GetActorLocation() + FVector(0.f, 0.f, 170.f), Text);
}

void AMMOCharacter::MMONpcTalk()
{
	// Gelistirici LLM testi: Shipping'de ucretli LLM istegi atilmasin (SECURITY_AUDIT Y-6 / Y-4).
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	// Bu Exec komutu client-side calisir (RPC degil), o yuzden kendi cooldown'unu
	// tutar — yoksa konsoldan spamlanirsa sinirsiz eszamanli/ucretli LLM istegi atilir.
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	if (Now - LastNpcTalkDebugTime < NpcTalkDebugCooldown)
	{
		return;
	}
	LastNpcTalkDebugTime = Now;

	UGameInstance* GI = GetGameInstance();
	UMMODialogueSubsystem* Dialogue = GI ? GI->GetSubsystem<UMMODialogueSubsystem>() : nullptr;
	if (!Dialogue)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MMONpcTalk] Dialogue subsystem yok."));
		return;
	}

	// Oyuncu baglami (seviye) — replige renk katsin.
	FString PlayerInfo = TEXT("bir maceraci");
	if (Stats)
	{
		PlayerInfo = FString::Printf(TEXT("seviye %d bir maceraci"), Stats->Level);
	}

	FMMOOnDialogueReady OnReady;
	OnReady.BindLambda([](bool bSuccess, const FString& Text)
	{
		UE_LOG(LogTemp, Display, TEXT("[NPC] %s"), *Text);
		if (GEngine)
		{
			const FColor Col = bSuccess ? FColor::Green : FColor::Orange;
			GEngine->AddOnScreenDebugMessage(-1, 8.f, Col, FString::Printf(TEXT("NPC: %s"), *Text));
		}
	});

	Dialogue->RequestNpcLine(
		TEXT("Satici Merek"),
		TEXT("kasabanin aksi ama durust silah saticisi"),
		PlayerInfo,
		OnReady,
		GetPlayerState() ? GetPlayerState()->GetPlayerName() : FString());
}

void AMMOCharacter::ServerSpawnMob_Implementation(FName ArchetypeId)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	// Sunucu yoneticisi disindakiler mob olusturup XP/ganimet uretemez.
	if (!MMOAdmin::IsServerAdmin(Cast<APlayerController>(GetController())))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Oyuncunun ~3m onune, hafif yukariya spawn'la.
	const FVector Loc = GetActorLocation() + GetActorForwardVector() * 300.f + FVector(0.f, 0.f, 60.f);
	const FTransform SpawnTM(FRotator::ZeroRotator, Loc);

	if (AMMOMob* Mob = World->SpawnActorDeferred<AMMOMob>(AMMOMob::StaticClass(), SpawnTM))
	{
		// ArchetypeId'yi BeginPlay'den once ata ki ApplyArchetype dogru arketipi kullansin.
		Mob->ArchetypeId = ArchetypeId;
		UGameplayStatics::FinishSpawningActor(Mob, SpawnTM);
	}
}

void AMMOCharacter::MulticastMeleeFX_Implementation(FVector ImpactLocation, int32 AnimIndex)
{
	// Vurus animasyonu (her makinede yerel oynatilir; sunucu hangi varyasyon oldugunu bildirir).
	PlayAttackAnimLocal(AnimIndex);

	if (UWorld* World = GetWorld())
	{
		// Kisa kirmizi darbe patlamasi (her makinede yerel cizilir).
		DrawDebugSphere(World, ImpactLocation, 35.f, 8, FColor(255, 90, 60), false, 0.2f, 0, 2.5f);
	}
}

void AMMOCharacter::MMODamage(float Amount)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	// Standart hasar akisindan gecir: TakeDamage -> Stats->ApplyDamage.
	UGameplayStatics::ApplyDamage(this, Amount, GetController(), this, nullptr);
}

void AMMOCharacter::MMOHeal(float Amount)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	if (Stats)
	{
		Stats->Heal(Amount);
	}
}

void AMMOCharacter::MMOGainXP(int32 Amount)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	if (Stats)
	{
		Stats->GainExperience(Amount);
	}
}

void AMMOCharacter::MMOUseMana(float Amount)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	if (Stats)
	{
		Stats->ConsumeMana(Amount);
	}
}

void AMMOCharacter::UseHotbar0() { if (SkillComponent) { SkillComponent->UseHotbarSlot(0); } }
void AMMOCharacter::UseHotbar1() { if (SkillComponent) { SkillComponent->UseHotbarSlot(1); } }
void AMMOCharacter::UseHotbar2() { if (SkillComponent) { SkillComponent->UseHotbarSlot(2); } }
void AMMOCharacter::UseHotbar3() { if (SkillComponent) { SkillComponent->UseHotbarSlot(3); } }
void AMMOCharacter::UseHotbar4() { if (SkillComponent) { SkillComponent->UseHotbarSlot(4); } }
void AMMOCharacter::UseHotbar5() { if (SkillComponent) { SkillComponent->UseHotbarSlot(5); } }
void AMMOCharacter::UseHotbar6() { if (SkillComponent) { SkillComponent->UseHotbarSlot(6); } }
void AMMOCharacter::UseHotbar7() { if (SkillComponent) { SkillComponent->UseHotbarSlot(7); } }
void AMMOCharacter::UseHotbar8() { if (SkillComponent) { SkillComponent->UseHotbarSlot(8); } }
void AMMOCharacter::UseHotbar9() { if (SkillComponent) { SkillComponent->UseHotbarSlot(9); } }

bool AMMOCharacter::TryBeginConsumableUse()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float Now = World->GetTimeSeconds();
	// Uzak istemcinin RPC'lerini sunucuda yargilarken kapi biraz KISA tutulur: istemci ve sunucu damgalari
	// bagimsizdir, ag titresimi (RPC1 gec, RPC2 erken) ~1 sn araliktaki mesru ikinci kullanimi sessizce
	// yutmasin (istemci kendi kilidini zaten harcamis olur). Host/yerel ve istemci tarafi tam sure kullanir.
	const float Cooldown = (HasAuthority() && !IsLocallyControlled()) ? ConsumableUseCooldownServerRemote : ConsumableUseCooldown;
	if (Now - LastConsumableUseTime < Cooldown)
	{
		// Sessiz ret yerine ipucu (ayni metin 0.5 sn icinde tekrar gonderilmez; uzakta sahibine gider).
		MMONotify::ToOwner(this, TEXT("Esya kullanimi bekleniyor"), FColor::Silver, 1.f, MMONotify::EMMONotifyKind::Hint);
		return false;
	}

	LastConsumableUseTime = Now;
	return true;
}

void AMMOCharacter::UseHealthPotion()
{
	static const FName PotionId(TEXT("HealthPotion"));

	if (!Stats || !Inventory || Stats->bIsDead)
	{
		return;
	}

	// On kontroller replike verilerle her iki tarafta da yapilabilir. ToOwner: yerelde ekran, sunucuda
	// (RPC ile gelen uzak istemci) sahibe hint; eskiden IsLocallyControlled yuzunden uzakta kayboluyordu.
	if (Stats->Health >= Stats->MaxHealth)
	{
		MMONotify::ToOwner(this, TEXT("Canin zaten dolu"), FColor::Silver, 1.5f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	if (Inventory->GetItemCount(PotionId) <= 0)
	{
		MMONotify::ToOwner(this, TEXT("Iksir yok!"), FColor::Silver, 1.5f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	// Hem sahip istemcide RPC kuyrugunu hem de otoritede dogrudan RPC tekrarlarini
	// sinirla. Basarisiz on kontroller damgayi ilerletmez; gecerli tuketilebilirlerin
	// hepsi ayni 1 sn oyun dengesi kapisini paylasir.
	if (!TryBeginConsumableUse())
	{
		return;
	}

	// Istemci: sunucuya istek gonder (envanter/heal sadece otoritede degisir).
	if (!HasAuthority())
	{
		ServerUseHealthPotion();
		return;
	}

	if (Inventory->RemoveItem(PotionId, 1))
	{
		Stats->Heal(PotionHealAmount);
		// Otorite dali (istemci yukarida RPC'ye doner): uzak istemciye de ToOwner ile (B5).
		MMONotify::ToOwner(this,
			FString::Printf(TEXT("Iksir icildi: +%.0f HP (kalan %d)"), PotionHealAmount, Inventory->GetItemCount(PotionId)),
			FColor(255, 80, 80), 2.f);
	}
}

void AMMOCharacter::ServerUseHealthPotion_Implementation()
{
	UseHealthPotion();
}

void AMMOCharacter::UseManaPotion()
{
	UseConsumable(FName(TEXT("ManaPotion")));
}

void AMMOCharacter::MMOUse(const FString& ItemId)
{
	UseConsumable(FName(*ItemId));
}

void AMMOCharacter::UseConsumable(FName ItemId)
{
	if (!Stats || !Inventory || Stats->bIsDead)
	{
		return;
	}

	const FMMOConsumableDef* Def = FMMOConsumableDatabase::Find(ItemId);
	if (!Def)
	{
		return;
	}

	if (Inventory->GetItemCount(ItemId) <= 0)
	{
		MMONotify::ToOwner(this, FString::Printf(TEXT("%s yok!"), *Def->Name), FColor::Silver, 1.5f, MMONotify::EMMONotifyKind::Hint);
		return;
	}
	// Saf iksir hic can/mana yenileyemiyorsa esyayi ve ortak cooldown'u harcama.
	// Etki parsomenleri tam can/mana ile de kullanilabilir (buff yenileme).
	const bool bCanRestoreHealth = Def->Heal > 0.f && Stats->Health < Stats->MaxHealth;
	const bool bCanRestoreMana = Def->ManaRestore > 0.f && Stats->Mana < Stats->MaxMana;
	if (Def->EffectId.IsNone() && !bCanRestoreHealth && !bCanRestoreMana)
	{
		MMONotify::ToOwner(this, TEXT("Can/mana zaten dolu"), FColor::Silver, 1.5f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	if (!TryBeginConsumableUse())
	{
		return;
	}

	// Istemci: istegi sunucuya gonder (esya harcamasi + etki sadece otoritede).
	if (!HasAuthority())
	{
		ServerUseConsumable(ItemId);
		return;
	}

	if (Inventory->RemoveItem(ItemId, 1))
	{
		if (Def->Heal > 0.f)
		{
			Stats->Heal(Def->Heal);
		}
		if (Def->ManaRestore > 0.f)
		{
			Stats->RestoreMana(Def->ManaRestore);
		}
		if (!Def->EffectId.IsNone())
		{
			Stats->ApplyEffect(Def->EffectId, this);
		}

		MMONotify::ToOwner(this,
			FString::Printf(TEXT("Kullanildi: %s (kalan %d)"), *Def->Name, Inventory->GetItemCount(ItemId)),
			FColor(120, 200, 255), 2.f);
	}
}

void AMMOCharacter::ServerUseConsumable_Implementation(FName ItemId)
{
	UseConsumable(ItemId);
}

void AMMOCharacter::MMOGiveItem(const FString& ItemId, int32 Count)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	const FName Id(*ItemId);
	const int32 C = FMath::Max(1, Count);
	if (HasAuthority())
	{
		if (Inventory)
		{
			Inventory->AddItem(Id, C);
		}
	}
	else
	{
		ServerGiveItem(Id, C);
	}
}

void AMMOCharacter::ServerGiveItem_Implementation(FName ItemId, int32 Count)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	// Debug item komutunu yalnizca sunucu yoneticisi (host) kullanabilir; sayi siniri tek basina yeterli degil.
	if (!MMOAdmin::IsServerAdmin(Cast<APlayerController>(GetController())))
	{
		return;
	}

	// GUVENLIK (qa-kontrolcu bulgusu): Count sadece MMOGiveItem'in istemci-tarafi
	// yolunda clamp'leniyordu (FMath::Max(1,Count)) - bu RPC gövdesi ise ham geleni
	// dogrudan kullaniyordu. Hileli bir istemci ServerGiveItem'i DOGRUDAN (istemci
	// fonksiyonunu hic cagirmadan) devasa/negatif bir Count ile cagirabilirdi:
	// Items dizisindeki Stack.Count int32'yi tasip negatife donerdi, o item o
	// oyuncu icin KALICI OLARAK harcanamaz hale gelirdi. Ustten de sinirla
	// (BuyPotion/BuyConsumable'daki ayni desen).
	if (Inventory)
	{
		Inventory->AddItem(ItemId, FMath::Clamp(Count, 1, 999));
	}
}

void AMMOCharacter::MMOClass(const FString& ClassName)
{
	// Konsol kisayolu = gelistirici araci (SECURITY_AUDIT Y-6); oyuncu sinif secimi HUD panelinden
	// (RequestSetupForClass) yapilir ve Shipping'de de calisir.
	if (!MMODebugCommandsEnabled() || !SkillComponent)
	{
		return;
	}

	const FString C = ClassName.TrimStartAndEnd().ToLower();
	EMMOClass NewClass = EMMOClass::Warrior;
	if (C == TEXT("warrior") || C == TEXT("savasci") || C == TEXT("savaşçı"))
	{
		NewClass = EMMOClass::Warrior;
	}
	else if (C == TEXT("archer") || C == TEXT("okcu") || C == TEXT("okçu"))
	{
		NewClass = EMMOClass::Archer;
	}
	else if (C == TEXT("mage") || C == TEXT("buyucu") || C == TEXT("büyücü"))
	{
		NewClass = EMMOClass::Mage;
	}
	else if (C == TEXT("priest") || C == TEXT("rahip"))
	{
		NewClass = EMMOClass::Priest;
	}
	else
	{
		if (GEngine && IsLocallyControlled())
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Silver,
				TEXT("Gecersiz sinif. Kullanim: MMOClass savasci|okcu|buyucu|rahip"));
		}
		return;
	}

	SkillComponent->RequestSetupForClass(NewClass);
}

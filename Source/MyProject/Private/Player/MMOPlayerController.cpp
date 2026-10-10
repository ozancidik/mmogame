// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/MMOPlayerController.h"
#include "Characters/MMOCharacter.h"
#include "Characters/MMOMob.h"
#include "World/MMOVendorNPC.h"
#include "World/MMOQuestGiverNPC.h"
#include "World/MMOTalkerNPC.h"
#include "World/MMOHealerNPC.h"
#include "World/MMOWarehouseNPC.h"
#include "AI/MMODialogueSubsystem.h"
#include "Components/MMOStatsComponent.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOSkillComponent.h"
#include "Skills/MMOSkillDatabase.h"
#include "Items/MMOItemDatabase.h"
#include "Engine/GameInstance.h"
#include "Core/MMOTargeting.h"
#include "Core/MMOGameMode.h"
#include "Core/MMOAdmin.h"
#include "Core/MMOMetrics.h"
#include "Core/MMONotify.h"
#include "UI/MMOHUD.h"
#include "Components/InputComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NavigationSystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/MMOPartyComponent.h"
#include "Components/MMOTradeComponent.h"
#include "Components/MMOClanComponent.h"
#include "Components/MMOFriendsComponent.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#if !UE_BUILD_SHIPPING
#include "Save/MMOCharacterSaveJson.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#endif

AMMOPlayerController::AMMOPlayerController()
{
	// OS imlecini gizle (kendi imlecimizi HUD'da ciziyoruz) ama fare takibini surdur.
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::None;
}

void AMMOPlayerController::MMOTravel()
{
	// Uzak istemci bu komutla tum oturumu tasiyamaz. Yonetici kontrolu GameMode'da da var.
	if (!MMOAdmin::IsServerAdmin(this))
	{
		return;
	}
	AMMOGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMMOGameMode>() : nullptr;
	FString FailureReason;
	if (!GM || !GM->RequestZoneTravel(this, FailureReason))
	{
		if (FailureReason.IsEmpty())
		{
			FailureReason = TEXT("Oyun modu gecis icin hazir degil");
		}
		UE_LOG(LogTemp, Warning, TEXT("MMOTravel rejected: %s"), *FailureReason);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FailureReason);
		}
	}
}

void AMMOPlayerController::BeginPlay()
{
	Super::BeginPlay();
	#if !UE_BUILD_SHIPPING
	if (HasAuthority() && !FirstActionMetricsSession.IsValid())
	{
		FirstActionMetricsSession = FGuid::NewGuid();
		FirstActionMetricsStart = MMOMetrics::Seconds();
	}
	#endif

	// OS imleci gizli ama fare serbest ve konumu takip edilir; kendi imlecimizi HUD cizer.
	bShowMouseCursor = true;
	CurrentMouseCursor = EMouseCursor::None;
}

void AMMOPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AMMOPlayerController::OnLeftClickPressed);
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AMMOPlayerController::OnLeftClickReleased);
		InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AMMOPlayerController::OnRightMousePressed);
		InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AMMOPlayerController::OnRightMouseReleased);
		InputComponent->BindAxisKey(EKeys::MouseX, this, &AMMOPlayerController::OnMouseX);
		InputComponent->BindAxisKey(EKeys::MouseY, this, &AMMOPlayerController::OnMouseY);
		// K = skill kitabi, I = envanter.
		InputComponent->BindKey(EKeys::K, IE_Pressed, this, &AMMOPlayerController::OnToggleSkillBook);
		InputComponent->BindKey(EKeys::I, IE_Pressed, this, &AMMOPlayerController::OnToggleInventory);
		InputComponent->BindKey(EKeys::C, IE_Pressed, this, &AMMOPlayerController::OnToggleCharacter);

		// Z = en yakin dusmani sec, R = saldiri ac/kapa (KO tarzi).
		InputComponent->BindKey(EKeys::Z, IE_Pressed, this, &AMMOPlayerController::OnTargetNearestEnemy);
		InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AMMOPlayerController::OnToggleAttack);

		// Q / E = kamerayi sola / saga cevir (touchpad dostu; PlayerTick'te islenir).
		// (Eski E=dukkani kapat kaldirildi; artik pencereler sag ustteki X ile kapatiliyor.)

		// Space = ziplama.
		InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AMMOPlayerController::OnJumpPressed);
		InputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &AMMOPlayerController::OnJumpReleased);

		// Enter = sohbet yazi kutusu.
		InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AMMOPlayerController::OnChatKey);
	}
}

void AMMOPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseChatInput();
	Super::EndPlay(EndPlayReason);
}

void AMMOPlayerController::SeamlessTravelTo(APlayerController* NewPC)
{
	// Seamless travel'da kontrolcu EndPlay almayabilir; acik sohbet kutusu yeni haritada
	// asili kalip hareketi kilitlemesin.
	CloseChatInput();
	Super::SeamlessTravelTo(NewPC);
#if !UE_BUILD_SHIPPING
	if (HasAuthority() && FirstActionMetricsSession.IsValid())
	{
		if (AMMOPlayerController* MMO = Cast<AMMOPlayerController>(NewPC))
		{
			MMO->FirstActionMetricsSession = FirstActionMetricsSession;
			MMO->FirstActionMetricsStart = FirstActionMetricsStart;
			MMO->RecordedFirstActionMetrics = RecordedFirstActionMetrics;
		}
	}
#endif
}

bool AMMOPlayerController::RecordFirstQuestMetric(FName Action, FName QuestId)
{
#if !UE_BUILD_SHIPPING
	if (QuestId.IsNone() || (Action != TEXT("quest_accept") && Action != TEXT("quest_claim"))) return false;
	return RecordFirstActionMetric(Action, FString::Printf(TEXT("quest=%s"), *QuestId.ToString()));
#else
	static_cast<void>(Action); static_cast<void>(QuestId); return false;
#endif
}

bool AMMOPlayerController::RecordFirstLootMetric(FName Kind, FName ItemId, int32 Amount)
{
#if !UE_BUILD_SHIPPING
	if (ItemId.IsNone() || Amount <= 0 || (Kind != TEXT("gold") && Kind != TEXT("item") && Kind != TEXT("equipment"))
		|| (Kind == TEXT("equipment") && Amount != 1) || (Kind == TEXT("gold") && ItemId != TEXT("Gold"))) return false;
	return RecordFirstActionMetric(TEXT("loot"), FString::Printf(TEXT("kind=%s item=%s amount=%d"), *Kind.ToString(), *ItemId.ToString(), Amount));
#else
	static_cast<void>(Kind); static_cast<void>(ItemId); static_cast<void>(Amount); return false;
#endif
}

bool AMMOPlayerController::RecordFirstKillMetric(FName MobId, int32 MobLevel, int32 XP, FName Credit, bool bElite)
{
#if !UE_BUILD_SHIPPING
	const AMMOCharacter* MetricPawn = Cast<AMMOCharacter>(GetPawn());
	if (MobId.IsNone() || MobLevel < 1 || XP < 0 || !MetricPawn || !MetricPawn->GetStats() || MetricPawn->GetStats()->bIsDead
		|| (Credit != TEXT("fatal") && Credit != TEXT("contribution") && Credit != TEXT("party") && Credit != TEXT("fallback"))) return false;
	return RecordFirstActionMetric(TEXT("kill"), FString::Printf(TEXT("mob=%s mlv=%d xp=%d credit=%s elite=%d"),
		*MobId.ToString(), MobLevel, XP, *Credit.ToString(), bElite ? 1 : 0));
#else
	static_cast<void>(MobId); static_cast<void>(MobLevel); static_cast<void>(XP); static_cast<void>(Credit); static_cast<void>(bElite); return false;
#endif
}

bool AMMOPlayerController::RecordFirstEquipMetric(FName ItemId, int32 InstanceId, int32 Slot)
{
#if !UE_BUILD_SHIPPING
	const FMMOItemDef* Def = FMMOItemDatabase::Find(ItemId);
	if (!Def || InstanceId <= 0 || Slot < 0 || Slot >= static_cast<int32>(EMMOEquipSlot::MAX)
		|| Slot != static_cast<int32>(Def->Slot)) return false;
	return RecordFirstActionMetric(TEXT("equip"), FString::Printf(TEXT("item=%s instance=%d slot=%d"), *ItemId.ToString(), InstanceId, Slot));
#else
	static_cast<void>(ItemId); static_cast<void>(InstanceId); static_cast<void>(Slot); return false;
#endif
}

bool AMMOPlayerController::RecordFirstSkillMetric(FName SkillId)
{
#if !UE_BUILD_SHIPPING
	const FMMOSkillDef* Def = FMMOSkillDatabase::Find(SkillId);
	const AMMOCharacter* MetricPawn = Cast<AMMOCharacter>(GetPawn());
	const UMMOSkillComponent* Skills = MetricPawn ? MetricPawn->GetSkillComponent() : nullptr;
	if (!Def || !Skills || !MetricPawn->GetStats() || MetricPawn->GetStats()->bIsDead
		|| !Skills->IsLearned(SkillId) || Skills->CharacterClass != Def->RequiredClass
		|| MetricPawn->GetStats()->Level < Def->RequiredLevel || !FMath::IsFinite(Def->ManaCost) || Def->ManaCost < 0.f) return false;
	return RecordFirstActionMetric(TEXT("skill"), FString::Printf(TEXT("skill=%s effect=%d mana=%.3f"),
		*Def->SkillId.ToString(), static_cast<int32>(Def->Effect), Def->ManaCost));
#else
	static_cast<void>(SkillId); return false;
#endif
}

bool AMMOPlayerController::RecordFirstBasicAttackMetric(FName TargetKind, FName TargetId, int32 TargetLevel)
{
#if !UE_BUILD_SHIPPING
	const AMMOCharacter* MetricPawn = Cast<AMMOCharacter>(GetPawn());
	if (!MetricPawn || !MetricPawn->GetStats() || MetricPawn->GetStats()->bIsDead || TargetId.IsNone() || TargetLevel < 1
		|| (TargetKind != TEXT("mob") && TargetKind != TEXT("player"))
		|| (TargetKind == TEXT("player") && TargetId != TEXT("Player"))) return false;
	return RecordFirstActionMetric(TEXT("basic_attack"), FString::Printf(TEXT("target=%s targetid=%s tlv=%d"),
		*TargetKind.ToString(), *TargetId.ToString(), TargetLevel));
#else
	static_cast<void>(TargetKind); static_cast<void>(TargetId); static_cast<void>(TargetLevel); return false;
#endif
}

bool AMMOPlayerController::RecordFirstDeathMetric(FName KillerMobId, int32 XPLoss)
{
#if !UE_BUILD_SHIPPING
	const AMMOCharacter* MetricPawn = Cast<AMMOCharacter>(GetPawn());
	const UMMOStatsComponent* Stats = MetricPawn ? MetricPawn->GetStats() : nullptr;
	if (!Stats || !Stats->bIsDead || !FMath::IsFinite(Stats->Health) || Stats->Health != 0.f || XPLoss < 0) return false;
	return RecordFirstActionMetric(TEXT("death"), FString::Printf(TEXT("by=%s xploss=%d"),
		KillerMobId.IsNone() ? TEXT("none") : *KillerMobId.ToString(), XPLoss));
#else
	static_cast<void>(KillerMobId); static_cast<void>(XPLoss); return false;
#endif
}

bool AMMOPlayerController::RecordFirstLevelUpMetric(int32 PreviousLevel)
{
#if !UE_BUILD_SHIPPING
	const AMMOCharacter* MetricPawn = Cast<AMMOCharacter>(GetPawn());
	const UMMOStatsComponent* Stats = IsValid(MetricPawn) ? MetricPawn->GetStats() : nullptr;
	if (!IsValid(Stats) || Stats->bIsDead || PreviousLevel < 1 || PreviousLevel >= 80
		|| Stats->Level != PreviousLevel + 1 || Stats->Level > Stats->MaxLevel) return false;
	return RecordFirstActionMetric(TEXT("levelup"), FString::Printf(TEXT("from=%d to=%d"), PreviousLevel, Stats->Level));
#else
	static_cast<void>(PreviousLevel); return false;
#endif
}

bool AMMOPlayerController::RecordFirstMoveMetric(FName MovementMode, double Distance)
{
#if !UE_BUILD_SHIPPING
	const AMMOCharacter* MetricPawn = Cast<AMMOCharacter>(GetPawn());
	const UMMOStatsComponent* Stats = IsValid(MetricPawn) ? MetricPawn->GetStats() : nullptr;
	if (!IsValid(Stats) || Stats->bIsDead || !FMath::IsFinite(Stats->Health) || Stats->Health <= 0.f
		|| Stats->Level < 1 || Stats->Level > 80 || !FMath::IsFinite(Distance) || Distance < 1.0
		|| (MovementMode != TEXT("walking") && MovementMode != TEXT("navwalking") && MovementMode != TEXT("falling"))) return false;
	return RecordFirstActionMetric(TEXT("move"), FString::Printf(TEXT("mode=%s distance=%.3f"), *MovementMode.ToString(), Distance));
#else
	static_cast<void>(MovementMode); static_cast<void>(Distance); return false;
#endif
}

bool AMMOPlayerController::RecordFirstActionMetric(FName Action, const FString& Payload)
{
#if !UE_BUILD_SHIPPING
	const AMMOCharacter* MetricPawn = Cast<AMMOCharacter>(GetPawn());
	if (!HasAuthority() || !MetricPawn || !MetricPawn->HasAuthority() || !MetricPawn->IsPlayerControlled()
		|| !MetricPawn->GetStats() || !FirstActionMetricsSession.IsValid() || FirstActionMetricsStart < 0.0
		|| (Action != TEXT("quest_accept") && Action != TEXT("quest_claim") && Action != TEXT("loot") && Action != TEXT("kill") && Action != TEXT("equip") && Action != TEXT("skill") && Action != TEXT("basic_attack") && Action != TEXT("death") && Action != TEXT("levelup") && Action != TEXT("move"))
		|| RecordedFirstActionMetrics.Contains(Action))
	{
		return false;
	}
	RecordedFirstActionMetrics.Add(Action);
	const double Now = MMOMetrics::Seconds();
	UE_LOG(LogTemp, Display, TEXT("[MMOMETRIC] t=%.3f ev=first_%s who=%s lv=%d %s scope=controller sid=%s start=%.3f elapsed=%.3f"),
		Now, *Action.ToString(), *MMOMetrics::WhoName(MetricPawn), MetricPawn->GetStats()->Level, *Payload,
		*FirstActionMetricsSession.ToString(EGuidFormats::Digits), FirstActionMetricsStart, FMath::Max(0.0, Now - FirstActionMetricsStart));
	return true;
#else
	static_cast<void>(Action);
	static_cast<void>(Payload);
	return false;
#endif
}

#if !UE_BUILD_SHIPPING
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"

bool AMMOPlayerController::RunFirstQuestMetricsSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{
		OutError = TEXT("server game world required"); return false;
	}
	TArray<AActor*> Fixtures;
	const bool bPassed = [&]()
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AMMOPlayerController* PCs[3] = {};
		AMMOCharacter* Pawns[3] = {};
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector Location(1000000 + i * 5000, 1000000, 5000);
			PCs[i] = World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(), Location, FRotator::ZeroRotator, Params);
			Pawns[i] = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
			if (PCs[i]) Fixtures.Add(PCs[i]);
			if (Pawns[i]) Fixtures.Add(Pawns[i]);
			if (!PCs[i] || !Pawns[i]) { OutError = TEXT("fixture spawn failed"); return false; }
		}
		auto Check = [&](bool Condition, const TCHAR* Label)
		{
			if (!Condition) { OutError = Label; return false; }
			++OutChecks; return true;
		};
		const FName Accept(TEXT("quest_accept")), Claim(TEXT("quest_claim")), Quest(TEXT("Q_Goblins"));
		auto DeathMetric = [](AMMOPlayerController* PC)
		{
			UMMOStatsComponent* S = Cast<AMMOCharacter>(PC->GetPawn())->GetStats();
			const bool Dead = S->bIsDead; const float Health = S->Health;
			S->bIsDead = true; S->Health = 0.f;
			const bool Recorded = PC->RecordFirstDeathMetric(TEXT("Goblin"), 5);
			S->bIsDead = Dead; S->Health = Health;
			return Recorded;
		};
		if (!Check(!PCs[0]->RecordFirstQuestMetric(Accept, Quest)
			&& !PCs[0]->RecordFirstLootMetric(TEXT("gold"), TEXT("Gold"), 2)
			&& !PCs[0]->RecordFirstEquipMetric(TEXT("ShortSword"), 101, 0)
			&& !PCs[0]->RecordFirstSkillMetric(TEXT("War_PowerStrike"))
			&& !PCs[0]->RecordFirstBasicAttackMetric(TEXT("mob"), TEXT("Goblin"), 1)
			&& !PCs[0]->RecordFirstKillMetric(TEXT("Goblin"), 1, 20, TEXT("fatal"), false)
			&& !PCs[0]->RecordFirstDeathMetric(TEXT("Goblin"), 5), TEXT("controller without pawn emitted"))) return false;
		PCs[0]->Possess(Pawns[0]);
		if (!Check(!PCs[0]->RecordFirstQuestMetric(TEXT("unknown"), Quest) && !PCs[0]->RecordFirstQuestMetric(Accept, NAME_None)
			&& !PCs[0]->RecordFirstLootMetric(TEXT("unknown"), TEXT("Gold"), 1)
			&& !PCs[0]->RecordFirstLootMetric(TEXT("gold"), NAME_None, 1)
			&& !PCs[0]->RecordFirstLootMetric(TEXT("gold"), TEXT("Gold"), 0)
			&& !PCs[0]->RecordFirstBasicAttackMetric(TEXT("unknown"), TEXT("Goblin"), 1)
			&& !PCs[0]->RecordFirstBasicAttackMetric(TEXT("mob"), NAME_None, 1)
			&& !PCs[0]->RecordFirstBasicAttackMetric(TEXT("mob"), TEXT("Goblin"), 0)
			&& !PCs[0]->RecordFirstBasicAttackMetric(TEXT("player"), TEXT("DisplayName"), 1)
			&& !PCs[0]->RecordFirstSkillMetric(NAME_None)
			&& !PCs[0]->RecordFirstSkillMetric(TEXT("UnknownSkill"))
			&& !PCs[0]->RecordFirstSkillMetric(TEXT("Pri_Smite"))
			&& !PCs[0]->RecordFirstEquipMetric(NAME_None, 101, 0)
			&& !PCs[0]->RecordFirstEquipMetric(TEXT("Unknown"), 101, 0)
			&& !PCs[0]->RecordFirstEquipMetric(TEXT("ShortSword"), 0, 0)
			&& !PCs[0]->RecordFirstEquipMetric(TEXT("ShortSword"), 101, -1)
			&& !PCs[0]->RecordFirstEquipMetric(TEXT("ShortSword"), 101, 4)
			&& !PCs[0]->RecordFirstEquipMetric(TEXT("ShortSword"), 101, 1)
			&& !PCs[0]->RecordFirstKillMetric(NAME_None, 1, 20, TEXT("fatal"), false)
			&& !PCs[0]->RecordFirstKillMetric(TEXT("Goblin"), 0, 20, TEXT("fatal"), false)
			&& !PCs[0]->RecordFirstKillMetric(TEXT("Goblin"), 1, -1, TEXT("fatal"), false)
			&& !PCs[0]->RecordFirstKillMetric(TEXT("Goblin"), 1, 20, TEXT("unknown"), false)
			&& !PCs[0]->RecordFirstDeathMetric(TEXT("Goblin"), 5), TEXT("invalid event emitted"))) return false;
		if (!Check(PCs[0]->RecordFirstQuestMetric(Accept, Quest) && !PCs[0]->RecordFirstQuestMetric(Accept, TEXT("Q_Cleanup"))
			&& PCs[0]->RecordFirstLootMetric(TEXT("gold"), TEXT("Gold"), 2)
			&& !PCs[0]->RecordFirstLootMetric(TEXT("item"), TEXT("HealthPotion"), 1)
			&& PCs[0]->RecordFirstSkillMetric(TEXT("War_PowerStrike"))
			&& PCs[0]->RecordFirstBasicAttackMetric(TEXT("mob"), TEXT("Goblin"), 1)
			&& !PCs[0]->RecordFirstSkillMetric(TEXT("War_PowerStrike"))
			&& !PCs[0]->RecordFirstBasicAttackMetric(TEXT("mob"), TEXT("Goblin"), 1)
			&& PCs[0]->RecordFirstEquipMetric(TEXT("ShortSword"), 101, 0)
			&& !PCs[0]->RecordFirstSkillMetric(TEXT("War_PowerStrike"))
			&& !PCs[0]->RecordFirstBasicAttackMetric(TEXT("mob"), TEXT("Goblin"), 1)
			&& !PCs[0]->RecordFirstEquipMetric(TEXT("LeatherArmor"), 102, 1)
			&& PCs[0]->RecordFirstKillMetric(TEXT("Goblin"), 1, 20, TEXT("fatal"), false)
			&& !PCs[0]->RecordFirstKillMetric(TEXT("Wolf"), 2, 40, TEXT("party"), false)
			&& DeathMetric(PCs[0]) && !DeathMetric(PCs[0]), TEXT("first action not unique"))) return false;
		PCs[0]->Possess(Pawns[1]);
		if (!Check(!PCs[0]->RecordFirstQuestMetric(Accept, Quest) && PCs[0]->RecordFirstQuestMetric(Claim, Quest)
			&& !PCs[0]->RecordFirstLootMetric(TEXT("item"), TEXT("HealthPotion"), 1)
			&& !PCs[0]->RecordFirstSkillMetric(TEXT("War_PowerStrike"))
			&& !PCs[0]->RecordFirstBasicAttackMetric(TEXT("mob"), TEXT("Goblin"), 1)
			&& !PCs[0]->RecordFirstEquipMetric(TEXT("LeatherArmor"), 102, 1)
			&& !PCs[0]->RecordFirstKillMetric(TEXT("Wolf"), 2, 40, TEXT("party"), false)
			&& !DeathMetric(PCs[0]), TEXT("pawn replacement reset first event"))) return false;
		const FGuid OriginalSession = PCs[0]->FirstActionMetricsSession;
		const double OriginalStart = PCs[0]->FirstActionMetricsStart;
		PCs[0]->SeamlessTravelTo(PCs[1]);
		PCs[1]->Possess(Pawns[2]);
		if (!Check(PCs[1]->FirstActionMetricsSession == OriginalSession && PCs[1]->FirstActionMetricsStart == OriginalStart
			&& !PCs[1]->RecordFirstQuestMetric(Accept, Quest) && !PCs[1]->RecordFirstQuestMetric(Claim, Quest)
			&& !PCs[1]->RecordFirstLootMetric(TEXT("item"), TEXT("HealthPotion"), 1)
			&& !PCs[1]->RecordFirstSkillMetric(TEXT("War_PowerStrike"))
			&& !PCs[1]->RecordFirstBasicAttackMetric(TEXT("mob"), TEXT("Goblin"), 1)
			&& !PCs[1]->RecordFirstEquipMetric(TEXT("LeatherArmor"), 102, 1)
			&& !PCs[1]->RecordFirstKillMetric(TEXT("Wolf"), 2, 40, TEXT("party"), false)
			&& !DeathMetric(PCs[1]), TEXT("controller transfer reset first event"))) return false;
		PCs[2]->Possess(Pawns[0]);
		return Check(PCs[2]->FirstActionMetricsSession != OriginalSession && PCs[2]->RecordFirstQuestMetric(Accept, Quest)
			&& PCs[2]->RecordFirstLootMetric(TEXT("gold"), TEXT("Gold"), 2)
			&& PCs[2]->RecordFirstSkillMetric(TEXT("War_PowerStrike"))
			&& PCs[2]->RecordFirstBasicAttackMetric(TEXT("mob"), TEXT("Goblin"), 1)
			&& PCs[2]->RecordFirstEquipMetric(TEXT("ShortSword"), 101, 0)
			&& PCs[2]->RecordFirstKillMetric(TEXT("Goblin"), 1, 20, TEXT("fatal"), false)
			&& DeathMetric(PCs[2]), TEXT("new controller did not start independent session"));
	}();
	for (AActor* Fixture : Fixtures) { if (IsValid(Fixture)) Fixture->Destroy(); }
	return bPassed; // Same synchronous frame: no player load timer or disk save.
}

namespace
{
	struct FMMOFirstQuestMetricsTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestFirstQuestMetrics"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = AMMOPlayerController::RunFirstQuestMetricsSelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_FIRST_QUEST_METRICS_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOFirstQuestMetricsTestExec GMMOFirstQuestMetricsTestExec;
}
#endif

#if !UE_BUILD_SHIPPING
bool AMMOPlayerController::RunTradeChatContextSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{ OutError = TEXT("trade chat requires server game world"); return false; }
	AMMOPlayerController* PCs[3] = {}; AMMOCharacter* Pawns[3] = {}; UMMOTradeComponent* Trades[3] = {};
	AMMOCharacter* RetiredPawn = nullptr; UMMOTradeComponent* RetiredTrade = nullptr; FMMOCharacterSaveData RetiredInventory;
	const bool Passed = [&]()
	{
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		FMMOCharacterSaveData Before[3];
		for (int32 I = 0; I < 3; ++I)
		{
			PCs[I] = World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(), Params);
			Pawns[I] = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(1500000 + I * 100, 1500000, 5000), FRotator::ZeroRotator, Params);
			if (!PCs[I] || !Pawns[I] || !Pawns[I]->GetInventory() || !Pawns[I]->GetTradeComponent())
			{ OutError = TEXT("trade chat fixture spawn"); return false; }
			PCs[I]->Possess(Pawns[I]); PCs[I]->SetActorTickEnabled(false); Pawns[I]->SetActorTickEnabled(false);
			Pawns[I]->SetActorEnableCollision(false);
			Trades[I] = Pawns[I]->GetTradeComponent(); Pawns[I]->GetInventory()->FillSaveData(Before[I]);
		}
		auto Unchanged = [&]()
		{
			for (int32 I = 0; I < 3; ++I)
			{
				FMMOCharacterSaveData After; Pawns[I]->GetInventory()->FillSaveData(After);
				TArray<FString> Differences; FMMOCharacterSaveJson::FindDifferences(Before[I], After, Differences);
				if (!Differences.IsEmpty()) return false;
			}
			return true;
		};
		auto Check = [&](bool Value, const TCHAR* Label)
		{
			if (!Value) { OutError = FString::Printf(TEXT("%s (check=%d)"), Label, OutChecks + 1); return false; }
			++OutChecks; return true;
		};
		auto Chat = [&](const TCHAR* Message, AMMOCharacter* SeenPawn, uint64 Invite, uint64 Session)
		{
			PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
			PCs[1]->ServerSendChat_Implementation(Message, SeenPawn, Invite, Session, 0, 0, FGuid(), 0, FGuid());
		};
		auto OffersEqual = [](const FMMOTradeOffer& A, const FMMOTradeOffer& B)
		{
			if (A.Gold != B.Gold || A.bConfirmed != B.bConfirmed || A.Items.Num() != B.Items.Num()) return false;
			for (int32 I = 0; I < A.Items.Num(); ++I)
			{
				const auto& X = A.Items[I]; const auto& Y = B.Items[I];
				if (X.InstanceId != Y.InstanceId || X.ItemId != Y.ItemId || X.Plus != Y.Plus || X.Rarity != Y.Rarity) return false;
			}
			return true;
		};
		auto RejectInviteUnchanged = [&](AMMOCharacter* SeenPawn)
		{
			struct FState
			{
				bool Active; FString Name; FMMOTradeOffer Mine, Theirs; int32 Revision; uint64 Session, Invite;
				TWeakObjectPtr<AMMOCharacter> Partner, Pending;
				float PendingTime, InviteTime, OfferTime, TradeTime, ConfirmTime; bool TimerActive;
			};
			UMMOTradeComponent* Components[4] = {Trades[0], Trades[1], Trades[2], RetiredTrade};
			FState States[4];
			for (int32 I = 0; I < 4; ++I) if (const auto* T = Components[I])
			{
				States[I] = {T->bTradeActive, T->PartnerName, T->MyOffer, T->TheirOffer, T->OfferRevision,
					T->TradeSessionId, T->PendingInviteId, T->Partner, T->PendingInviteFrom,
					T->PendingInviteTime, T->LastInviteTime, T->LastOfferChangeTime, T->LastTradeChangeTime,
					T->LastConfirmTime, World->GetTimerManager().IsTimerActive(T->ValidateTimerHandle)};
			}
			Chat(TEXT("/trade"), SeenPawn, 0, 0);
			for (int32 I = 0; I < 4; ++I) if (const auto* T = Components[I])
			{
				const auto& S = States[I];
				if (T->bTradeActive != S.Active || T->PartnerName != S.Name || !OffersEqual(T->MyOffer, S.Mine)
					|| !OffersEqual(T->TheirOffer, S.Theirs) || T->OfferRevision != S.Revision || T->TradeSessionId != S.Session
					|| T->PendingInviteId != S.Invite || T->Partner != S.Partner || T->PendingInviteFrom != S.Pending
					|| T->PendingInviteTime != S.PendingTime || T->LastInviteTime != S.InviteTime
					|| T->LastOfferChangeTime != S.OfferTime || T->LastTradeChangeTime != S.TradeTime || T->LastConfirmTime != S.ConfirmTime
					|| World->GetTimerManager().IsTimerActive(T->ValidateTimerHandle) != S.TimerActive) return false;
			}
			if (RetiredPawn)
			{
				FMMOCharacterSaveData After; RetiredPawn->GetInventory()->FillSaveData(After); TArray<FString> Differences;
				FMMOCharacterSaveJson::FindDifferences(RetiredInventory, After, Differences); if (!Differences.IsEmpty()) return false;
			}
			return Unchanged();
		};
		Trades[0]->RequestInviteNearest();
		const uint64 Old = Trades[1]->PendingInviteId;
		if (!Check(Old != 0 && !Trades[1]->bTradeActive && Unchanged(), TEXT("chat first real nearest invite"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
		PCs[1]->OnChatCommitted(FText::FromString(TEXT("/trade cancel")), ETextCommit::OnEnter);
		Trades[2]->RequestInviteNearest(); // A different sender bypasses neither spam guard nor invite cooldown.
		const uint64 Fresh = Trades[1]->PendingInviteId;
		if (!Check(Fresh > Old && !Trades[1]->bTradeActive && Unchanged(), TEXT("chat committed decline then different sender invite"))) return false;
		Chat(TEXT("/trade accept"), Pawns[1], Old, 0);
		if (!Check(!Trades[1]->bTradeActive && Trades[1]->PendingInviteId == Fresh && Unchanged(), TEXT("chat stale accept preserves new invite"))) return false;
		Chat(TEXT("/trade iptal"), Pawns[1], Old, 0);
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
		PCs[1]->Say(TEXT("/trade kabul"));
		if (!Check(Trades[1]->bTradeActive && Trades[2]->bTradeActive && !Trades[0]->bTradeActive
			&& Trades[1]->TradeSessionId == Trades[2]->TradeSessionId && Unchanged(), TEXT("chat stale decline preserved invite for fresh Say accept"))) return false;
		const uint64 Active = Trades[1]->TradeSessionId;
		Chat(TEXT("/trade cancel"), Pawns[1], Fresh, 0);
		if (!Check(Trades[1]->bTradeActive && Trades[2]->bTradeActive && Trades[1]->TradeSessionId == Active && Unchanged(), TEXT("chat inactive intent cannot cancel active session"))) return false;
		Chat(TEXT("/trade cancel"), Pawns[0], Fresh, Active);
		if (!Check(Trades[1]->bTradeActive && Trades[2]->bTradeActive && Unchanged(), TEXT("chat wrong pawn context cannot cancel matching numeric session"))) return false;
		Chat(TEXT("/trade cancel"), nullptr, Fresh, Active);
		if (!Check(Trades[1]->bTradeActive && Trades[2]->bTradeActive && Unchanged(), TEXT("chat null context has no current-state fallback"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
		PCs[1]->Say(TEXT("/trade iptal"));
		if (!Check(!Trades[1]->bTradeActive && !Trades[2]->bTradeActive && Unchanged(), TEXT("chat fresh Say active cancel cleans both/no transfer"))) return false;
		// Issuing chat must bind to its sending pawn BEFORE invite cooldown or target mutation.
		Pawns[2]->SetActorLocation(FVector(1501000, 1500000, 5000));
		Trades[1]->LastInviteTime = World->GetTimeSeconds() - 4.f;
		if (!Check(RejectInviteUnchanged(nullptr), TEXT("trade issuing null pawn mutated invite state"))) return false;
		if (!Check(RejectInviteUnchanged(Pawns[0]), TEXT("trade issuing foreign pawn mutated invite state"))) return false;
		const uint64 BeforeInvite = Trades[0]->PendingInviteId;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f; PCs[1]->Say(TEXT("/trade"));
		if (!Check(Trades[0]->PendingInviteId > BeforeInvite && Trades[0]->PendingInviteFrom.Get() == Pawns[1]
			&& Trades[1]->LastInviteTime == World->GetTimeSeconds() && Unchanged(), TEXT("trade issuing fresh Say failed"))) return false;
		Trades[0]->RequestAccept();
		if (!Check(Trades[0]->bTradeActive && Trades[1]->bTradeActive && Unchanged(), TEXT("trade issuing fresh invite acceptance failed"))) return false;
		Trades[1]->LastInviteTime = World->GetTimeSeconds() - 4.f;
		if (!Check(RejectInviteUnchanged(Pawns[0]), TEXT("trade issuing stale pawn consumed active trade cooldown"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f; PCs[1]->Say(TEXT("/trade iptal"));
		if (!Check(!Trades[0]->bTradeActive && !Trades[1]->bTradeActive && Unchanged(), TEXT("trade issuing fresh pair cancel failed"))) return false;
		RetiredPawn = Pawns[1]; RetiredTrade = Trades[1]; RetiredPawn->SetTravelSaveCommitted(true);
		RetiredPawn->GetInventory()->FillSaveData(RetiredInventory);
		AMMOCharacter* Replacement = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), RetiredPawn->GetActorLocation(), FRotator::ZeroRotator, Params);
		if (!Replacement) { OutError = TEXT("trade issuing replacement spawn failed"); return false; }
		Pawns[1] = Replacement; Trades[1] = Replacement->GetTradeComponent();
		PCs[1]->Possess(Replacement); Replacement->SetActorTickEnabled(false); Replacement->SetActorEnableCollision(false);
		Replacement->GetInventory()->FillSaveData(Before[1]);
		if (!Check(PCs[1]->GetPawn() == Replacement && RetiredPawn->GetController() == nullptr && Trades[1] && Unchanged(),
			TEXT("trade issuing actual replacement possession failed"))) return false;
		if (!Check(RejectInviteUnchanged(RetiredPawn), TEXT("trade issuing retired pawn invited on replacement"))) return false;
		if (!Check(RejectInviteUnchanged(nullptr), TEXT("trade issuing null context invited on replacement"))) return false;
		const uint64 ReplacementBeforeInvite = Trades[0]->PendingInviteId;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
		PCs[1]->OnChatCommitted(FText::FromString(TEXT("/trade")), ETextCommit::OnEnter);
		if (!Check(Trades[0]->PendingInviteId > ReplacementBeforeInvite && Trades[0]->PendingInviteFrom.Get() == Replacement
			&& Unchanged(), TEXT("trade issuing fresh replacement Enter failed"))) return false;
		Trades[0]->RequestAccept();
		if (!Check(Trades[0]->bTradeActive && Trades[1]->bTradeActive && Unchanged(), TEXT("trade issuing fresh replacement accept failed"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f; PCs[1]->Say(TEXT("/trade iptal"));
		return Check(!Trades[0]->bTradeActive && !Trades[1]->bTradeActive && Unchanged(), TEXT("trade issuing replacement cleanup changed inventory"));
	}();
	for (AMMOCharacter* Pawn : Pawns) if (IsValid(Pawn)) Pawn->SetTravelSaveCommitted(true);
	for (int32 I = 0; I < 3; ++I) { if (PCs[I]) { PCs[I]->UnPossess(); PCs[I]->Destroy(); } if (Pawns[I]) Pawns[I]->Destroy(); }
	if (IsValid(RetiredPawn)) RetiredPawn->Destroy();
	return Passed;
}
#endif

#if !UE_BUILD_SHIPPING
bool AMMOPlayerController::RunPartyChatContextSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	if (!World || !World->IsGameWorld() || World->GetNetMode() != NM_Standalone
		|| !FParse::Param(FCommandLine::Get(), TEXT("unattended")) || !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
	{ OutError = TEXT("party chat requires disposable standalone headless world"); return false; }
	AMMOPlayerController* PCs[3] = {}; AMMOCharacter* Pawns[3] = {}; UMMOPartyComponent* Parties[3] = {};
	AMMOHUD* HUDs[3] = {};
	const bool Passed = [&]()
	{
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		FMMOCharacterSaveData Before[3];
		for (int32 I = 0; I < 3; ++I)
		{
			PCs[I] = World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(), Params);
			Pawns[I] = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(1800000 + I * 100, 1800000, 5000), FRotator::ZeroRotator, Params);
			if (!PCs[I] || !Pawns[I] || !Pawns[I]->GetInventory() || !Pawns[I]->GetPartyComponent())
			{ OutError = TEXT("party chat fixture spawn"); return false; }
			PCs[I]->Possess(Pawns[I]); PCs[I]->SetActorTickEnabled(false); Pawns[I]->SetActorTickEnabled(false);
			Pawns[I]->SetActorEnableCollision(false);
			Parties[I] = Pawns[I]->GetPartyComponent(); Pawns[I]->GetInventory()->FillSaveData(Before[I]);
		}
		auto InventoryUnchanged = [&]()
		{
			for (int32 I = 0; I < 3; ++I)
			{
				FMMOCharacterSaveData After; Pawns[I]->GetInventory()->FillSaveData(After);
				TArray<FString> Differences; FMMOCharacterSaveJson::FindDifferences(Before[I], After, Differences);
				if (!Differences.IsEmpty()) return false;
			}
			return true;
		};
		auto Check = [&](bool Value, const TCHAR* Label)
		{
			if (!Value) { OutError = FString::Printf(TEXT("%s (check=%d)"), Label, OutChecks + 1); return false; }
			++OutChecks; return true;
		};
		auto RejectedUnchanged = [&](const TCHAR* Message, AMMOCharacter* SeenPawn, uint64 Id, const FGuid& SeenMembership = FGuid())
		{
			TArray<TObjectPtr<AMMOCharacter>> Members[3]; TObjectPtr<AMMOCharacter> Leaders[3];
			TWeakObjectPtr<AMMOCharacter> Pending[3]; uint64 Ids[3]; float Times[3], InviteTimes[3]; FGuid Sessions[3], Memberships[3], SourceContexts[3], PendingSourceContexts[3];
			for (int32 I = 0; I < 3; ++I)
			{
				Members[I] = Parties[I]->Members; Leaders[I] = Parties[I]->Leader;
				Sessions[I] = Parties[I]->PartySessionId; Memberships[I] = Parties[I]->PartyMembershipId;
				SourceContexts[I] = Parties[I]->PartyInviteContextId; PendingSourceContexts[I] = Parties[I]->PendingInviterContextId;
				Pending[I] = Parties[I]->PendingInviteFrom; Ids[I] = Parties[I]->PendingInviteId; Times[I] = Parties[I]->PendingInviteTime;
				InviteTimes[I] = Parties[I]->LastInviteTime;
			}
			PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
			PCs[1]->ServerSendChat_Implementation(Message, SeenPawn, 0, 0, Id, 0, SeenMembership, 0, FGuid());
			for (int32 I = 0; I < 3; ++I)
			{
				if (Parties[I]->Members != Members[I] || Parties[I]->Leader != Leaders[I]
					|| Parties[I]->PendingInviteFrom != Pending[I] || Parties[I]->PendingInviteId != Ids[I]
					|| Parties[I]->PendingInviteTime != Times[I] || Parties[I]->PartySessionId != Sessions[I]
					|| Parties[I]->PartyMembershipId != Memberships[I] || Parties[I]->PartyInviteContextId != SourceContexts[I]
					|| Parties[I]->PendingInviterContextId != PendingSourceContexts[I]
					|| Parties[I]->LastInviteTime != InviteTimes[I]) return false;
			}
			return InventoryUnchanged();
		};
		Parties[0]->RequestInviteNearest(); const uint64 Old = Parties[1]->PendingInviteId;
		if (!Check(Old != 0 && Parties[1]->PendingInviteFrom.Get() == Pawns[0] && InventoryUnchanged(), TEXT("party chat first nearest invite"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
		PCs[1]->OnChatCommitted(FText::FromString(TEXT("/parti ret")), ETextCommit::OnEnter);
		Parties[2]->RequestInviteNearest(); const uint64 Fresh = Parties[1]->PendingInviteId;
		if (!Check(Fresh > Old && Parties[1]->PendingInviteFrom.Get() == Pawns[2] && InventoryUnchanged(), TEXT("party committed decline then new sender"))) return false;
		if (!Check(RejectedUnchanged(TEXT("/parti accept"), Pawns[1], Old), TEXT("party chat stale accept"))) return false;
		if (!Check(RejectedUnchanged(TEXT("/parti decline"), Pawns[1], Old), TEXT("party chat stale decline"))) return false;
		// Real native possession switch; matching numeric generation is deliberately seeded in the new pawn.
		PCs[0]->UnPossess(); PCs[1]->UnPossess(); PCs[1]->Possess(Pawns[0]);
		if (!Parties[0]->SetPendingInviteServer(Pawns[2])) return false;
		Parties[0]->PendingInviteId = Fresh;
		const bool ReplacementRejected = PCs[1]->GetPawn() == Pawns[0]
			&& RejectedUnchanged(TEXT("/parti kabul"), Pawns[1], Fresh);
		PCs[1]->UnPossess(); PCs[1]->Possess(Pawns[1]); PCs[0]->Possess(Pawns[0]);
		Parties[0]->PendingInviteFrom.Reset(); Parties[0]->PendingInviteTime = -1.f;
		Parties[0]->PendingInviterContextId.Invalidate();
		if (!Check(ReplacementRejected, TEXT("party chat replaced pawn matching generation"))) return false;
		if (!Check(RejectedUnchanged(TEXT("/parti ret"), nullptr, Fresh), TEXT("party chat null context no fallback"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f; PCs[1]->Say(TEXT("/parti kabul"));
		if (!Check(Parties[1]->Members.Num() == 1 && Parties[1]->Members.Contains(Pawns[2]) && Parties[1]->Leader == Pawns[2]
			&& Parties[2]->Members.Num() == 1 && Parties[2]->Members.Contains(Pawns[1]) && Parties[2]->Leader == Pawns[2]
			&& !Parties[0]->IsInParty() && !Parties[1]->PendingInviteFrom.IsValid() && InventoryUnchanged(), TEXT("party fresh Say correct sender"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
		PCs[1]->OnChatCommitted(FText::FromString(TEXT("/parti leave")), ETextCommit::OnEnter);
		if (!Check(!Parties[1]->IsInParty() && !Parties[2]->IsInParty() && Parties[1]->Leader == nullptr
			&& Parties[2]->Leader == nullptr && InventoryUnchanged(), TEXT("party current leave unchanged"))) return false;
		// New leave cases seed native invite context, not delayed network delivery.
		auto FormPair = [&](int32 From, int32 To) {
			if (Parties[To]->SetPendingInviteServer(Pawns[From])) Parties[To]->RequestAccept();
		};
		auto PairMatches = [&](int32 A, int32 B) {
			return Parties[A]->Members.Num() == 1 && Parties[A]->Members.Contains(Pawns[B])
				&& Parties[B]->Members.Num() == 1 && Parties[B]->Members.Contains(Pawns[A])
				&& Parties[A]->PartySessionId.IsValid() && Parties[A]->PartySessionId == Parties[B]->PartySessionId
				&& Parties[A]->PartyMembershipId.IsValid() && Parties[B]->PartyMembershipId.IsValid()
				&& Parties[A]->PartyMembershipId != Parties[B]->PartyMembershipId && InventoryUnchanged();
		};
		auto Solo = [&](int32 I) { return !Parties[I]->IsInParty() && !Parties[I]->Leader
			&& !Parties[I]->PartySessionId.IsValid() && !Parties[I]->PartyMembershipId.IsValid(); };
		FormPair(2, 1); const FGuid OldMembership = Parties[1]->PartyMembershipId;
		if (!Check(PairMatches(1, 2), TEXT("leave chat membership setup"))) return false;
		// Genuine native possession switch inside the same party; matching new-pawn ID isolates pawn check.
		PCs[2]->UnPossess(); PCs[1]->UnPossess(); PCs[1]->Possess(Pawns[2]);
		const bool WrongPawnRejected = PCs[1]->GetPawn() == Pawns[2]
			&& RejectedUnchanged(TEXT("/parti leave"), Pawns[1], 0, Parties[2]->PartyMembershipId);
		PCs[1]->UnPossess(); PCs[1]->Possess(Pawns[1]); PCs[2]->Possess(Pawns[2]);
		if (!Check(WrongPawnRejected, TEXT("leave chat replaced pawn matching membership"))) return false;
		if (!Check(RejectedUnchanged(TEXT("/parti ayril"), nullptr, 0, OldMembership), TEXT("leave chat null pawn no fallback"))) return false;
		if (!Check(RejectedUnchanged(TEXT("/parti leave"), Pawns[1], 0, FGuid()), TEXT("leave chat zero membership no fallback"))) return false;
		if (!Check(RejectedUnchanged(TEXT("/parti ayril"), Pawns[1], 0, Parties[2]->PartyMembershipId), TEXT("leave chat foreign membership"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f; PCs[1]->Say(TEXT("/parti leave"));
		if (!Check(Solo(1) && Solo(2) && InventoryUnchanged(), TEXT("leave chat fresh Say"))) return false;
		FormPair(0, 1);
		if (!Check(PairMatches(0, 1) && Parties[1]->PartyMembershipId != OldMembership, TEXT("leave chat replacement setup"))) return false;
		if (!Check(RejectedUnchanged(TEXT("/parti leave"), Pawns[1], 0, OldMembership), TEXT("leave chat old membership removed replacement"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
		PCs[1]->OnChatCommitted(FText::FromString(TEXT("/parti ayril")), ETextCommit::OnEnter);
		if (!Check(Solo(0) && Solo(1) && InventoryUnchanged(), TEXT("leave chat fresh commit"))) return false;
		// Late HUD setup avoids unrelated invite/leave notifications in routing assertions.
		const FString NamePrefix = TEXT("ZPC") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(16);
		FString Names[3];
		for (int32 I = 0; I < 3; ++I)
		{
			if (!PCs[I]->PlayerState) { OutError = TEXT("party routing PlayerState missing"); return false; }
			Names[I] = NamePrefix + FString::FromInt(I); PCs[I]->PlayerState->SetPlayerName(Names[I]);
			PCs[I]->ClientSetHUD(AMMOHUD::StaticClass()); HUDs[I] = Cast<AMMOHUD>(PCs[I]->GetHUD());
			if (!HUDs[I]) { OutError = TEXT("party routing HUD missing"); return false; }
			HUDs[I]->SetActorTickEnabled(false);
		}
		auto CopyLines = [&]()
		{
			TArray<TArray<FMMOChatLine>> Lines;
			for (AMMOHUD* HUD : HUDs) Lines.Add(HUD->GetChatLinesForTests());
			return Lines;
		};
		auto LinesEqual = [&](const TArray<TArray<FMMOChatLine>>& BeforeLines)
		{
			for (int32 I = 0; I < 3; ++I)
			{
				const auto& After = HUDs[I]->GetChatLinesForTests(); const auto& Was = BeforeLines[I];
				if (After.Num() != Was.Num()) return false;
				for (int32 L = 0; L < Was.Num(); ++L)
					if (After[L].Text != Was[L].Text || After[L].Color != Was[L].Color || After[L].Time != Was[L].Time) return false;
			}
			return true;
		};
		auto RejectRouting = [&](const TCHAR* Message, AMMOCharacter* SeenPawn, const FGuid& Membership)
		{
			const auto Lines = CopyLines();
			return RejectedUnchanged(Message, SeenPawn, 0, Membership) && LinesEqual(Lines);
		};
		auto Say = [&](const FString& Text) { PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f; PCs[1]->Say(Text); };
		auto PartyDelivered = [&](const TArray<TArray<FMMOChatLine>>& BeforeLines, const FString& Text, bool bAll)
		{
			for (int32 I = 0; I < 3; ++I)
			{
				const auto& Lines = HUDs[I]->GetChatLinesForTests(); const bool Receives = bAll || I != 2;
				if (Lines.Num() != BeforeLines[I].Num() + (Receives ? 1 : 0)) return false;
				if (Receives && (Lines.Last().Text != FString::Printf(TEXT("[Parti] %s: %s"), *Names[1], *Text)
					|| Lines.Last().Color != FLinearColor(0.45f, 0.85f, 1.f))) return false;
			}
			return InventoryUnchanged();
		};
		FormPair(2, 1); const FGuid SendingMembership = Parties[1]->PartyMembershipId;
		if (!Check(PairMatches(1, 2), TEXT("party routing old group setup"))) return false;
		Parties[1]->RequestLeave(); FormPair(0, 1); const FGuid CurrentMembership = Parties[1]->PartyMembershipId;
		if (!Check(PairMatches(0, 1) && CurrentMembership != SendingMembership && Solo(2), TEXT("party routing replacement group"))) return false;
		if (!Check(RejectRouting(TEXT("/p old_group_secret"), Pawns[1], SendingMembership), TEXT("old party message leaked into replacement group"))) return false;
		if (!Check(RejectRouting(TEXT("/p zero_secret"), Pawns[1], FGuid()), TEXT("zero party membership message delivered"))) return false;
		if (!Check(RejectRouting(TEXT("/p foreign_secret"), Pawns[1], Parties[0]->PartyMembershipId), TEXT("foreign party membership message delivered"))) return false;
		if (!Check(RejectRouting(TEXT("/p random_secret"), Pawns[1], FGuid::NewGuid()), TEXT("unknown party membership message delivered"))) return false;
		if (!Check(RejectRouting(TEXT("/p null_secret"), nullptr, CurrentMembership), TEXT("null pawn party message delivered"))) return false;
		if (!Check(RejectRouting(TEXT("/p foreign_pawn_secret"), Pawns[2], CurrentMembership), TEXT("foreign pawn party message delivered"))) return false;
		PCs[0]->UnPossess(); PCs[1]->UnPossess(); PCs[1]->Possess(Pawns[0]); PCs[0]->Possess(Pawns[1]);
		const bool OldPawnRejected = PCs[1]->GetPawn() == Pawns[0]
			&& RejectRouting(TEXT("/p old_pawn_secret"), Pawns[1], Parties[0]->PartyMembershipId);
		PCs[0]->UnPossess(); PCs[1]->UnPossess(); PCs[1]->Possess(Pawns[1]); PCs[0]->Possess(Pawns[0]);
		if (!Check(OldPawnRejected, TEXT("old pawn party message matching replacement membership delivered"))) return false;
		auto Lines = CopyLines(); Say(TEXT("/P fresh_secret"));
		if (!Check(PartyDelivered(Lines, TEXT("fresh_secret"), false) && PairMatches(0, 1), TEXT("fresh Say party routing"))) return false;
		Lines = CopyLines(); PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
		PCs[1]->OnChatCommitted(FText::FromString(TEXT("/p committed_secret")), ETextCommit::OnEnter);
		if (!Check(PartyDelivered(Lines, TEXT("committed_secret"), false), TEXT("fresh committed party routing"))) return false;
		Lines = CopyLines(); Say(TEXT("/p")); Say(TEXT("/p   "));
		if (!Check(LinesEqual(Lines), TEXT("empty party prefix leaked to general chat"))) return false;
		const FString Nested = TEXT("/w ") + Names[2] + TEXT(" party_literal");
		Lines = CopyLines(); Say(TEXT("/p ") + Nested);
		if (!Check(PartyDelivered(Lines, Nested, false), TEXT("party body escaped to outside whisper recipient"))) return false;
		const FString NestedAlias = TEXT("/f ") + Names[2] + TEXT(" party_alias_literal");
		Lines = CopyLines(); Say(TEXT("/p ") + NestedAlias);
		if (!Check(PartyDelivered(Lines, NestedAlias, false), TEXT("party body escaped to outside alias whisper"))) return false;
		if (!Check(RejectRouting(*FString(TEXT("/p ") + Nested), Pawns[1], SendingMembership), TEXT("stale nested party body bypassed membership"))) return false;
		FormPair(0, 2); Lines = CopyLines(); Say(TEXT("/p continuing_membership"));
		if (!Check(Parties[1]->PartyMembershipId == CurrentMembership && PartyDelivered(Lines, TEXT("continuing_membership"), true), TEXT("continuous member after join party routing"))) return false;
		Parties[1]->RequestLeave(); Lines = CopyLines(); Say(TEXT("/p solo_secret"));
		if (!Check(Solo(1) && HUDs[1]->GetChatLinesForTests().Num() == Lines[1].Num() + 1
			&& HUDs[1]->GetChatLinesForTests().Last().Text == TEXT("Bir partide degilsin; mesaj gonderilmedi.")
			&& HUDs[0]->GetChatLinesForTests().Num() == Lines[0].Num() && HUDs[2]->GetChatLinesForTests().Num() == Lines[2].Num(), TEXT("fresh solo party feedback/routing"))) return false;
		Lines = CopyLines(); Say(TEXT("ordinary_general")); bool GeneralOK = true;
		for (int32 I = 0; I < 3; ++I) GeneralOK &= HUDs[I]->GetChatLinesForTests().Num() == Lines[I].Num() + 1
			&& HUDs[I]->GetChatLinesForTests().Last().Text == Names[1] + TEXT(": ordinary_general");
		if (!Check(GeneralOK, TEXT("normal general routing changed"))) return false;
		Lines = CopyLines(); Say(TEXT("/w ") + Names[2] + TEXT(" ordinary_whisper"));
		if (!Check(HUDs[0]->GetChatLinesForTests().Num() == Lines[0].Num()
			&& HUDs[1]->GetChatLinesForTests().Num() == Lines[1].Num() + 1 && HUDs[2]->GetChatLinesForTests().Num() == Lines[2].Num() + 1
			&& HUDs[1]->GetChatLinesForTests().Last().Text == FString::Printf(TEXT("[%s adli oyuncuya] ordinary_whisper"), *Names[2])
			&& HUDs[2]->GetChatLinesForTests().Last().Text == FString::Printf(TEXT("[%s fisildiyor] ordinary_whisper"), *Names[1]), TEXT("normal whisper routing changed"))) return false;
		Lines = CopyLines(); Say(TEXT("/f ") + Names[2] + TEXT(" ordinary_alias"));
		if (!Check(HUDs[0]->GetChatLinesForTests().Num() == Lines[0].Num()
			&& HUDs[1]->GetChatLinesForTests().Num() == Lines[1].Num() + 1 && HUDs[2]->GetChatLinesForTests().Num() == Lines[2].Num() + 1
			&& HUDs[1]->GetChatLinesForTests().Last().Text == FString::Printf(TEXT("[%s adli oyuncuya] ordinary_alias"), *Names[2])
			&& HUDs[2]->GetChatLinesForTests().Last().Text == FString::Printf(TEXT("[%s fisildiyor] ordinary_alias"), *Names[1]), TEXT("normal whisper alias routing changed"))) return false;
		// Delayed chat invitations must not acquire the replacement pawn/group's context.
		Parties[0]->RequestLeave();
		auto ClearInvites = [&]() {
			for (UMMOPartyComponent* P : Parties) {
				P->PendingInviteFrom.Reset(); P->PendingInviteTime = -1.f;
				P->PendingInviterContextId.Invalidate(); P->LastInviteTime = -100.f;
			}
		};
		auto FreshInvite = [&](const TCHAR* Text, bool bCommitted) {
			PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
			if (bCommitted) PCs[1]->OnChatCommitted(FText::FromString(Text), ETextCommit::OnEnter);
			else PCs[1]->Say(Text);
		};
		Pawns[0]->SetActorLocation(FVector(1800000, 1800000, 5000));
		Pawns[1]->SetActorLocation(FVector(1800100, 1800000, 5000));
		Pawns[2]->SetActorLocation(FVector(1800400, 1800000, 5000));
		ClearInvites();
		if (!Check(Solo(0) && Solo(1) && Solo(2), TEXT("invite chat solo setup"))) return false;
		if (!Check(RejectedUnchanged(TEXT("/parti"), nullptr, 0), TEXT("invite chat null pawn applied"))) return false;
		if (!Check(RejectedUnchanged(TEXT("/parti davet"), Pawns[2], 0), TEXT("invite chat foreign pawn applied"))) return false;
		PCs[0]->UnPossess(); PCs[1]->UnPossess(); PCs[1]->Possess(Pawns[0]); PCs[0]->Possess(Pawns[1]);
		const bool ReplacedInviteRejected = RejectedUnchanged(TEXT("/parti"), Pawns[1], 0);
		PCs[0]->UnPossess(); PCs[1]->UnPossess(); PCs[1]->Possess(Pawns[1]); PCs[0]->Possess(Pawns[0]);
		if (!Check(ReplacedInviteRejected, TEXT("invite chat replacement pawn applied"))) return false;
		if (!Check(RejectedUnchanged(TEXT("/parti davet"), Pawns[1], 0, FGuid::NewGuid()), TEXT("invite chat solo stale group applied"))) return false;
		const uint64 SoloNonce = Parties[0]->PendingInviteId;
		FreshInvite(TEXT("/parti"), false);
		if (!Check(Parties[0]->PendingInviteId > SoloNonce && Parties[0]->PendingInviteFrom.Get() == Pawns[1]
			&& Solo(1) && InventoryUnchanged(), TEXT("invite chat fresh solo Say"))) return false;
		Parties[0]->RequestDecline();
		if (!Check(!Parties[0]->PendingInviteFrom.IsValid() && Solo(0) && Solo(1), TEXT("invite chat fresh solo decline"))) return false;
		FormPair(1, 0); const FGuid IssuingMembership = Parties[1]->PartyMembershipId;
		Pawns[0]->SetActorLocation(FVector(1801000, 1800000, 5000));
		Pawns[2]->SetActorLocation(FVector(1800200, 1800000, 5000));
		ClearInvites();
		if (!Check(PairMatches(1, 0) && Parties[1]->IsPartyLeader(), TEXT("invite chat grouped setup"))) return false;
		if (!Check(RejectedUnchanged(TEXT("/parti"), Pawns[1], 0), TEXT("invite chat zero grouped membership applied"))) return false;
		if (!Check(RejectedUnchanged(TEXT("/parti davet"), Pawns[1], 0, Parties[0]->PartyMembershipId), TEXT("invite chat foreign grouped membership applied"))) return false;
		if (!Check(RejectedUnchanged(TEXT("/parti"), Pawns[1], 0, FGuid::NewGuid()), TEXT("invite chat random grouped membership applied"))) return false;
		const uint64 GroupNonce = Parties[2]->PendingInviteId;
		FreshInvite(TEXT("/parti davet"), true);
		if (!Check(Parties[2]->PendingInviteId > GroupNonce && Parties[2]->PendingInviteFrom.Get() == Pawns[1]
			&& PairMatches(1, 0) && Solo(2), TEXT("invite chat fresh grouped committed"))) return false;
		Parties[2]->RequestAccept();
		if (!Check(Parties[1]->Members.Num() == 2 && Parties[2]->Members.Num() == 2
			&& Parties[2]->Leader == Pawns[1] && InventoryUnchanged(), TEXT("invite chat fresh grouped accept"))) return false;
		Parties[2]->RequestLeave(); ClearInvites();
		if (!Check(PairMatches(1, 0) && Parties[1]->PartyMembershipId == IssuingMembership && Solo(2), TEXT("invite chat continuous membership"))) return false;
		Parties[1]->RequestLeave(); ClearInvites();
		if (!Check(RejectedUnchanged(TEXT("/parti"), Pawns[1], 0, IssuingMembership), TEXT("invite chat departed group applied to solo"))) return false;
		FormPair(0, 1); ClearInvites();
		if (!Check(PairMatches(0, 1) && Parties[1]->PartyMembershipId != IssuingMembership, TEXT("invite chat replacement group setup"))) return false;
		if (!Check(RejectedUnchanged(TEXT("/parti davet"), Pawns[1], 0, IssuingMembership), TEXT("invite chat old group applied to replacement"))) return false;
		Parties[1]->RequestLeave(); FormPair(1, 0); ClearInvites();
		if (!Check(RejectedUnchanged(TEXT("/parti"), Pawns[1], 0, IssuingMembership), TEXT("invite chat rejoined leader old membership applied"))) return false;
		const uint64 RecoveryNonce = Parties[2]->PendingInviteId;
		FreshInvite(TEXT("/parti"), true);
		if (!Check(Parties[2]->PendingInviteId > RecoveryNonce && Parties[2]->PendingInviteFrom.Get() == Pawns[1]
			&& Parties[1]->IsPartyLeader(), TEXT("invite chat fresh rejoined leader recovery"))) return false;
		Parties[2]->RequestAccept();
		if (!Check(Parties[0]->Members.Num() == 2 && Parties[1]->Members.Num() == 2 && Parties[2]->Members.Num() == 2
			&& Parties[1]->Leader == Pawns[1] && InventoryUnchanged(), TEXT("invite chat recovery accept"))) return false;
		return Check(InventoryUnchanged(), TEXT("party routing changed inventory"));
	}();
	for (int32 I = 0; I < 3; ++I) { if (Parties[I]) Parties[I]->RequestLeave(); }
	for (AMMOCharacter* Pawn : Pawns) if (IsValid(Pawn)) Pawn->SetTravelSaveCommitted(true);
	for (int32 I = 0; I < 3; ++I) { if (PCs[I]) { PCs[I]->UnPossess(); PCs[I]->Destroy(); } if (Pawns[I]) Pawns[I]->Destroy(); }
	for (AMMOHUD* HUD : HUDs) if (IsValid(HUD)) HUD->Destroy();
	return Passed;
}
#endif

#if !UE_BUILD_SHIPPING
bool AMMOPlayerController::RunClanChatContextSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	if (!World || !World->IsGameWorld() || World->GetNetMode() != NM_Standalone
		|| !FParse::Param(FCommandLine::Get(), TEXT("unattended")) || !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
	{ OutError = TEXT("clan chat requires disposable standalone headless world"); return false; }
	const int32 InitialChecks = OutChecks;
	AMMOPlayerController* PCs[4] = {}; AMMOCharacter* Pawns[4] = {}; UMMOClanComponent* Clans[4] = {}; AMMOHUD* HUDs[4] = {};
	AMMOCharacter* RetiredPawn = nullptr;
	const bool Passed = [&]()
	{
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		FMMOCharacterSaveData Before[4];
		for (int32 I = 0; I < 4; ++I)
		{
			PCs[I] = World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(), Params);
			Pawns[I] = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(2100000 + I * 100, 2100000, 5000), FRotator::ZeroRotator, Params);
			if (!PCs[I] || !Pawns[I] || !Pawns[I]->GetInventory() || !Pawns[I]->GetClanComponent())
			{ OutError = TEXT("clan chat fixture spawn"); return false; }
			PCs[I]->Possess(Pawns[I]); PCs[I]->SetActorTickEnabled(false); Pawns[I]->SetActorTickEnabled(false);
			Pawns[I]->SetActorEnableCollision(false);
			Clans[I] = Pawns[I]->GetClanComponent(); Pawns[I]->GetInventory()->FillSaveData(Before[I]);
		}
		const FString ClanA = TEXT("A") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(11);
		const FString ClanB = TEXT("B") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(11);
		const FString ClanC = TEXT("C") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(11);
		Clans[0]->ClanName = ClanA; Clans[0]->ClanMembershipId = FGuid::NewGuid(); Clans[0]->bIsLeader = true;
		Clans[2]->ClanName = ClanB; Clans[2]->ClanMembershipId = FGuid::NewGuid(); Clans[2]->bIsLeader = true;
		Clans[3]->ClanName = ClanC; Clans[3]->ClanMembershipId = FGuid::NewGuid(); Clans[3]->bIsLeader = true;
		auto InventoryUnchanged = [&]()
		{
			for (int32 I = 0; I < 4; ++I)
			{
				FMMOCharacterSaveData After; Pawns[I]->GetInventory()->FillSaveData(After);
				TArray<FString> Differences; FMMOCharacterSaveJson::FindDifferences(Before[I], After, Differences);
				if (!Differences.IsEmpty()) return false;
			}
			return true;
		};
		auto Check = [&](bool Value, const TCHAR* Label)
		{
			if (!Value) { OutError = FString::Printf(TEXT("%s (check=%d)"), Label, OutChecks + 1); return false; }
			++OutChecks; return true;
		};
		auto RejectedUnchanged = [&](AMMOCharacter* SeenPawn, uint64 Id)
		{
			FMMOCharacterSaveData Saved[4]; FString Names[4]; bool Leaders[4]; uint64 Ids[4];
			for (int32 I = 0; I < 4; ++I)
			{
				Clans[I]->FillSaveData(Saved[I]); Names[I] = Clans[I]->ClanName;
				Leaders[I] = Clans[I]->bIsLeader; Ids[I] = Clans[I]->PendingInviteId;
			}
			PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
			PCs[1]->ServerSendChat_Implementation(TEXT("/klan kabul"), SeenPawn, 0, 0, 0, Id, FGuid(), 0, FGuid());
			for (int32 I = 0; I < 4; ++I)
			{
				FMMOCharacterSaveData After; Clans[I]->FillSaveData(After);
				TArray<FString> Differences; FMMOCharacterSaveJson::FindDifferences(Saved[I], After, Differences);
				if (!Differences.IsEmpty() || Names[I] != Clans[I]->ClanName || Leaders[I] != Clans[I]->bIsLeader
					|| Ids[I] != Clans[I]->PendingInviteId) return false;
			}
			return InventoryUnchanged();
		};
		Clans[0]->CmdInvite(); const uint64 Old = Clans[1]->PendingInviteId;
		if (!Check(Old != 0 && !Clans[1]->IsInClan() && InventoryUnchanged(), TEXT("clan first nearest invite"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
		PCs[1]->OnChatCommitted(FText::FromString(TEXT("/klan kabul")), ETextCommit::OnEnter);
		if (!Check(Clans[1]->ClanName == ClanA && !Clans[1]->bIsLeader && Clans[1]->PendingInviteId == Old
			&& InventoryUnchanged(), TEXT("clan committed fresh accept"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f; PCs[1]->Say(TEXT("/klan ayril"));
		Clans[2]->CmdInvite(); const uint64 Fresh = Clans[1]->PendingInviteId;
		if (!Check(Fresh > Old && !Clans[1]->IsInClan() && Clans[0]->ClanName == ClanA && Clans[2]->ClanName == ClanB
			&& Clans[3]->ClanName == ClanC && InventoryUnchanged(), TEXT("clan Say leave then new sender"))) return false;
		if (!Check(RejectedUnchanged(Pawns[1], Old), TEXT("clan chat stale accept"))) return false;
		if (!Check(RejectedUnchanged(Pawns[1], 0), TEXT("clan chat zero generation"))) return false;
		if (!Check(RejectedUnchanged(Pawns[1], Fresh + 1), TEXT("clan chat future generation"))) return false;
		// A real possession switch with a valid invitation on the replacement pawn and an equal numeric ID.
		Clans[0]->ClanName.Reset(); Clans[0]->bIsLeader = false;
		PCs[0]->UnPossess(); PCs[1]->UnPossess(); PCs[1]->Possess(Pawns[0]);
		Clans[0]->PendingInviteId = Fresh - 1; Clans[3]->CmdInvite();
		const bool ReplacementRejected = PCs[1]->GetPawn() == Pawns[0] && Clans[0]->PendingInviteId == Fresh
			&& RejectedUnchanged(Pawns[1], Fresh);
		PCs[1]->UnPossess(); PCs[1]->Possess(Pawns[1]); PCs[0]->Possess(Pawns[0]);
		if (!Check(ReplacementRejected, TEXT("clan chat replaced pawn matching generation"))) return false;
		Clans[0]->CmdAccept();
		if (!Check(Clans[0]->ClanName == ClanC && !Clans[0]->bIsLeader && Clans[0]->PendingInviteId == Fresh
			&& InventoryUnchanged(), TEXT("replacement invitation preserved for current accept"))) return false;
		Clans[0]->CmdLeave(); Clans[0]->ClanName = ClanA; Clans[0]->ClanMembershipId = FGuid::NewGuid(); Clans[0]->bIsLeader = true;
		if (!Check(RejectedUnchanged(nullptr, Fresh), TEXT("clan chat null pawn no fallback"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f; PCs[1]->Say(TEXT("/klan kabul"));
		if (!Check(Clans[1]->ClanName == ClanB && !Clans[1]->bIsLeader && Clans[1]->PendingInviteId == Fresh
			&& InventoryUnchanged(), TEXT("clan fresh Say correct sender"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
		PCs[1]->OnChatCommitted(FText::FromString(TEXT("/klan ayril")), ETextCommit::OnEnter);
		if (!Check(!Clans[1]->IsInClan() && !Clans[1]->bIsLeader && Clans[1]->PendingInviteId == Fresh
			&& Clans[0]->ClanName == ClanA && Clans[2]->ClanName == ClanB && Clans[3]->ClanName == ClanC
			&& InventoryUnchanged(), TEXT("clan committed current leave unchanged"))) return false;

		// Native HUD buffers expose deliveries, not rendering/remote RPC transport. No world tick/load/save.
		const FString Prefix = TEXT("ZClanCtx") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(16);
		for (int32 I = 0; I < 4; ++I)
		{
			if (!PCs[I]->PlayerState) { OutError = TEXT("clan command fixture PlayerState missing"); return false; }
			PCs[I]->PlayerState->SetPlayerName(Prefix + FString::FromInt(I));
			PCs[I]->ClientSetHUD(AMMOHUD::StaticClass()); HUDs[I] = Cast<AMMOHUD>(PCs[I]->GetHUD());
			if (!HUDs[I]) { OutError = TEXT("clan command fixture HUD missing"); return false; }
			HUDs[I]->SetActorTickEnabled(false);
		}
		if (!Check(InventoryUnchanged(), TEXT("clan command HUD fixture"))) return false;
		// Controller 1 now owns a different pawn with a different clan/leadership and valid nearby targets.
		PCs[0]->UnPossess(); PCs[1]->UnPossess(); PCs[1]->Possess(Pawns[0]);
		Clans[0]->ClanName = ClanB; Clans[0]->ClanMembershipId = FGuid::NewGuid(); Clans[0]->bIsLeader = true;
		Clans[2]->ClanName = ClanB; Clans[2]->ClanMembershipId = FGuid::NewGuid(); Clans[2]->bIsLeader = false;
		if (!Check(PCs[1]->GetPawn() == Pawns[0] && Pawns[0]->GetController() == PCs[1]
			&& !Pawns[1]->GetController(), TEXT("clan command possession switch"))) return false;
		PCs[0]->Possess(Pawns[1]); // Keep the previous pawn as a controlled nearest invite candidate.
		auto Send = [&](const FString& Text, AMMOCharacter* SeenPawn)
		{
			PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
			PCs[1]->ServerSendChat_Implementation(Text, SeenPawn, 0, 0, 0, 0, FGuid(), 0, Clans[0]->ClanMembershipId);
		};
		auto RejectCommand = [&](const FString& Text, AMMOCharacter* SeenPawn)
		{
			FMMOCharacterSaveData Saved[4]; bool Leaders[4]; uint64 Ids[4]; int32 Lines[4];
			for (int32 I = 0; I < 4; ++I)
			{
				Clans[I]->FillSaveData(Saved[I]); Leaders[I] = Clans[I]->bIsLeader;
				Ids[I] = Clans[I]->PendingInviteId; Lines[I] = HUDs[I]->GetChatLinesForTests().Num();
			}
			Send(Text, SeenPawn);
			for (int32 I = 0; I < 4; ++I)
			{
				FMMOCharacterSaveData After; Clans[I]->FillSaveData(After); TArray<FString> Differences;
				FMMOCharacterSaveJson::FindDifferences(Saved[I], After, Differences);
				if (!Differences.IsEmpty() || Leaders[I] != Clans[I]->bIsLeader || Ids[I] != Clans[I]->PendingInviteId
					|| Lines[I] != HUDs[I]->GetChatLinesForTests().Num()) return false;
			}
			return InventoryUnchanged();
		};
		const FString Kick = TEXT("/klan at ") + PCs[2]->PlayerState->GetPlayerName();
		const FString Commands[] = {TEXT("/klan ayril"), Kick, TEXT("/klan davet"), TEXT("/KLAN LISTE"), TEXT("/g old pawn message"), TEXT("/klan")};
		for (AMMOCharacter* SeenPawn : {Pawns[1], static_cast<AMMOCharacter*>(nullptr), Pawns[3]})
		{
			for (const FString& Command : Commands)
			{
				if (!Check(RejectCommand(Command, SeenPawn), *FString::Printf(TEXT("stale clan command mutated/delivered: %s"), *Command))) return false;
			}
		}
		Clans[0]->ClanName.Reset(); Clans[0]->bIsLeader = false;
		const FString Create = TEXT("/klan kur ") + ClanA;
		if (!Check(RejectCommand(Create, Pawns[1]), TEXT("stale clan creation routed to replacement"))) return false;
		if (!Check(RejectCommand(Create, nullptr), TEXT("null clan creation used server fallback"))) return false;
		Clans[0]->ClanName = ClanB; Clans[0]->ClanMembershipId = FGuid::NewGuid(); Clans[0]->bIsLeader = true;
		const int32 CallerLines = HUDs[1]->GetChatLinesForTests().Num();
		const int32 VictimLines = HUDs[2]->GetChatLinesForTests().Num();
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f; PCs[1]->Say(TEXT("/G fresh message"));
		if (!Check(HUDs[1]->GetChatLinesForTests().Num() == CallerLines + 1
			&& HUDs[2]->GetChatLinesForTests().Num() == VictimLines + 1
			&& HUDs[0]->GetChatLinesForTests().IsEmpty() && HUDs[3]->GetChatLinesForTests().IsEmpty(), TEXT("fresh clan message routing"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
		PCs[1]->OnChatCommitted(FText::FromString(TEXT("/klan liste")), ETextCommit::OnEnter);
		if (!Check(HUDs[1]->GetChatLinesForTests().Num() == CallerLines + 2
			&& HUDs[2]->GetChatLinesForTests().Num() == VictimLines + 1, TEXT("fresh clan list owner delivery"))) return false;
		// The same-frame inviter already issued an earlier invite: its 3s cooldown must still apply.
		const int32 BeforeInvite = HUDs[1]->GetChatLinesForTests().Num();
		Send(TEXT("/klan davet"), Pawns[0]);
		if (!Check(Clans[1]->PendingInviteId == Fresh && !Clans[1]->IsInClan()
			&& HUDs[1]->GetChatLinesForTests().Num() == BeforeInvite + 1, TEXT("fresh clan invite cooldown feedback"))) return false;
		Clans[0]->bIsLeader = false;
		Send(Kick, Pawns[0]);
		if (!Check(Clans[2]->ClanName == ClanB && InventoryUnchanged(), TEXT("fresh nonleader kick bypassed authority"))) return false;
		Clans[0]->bIsLeader = true; Pawns[0]->GetStats()->bIsDead = true;
		Send(Kick, Pawns[0]); Pawns[0]->GetStats()->bIsDead = false;
		if (!Check(Clans[2]->ClanName == ClanB && InventoryUnchanged(), TEXT("fresh dead kick bypassed authority"))) return false;
		Send(Kick, Pawns[0]);
		if (!Check(!Clans[2]->IsInClan() && Clans[0]->ClanName == ClanB && InventoryUnchanged(), TEXT("fresh leader clan kick"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f; PCs[1]->Say(TEXT("/klan ayril"));
		if (!Check(!Clans[0]->IsInClan() && !Clans[0]->bIsLeader && InventoryUnchanged(), TEXT("fresh replacement clan leave"))) return false;
		const int32 BeforeInvalidCreate = HUDs[1]->GetChatLinesForTests().Num();
		Send(TEXT("/klan kur !"), Pawns[0]);
		if (!Check(!Clans[0]->IsInClan() && HUDs[1]->GetChatLinesForTests().Num() == BeforeInvalidCreate + 1
			&& InventoryUnchanged(), TEXT("fresh clan creation validation feedback"))) return false;
		// Same pawn, different membership; valid load applies the actual runtime entry path without disk IO.
		auto LoadClan = [&](int32 I, const FString& Name, bool Leader = false)
		{
			FMMOCharacterSaveData Data; Data.ClanName = Name; Data.bClanLeader = Leader; Clans[I]->ApplySaveData(Data);
		};
		auto LinesCopy = [&]()
		{
			TArray<TArray<FMMOChatLine>> Result; for (AMMOHUD* H : HUDs) Result.Add(H->GetChatLinesForTests()); return Result;
		};
		auto RejectMember = [&](const FString& Text, const FGuid& Seen)
		{
			FMMOCharacterSaveData Saved[4]; FGuid Ids[4]; uint64 Invites[4]; const auto Lines = LinesCopy();
			for (int32 I = 0; I < 4; ++I) { Clans[I]->FillSaveData(Saved[I]); Ids[I] = Clans[I]->ClanMembershipId; Invites[I] = Clans[I]->PendingInviteId; }
			PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
			PCs[1]->ServerSendChat_Implementation(Text, Pawns[0], 0, 0, 0, 0, FGuid(), 0, Seen);
			for (int32 I = 0; I < 4; ++I)
			{
				FMMOCharacterSaveData After; Clans[I]->FillSaveData(After); TArray<FString> Differences;
				FMMOCharacterSaveJson::FindDifferences(Saved[I], After, Differences);
				const auto& NewLines = HUDs[I]->GetChatLinesForTests();
				if (!Differences.IsEmpty() || Ids[I] != Clans[I]->ClanMembershipId || Invites[I] != Clans[I]->PendingInviteId || NewLines.Num() != Lines[I].Num()) return false;
				for (int32 L = 0; L < NewLines.Num(); ++L)
					if (NewLines[L].Text != Lines[I][L].Text || NewLines[L].Color != Lines[I][L].Color || NewLines[L].Time != Lines[I][L].Time) return false;
			}
			return InventoryUnchanged();
		};
		auto FreshSay = [&](const FString& Text) { PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f; PCs[1]->Say(Text); };
		auto Delivered = [&](const TArray<TArray<FMMOChatLine>>& BeforeLines, const FString& Text, bool Joined)
		{
			for (int32 I = 0; I < 4; ++I)
			{
				const auto& Lines = HUDs[I]->GetChatLinesForTests(); const bool Receives = I == 1 || I == 2 || (Joined && I == 0);
				if (Lines.Num() != BeforeLines[I].Num() + (Receives ? 1 : 0)) return false;
				if (Receives && (Lines.Last().Text != FString::Printf(TEXT("[Klan] %s: %s"), *PCs[1]->PlayerState->GetPlayerName(), *Text)
					|| Lines.Last().Color != FLinearColor(0.4f, 0.9f, 0.4f))) return false;
			}
			return InventoryUnchanged();
		};
		LoadClan(0, ClanA, true); LoadClan(1, ClanA); LoadClan(2, ClanB, true); LoadClan(3, ClanC, true);
		const FGuid OldA = Clans[0]->ClanMembershipId;
		if (!Check(OldA.IsValid() && Clans[1]->ClanMembershipId.IsValid() && OldA != Clans[1]->ClanMembershipId && InventoryUnchanged(), TEXT("clan membership load identities"))) return false;
		Clans[0]->CmdLeave(); const bool InvalidAfterLeave = !Clans[0]->ClanMembershipId.IsValid(); LoadClan(0, ClanB);
		const FGuid CurrentB = Clans[0]->ClanMembershipId;
		if (!Check(InvalidAfterLeave && CurrentB.IsValid() && CurrentB != OldA && !Clans[0]->bIsLeader && Clans[2]->bIsLeader, TEXT("clan membership A to B context"))) return false;
		if (!Check(RejectMember(TEXT("/g old_A_secret"), OldA), TEXT("old clan message leaked into replacement clan"))) return false;
		if (!Check(RejectMember(TEXT("/klan ayril"), OldA), TEXT("old clan leave removed replacement membership"))) return false;
		for (const FGuid& Id : {FGuid(), Clans[2]->ClanMembershipId, FGuid::NewGuid()})
		{
			if (!Check(RejectMember(TEXT("/g wrong_context"), Id), TEXT("invalid clan chat context delivered"))) return false;
			if (!Check(RejectMember(TEXT("/klan ayril"), Id), TEXT("invalid clan leave context mutated"))) return false;
		}
		auto Lines = LinesCopy(); FreshSay(TEXT("/G fresh_B"));
		if (!Check(Delivered(Lines, TEXT("fresh_B"), false), TEXT("fresh Say membership clan delivery"))) return false;
		Lines = LinesCopy(); PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
		PCs[1]->OnChatCommitted(FText::FromString(TEXT("/g committed_B")), ETextCommit::OnEnter);
		if (!Check(Delivered(Lines, TEXT("committed_B"), false), TEXT("fresh Enter membership clan delivery"))) return false;
		LoadClan(1, ClanB); Lines = LinesCopy(); FreshSay(TEXT("/g continuing_B"));
		if (!Check(Clans[0]->ClanMembershipId == CurrentB && Delivered(Lines, TEXT("continuing_B"), true), TEXT("continuous member clan join routing"))) return false;
		Clans[0]->CmdLeave(); LoadClan(0, ClanB); const FGuid RejoinedB = Clans[0]->ClanMembershipId;
		if (!Check(RejoinedB.IsValid() && RejoinedB != CurrentB, TEXT("same clan rejoin reused context"))) return false;
		if (!Check(RejectMember(TEXT("/g old_B_secret"), CurrentB), TEXT("same clan rejoin accepted old message"))) return false;
		if (!Check(RejectMember(TEXT("/klan ayril"), CurrentB), TEXT("same clan rejoin accepted old leave"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
		PCs[1]->OnChatCommitted(FText::FromString(TEXT("/klan ayril")), ETextCommit::OnEnter);
		if (!Check(!Clans[0]->IsInClan() && !Clans[0]->ClanMembershipId.IsValid() && Clans[1]->ClanName == ClanB && Clans[2]->bIsLeader, TEXT("fresh Enter membership leave"))) return false;
		LoadClan(0, ClanB); FreshSay(TEXT("/klan ayril"));
		if (!Check(!Clans[0]->IsInClan() && !Clans[0]->ClanMembershipId.IsValid(), TEXT("fresh Say membership leave"))) return false;
		LoadClan(0, ClanB); const FGuid BeforeKick = Clans[0]->ClanMembershipId, LeaderId = Clans[2]->ClanMembershipId;
		Clans[2]->CmdKick(PCs[1]->PlayerState->GetPlayerName());
		if (!Check(!Clans[0]->IsInClan() && !Clans[0]->ClanMembershipId.IsValid() && Clans[2]->ClanMembershipId == LeaderId, TEXT("kick context invalidation changed leader context"))) return false;
		LoadClan(0, ClanB);
		if (!Check(Clans[0]->ClanMembershipId.IsValid() && Clans[0]->ClanMembershipId != BeforeKick
			&& RejectMember(TEXT("/g before_kick_secret"), BeforeKick) && RejectMember(TEXT("/klan ayril"), BeforeKick), TEXT("kick rejoin reused sender membership"))) return false;
		LoadClan(0, TEXT("!"));
		if (!Check(!Clans[0]->IsInClan() && !Clans[0]->ClanMembershipId.IsValid(), TEXT("invalid saved clan retained context"))) return false;
		LoadClan(0, ClanB); LoadClan(0, FString());
		if (!Check(!Clans[0]->IsInClan() && !Clans[0]->ClanMembershipId.IsValid(), TEXT("empty saved clan retained context"))) return false;
		LoadClan(0, ClanB); const FGuid BeforeReload = Clans[0]->ClanMembershipId; LoadClan(0, ClanB);
		if (!Check(Clans[0]->ClanMembershipId.IsValid() && Clans[0]->ClanMembershipId != BeforeReload
			&& RejectMember(TEXT("/g before_reload"), BeforeReload) && RejectMember(TEXT("/klan ayril"), BeforeReload), TEXT("same clan reload retained stale context"))) return false;
		// A fresh sender avoids the prior fixture's invite cooldown; no clock/timer bypass.
		Clans[0]->CmdLeave(); RetiredPawn = Pawns[0]; RetiredPawn->SetTravelSaveCommitted(true);
		PCs[1]->UnPossess();
		Pawns[0] = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(2100000, 2100000, 5000), FRotator::ZeroRotator, Params);
		if (!Pawns[0]) { OutError = TEXT("clan authority membership fixture pawn"); return false; }
		PCs[1]->Possess(Pawns[0]); Pawns[0]->SetActorTickEnabled(false); Pawns[0]->SetActorEnableCollision(false);
		Clans[0] = Pawns[0]->GetClanComponent(); Pawns[0]->GetInventory()->FillSaveData(Before[0]);
		Clans[1]->CmdLeave(); Clans[2]->CmdLeave();
		LoadClan(0, ClanA, true); const FGuid AuthorityA = Clans[0]->ClanMembershipId;
		Clans[0]->CmdLeave(); LoadClan(0, ClanB, true); LoadClan(2, ClanB);
		const FGuid AuthorityB = Clans[0]->ClanMembershipId;
		if (!Check(PCs[1]->GetPawn() == Pawns[0] && AuthorityA.IsValid() && AuthorityB.IsValid() && AuthorityA != AuthorityB
			&& Clans[0]->bIsLeader && !Clans[2]->bIsLeader && Clans[2]->ClanName == ClanB && !Clans[1]->IsInClan(), TEXT("clan authority membership A to B setup"))) return false;
		const FString AuthorityCommands[] = {Kick, TEXT("/klan davet"), TEXT("/KLAN LISTE")};
		for (const FString& Command : AuthorityCommands)
			if (!Check(RejectMember(Command, AuthorityA), *FString::Printf(TEXT("old membership authority command changed replacement: %s"), *Command))) return false;
		for (const FGuid& Id : {FGuid(), Clans[2]->ClanMembershipId, FGuid::NewGuid()})
			for (const FString& Command : AuthorityCommands)
				if (!Check(RejectMember(Command, Id), *FString::Printf(TEXT("invalid membership authority command changed replacement: %s"), *Command))) return false;
		Lines = LinesCopy(); PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
		PCs[1]->OnChatCommitted(FText::FromString(TEXT("/KLAN LISTE")), ETextCommit::OnEnter);
		if (!Check(HUDs[1]->GetChatLinesForTests().Num() == Lines[1].Num() + 1
			&& HUDs[1]->GetChatLinesForTests().Last().Text.Contains(ClanB)
			&& HUDs[1]->GetChatLinesForTests().Last().Text.Contains(PCs[2]->PlayerState->GetPlayerName())
			&& HUDs[0]->GetChatLinesForTests().Num() == Lines[0].Num() && HUDs[2]->GetChatLinesForTests().Num() == Lines[2].Num()
			&& HUDs[3]->GetChatLinesForTests().Num() == Lines[3].Num(), TEXT("fresh Enter authority clan list"))) return false;
		const uint64 BeforeAuthorityInvite = Clans[1]->PendingInviteId; Lines = LinesCopy(); FreshSay(TEXT("/KLAN DAVET"));
		if (!Check(Clans[1]->PendingInviteId > BeforeAuthorityInvite && !Clans[1]->IsInClan()
			&& Clans[0]->ClanMembershipId == AuthorityB && Clans[0]->bIsLeader
			&& HUDs[1]->GetChatLinesForTests().Num() == Lines[1].Num() + 1 && HUDs[0]->GetChatLinesForTests().Num() == Lines[0].Num() + 1
			&& HUDs[2]->GetChatLinesForTests().Num() == Lines[2].Num() && HUDs[3]->GetChatLinesForTests().Num() == Lines[3].Num(), TEXT("fresh Say authority clan invite"))) return false;
		PCs[0]->LastChatServerTime = World->GetTimeSeconds() - 1.f; PCs[0]->Say(TEXT("/klan kabul"));
		if (!Check(Clans[1]->ClanName == ClanB && Clans[1]->ClanMembershipId.IsValid() && !Clans[1]->bIsLeader
			&& Clans[0]->ClanMembershipId == AuthorityB, TEXT("authority invitation accepted with recipient context"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
		PCs[1]->OnChatCommitted(FText::FromString(Kick), ETextCommit::OnEnter);
		if (!Check(!Clans[2]->IsInClan() && !Clans[2]->ClanMembershipId.IsValid() && Clans[0]->ClanMembershipId == AuthorityB
			&& Clans[0]->bIsLeader && Clans[1]->ClanName == ClanB && InventoryUnchanged(), TEXT("fresh Enter authority clan kick"))) return false;
		Clans[0]->CmdLeave(); LoadClan(0, ClanB, true); Clans[1]->CmdLeave(); LoadClan(2, ClanB);
		const FGuid AuthorityRejoin = Clans[0]->ClanMembershipId;
		if (!Check(AuthorityRejoin.IsValid() && AuthorityRejoin != AuthorityB && Clans[0]->bIsLeader && !Clans[1]->IsInClan()
			&& Clans[2]->ClanName == ClanB, TEXT("authority same-name rejoin setup"))) return false;
		for (const FString& Command : AuthorityCommands)
			if (!Check(RejectMember(Command, AuthorityB), *FString::Printf(TEXT("same-name rejoin accepted old authority command: %s"), *Command))) return false;
		LoadClan(0, ClanB, true); const FGuid AuthorityReload = Clans[0]->ClanMembershipId;
		if (!Check(AuthorityReload.IsValid() && AuthorityReload != AuthorityRejoin && Clans[0]->bIsLeader, TEXT("authority same-name reload setup"))) return false;
		for (const FString& Command : AuthorityCommands)
			if (!Check(RejectMember(Command, AuthorityRejoin), *FString::Printf(TEXT("same-name reload accepted old authority command: %s"), *Command))) return false;
		Lines = LinesCopy(); FreshSay(TEXT("/klan liste"));
		if (!Check(HUDs[1]->GetChatLinesForTests().Num() == Lines[1].Num() + 1
			&& HUDs[1]->GetChatLinesForTests().Last().Text.Contains(ClanB)
			&& HUDs[0]->GetChatLinesForTests().Num() == Lines[0].Num() && HUDs[2]->GetChatLinesForTests().Num() == Lines[2].Num()
			&& HUDs[3]->GetChatLinesForTests().Num() == Lines[3].Num() && InventoryUnchanged(), TEXT("fresh Say authority list after reload"))) return false;
		return Check(InventoryUnchanged(), TEXT("clan membership routing changed inventory"));
	}();
	// Same synchronous frame: no load timer fires; erase fixture clans before teardown/leader election.
	for (int32 I = 0; I < 4; ++I) { if (Clans[I]) { Clans[I]->ClanName.Reset(); Clans[I]->bIsLeader = false; } }
	for (AMMOCharacter* Pawn : Pawns) if (IsValid(Pawn)) Pawn->SetTravelSaveCommitted(true);
	for (int32 I = 0; I < 4; ++I) { if (PCs[I]) { PCs[I]->UnPossess(); PCs[I]->Destroy(); } if (Pawns[I]) Pawns[I]->Destroy(); }
	if (IsValid(RetiredPawn)) RetiredPawn->Destroy();
	for (AMMOHUD* HUD : HUDs) { if (IsValid(HUD)) HUD->Destroy(); }
	if (Passed) UE_LOG(LogTemp, Log, TEXT("[MMO_CLAN_CHAT_CONTEXT_TEST] PASS checks=%d"), OutChecks - InitialChecks);
	return Passed;
}
#endif

#if !UE_BUILD_SHIPPING
bool AMMOPlayerController::RunFriendsChatContextSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() != NM_Standalone
		|| !FParse::Param(FCommandLine::Get(), TEXT("unattended"))
		|| !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
	{ OutError = TEXT("disposable standalone headless world required"); return false; }
	AMMOPlayerController* PCs[4] = {}; AMMOCharacter* Pawns[4] = {};
	UMMOFriendsComponent* Friends[4] = {}; AMMOHUD* HUDs[4] = {};
	const bool Passed = [&]()
	{
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FString Prefix = TEXT("ZFC") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(16);
		FString Names[4]; FMMOCharacterSaveData Inventory[4];
		for (int32 I = 0; I < 4; ++I)
		{
			PCs[I] = World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(), Params);
			Pawns[I] = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(2400000 + I * 100, 2400000, 5000), FRotator::ZeroRotator, Params);
			if (!PCs[I] || !Pawns[I] || !Pawns[I]->GetInventory() || !Pawns[I]->GetFriendsComponent())
			{ OutError = TEXT("friend chat fixture spawn"); return false; }
			PCs[I]->Possess(Pawns[I]); PCs[I]->SetActorTickEnabled(false); Pawns[I]->SetActorTickEnabled(false);
			Pawns[I]->SetActorEnableCollision(false); Friends[I] = Pawns[I]->GetFriendsComponent();
			if (!PCs[I]->PlayerState) { OutError = TEXT("friend chat fixture PlayerState missing"); return false; }
			Names[I] = Prefix + FString::FromInt(I); PCs[I]->PlayerState->SetPlayerName(Names[I]);
			PCs[I]->ClientSetHUD(AMMOHUD::StaticClass()); HUDs[I] = Cast<AMMOHUD>(PCs[I]->GetHUD());
			if (!HUDs[I]) { OutError = TEXT("friend chat fixture HUD missing"); return false; }
			HUDs[I]->SetActorTickEnabled(false); Pawns[I]->GetInventory()->FillSaveData(Inventory[I]);
		}
		auto Check = [&](bool Value, const TCHAR* Label)
		{
			++OutChecks; if (!Value) OutError = FString::Printf(TEXT("%s (check=%d)"), Label, OutChecks); return Value;
		};
		auto InventoryUnchanged = [&]()
		{
			for (int32 I = 0; I < 4; ++I)
			{
				FMMOCharacterSaveData After; Pawns[I]->GetInventory()->FillSaveData(After); TArray<FString> Differences;
				FMMOCharacterSaveJson::FindDifferences(Inventory[I], After, Differences); if (!Differences.IsEmpty()) return false;
			}
			return true;
		};
		// Controller identity/name stays live, but the old and replacement pawn own different lists.
		PCs[3]->UnPossess(); PCs[1]->UnPossess(); PCs[1]->Possess(Pawns[3]); PCs[3]->Possess(Pawns[1]);
		Friends[3]->Friends.Add(Names[2]); Friends[2]->Friends.Add(Names[1]);
		Friends[0]->CmdAdd(Names[1]); // Real pending consent on replacement, not on the sending old pawn.
		if (!Check(PCs[1]->GetPawn() == Pawns[3] && Friends[3]->HasFriend(Names[2])
			&& !Friends[3]->HasFriend(Names[0]) && InventoryUnchanged(), TEXT("friend actual possession fixture"))) return false;
		auto Send = [&](const FString& Text, AMMOCharacter* SeenPawn, uint64 SeenFriendId = 0)
		{
			PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
			PCs[1]->ServerSendChat_Implementation(Text, SeenPawn, 0, 0, 0, 0, FGuid(), SeenFriendId, FGuid());
		};
		auto Reject = [&](const FString& Text, AMMOCharacter* SeenPawn)
		{
			TArray<FString> Lists[4]; int32 Lines[4];
			for (int32 I = 0; I < 4; ++I) { Lists[I] = Friends[I]->Friends; Lines[I] = HUDs[I]->GetChatLinesForTests().Num(); }
			Send(Text, SeenPawn);
			for (int32 I = 0; I < 4; ++I)
			{
				if (Lists[I] != Friends[I]->Friends || Lines[I] != HUDs[I]->GetChatLinesForTests().Num()) return false;
			}
			return InventoryUnchanged();
		};
		const FString Remove = TEXT("/arkadas sil ") + Names[2];
		const FString Add = TEXT("/arkadas ekle ") + Names[2];
		const FString Commands[] = {Remove, TEXT("/arkadas kabul"), TEXT("/arkadas accept"), Add, TEXT("/ARKADAS LISTE"), TEXT("/arkadas")};
		for (AMMOCharacter* SeenPawn : {Pawns[1], static_cast<AMMOCharacter*>(nullptr), Pawns[0]})
		{
			for (const FString& Command : Commands)
			{
				if (!Check(Reject(Command, SeenPawn), *FString::Printf(TEXT("stale friend command mutated/delivered: %s"), *Command))) return false;
			}
		}
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f; PCs[1]->Say(TEXT("/arkadas accept"));
		if (!Check(Friends[3]->Friends.Num() == 2 && Friends[3]->HasFriend(Names[0]) && Friends[0]->HasFriend(Names[1])
			&& Friends[1]->Friends.IsEmpty() && Friends[2]->HasFriend(Names[1]), TEXT("fresh consent did not preserve correct pending/list"))) return false;
		Send(TEXT("/arkadas kabul"), Pawns[3]);
		if (!Check(Friends[3]->Friends.Num() == 2 && Friends[0]->Friends.Num() == 1, TEXT("duplicate consent added entries"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
		PCs[1]->OnChatCommitted(FText::FromString(Remove), ETextCommit::OnEnter);
		if (!Check(!Friends[3]->HasFriend(Names[2]) && !Friends[2]->HasFriend(Names[1])
			&& Friends[3]->HasFriend(Names[0]), TEXT("fresh mutual removal"))) return false;
		Send(Add, Pawns[3]);
		if (!Check(!Friends[3]->HasFriend(Names[2]) && !Friends[2]->HasFriend(Names[1]), TEXT("fresh request skipped target consent"))) return false;
		Friends[2]->CmdAccept();
		if (!Check(Friends[3]->HasFriend(Names[2]) && Friends[2]->HasFriend(Names[1]), TEXT("fresh re-add consent"))) return false;
		const int32 BeforeList = HUDs[1]->GetChatLinesForTests().Num();
		const int32 OtherLines = HUDs[0]->GetChatLinesForTests().Num() + HUDs[2]->GetChatLinesForTests().Num() + HUDs[3]->GetChatLinesForTests().Num();
		Send(TEXT("/arkadas"), Pawns[3]);
		if (!Check(HUDs[1]->GetChatLinesForTests().Num() == BeforeList + Friends[3]->Friends.Num() + 1
			&& OtherLines == HUDs[0]->GetChatLinesForTests().Num() + HUDs[2]->GetChatLinesForTests().Num() + HUDs[3]->GetChatLinesForTests().Num(), TEXT("fresh list owner delivery"))) return false;
		const int32 BeforeInvalid = HUDs[1]->GetChatLinesForTests().Num();
		Send(TEXT("/arkadas ekle"), Pawns[3]);
		if (!Check(HUDs[1]->GetChatLinesForTests().Num() == BeforeInvalid + 1 && Friends[3]->Friends.Num() == 2, TEXT("fresh empty add feedback"))) return false;
		Send(TEXT("/arkadas ekle ") + Names[1], Pawns[3]);
		if (!Check(!Friends[3]->HasFriend(Names[1]) && Friends[3]->Friends.Num() == 2, TEXT("fresh self add accepted"))) return false;
		if (!Check(InventoryUnchanged(), TEXT("friend chat changed inventory"))) return false;
		// A consumed invitation must never accept a later B on the very same pawn.
		Friends[3]->CmdRemove(Names[0]); Friends[3]->CmdRemove(Names[2]);
		if (!Check(Friends[3]->Friends.IsEmpty() && Friends[0]->Friends.IsEmpty() && Friends[2]->Friends.IsEmpty(), TEXT("generation fixture mutual cleanup"))) return false;
		Friends[0]->CmdAdd(Names[1]); const uint64 Old = Friends[3]->GetPendingInviteId();
		if (!Check(Old > 0 && Friends[3]->Friends.IsEmpty(), TEXT("friend A generation issuance"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f; PCs[1]->Say(TEXT("/arkadas kabul"));
		if (!Check(Friends[3]->HasFriend(Names[0]) && Friends[0]->HasFriend(Names[1]) && Friends[3]->GetPendingInviteId() == Old, TEXT("fresh A acceptance/reset generation"))) return false;
		Friends[3]->CmdRemove(Names[0]); Friends[2]->CmdAdd(Names[1]); const uint64 Fresh = Friends[3]->GetPendingInviteId();
		if (!Check(Fresh > Old && Friends[3]->Friends.IsEmpty() && Friends[2]->Friends.IsEmpty(), TEXT("friend B replacement generation"))) return false;
		for (uint64 SeenId : {Old, static_cast<uint64>(0), Fresh + 1})
		{
			for (const TCHAR* Command : {TEXT("/arkadas kabul"), TEXT("/arkadas accept")})
			{
				Send(Command, Pawns[3], SeenId);
				if (!Check(Friends[3]->Friends.IsEmpty() && Friends[2]->Friends.IsEmpty() && Friends[0]->Friends.IsEmpty()
					&& Friends[3]->GetPendingInviteId() == Fresh, TEXT("stale friend accept consumed replacement B"))) return false;
			}
		}
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f;
		PCs[1]->OnChatCommitted(FText::FromString(TEXT("/arkadas accept")), ETextCommit::OnEnter);
		if (!Check(Friends[3]->Friends.Num() == 1 && Friends[3]->HasFriend(Names[2]) && Friends[2]->HasFriend(Names[1])
			&& Friends[0]->Friends.IsEmpty() && Friends[3]->GetPendingInviteId() == Fresh, TEXT("fresh B acceptance after stale requests"))) return false;
		Send(TEXT("/arkadas kabul"), Pawns[3], Fresh);
		if (!Check(Friends[3]->Friends.Num() == 1 && Friends[2]->Friends.Num() == 1, TEXT("same generation replay duplicate"))) return false;
		Friends[3]->CmdRemove(Names[2]); Friends[2]->CmdAdd(Names[1]); const uint64 Reissued = Friends[3]->GetPendingInviteId();
		Send(TEXT("/arkadas accept"), Pawns[3], Fresh);
		if (!Check(Reissued > Fresh && Friends[3]->Friends.IsEmpty() && Friends[2]->Friends.IsEmpty(), TEXT("same inviter reissue accepted old ID"))) return false;
		PCs[1]->LastChatServerTime = World->GetTimeSeconds() - 1.f; PCs[1]->Say(TEXT("/arkadas kabul"));
		if (!Check(Friends[3]->HasFriend(Names[2]) && Friends[2]->HasFriend(Names[1]) && Friends[3]->GetPendingInviteId() == Reissued, TEXT("fresh same inviter reissue"))) return false;
		if (!Check(InventoryUnchanged(), TEXT("friend generation changed inventory"))) return false;
		return UMMOFriendsComponent::RunInviteGenerationBoundarySelfTest(Pawns[0], Pawns[2], Pawns[3], OutError, OutChecks);
	}();
	// Same synchronous frame; no auto-load/save timer fires. Suppress synthetic cleanup presence.
	for (AMMOCharacter* Pawn : Pawns) if (IsValid(Pawn)) Pawn->SetTravelSaveCommitted(true);
	for (AMMOPlayerController* PC : PCs) if (IsValid(PC)) { PC->UnPossess(); PC->Destroy(); }
	for (AMMOCharacter* Pawn : Pawns) if (IsValid(Pawn)) Pawn->Destroy();
	for (AMMOHUD* HUD : HUDs) if (IsValid(HUD)) HUD->Destroy();
	return Passed;
}
namespace
{
	struct FMMOFriendsChatContextTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestFriendsChatContext"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = AMMOPlayerController::RunFriendsChatContextSelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_FRIENDS_CHAT_CONTEXT_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOFriendsChatContextTestExec GMMOFriendsChatContextTestExec;
}
#endif

void AMMOPlayerController::Say(const FString& Message)
{
	SendChatWithContext(Message);
}

void AMMOPlayerController::SendChatWithContext(const FString& Message)
{
	AMMOCharacter* Me = Cast<AMMOCharacter>(GetPawn());
	const UMMOTradeComponent* Trade = Me ? Me->GetTradeComponent() : nullptr;
	const UMMOPartyComponent* Party = Me ? Me->GetPartyComponent() : nullptr;
	const UMMOClanComponent* Clan = Me ? Me->GetClanComponent() : nullptr;
	const UMMOFriendsComponent* Friends = Me ? Me->GetFriendsComponent() : nullptr;
	ServerSendChat(Message, Me, Trade ? Trade->PendingInviteId : 0,
		Trade && Trade->bTradeActive ? Trade->TradeSessionId : 0, Party ? Party->PendingInviteId : 0,
		Clan ? Clan->PendingInviteId : 0, Party ? Party->PartyMembershipId : FGuid(), Friends ? Friends->GetPendingInviteId() : 0, Clan ? Clan->ClanMembershipId : FGuid());
}

void AMMOPlayerController::ServerSendChat_Implementation(const FString& Message, AMMOCharacter* SeenCommandPawn,
	uint64 SeenTradeInviteId, uint64 SeenActiveTradeSessionId, uint64 SeenPartyInviteId, uint64 SeenClanInviteId, const FGuid& SeenPartyMembershipId, uint64 SeenFriendInviteId, const FGuid& SeenClanMembershipId)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// NETWORK_AUDIT O1: girdi boyutu ve hiz siniri TEK yerde, hicbir komut dali parse edilmeden
	// once uygulanir. Uzunluk siniri en basta (sinirsiz bir Message ile Replace/Split/yankilanan
	// hata mesajlari bant ve CPU yakmasin); throttle, mesaj gecerli ya da hatali olsun her
	// denemeyi sayar (eskiden yalniz bazi dallar LastChatServerTime'i guncelliyordu).
	static constexpr int32 MaxChatInputLength = 200;
	const float Now = World->GetTimeSeconds();
	if (Now - LastChatServerTime < 0.7f)
	{
		return;
	}
	LastChatServerTime = Now;

	FString Msg = Message.Left(MaxChatInputLength).Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" ")).TrimStartAndEnd();
	// NETWORK_AUDIT D5/D9: kontrol karakterleri ve cift yonlu/sifir genislikli bicim karakterleri
	// (U+200B-200F, U+202A-202E, U+2066-2069) satir gorunumunu taklit/bozmak icin kullanilabilir: bosluga cevrilir.
	// SECURITY_AUDIT D-9: ASCII <32 + DEL (127) + C1 kontrolleri (0x80-0x9F) de ayni sekilde temizlenir.
	for (TCHAR& C : Msg)
	{
		if (C < 32 || (C >= 127 && C <= 0x9F) || (C >= 0x200B && C <= 0x200F) || (C >= 0x202A && C <= 0x202E) || (C >= 0x2066 && C <= 0x2069))
		{
			C = TEXT(' ');
		}
	}
	Msg.TrimStartAndEndInline();
	uint8 Channel = 0;
	if (Msg.Equals(TEXT("/p"), ESearchCase::IgnoreCase))
	{
		return; // bos parti mesaji: genel sohbete "/p" olarak sizmasin
	}
	if (Msg.Equals(TEXT("/yardim"), ESearchCase::IgnoreCase) || Msg.Equals(TEXT("/help"), ESearchCase::IgnoreCase))
	{
		ClientReceiveChat(FString(), TEXT("Komutlar: /p mesaj (parti), /parti (davet; kabul, ret, ayril), /w isim mesaj (fisilti), /who (cevrimici oyuncular), /trade (ticaret; kabul, iptal), /klan (kur ad, davet, kabul, ayril, at isim, liste), /g mesaj (klan sohbeti), /arkadas (ekle isim, kabul, sil isim, liste), /yardim"), 2);
		return;
	}
	if (Msg.Equals(TEXT("/who"), ESearchCase::IgnoreCase))
	{
		FString List;
		int32 Count = 0;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const AMMOPlayerController* PC = Cast<AMMOPlayerController>(It->Get());
			if (!PC || !PC->PlayerState)
			{
				continue;
			}
			const AMMOCharacter* Ch = Cast<AMMOCharacter>(PC->GetPawn());
			const int32 Lv = (Ch && Ch->GetStats()) ? Ch->GetStats()->Level : 0;
			List += FString::Printf(TEXT("%s%s (Lv%d)"), Count > 0 ? TEXT(", ") : TEXT(""), *PC->PlayerState->GetPlayerName(), Lv);
			++Count;
		}
		ClientReceiveChat(FString(), FString::Printf(TEXT("Cevrimici (%d): %s"), Count, *List), 2);
		return;
	}
	// Parti: "/parti" (en yakin oyuncuya davet), "/parti kabul", "/parti ret", "/parti ayril".
	if (Msg.Equals(TEXT("/parti"), ESearchCase::IgnoreCase) || Msg.StartsWith(TEXT("/parti "), ESearchCase::IgnoreCase))
	{
		UMMOPartyComponent* Party = nullptr;
		if (const AMMOCharacter* Me = Cast<AMMOCharacter>(GetPawn()))
		{
			Party = Me->GetPartyComponent();
		}
		const FString Sub = Msg.RightChop(6).TrimStartAndEnd();
		if (!Party)
		{
			ClientReceiveChat(FString(), TEXT("Parti su an kullanilamaz."), 2);
		}
		else if (Sub.IsEmpty() || Sub.Equals(TEXT("davet"), ESearchCase::IgnoreCase))
		{
			// Bind issuing chat intent before nearest selection, cooldown, or pending mutation.
			if (!IsValid(SeenCommandPawn) || SeenCommandPawn != GetPawn()) return;
			if (Party->IsInParty()
				? (!SeenPartyMembershipId.IsValid() || SeenPartyMembershipId != Party->PartyMembershipId)
				: SeenPartyMembershipId.IsValid()) return;
			Party->RequestInviteNearest();
		}
		else if (Sub.Equals(TEXT("kabul"), ESearchCase::IgnoreCase) || Sub.Equals(TEXT("accept"), ESearchCase::IgnoreCase))
		{
			if (SeenCommandPawn == GetPawn()) Party->RequestAcceptSeen(SeenPartyInviteId);
		}
		else if (Sub.Equals(TEXT("ret"), ESearchCase::IgnoreCase) || Sub.Equals(TEXT("decline"), ESearchCase::IgnoreCase))
		{
			if (SeenCommandPawn == GetPawn()) Party->RequestDeclineSeen(SeenPartyInviteId);
		}
		else if (Sub.Equals(TEXT("ayril"), ESearchCase::IgnoreCase) || Sub.Equals(TEXT("leave"), ESearchCase::IgnoreCase))
		{
			if (SeenCommandPawn == GetPawn()) Party->RequestLeaveSeen(SeenPartyMembershipId);
		}
		else
		{
			ClientReceiveChat(FString(), TEXT("Kullanim: /parti, /parti kabul, /parti ret, /parti ayril"), 2);
		}
		return;
	}
	// Ticaret: "/trade" (en yakin oyuncuya davet), "/trade kabul", "/trade iptal".
	if (Msg.Equals(TEXT("/trade"), ESearchCase::IgnoreCase) || Msg.StartsWith(TEXT("/trade "), ESearchCase::IgnoreCase))
	{
		UMMOTradeComponent* Trade = nullptr;
		if (const AMMOCharacter* Me = Cast<AMMOCharacter>(GetPawn()))
		{
			Trade = Me->GetTradeComponent();
		}
		const FString Sub = Msg.RightChop(6).TrimStartAndEnd();
		if (!Trade)
		{
			ClientReceiveChat(FString(), TEXT("Ticaret su an kullanilamaz."), 2);
		}
			else if (Sub.IsEmpty())
			{
				if (!IsValid(SeenCommandPawn) || SeenCommandPawn != GetPawn()) return;
				Trade->RequestInviteNearest();
		}
		else if (Sub.Equals(TEXT("kabul"), ESearchCase::IgnoreCase) || Sub.Equals(TEXT("accept"), ESearchCase::IgnoreCase))
		{
			if (SeenCommandPawn == GetPawn()) Trade->RequestAcceptSeen(SeenTradeInviteId);
		}
		else if (Sub.Equals(TEXT("iptal"), ESearchCase::IgnoreCase) || Sub.Equals(TEXT("cancel"), ESearchCase::IgnoreCase))
		{
			if (SeenCommandPawn == GetPawn())
			{
				if (SeenActiveTradeSessionId != 0) Trade->RequestCancelSeen(SeenActiveTradeSessionId);
				else Trade->RequestDeclineInviteSeen(SeenTradeInviteId);
			}
		}
		else
		{
			ClientReceiveChat(FString(), TEXT("Kullanim: /trade, /trade kabul, /trade iptal"), 2);
		}
		return;
	}
	// Klan: "/klan kur <ad>", "/klan davet", "/klan kabul", "/klan ayril", "/klan at <isim>", "/klan liste".
	if (Msg.Equals(TEXT("/klan"), ESearchCase::IgnoreCase) || Msg.StartsWith(TEXT("/klan "), ESearchCase::IgnoreCase))
	{
		// Bind every clan command to the pawn that sent it, before reading replacement state.
		if (!IsValid(SeenCommandPawn) || SeenCommandPawn != GetPawn()) return;
		UMMOClanComponent* Clan = nullptr;
		if (const AMMOCharacter* Me = Cast<AMMOCharacter>(GetPawn()))
		{
			Clan = Me->GetClanComponent();
		}
		const FString Rest = Msg.RightChop(5).TrimStartAndEnd();
		FString Sub = Rest;
		FString Arg;
		Rest.Split(TEXT(" "), &Sub, &Arg);
		Arg = Arg.TrimStartAndEnd();
		if (!Clan)
		{
			ClientReceiveChat(FString(), TEXT("Klan su an kullanilamaz."), 2);
		}
		else if (Sub.Equals(TEXT("kur"), ESearchCase::IgnoreCase))
		{
			Clan->CmdCreate(Arg);
		}
		else if (Sub.Equals(TEXT("davet"), ESearchCase::IgnoreCase))
		{
			if (Clan->IsInClan() && (!SeenClanMembershipId.IsValid() || SeenClanMembershipId != Clan->ClanMembershipId)) return;
			Clan->CmdInvite();
		}
		else if (Sub.Equals(TEXT("kabul"), ESearchCase::IgnoreCase))
		{
			Clan->CmdAcceptSeen(SeenClanInviteId);
		}
		else if (Sub.Equals(TEXT("ayril"), ESearchCase::IgnoreCase))
		{
			if (Clan->IsInClan() && (!SeenClanMembershipId.IsValid() || SeenClanMembershipId != Clan->ClanMembershipId)) return;
			Clan->CmdLeave();
		}
		else if (Sub.Equals(TEXT("at"), ESearchCase::IgnoreCase))
		{
			if (Clan->IsInClan() && (!SeenClanMembershipId.IsValid() || SeenClanMembershipId != Clan->ClanMembershipId)) return;
			Clan->CmdKick(Arg);
		}
		else if (Sub.Equals(TEXT("liste"), ESearchCase::IgnoreCase))
		{
			if (Clan->IsInClan() && (!SeenClanMembershipId.IsValid() || SeenClanMembershipId != Clan->ClanMembershipId)) return;
			Clan->CmdList();
		}
		else
		{
			ClientReceiveChat(FString(), TEXT("Kullanim: /klan kur ad, /klan davet, /klan kabul, /klan ayril, /klan at isim, /klan liste"), 2);
		}
		return;
	}
	// Arkadas listesi: "/arkadas ekle <isim>", "/arkadas kabul", "/arkadas sil <isim>" veya "/arkadas liste".
	if (Msg.Equals(TEXT("/arkadas"), ESearchCase::IgnoreCase) || Msg.StartsWith(TEXT("/arkadas "), ESearchCase::IgnoreCase))
	{
		// Reject old consent/list mutations before selecting the replacement pawn component.
		if (!IsValid(SeenCommandPawn) || SeenCommandPawn != GetPawn()) return;
		UMMOFriendsComponent* Friends = nullptr;
		if (const AMMOCharacter* Me = Cast<AMMOCharacter>(GetPawn()))
		{
			Friends = Me->GetFriendsComponent();
		}
		const FString Rest = Msg.RightChop(8).TrimStartAndEnd();
		FString Sub = Rest;
		FString Arg;
		Rest.Split(TEXT(" "), &Sub, &Arg);
		Arg = Arg.TrimStartAndEnd();
		if (!Friends)
		{
			ClientReceiveChat(FString(), TEXT("Arkadas listesi su an kullanilamaz."), 2);
		}
		else if (Sub.IsEmpty() || Sub.Equals(TEXT("liste"), ESearchCase::IgnoreCase))
		{
			Friends->CmdList();
		}
		else if (Sub.Equals(TEXT("ekle"), ESearchCase::IgnoreCase))
		{
			Friends->CmdAdd(Arg);
		}
		else if (Sub.Equals(TEXT("kabul"), ESearchCase::IgnoreCase) || Sub.Equals(TEXT("accept"), ESearchCase::IgnoreCase))
		{
			Friends->CmdAcceptSeen(SeenFriendInviteId);
		}
		else if (Sub.Equals(TEXT("sil"), ESearchCase::IgnoreCase))
		{
			Friends->CmdRemove(Arg);
		}
		else
		{
			ClientReceiveChat(FString(), TEXT("Kullanim: /arkadas ekle isim, /arkadas kabul, /arkadas sil isim, /arkadas liste"), 2);
		}
		return;
	}
	// Klan sohbeti: "/g mesaj" (yalniz ayni klan adindaki cevrimici oyunculara).
	if (Msg.Equals(TEXT("/g"), ESearchCase::IgnoreCase))
	{
		return; // bos klan mesaji: genel sohbete "/g" olarak sizmasin
	}
	if (Msg.StartsWith(TEXT("/g "), ESearchCase::IgnoreCase))
	{
		if (!IsValid(SeenCommandPawn) || SeenCommandPawn != GetPawn()) return;
		const FString ClanText = Msg.RightChop(3).TrimStart().Left(120);
		if (ClanText.IsEmpty())
		{
			return;
		}
		if (const AMMOCharacter* Me = Cast<AMMOCharacter>(GetPawn()))
		{
			if (UMMOClanComponent* Clan = Me->GetClanComponent())
			{
				if (Clan->IsInClan() && (!SeenClanMembershipId.IsValid() || SeenClanMembershipId != Clan->ClanMembershipId)) return;
				Clan->CmdChat(ClanText);
				return;
			}
		}
		ClientReceiveChat(FString(), TEXT("Klan su an kullanilamaz."), 2);
		return;
	}
	if (Msg.StartsWith(TEXT("/p "), ESearchCase::IgnoreCase))
	{
		Channel = 1;
		Msg = Msg.RightChop(3).TrimStart();
	}

	// Fisilti: "/w Isim mesaj" (ya da /f). Hedef ada gore bulunur; yalniz hedefe ve gondericiye gider.
	// A party message body is literal text; it cannot select a different recipient/channel.
	if (Channel == 0 && (Msg.StartsWith(TEXT("/w "), ESearchCase::IgnoreCase) || Msg.StartsWith(TEXT("/f "), ESearchCase::IgnoreCase)))
	{
		FString Rest = Msg.RightChop(3).TrimStart();
		FString TargetName, WhisperText;
		if (!Rest.Split(TEXT(" "), &TargetName, &WhisperText))
		{
			ClientReceiveChat(FString(), TEXT("Kullanim: /w Isim mesaj"), 2);
			return;
		}
		WhisperText = WhisperText.TrimStart().Left(120);
		if (TargetName.IsEmpty() || WhisperText.IsEmpty())
		{
			ClientReceiveChat(FString(), TEXT("Kullanim: /w Isim mesaj"), 2);
			return;
		}

		const FString From = PlayerState ? PlayerState->GetPlayerName() : FString(TEXT("?"));
		AMMOPlayerController* Target = nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AMMOPlayerController* PC = Cast<AMMOPlayerController>(It->Get());
			if (PC && PC != this && PC->PlayerState && PC->PlayerState->GetPlayerName().Equals(TargetName, ESearchCase::IgnoreCase))
			{
				Target = PC;
				break;
			}
		}
		if (!Target)
		{
			ClientReceiveChat(FString(), FString::Printf(TEXT("%s adli oyuncu bulunamadi."), *TargetName), 2);
			return;
		}
		Target->ClientReceiveChat(From, WhisperText, 3);
		ClientReceiveChat(Target->PlayerState->GetPlayerName(), WhisperText, 4);
		return;
	}
	Msg = Msg.Left(120);
	if (Msg.IsEmpty())
	{
		return;
	}

	const FString Sender = PlayerState ? PlayerState->GetPlayerName() : FString(TEXT("?"));

	if (Channel == 1)
	{
		// The party observed while sending may have ended or the controller may own another pawn.
		if (!IsValid(SeenCommandPawn) || SeenCommandPawn != GetPawn()) return;
		AMMOCharacter* Me = Cast<AMMOCharacter>(GetPawn());
		UMMOPartyComponent* Party = Me ? Me->GetPartyComponent() : nullptr;
		if (!Party || !Party->IsInParty())
		{
			ClientReceiveChat(FString(), TEXT("Bir partide degilsin; mesaj gonderilmedi."), 2);
			return;
		}
		if (!SeenPartyMembershipId.IsValid() || SeenPartyMembershipId != Party->PartyMembershipId) return;
		TArray<AMMOCharacter*> Members;
		Party->GetPartyInRange(Me->GetActorLocation(), 1.0e7f, Members);
		Members.AddUnique(Me);
		for (AMMOCharacter* M : Members)
		{
			if (AMMOPlayerController* PC = M ? Cast<AMMOPlayerController>(M->GetController()) : nullptr)
			{
				PC->ClientReceiveChat(Sender, Msg, 1);
			}
		}
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AMMOPlayerController* PC = Cast<AMMOPlayerController>(It->Get()))
		{
			PC->ClientReceiveChat(Sender, Msg, 0);
		}
	}
}

void AMMOPlayerController::ClientReceiveChat_Implementation(const FString& Sender, const FString& Message, uint8 Channel)
{
	AMMOHUD* HUD = Cast<AMMOHUD>(GetHUD());
	if (!HUD)
	{
		return;
	}
	FLinearColor Col = FLinearColor::White;
	FString Line;
	if (Channel == 2)
	{
		Col = FLinearColor(1.f, 0.65f, 0.2f);
		Line = Message;
	}
	else if (Channel == 1)
	{
		Col = FLinearColor(0.45f, 0.85f, 1.f);
		Line = FString::Printf(TEXT("[Parti] %s: %s"), *Sender, *Message);
	}
	else if (Channel == 3)
	{
		Col = FLinearColor(1.f, 0.55f, 0.85f);
		Line = FString::Printf(TEXT("[%s fisildiyor] %s"), *Sender, *Message);
	}
	else if (Channel == 4)
	{
		Col = FLinearColor(1.f, 0.55f, 0.85f);
		Line = FString::Printf(TEXT("[%s adli oyuncuya] %s"), *Sender, *Message);
	}
	else if (Channel == 5)
	{
		Col = FLinearColor(0.4f, 0.9f, 0.4f);
		Line = FString::Printf(TEXT("[Klan] %s: %s"), *Sender, *Message);
	}
	else
	{
		Line = FString::Printf(TEXT("%s: %s"), *Sender, *Message);
	}
	// Sistem mesaji birden fazla satir tasiyabilir ('\n'; ornegin /arkadas liste tek RPC'de gelir):
	// HUD tek satir cizdigi icin satirlara bol. Kullanici girdisindeki '\n' sunucuda zaten ayiklanir.
	TArray<FString> Parts;
	Line.ParseIntoArray(Parts, TEXT("\n"), true);
	for (const FString& Part : Parts)
	{
		HUD->AddChatLine(Part, Col);
	}
}

void AMMOPlayerController::OnChatKey()
{
	if (!ChatBox.IsValid())
	{
		bChatOpenRequested = true;
	}
}

void AMMOPlayerController::OpenChatInput()
{
	if (!IsLocalController() || ChatBox.IsValid() || !FSlateApplication::IsInitialized())
	{
		return;
	}
	UWorld* World = GetWorld();
	UGameViewportClient* VC = World ? World->GetGameViewport() : nullptr;
	if (!VC)
	{
		return;
	}

	SAssignNew(ChatContainer, SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(16.f, 0.f, 0.f, 96.f))
		[
			SNew(SBox)
			.WidthOverride(400.f)
			[
				SAssignNew(ChatBox, SEditableTextBox)
				.HintText(FText::FromString(TEXT("Mesaj yaz - Enter gonder, Esc iptal, /p parti, /w isim mesaj")))
				.ClearKeyboardFocusOnCommit(false)
				.OnTextCommitted(FOnTextCommitted::CreateUObject(this, &AMMOPlayerController::OnChatCommitted))
			]
		];
	VC->AddViewportWidgetContent(ChatContainer.ToSharedRef(), 50);
	FSlateApplication::Get().SetKeyboardFocus(ChatBox, EFocusCause::SetDirectly);
}

void AMMOPlayerController::CloseChatInput()
{
	if (ChatContainer.IsValid())
	{
		UWorld* World = GetWorld();
		if (UGameViewportClient* VC = World ? World->GetGameViewport() : nullptr)
		{
			VC->RemoveViewportWidgetContent(ChatContainer.ToSharedRef());
		}
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		}
	}
	ChatContainer.Reset();
	ChatBox.Reset();
}

void AMMOPlayerController::OnChatCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		const FString Msg = Text.ToString().TrimStartAndEnd();
		if (!Msg.IsEmpty())
		{
			SendChatWithContext(Msg);
		}
	}
	// Enter, Esc (OnCleared) ya da odak kaybi: her durumda kutuyu kapat.
	bChatCloseRequested = true;
}

void AMMOPlayerController::OnJumpPressed()
{
	// UI penceresi acikken ziplama.
	if (IsAnyUIWindowOpen())
	{
		return;
	}
	if (ACharacter* Ch = Cast<ACharacter>(GetPawn()))
	{
		Ch->Jump();
	}
}

void AMMOPlayerController::OnJumpReleased()
{
	if (ACharacter* Ch = Cast<ACharacter>(GetPawn()))
	{
		Ch->StopJumping();
	}
}

void AMMOPlayerController::OnCloseShop()
{
	if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		if (H->IsShopOpen())
		{
			H->CloseShop();
		}
		if (H->IsQuestWindowOpen())
		{
			H->CloseQuestWindow();
		}
	}
}

AMMOCharacter* AMMOPlayerController::GetSelectedTarget() const
{
	AMMOCharacter* Target = SelectedTarget.Get();
	if (!Target || !Target->GetStats() || Target->GetStats()->bIsDead)
	{
		return nullptr;
	}
	return Target;
}

void AMMOPlayerController::ReleaseAttackTarget(const AMMOCharacter* Rejected)
{
	// Reddedilen hedef artik kovalanan hedef degilse (oyuncu baska yere tikladi) dokunma.
	if (!Rejected || TargetEnemy.Get() != Rejected)
	{
		return;
	}
	RejectedTarget = TargetEnemy;
	RejectedTargetTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	TargetEnemy.Reset();
	StopMovement();
}

void AMMOPlayerController::SelectTarget(AMMOCharacter* NewTarget)
{
	// KO TargetSelect: hedef degistiyse auto-attack iptal edilir.
	if (SelectedTarget.Get() != NewTarget)
	{
		TargetEnemy.Reset();
	}

	SelectedTarget = NewTarget;

	// KO RotateTo: karakter hedefe doner (yurumez, saldirmaz).
	APawn* P = GetPawn();
	if (P && NewTarget)
	{
		const FVector Dir = (NewTarget->GetActorLocation() - P->GetActorLocation()).GetSafeNormal2D();
		if (!Dir.IsNearlyZero())
		{
			P->SetActorRotation(Dir.Rotation());
		}
	}
}

void AMMOPlayerController::OnTargetNearestEnemy()
{
	APawn* P = GetPawn();
	UWorld* World = GetWorld();
	if (!P || !World)
	{
		return;
	}

	// En yakin canli mobu bul (KO: CharacterGetByNearestEnemy).
	AMMOMob* Nearest = MMOTargeting::FindNearestMob(this, P->GetActorLocation(), TargetSearchRange);

	SelectTarget(Nearest);

	if (!Nearest && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Silver, TEXT("Yakinlarda dusman yok"));
	}
}

void AMMOPlayerController::OnToggleAttack()
{
	// KO KM_TOGGLE_ATTACK: saldiriyorsa durdur...
	if (TargetEnemy.IsValid())
	{
		TargetEnemy.Reset();
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Silver, TEXT("Saldiri durduruldu"));
		}
		return;
	}

	// ...degilse secili hedefe saldiriya gec (TryStartAttack).
	AMMOCharacter* Target = GetSelectedTarget();
	if (Target)
	{
		TargetEnemy = Target;
		bHasMoveTarget = false;
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Silver, TEXT("Hedef yok - Z ile sec"));
	}
}

bool AMMOPlayerController::IsSkillBookOpen() const
{
	if (const AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		return H->IsSkillBookOpen();
	}
	return false;
}

bool AMMOPlayerController::IsAnyUIWindowOpen() const
{
	if (const AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		return H->IsSkillBookOpen() || H->IsInventoryOpen() || H->IsShopOpen() || H->IsQuestWindowOpen() || H->IsStatWindowOpen() || H->IsWarehouseOpen() || H->IsTradeOpen();
	}
	return false;
}

void AMMOPlayerController::OnToggleSkillBook()
{
	if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		H->ToggleSkillBook();
	}
}

void AMMOPlayerController::OnToggleInventory()
{
	if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		H->ToggleInventory();
	}
}

void AMMOPlayerController::OnToggleCharacter()
{
	if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		H->ToggleStatWindow();
	}
}

bool AMMOPlayerController::IsInAttackMode() const
{
	const AMMOCharacter* Enemy = TargetEnemy.Get();
	return Enemy && Enemy->GetStats() && !Enemy->GetStats()->bIsDead;
}

void AMMOPlayerController::OnLeftClickPressed()
{
	// UI penceresi acikken tiklama pencereye gider, karakteri hareket ettirmez.
	if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		float MX = 0.f, MY = 0.f;
		if (GetMousePosition(MX, MY) && H->HandleClassChoiceClick(FVector2D(MX, MY)))
		{
			return;
		}
		if (GetMousePosition(MX, MY) && H->HandleZoneTravelClick(FVector2D(MX, MY)))
		{
			return;
		}
		if (H->IsAnyWindowOpen())
		{
			// Tiklama, imlecin ustundeki acik pencereye gider (X ile kapatma dahil); karakter hareket etmez.
			if (GetMousePosition(MX, MY))
			{
				H->HandleWindowClick(FVector2D(MX, MY));
			}
			return;
		}
	}

	// Pencere yok: sol tus basildi. Basili kaldigi surece her kare imlecin oldugu yere git
	// (basili-tut-git). Sol tus kamerayi cevirmez.
	bLeftPressed = true;
}

void AMMOPlayerController::DoLeftClickAction()
{
	// Bir pencere acikken dunyaya tiklama gitmesin (dukkan/gorev/envanter vs.).
	if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		if (H->IsAnyWindowOpen())
		{
			return;
		}
	}

	// Fare altindaki dunyayi trace et.
	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, false, Hit) || !Hit.bBlockingHit)
	{
		return;
	}

	// Saticiya tiklandi mi? (menzil icindeyse dukkani ac)
	if (AMMOVendorNPC* Vendor = Cast<AMMOVendorNPC>(Hit.GetActor()))
	{
		APawn* MyPawn = GetPawn();
		if (MyPawn && FVector::Dist(MyPawn->GetActorLocation(), Vendor->GetActorLocation()) <= 500.f)
		{
			if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
			{
				H->OpenShop();
				// Kisa karsilama replikleri (istemci-tarafi flourish; LLM gerektirmez).
				const FString Greet = Vendor->PickGreeting();
				if (!Greet.IsEmpty())
				{
					H->ShowNpcDialogue(Vendor->VendorName, Greet);
				}
			}
		}
		else if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("Saticiya yaklas"));
		}
		return;
	}

	// Gorevliye tiklandi mi? (menzil icindeyse gorev penceresini ac)
	if (AMMOQuestGiverNPC* QuestGiver = Cast<AMMOQuestGiverNPC>(Hit.GetActor()))
	{
		APawn* MyPawn = GetPawn();
		if (MyPawn && FVector::Dist(MyPawn->GetActorLocation(), QuestGiver->GetActorLocation()) <= 500.f)
		{
			if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
			{
				H->OpenQuestWindow();
				const FString Greet = QuestGiver->PickGreeting();
				if (!Greet.IsEmpty())
				{
					H->ShowNpcDialogue(QuestGiver->NpcName, Greet);
				}
			}
		}
		else if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("Gorevliye yaklas"));
		}
		return;
	}

	// Konusan NPC'ye tiklandi mi? (menzil icindeyse LLM'den replik iste)
	if (AMMOTalkerNPC* Talker = Cast<AMMOTalkerNPC>(Hit.GetActor()))
	{
		APawn* MyPawn = GetPawn();
		if (MyPawn && FVector::Dist(MyPawn->GetActorLocation(), Talker->GetActorLocation()) <= 500.f)
		{
			// Kisa spam korumasi (ust uste tiklamada yeni istek atma).
			const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
			if (Now - LastNpcTalkRequestTime >= 1.5f)
			{
				LastNpcTalkRequestTime = Now;
				ServerRequestNpcDialogue(Talker);
			}
		}
		else if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("NPC'ye yaklas"));
		}
		return;
	}

	// Sifaciya tiklandi mi? (menzil icindeyse sunucudan iyilestirme iste — altin duser)
	if (AMMOHealerNPC* Healer = Cast<AMMOHealerNPC>(Hit.GetActor()))
	{
		APawn* MyPawn = GetPawn();
		if (MyPawn && FVector::Dist(MyPawn->GetActorLocation(), Healer->GetActorLocation()) <= 500.f)
		{
			// Kisa spam korumasi (talker'daki desenin ayni si): sol tus basili tutulurken
			// DoLeftClickAction her karede calisiyor; throttle olmadan DoT ile tam-can
			// arasinda gidip gelen oyuncu her karede tekrar altin dusurebiliyordu.
			const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
			if (Now - LastHealRequestTime >= 1.5f)
			{
				LastHealRequestTime = Now;
				ServerRequestHeal(Healer);
			}
		}
		else if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("Sifaciya yaklas"));
		}
		return;
	}

	// Depocuya tiklandi mi? (menzil icindeyse depo penceresini ac; tasima sunucuda dogrulanir)
	if (AMMOWarehouseNPC* Keeper = Cast<AMMOWarehouseNPC>(Hit.GetActor()))
	{
		APawn* MyPawn = GetPawn();
		if (MyPawn && FVector::Dist(MyPawn->GetActorLocation(), Keeper->GetActorLocation()) <= 500.f)
		{
			if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
			{
				H->OpenWarehouse();
				const FString Greet = Keeper->PickGreeting();
				if (!Greet.IsEmpty())
				{
					H->ShowNpcDialogue(Keeper->WarehouseName, Greet);
				}
			}
		}
		else if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("Depocuya yaklas"));
		}
		return;
	}

	// Bir karaktere (mob veya diger oyuncu) tiklandi mi? Kendine tiklama sayilmaz.
	// (Oyuncuya vurma PvP kontrolu sunucuda yapilir; NPC'ler AMMOCharacter degil, otomatik haric.)
	AMMOCharacter* HitChar = Cast<AMMOCharacter>(Hit.GetActor());
	if (HitChar && HitChar != GetPawn() && HitChar->GetStats() && !HitChar->GetStats()->bIsDead)
	{
		// Hedefe git ve saldir.
		SelectedTarget = HitChar;
		bHasMoveTarget = false;
		// Yakin zamanda saldirisi reddedilen hedef (PvP kapali vb.): yalniz sec, kovalama.
		const float ClickNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		const bool bRecentlyRejected = RejectedTarget.Get() == HitChar
			&& ClickNow - RejectedTargetTime < RejectedTargetSeconds;
		// Oyuncu hedefi PvP kapaliyken (GameState aynasi) ya da parti/klan/hub kurali yuzunden vurulamazsa
		// kovalama baslatma: yalniz sec. Sunucu karari degismez; bu yalniz bosa giden yuruyus/RPC'yi keser.
		bool bCanChase = true;
		if (const AMMOCharacter* Me = Cast<AMMOCharacter>(GetPawn()))
		{
			const TCHAR* BlockReason = nullptr;
			if (!Cast<AMMOMob>(HitChar) && (!Me->IsPvPAllowed() || !Me->CanHarmPlayer(HitChar, BlockReason)))
			{
				bCanChase = false;
			}
		}
		if (!bRecentlyRejected && bCanChase)
		{
			TargetEnemy = HitChar;
		}
		else if (!bCanChase)
		{
			TargetEnemy.Reset();
		}
	}
	else
	{
		// Zemine tiklandi: oraya yuru.
		MoveToLocation = Hit.ImpactPoint;
		bHasMoveTarget = true;
		TargetEnemy.Reset();
	}
}

void AMMOPlayerController::OnLeftClickReleased()
{
	// Surukle-birak: skill kitabindan tasinan skill bir hotbar slotuna birakildiysa ata.
	if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		float MX = 0.f, MY = 0.f;
		if (GetMousePosition(MX, MY))
		{
			H->HandleSkillBookRelease(FVector2D(MX, MY));
		}
	}

	// Birakildi: tikla-git/saldiri/NPC eylemini garantile (cok kisa dokunuslar tick'e denk gelmemis olabilir).
	if (bLeftPressed)
	{
		DoLeftClickAction();
	}
	bLeftPressed = false;
}

void AMMOPlayerController::OnRightMousePressed()
{
	// Pencere acikken sag tik, imlecin ustundeki pencereye calisir (kitap: slota koy; envanter: +1 dene), kamera donmez.
	if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		if (H->IsAnyWindowOpen())
		{
			float MX = 0.f, MY = 0.f;
			if (GetMousePosition(MX, MY))
			{
				H->HandleWindowRightClick(FVector2D(MX, MY));
			}
			return;
		}
	}

	bRotatingCamera = true;
	// Imleci gizle/yakala ki fare kenara dayanmadan sinirsiz donebilsin.
	bShowMouseCursor = false;
}

void AMMOPlayerController::OnRightMouseReleased()
{
	bRotatingCamera = false;
	// Imleci geri getir (tikla-git icin).
	bShowMouseCursor = true;
}

void AMMOPlayerController::OnMouseX(float Value)
{
	if (Value == 0.f || IsAnyUIWindowOpen())
	{
		return;
	}

	// Kamera cevirme SADECE sag tus basiliyken. Sol tus kamerayi cevirmez.
	if (bRotatingCamera)
	{
		const float LookSensitivity = 1.6f;
		AddYawInput(Value * LookSensitivity);
	}
}

void AMMOPlayerController::OnMouseY(float Value)
{
	if (Value == 0.f || IsAnyUIWindowOpen())
	{
		return;
	}

	// Kamera cevirme SADECE sag tus basiliyken (ters-Y duzeltmesi). Sol tus kamerayi cevirmez.
	if (bRotatingCamera)
	{
		const float LookSensitivity = 1.6f;
		AddPitchInput(-Value * LookSensitivity);
	}
}

void AMMOPlayerController::SetOrientToMovement(bool bOrient)
{
	if (ACharacter* Ch = Cast<ACharacter>(GetPawn()))
	{
		if (UCharacterMovementComponent* Move = Ch->GetCharacterMovement())
		{
			if (Move->bOrientRotationToMovement != bOrient)
			{
				Move->bOrientRotationToMovement = bOrient;
			}
		}
	}
}

bool AMMOPlayerController::HasNavmesh() const
{
	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	return NavSys && NavSys->GetMainNavData() != nullptr;
}

void AMMOPlayerController::DrivePawnTowardLocation(const FVector& Dest)
{
	APawn* P = GetPawn();
	if (!P)
	{
		return;
	}

	// Tikla-git: karakter gittigi yone donsun (WASD strafe'i kapatmis olabilir).
	SetOrientToMovement(true);

	if (HasNavmesh())
	{
		// Yol bulma: cok sik yeniden hesaplama; aradaki karelerde path takibi surer.
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - LastPlayerRepathTime > 0.2f)
		{
			LastPlayerRepathTime = Now;
			UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, Dest);
		}
	}
	else
	{
		// Fallback: duz-cizgi.
		const FVector Dir = (Dest - P->GetActorLocation()).GetSafeNormal2D();
		P->AddMovementInput(Dir, 1.f);
	}
}

void AMMOPlayerController::DrivePawnTowardActor(AActor* Goal)
{
	APawn* P = GetPawn();
	if (!P || !Goal)
	{
		return;
	}

	// Hedefe kosarken de yone donsun.
	SetOrientToMovement(true);

	if (HasNavmesh())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - LastPlayerRepathTime > 0.2f)
		{
			LastPlayerRepathTime = Now;
			UAIBlueprintHelperLibrary::SimpleMoveToActor(this, Goal);
		}
	}
	else
	{
		const FVector Dir = (Goal->GetActorLocation() - P->GetActorLocation()).GetSafeNormal2D();
		P->AddMovementInput(Dir, 1.f);
	}
}

void AMMOPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// Depo penceresi: depocudan uzaklasinca (sunucu tasimayi reddeder) kendiliginden kapansin.
	if (AMMOHUD* WH = Cast<AMMOHUD>(GetHUD()); WH && WH->IsWarehouseOpen())
	{
		bool bNear = false;
		if (const APawn* WP = GetPawn())
		{
			for (TActorIterator<AMMOWarehouseNPC> It(GetWorld()); It; ++It)
			{
				if (FVector::DistSquared(WP->GetActorLocation(), It->GetActorLocation()) <= 700.f * 700.f)
				{
					bNear = true;
					break;
				}
			}
		}
		if (!bNear)
		{
			WH->CloseWarehouse();
		}
	}

	// Sohbet kutusu ac/kapa istekleri (callback/tus olayinin disinda guvenle).
	if (bChatCloseRequested)
	{
		bChatCloseRequested = false;
		CloseChatInput();
	}
	if (bChatOpenRequested)
	{
		bChatOpenRequested = false;
		OpenChatInput();
	}

	APawn* P = GetPawn();
	if (!P)
	{
		return;
	}

	// UI penceresi acikken: kamera donusunu zorla kapat + imleci serbest birak
	// (pencere sag-tik basiliyken acilmis olabilir; yoksa imlec takilir ve fare kamerayi cevirir).
	const bool bBookOpen = IsAnyUIWindowOpen();
	if (bBookOpen)
	{
		bRotatingCamera = false;
		if (!bShowMouseCursor)
		{
			bShowMouseCursor = true;
		}
	}

	// Olumden onceki zemin tiklamasi veya otomatik saldiri, canlanmadan sonra
	// oyuncuyu eski tehlikeli konuma geri goturmesin. Her istemci kendi yerel
	// hareket hedefini temizler; sunucu canlanma/hasar kararlarini ayri verir.
	if (const AMMOCharacter* MeChar = Cast<AMMOCharacter>(P))
	{
		if (const UMMOStatsComponent* MyStats = MeChar->GetStats(); MyStats && MyStats->bIsDead)
		{
			TargetEnemy.Reset();
			SelectedTarget.Reset();
			bHasMoveTarget = false;
			bLeftPressed = false;
			StopMovement();
			return;
		}
	}

	// --- WASD klavye hareketi (KO tarzi: W/S ileri-geri, A/D yerinde DONUS) ---
	// Enhanced Input mapping'lerini temizledigimiz icin WASD'yi burada C++ ile suruyoruz.
	// W/S = karakterin baktigi yonde ileri/geri. A/D = karakteri sola/saga dondur (yana KAYMAZ).
	if (!bBookOpen && !ChatBox.IsValid())
	{
		float ForwardAxis = 0.f;
		float TurnAxis = 0.f;
		if (IsInputKeyDown(EKeys::W)) { ForwardAxis += 1.f; }
		if (IsInputKeyDown(EKeys::S)) { ForwardAxis -= 1.f; }
		if (IsInputKeyDown(EKeys::D)) { TurnAxis += 1.f; }  // saga don
		if (IsInputKeyDown(EKeys::A)) { TurnAxis -= 1.f; }  // sola don

		if (ForwardAxis != 0.f || TurnAxis != 0.f)
		{
			// Hareket yonune donmeyi kapat ki donusu biz kontrol edelim, ileri giderken kaymasin.
			SetOrientToMovement(false);

			// Klavye kontrolu tikla-git yolunu ve hedefi iptal eder (manuel kontrol oncelikli).
			StopMovement();
			bHasMoveTarget = false;
			TargetEnemy.Reset();

			// A/D: karakteri yerinde dondur (yaw). Yana kayma yok.
			if (TurnAxis != 0.f)
			{
				const float YawSpeed = 140.f; // derece/sn
				P->AddActorWorldRotation(FRotator(0.f, TurnAxis * YawSpeed * DeltaTime, 0.f));
			}

			// W/S: karakterin (donmus) bakis yonunde ileri/geri.
			if (ForwardAxis != 0.f)
			{
				const FRotator YawRot(0.f, P->GetActorRotation().Yaw, 0.f);
				const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
				P->AddMovementInput(Forward, ForwardAxis);
			}
			return;
		}
	}

	// Sol tus basili: her kare imlecin oldugu yere git/saldir (tikla-git / basili-tut-git).
	if (bLeftPressed && !bBookOpen)
	{
		DoLeftClickAction();
	}

	// Dusman hedefi varsa: ona yol bul, menzilde saldir.
	if (TargetEnemy.IsValid())
	{
		AMMOCharacter* Enemy = TargetEnemy.Get();
		// Oyuncu oldu/canlanma korumasinda ise otomatik saldiri hedefini birak (yoksa canlanir
		// canlanmaz mob'a yuruyup korumayi kendisi bitirir - olum dongusu geri gelir).
		const AMMOCharacter* MeChar = Cast<AMMOCharacter>(P);
		const bool bMeDeadOrProtected = MeChar && ((MeChar->GetStats() && MeChar->GetStats()->bIsDead) || MeChar->IsSpawnProtected());
		if (!Enemy->GetStats() || Enemy->GetStats()->bIsDead || bMeDeadOrProtected)
		{
			TargetEnemy.Reset();
			StopMovement();
			return;
		}

		const FVector ToEnemy = Enemy->GetActorLocation() - P->GetActorLocation();
		if (ToEnemy.Size2D() > AttackReach)
		{
			DrivePawnTowardActor(Enemy);
		}
		else if (AMMOCharacter* MyChar = Cast<AMMOCharacter>(P))
		{
			// Menzilde: secili hedefi sunucuya tasiyarak saldir. nullptr kullanmak PvP
			// hedefini kaybedip her zaman en yakin moba dusuyordu.
			StopMovement();
			MyChar->AttackTarget(Enemy);
		}
	}
	// Aksi halde zemin hedefi varsa oraya yol bul.
	else if (bHasMoveTarget)
	{
		const FVector ToLoc = MoveToLocation - P->GetActorLocation();
		if (ToLoc.Size2D() > MoveAcceptanceRadius)
		{
			DrivePawnTowardLocation(MoveToLocation);
		}
		else
		{
			bHasMoveTarget = false;
			StopMovement();
		}
	}
}

void AMMOPlayerController::ServerRequestNpcDialogue_Implementation(AMMOTalkerNPC* Npc)
{
	if (!Npc)
	{
		return;
	}

	// Sunucu-otoriter mesafe dogrulamasi (istemciye guvenme).
	APawn* MyPawn = GetPawn();
	if (!MyPawn || FVector::Dist(MyPawn->GetActorLocation(), Npc->GetActorLocation()) > 700.f)
	{
		return;
	}

	// Sunucu-otoriter hiz siniri: istemci throttle'ina guvenme (hileli istemci
	// RPC'yi bombardiman edip her cagrida ucretli bir LLM istegi tetikleyebilir).
	const float ServerNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (ServerNow - LastNpcTalkServerTime < NpcTalkServerCooldown)
	{
		return;
	}
	LastNpcTalkServerTime = ServerNow;

	// Oyuncu baglami (replige renk katsin).
	FString PlayerInfo = TEXT("bir maceraci");
	if (AMMOCharacter* C = Cast<AMMOCharacter>(MyPawn))
	{
		if (UMMOStatsComponent* St = C->GetStats())
		{
			PlayerInfo = FString::Printf(TEXT("seviye %d bir maceraci"), St->Level);
		}
	}

	const FString SpeakerName = Npc->NpcName;
	const FString FallbackLine = Npc->PickFallbackLine();

	UGameInstance* GI = GetGameInstance();
	UMMODialogueSubsystem* Dialogue = GI ? GI->GetSubsystem<UMMODialogueSubsystem>() : nullptr;
	if (!Dialogue)
	{
		ClientReceiveNpcDialogue(SpeakerName, FallbackLine);
		return;
	}

	// LLM cevabi asenkron gelir; basarisizsa (anahtar/kredi yok) NPC'ye ozgu yedek replige duser.
	TWeakObjectPtr<AMMOPlayerController> WeakThis(this);
	FMMOOnDialogueReady OnReady;
	OnReady.BindLambda([WeakThis, SpeakerName, FallbackLine](bool bSuccess, const FString& Text)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		// LLM ciktisi guvenilmez: uzunluk 240'a kirpilir, kontrol karakterleri bosluga cevrilir (SECURITY_AUDIT Y-4/D-9).
		FString Safe = Text.Left(240);
		for (TCHAR& C : Safe)
		{
			if (C < 32)
			{
				C = TEXT(' ');
			}
		}
		WeakThis->ClientReceiveNpcDialogue(SpeakerName, bSuccess ? Safe : FallbackLine);
	});

	// Oyuncu adi yalniz gunluk kota sayaci anahtaridir (LLM'e gitmez).
	Dialogue->RequestNpcLine(Npc->NpcName, Npc->NpcRole, PlayerInfo, OnReady,
		PlayerState ? PlayerState->GetPlayerName() : FString());
}

void AMMOPlayerController::ClientReceiveNpcDialogue_Implementation(const FString& SpeakerName, const FString& Text)
{
	if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		H->ShowNpcDialogue(SpeakerName, Text);
	}
}

void AMMOPlayerController::ServerRequestHeal_Implementation(AMMOHealerNPC* Npc)
{
	// Sunucu-otoriter: iyilestirme ve altin dususu YALNIZCA burada (authority) yapilir.
	if (!Npc)
	{
		return;
	}

	// Mesafeyi sunucuda dogrula (hileli istemci uzaktan iyilestirme isteyemesin).
	APawn* MyPawn = GetPawn();
	if (!MyPawn || FVector::Dist(MyPawn->GetActorLocation(), Npc->GetActorLocation()) > 700.f)
	{
		return;
	}

	// SUNUCU-tarafi throttle (guvenlik, talker'daki desenle ayni): istemci-tarafi kapiya
	// guvenme — hileli istemci bu RPC'yi bombardiman edip her cagrida gercek altin
	// dususu/iyilestirme tetikleyemesin.
	const UWorld* World = GetWorld();
	const float ServerNow = World ? World->GetTimeSeconds() : 0.f;
	if (ServerNow - LastHealServerTime < HealServerCooldown)
	{
		return;
	}
	LastHealServerTime = ServerNow;

	AMMOCharacter* C = Cast<AMMOCharacter>(MyPawn);
	if (!C)
	{
		return;
	}
	UMMOStatsComponent* St = C->GetStats();
	UMMOInventoryComponent* Inv = C->GetInventory();
	if (!St || !Inv)
	{
		return;
	}

	const FString Speaker = Npc->HealerName;

	// Zaten tam saglikliysa (olu degil, can ve mana dolu) altin alma; kibarca gonder.
	const bool bFullHp = St->GetHealthPercent() >= 0.999f;
	// MaxMana<=0 olan karakterde (mana havuzu yok) iyilestirilecek mana yoktur => "dolu" say.
	// Aksi halde GetManaPercent 0 doner, tam canli oyuncu bedavaya altin kaptirir.
	const bool bFullMp = (St->MaxMana <= 0.f) || (St->GetManaPercent() >= 0.999f);
	if (!St->bIsDead && bFullHp && bFullMp)
	{
		ClientReceiveNpcDialogue(Speaker, Npc->AlreadyHealedLine);
		return;
	}

	// Yeterli altin var mi?
	// Bedel seviyeyle buyur (AMMOHealerNPC::ComputeHealCost); sabit taban Lv58+ bantlarinda bedavaydi.
	const int32 Cost = AMMOHealerNPC::ComputeHealCost(Npc->HealCost, St->Level);
	if (Inv->Gold < Cost)
	{
		ClientReceiveNpcDialogue(Speaker, Npc->InsufficientGoldLine);
		MMONotify::ToOwner(C, FString::Printf(TEXT("Sifa bedeli: %d altin"), Cost), FColor(255, 200, 80), 3.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	// Altini dus ve tam iyilestir (Heal/RestoreMana clamp'li; olu ise dirilt).
	Inv->AddGoldFrom(-Cost, TEXT("iyilestirme"));
	if (St->bIsDead)
	{
		St->Revive();
	}
	else
	{
		St->Heal(St->MaxHealth);
		St->RestoreMana(St->MaxMana);
	}

	// Onay balonu: sifacinin sabit repliklerinden biri (istemciye ozel gonderilir).
	ClientReceiveNpcDialogue(Speaker, Npc->PickGreeting());
	if (IsValid(C))
	{
		MMONotify::ToOwner(C, FString::Printf(TEXT("Sifa: -%d altin"), Cost), FColor(120, 255, 120), 3.f);
	}
}

#include "Components/MMOQuestComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOStatsComponent.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Save/MMOCharacterSaveJson.h"
#include "Save/MMOSaveGame.h"
#include "Tests/MMOQuestRewardCallbackProbe.h"
#include "UObject/StrongObjectPtr.h"
#include "World/MMOQuestGiverNPC.h"

bool UMMOQuestComponent::RunRewardCallbackSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client
		|| !FParse::Param(FCommandLine::Get(), TEXT("unattended"))
		|| !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
	{ OutError = TEXT("disposable authority headless game world required"); return false; }
	TArray<AActor*> Fixtures;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Origin(1000000, 1000000, 5000);
	const auto SpawnPawn = [&]()
	{
		AMMOCharacter* Pawn = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Origin, FRotator::ZeroRotator, Params);
		if (Pawn) { Fixtures.Add(Pawn); Pawn->SetActorEnableCollision(false); }
		return Pawn;
	};
	AMMOCharacter* Pawn = SpawnPawn();
	AMMOCharacter* Expected = SpawnPawn();
	AMMOCharacter* Independent = SpawnPawn();
	AMMOCharacter* Restored = SpawnPawn();
	AMMOQuestGiverNPC* Giver = World->SpawnActor<AMMOQuestGiverNPC>(AMMOQuestGiverNPC::StaticClass(), Origin, FRotator::ZeroRotator, Params);
	if (Giver) { Fixtures.Add(Giver); Giver->SetActorEnableCollision(false); }
	int32 Failures = 0;
	const auto Check = [&](const FString& Case, const TCHAR* Label, bool bOK)
	{
		++OutChecks;
		UE_LOG(LogTemp, Display, TEXT("[MMO_QUEST_CALLBACK_CASE] %s case=%s check=%s"), bOK ? TEXT("PASS") : TEXT("FAIL"), *Case, Label);
		if (!bOK) { ++Failures; if (OutError.IsEmpty()) OutError = Case + TEXT("/") + Label; }
	};
	const auto Snapshot = [](AMMOCharacter* C)
	{
		FMMOCharacterSaveData D;
		C->GetStats()->FillSaveData(D); C->GetInventory()->FillSaveData(D); C->GetQuestComponent()->FillSaveData(D);
		return D;
	};
	const auto Apply = [](AMMOCharacter* C, const FMMOCharacterSaveData& D)
	{
		C->GetStats()->ApplySaveData(D); C->GetInventory()->ApplySaveData(D); C->GetQuestComponent()->ApplySaveData(D);
	};
	const auto Same = [](const FMMOCharacterSaveData& A, const FMMOCharacterSaveData& B)
	{
		TArray<FString> Differences; FMMOCharacterSaveJson::FindDifferences(A, B, Differences); return Differences.IsEmpty();
	};
	const bool bCompleted = [&]()
	{
		if (!Pawn || !Expected || !Independent || !Restored || !Giver)
		{ OutError = TEXT("fixture spawn failed"); return false; }
		const FMMOQuestDef* Other = FMMOQuestDatabase::Find(TEXT("Q_Cleanup"));
		if (!Other) { OutError = TEXT("second quest unavailable"); return false; }
		const TCHAR* QuestIds[] = { TEXT("Q_Goblins"), TEXT("Q_Boss"), TEXT("Q_Gold5000") };
		const TCHAR* Modes[] = { TEXT("plain"), TEXT("gold"), TEXT("xp"), TEXT("inventory"), TEXT("level") };
		for (const TCHAR* Id : QuestIds)
		{
			const FMMOQuestDef* Def = FMMOQuestDatabase::Find(Id);
			if (!Def || Def->RewardXP <= 0 || Def->RewardGold < 0)
			{ OutError = TEXT("reward fixture data unavailable"); return false; }
			for (int32 Mode = 0; Mode < 5; ++Mode)
			{
				if (Mode == 3 && Def->RewardEquipmentId.IsNone() && Def->RewardConsumableId.IsNone()) continue;
				FMMOCharacterSaveData Seed;
				Seed.Level = FMath::Max(Def->RequiredLevel, Other->RequiredLevel);
				Seed.Gold = 20000;
				Pawn->GetStats()->ApplySaveData(Seed);
				// Guarantee a real level callback in every reward case.
				Seed.Experience = Pawn->GetStats()->ExperienceToNextLevel - 1;
				if (!Def->PrereqQuestId.IsNone()) Seed.CompletedQuests.AddUnique(Def->PrereqQuestId);
				if (!Other->PrereqQuestId.IsNone()) Seed.CompletedQuests.AddUnique(Other->PrereqQuestId);
				for (const FMMOQuestDef* ReadyDef : { Def, Other })
				{
					FMMOQuestProgress Ready; Ready.QuestId = ReadyDef->QuestId; Ready.KillProgress = ReadyDef->TargetCount;
					Seed.ActiveQuests.Add(Ready);
				}
				Apply(Pawn, Seed); Apply(Expected, Seed); Apply(Independent, Seed);
				UMMOQuestComponent* Quests = Pawn->GetQuestComponent();
				UMMOInventoryComponent* Inv = Pawn->GetInventory();
				UMMOStatsComponent* Stats = Pawn->GetStats();
				Expected->GetQuestComponent()->ClaimReward(Def->QuestId);
				const FMMOCharacterSaveData ExpectedOnce = Snapshot(Expected);
				bool bNestedUnchanged = true;
				TStrongObjectPtr<UMMOQuestRewardCallbackProbe> Probe(NewObject<UMMOQuestRewardCallbackProbe>());
				Probe->Callback = [&]()
				{
					const bool bStillActive = Quests->IsQuestActive(Def->QuestId) && !Quests->IsCompleted(Def->QuestId);
					const FMMOCharacterSaveData BeforeNested = Snapshot(Pawn);
					Quests->ClaimReward(Def->QuestId);
					Quests->ClaimReward(Def->QuestId);
					Quests->ClaimReward(Other->QuestId);
					bNestedUnchanged = bStillActive && Same(BeforeNested, Snapshot(Pawn));
					Independent->GetQuestComponent()->ClaimReward(Def->QuestId);
				};
				if (Mode == 1) Inv->OnGoldChanged.AddDynamic(Probe.Get(), &UMMOQuestRewardCallbackProbe::OnGold);
				if (Mode == 2) Stats->OnExperienceChanged.AddDynamic(Probe.Get(), &UMMOQuestRewardCallbackProbe::OnXP);
				if (Mode == 3) Inv->OnInventoryChanged.AddDynamic(Probe.Get(), &UMMOQuestRewardCallbackProbe::OnInventory);
				if (Mode == 4) Stats->OnLevelUp.AddDynamic(Probe.Get(), &UMMOQuestRewardCallbackProbe::OnLevel);
				Quests->ClaimReward(Def->QuestId);
				Inv->OnGoldChanged.RemoveDynamic(Probe.Get(), &UMMOQuestRewardCallbackProbe::OnGold);
				Stats->OnExperienceChanged.RemoveDynamic(Probe.Get(), &UMMOQuestRewardCallbackProbe::OnXP);
				Inv->OnInventoryChanged.RemoveDynamic(Probe.Get(), &UMMOQuestRewardCallbackProbe::OnInventory);
				Stats->OnLevelUp.RemoveDynamic(Probe.Get(), &UMMOQuestRewardCallbackProbe::OnLevel);
				Probe->Callback = nullptr;
				if (Mode == 0) Independent->GetQuestComponent()->ClaimReward(Def->QuestId);
				const FString Case = FString::Printf(TEXT("%s_%s"), Id, Modes[Mode]);
				const FMMOCharacterSaveData Once = Snapshot(Pawn);
				Check(Case, TEXT("callback_seen"), Mode == 0 ? Probe->Calls == 0 : Probe->Calls > 0);
				Check(Case, TEXT("nested_claims_unchanged"), bNestedUnchanged);
				Check(Case, TEXT("one_complete_reward"), Same(Once, ExpectedOnce));
				Check(Case, TEXT("other_quest_deferred"), Quests->IsQuestActive(Other->QuestId) && !Quests->IsCompleted(Other->QuestId));
				Check(Case, TEXT("independent_pawn_allowed"), Same(Snapshot(Independent), ExpectedOnce));
				Quests->ClaimReward(Def->QuestId);
				Check(Case, TEXT("repeat_unchanged"), Same(Snapshot(Pawn), Once));
				Quests->ClaimReward(Other->QuestId); Expected->GetQuestComponent()->ClaimReward(Other->QuestId);
				const FMMOCharacterSaveData After = Snapshot(Pawn);
				Check(Case, TEXT("next_claim_after_return"), Same(After, Snapshot(Expected)) && Quests->IsCompleted(Other->QuestId));
				Apply(Restored, After);
				Check(Case, TEXT("memory_save_roundtrip"), Same(After, Snapshot(Restored)));
			}
		}
		return true;
	}();
	for (AActor* Fixture : Fixtures) if (IsValid(Fixture)) Fixture->Destroy();
	// Controllerless and same-frame: no actual player save/load, RPC or PIE test.
	if (!bCompleted) return false;
	if (OutChecks != 112) { ++Failures; OutError += TEXT(" check_count_mismatch"); }
	if (Failures) OutError = FString::Printf(TEXT("failures=%d first=%s"), Failures, *OutError);
	return Failures == 0;
}

namespace
{
	struct FMMOQuestRewardCallbackTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestQuestRewardCallbacks"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = UMMOQuestComponent::RunRewardCallbackSelfTest(World, Error, Checks);
			Ar.Logf(TEXT("[MMO_QUEST_CALLBACK_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOQuestRewardCallbackTestExec GMMOQuestRewardCallbackTestExec;
}
#endif

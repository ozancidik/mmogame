#include "Components/MMOInventoryComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOStatsComponent.h"
#include "Engine/World.h"
#include "Items/MMOItemDatabase.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Save/MMOCharacterSaveJson.h"
#include "Save/MMOSaveGame.h"
#include "Tests/MMOUpgradeCallbackProbe.h"
#include "UObject/StrongObjectPtr.h"

bool UMMOInventoryComponent::RunUpgradeCallbackSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0;
	OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client
		|| !FParse::Param(FCommandLine::Get(), TEXT("unattended"))
		|| !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
	{
		OutError = TEXT("disposable authority headless game world required (fixture seeds process RNG)");
		return false;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AMMOCharacter* Pawn = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(1000000, 1000000, 5000), FRotator::ZeroRotator, Params);
	AMMOCharacter* Restore = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(1000200, 1000000, 5000), FRotator::ZeroRotator, Params);
	int32 Failures = 0;
	const auto Check = [&](const FString& Case, const TCHAR* Label, bool bOK)
	{
		++OutChecks;
		UE_LOG(LogTemp, Display, TEXT("[MMO_UPGRADE_CALLBACK_CASE] %s case=%s check=%s"), bOK ? TEXT("PASS") : TEXT("FAIL"), *Case, Label);
		if (!bOK) { ++Failures; if (OutError.IsEmpty()) OutError = Case + TEXT("/") + Label; }
	};
	const bool bCompleted = [&]()
	{
		if (!Pawn || !Restore || !Pawn->GetInventory() || !Restore->GetInventory() || !Pawn->GetStats())
		{
			OutError = TEXT("fixture spawn failed"); return false;
		}
		Pawn->SetActorEnableCollision(false); Restore->SetActorEnableCollision(false);
		UMMOInventoryComponent* Inv = Pawn->GetInventory();
		const FName ItemId(TEXT("ShortSword"));
		const FMMOItemDef* Def = FMMOItemDatabase::Find(ItemId);
		if (!Def) { OutError = TEXT("ShortSword missing"); return false; }
		const auto Prepare = [&](int32 Plus)
		{
			Pawn->GetStats()->bIsDead = false;
			Inv->EquipmentBag.Reset(); Inv->EquippedItems.Reset(); Inv->EquippedItems.SetNum(static_cast<int32>(EMMOEquipSlot::MAX));
			Inv->Items.Reset(); Inv->Warehouse.Reset(); Inv->NextInstanceId = 1;
			Inv->Gold = 10000; Inv->LastUpgradeServerTime = -1000;
			Inv->AddEquipmentInstance(ItemId, Plus);
			Inv->AddEquipmentInstance(ItemId, 0); // Same allocation remains valid when Swap changes element identity.
		};
		const auto Snapshot = [&]() { FMMOCharacterSaveData D; Inv->FillSaveData(D); return D; };
		const auto Same = [](const FMMOCharacterSaveData& A, const FMMOCharacterSaveData& B)
		{
			TArray<FString> Differences; FMMOCharacterSaveJson::FindDifferences(A, B, Differences); return Differences.IsEmpty();
		};
		const auto MatchesTarget = [](const FMMOCharacterSaveData& D, int32 ExpectedPlus, bool bEquipped)
		{
			const auto& Container = bEquipped ? D.EquippedItems : D.EquipmentBag;
			const FMMOEquipmentInstance* Found = Container.FindByPredicate([](const FMMOEquipmentInstance& I) { return I.InstanceId == 1; });
			return ExpectedPlus < 0 ? (!Found && !D.EquipmentBag.ContainsByPredicate([](const FMMOEquipmentInstance& I) { return I.InstanceId == 1; })
				&& !D.EquippedItems.ContainsByPredicate([](const FMMOEquipmentInstance& I) { return I.InstanceId == 1; }))
				: Found && Found->Plus == ExpectedPlus && Found->ItemId == FName(TEXT("ShortSword"));
		};
		// Find deterministic seeds using the real platform RNG. No production random seam.
		int32 Seeds[3] = { INDEX_NONE, INDEX_NONE, INDEX_NONE };
		for (int32 Index = 0; Index < 4096; ++Index)
		{
			const int32 Seed = Index * 104729 + 17; // MSVC's adjacent small seeds have correlated first rolls.
			FMath::RandInit(Seed);
			const int32 Roll = FMath::RandRange(1, 100);
			if (Roll <= 90 && Seeds[0] == INDEX_NONE) Seeds[0] = Seed;
			if (Roll > 90) { if (Seeds[1] == INDEX_NONE) Seeds[1] = Seed; if (Seeds[2] == INDEX_NONE) Seeds[2] = Seed; }
		}
		if (Seeds[0] < 0 || Seeds[1] < 0 || Seeds[2] < 0) { OutError = TEXT("RNG seeds missing"); return false; }
		const TCHAR* Outcomes[] = { TEXT("success"), TEXT("failure"), TEXT("burn") };
		const TCHAR* Modes[] = { TEXT("plain"), TEXT("snapshot"), TEXT("swap"), TEXT("equip"), TEXT("nested") };
		for (int32 Outcome = 0; Outcome < 3; ++Outcome)
		{
			const int32 Plus = Outcome == 2 ? 7 : 0;
			const int32 ExpectedPlus = Outcome == 0 ? 1 : Outcome == 1 ? 0 : -1;
			const int32 Cost = FMMOItemDatabase::GetUpgradeCost(Plus, Def->RequiredLevel);
			for (int32 Mode = 0; Mode < 5; ++Mode)
			{
				Prepare(Plus);
				const FString Case = FString::Printf(TEXT("%s_%s"), Outcomes[Outcome], Modes[Mode]);
				FMMOCharacterSaveData Observed;
				TStrongObjectPtr<UMMOUpgradeCallbackProbe> Probe(NewObject<UMMOUpgradeCallbackProbe>());
				Probe->Callback = [&]()
				{
					Observed = Snapshot(); // Gold listener must see the settled item outcome and debit together.
					if (Mode == 2 && Inv->EquipmentBag.Num() == 2) Inv->EquipmentBag.Swap(0, 1);
					if (Mode == 3) Inv->EquipByInstanceId(1); // Real public gameplay API, no bag reference escapes.
					if (Mode == 4) { Inv->UpgradeByInstanceId(1, -1); Inv->UpgradeByInstanceId(2, 0); }
				};
				if (Mode != 0) Inv->OnGoldChanged.AddDynamic(Probe.Get(), &UMMOUpgradeCallbackProbe::OnGold);
				FMath::RandInit(Seeds[Outcome]);
				Inv->UpgradeByInstanceId(1, Plus);
				Inv->OnGoldChanged.RemoveDynamic(Probe.Get(), &UMMOUpgradeCallbackProbe::OnGold);
				Probe->Callback = nullptr;
				const FMMOCharacterSaveData After = Snapshot();
				if (Mode == 0) Observed = After;
				Check(Case, TEXT("one_debit"), Inv->Gold == 10000 - Cost);
				Check(Case, TEXT("requested_target"), MatchesTarget(After, ExpectedPlus, Mode == 3 && ExpectedPlus >= 0));
				Check(Case, TEXT("spare_untouched"), Inv->EquipmentBag.ContainsByPredicate([](const FMMOEquipmentInstance& I) { return I.InstanceId == 2 && I.Plus == 0; }));
				Check(Case, TEXT("gold_callback_count"), Probe->Calls == (Mode == 0 ? 0 : 1));
				Check(Case, TEXT("callback_gold_settled"), Observed.Gold == 10000 - Cost);
				Check(Case, TEXT("callback_item_settled"), MatchesTarget(Observed, ExpectedPlus, false));
				Restore->GetInventory()->ApplySaveData(After);
				FMMOCharacterSaveData Restored; Restore->GetInventory()->FillSaveData(Restored);
				Check(Case, TEXT("memory_save_roundtrip"), Same(After, Restored));
				Inv->UpgradeByInstanceId(1, Plus);
				Check(Case, TEXT("immediate_repeat_unchanged"), Same(Snapshot(), After));
			}
		}
		for (int32 Gate = 0; Gate < 6; ++Gate)
		{
			Prepare(Gate == 3 ? FMMOItemDatabase::MaxPlus : 0);
			if (Gate == 1) Pawn->GetStats()->bIsDead = true;
			if (Gate == 2) Inv->Gold = 0;
			if (Gate == 5) Inv->LastUpgradeServerTime = World->GetTimeSeconds();
			const FString Case = FString::Printf(TEXT("reject_%d"), Gate);
			const FMMOCharacterSaveData Before = Snapshot();
			TStrongObjectPtr<UMMOUpgradeCallbackProbe> Probe(NewObject<UMMOUpgradeCallbackProbe>());
			Inv->OnGoldChanged.AddDynamic(Probe.Get(), &UMMOUpgradeCallbackProbe::OnGold);
			Inv->UpgradeByInstanceId(Gate == 0 ? 999 : 1, Gate == 4 ? 1 : -1);
			Inv->OnGoldChanged.RemoveDynamic(Probe.Get(), &UMMOUpgradeCallbackProbe::OnGold);
			Check(Case, TEXT("no_mutation"), Same(Snapshot(), Before));
			Check(Case, TEXT("no_gold_callback"), Probe->Calls == 0);
		}
		return true;
	}();
	if (IsValid(Pawn)) Pawn->Destroy();
	if (IsValid(Restore)) Restore->Destroy();
	// Controllerless, same-frame actors: no disk save/load, timer or network client fixture.
	if (!bCompleted) return false;
	if (OutChecks != 132) { ++Failures; OutError += TEXT(" check_count_mismatch"); }
	if (Failures) OutError = FString::Printf(TEXT("failures=%d first=%s"), Failures, *OutError);
	return Failures == 0;
}

namespace
{
	struct FMMOUpgradeCallbackTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestUpgradeCallbacks"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = UMMOInventoryComponent::RunUpgradeCallbackSelfTest(World, Error, Checks);
			Ar.Logf(TEXT("[MMO_UPGRADE_CALLBACK_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOUpgradeCallbackTestExec GMMOUpgradeCallbackTestExec;
}
#endif

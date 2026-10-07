#include "Components/MMOTradeComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOStatsComponent.h"
#include "Engine/World.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Save/MMOCharacterSaveJson.h"
#include "TimerManager.h"

bool UMMOTradeComponent::RunTradeSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{ OutError = TEXT("server game world required"); return false; }
	TArray<AMMOCharacter*> Fixtures;
	const bool bPassed = [&]()
	{
		AMMOCharacter* Pawns[2] = {};
		UMMOTradeComponent* Trades[2] = {};
		UMMOInventoryComponent* Inventories[2] = {};
		const FVector Origin(1200000, 1200000, 5000);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (int32 Side = 0; Side < 2; ++Side)
		{
			Pawns[Side] = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Origin, FRotator::ZeroRotator, Params);
			if (Pawns[Side]) Fixtures.Add(Pawns[Side]);
			if (!Pawns[Side] || !Pawns[Side]->GetStats() || !Pawns[Side]->GetTradeComponent() || !Pawns[Side]->GetInventory())
			{ OutError = TEXT("fixture spawn/components failed"); return false; }
			Pawns[Side]->SetActorEnableCollision(false);
			Pawns[Side]->SetActorTickEnabled(false);
			Trades[Side] = Pawns[Side]->GetTradeComponent(); Inventories[Side] = Pawns[Side]->GetInventory();
		}
		auto Check = [&](bool Condition, const TCHAR* Label)
		{
			if (!Condition) { OutError = FString::Printf(TEXT("%s (check=%d)"), Label, OutChecks + 1); return false; }
			++OutChecks; return true;
		};
		auto Snapshot = [](const UMMOInventoryComponent* Inv)
		{ FMMOCharacterSaveData Data; Inv->FillSaveData(Data); return Data; };
		auto Equal = [](const FMMOCharacterSaveData& A, const FMMOCharacterSaveData& B)
		{ TArray<FString> Differences; FMMOCharacterSaveJson::FindDifferences(A, B, Differences); return Differences.IsEmpty(); };
		auto Reset = [&]()
		{
			Trades[0]->EndTradeServer(TEXT("fixture reset")); Trades[1]->EndTradeServer(TEXT("fixture reset"));
			for (int32 Side = 0; Side < 2; ++Side)
			{
				Pawns[Side]->GetStats()->bIsDead = false; Pawns[Side]->SetActorLocation(Origin + FVector(Side * 100, 0, 0));
				FMMOCharacterSaveData Data; Data.Gold = 1000;
				FMMOEquipmentInstance Stored; Stored.InstanceId = 40; Stored.ItemId = TEXT("ShortSword");
				Data.Warehouse.Add(Stored);
				Data.EquippedItems.SetNum(static_cast<int32>(EMMOEquipSlot::MAX));
				FMMOEquipmentInstance Equipped = Stored; Equipped.InstanceId = 30;
				Data.EquippedItems[static_cast<int32>(EMMOEquipSlot::Weapon)] = Equipped;
				Inventories[Side]->ApplySaveData(Data);
				Inventories[Side]->AddItem(TEXT("ManaPotion"), 3);
				Inventories[Side]->AddEquipmentInstance(TEXT("ShortSword"), 3, EMMOItemRarity::Rare);
				Inventories[Side]->AddEquipmentInstance(TEXT("ShortSword"), 3, EMMOItemRarity::Rare);
				Inventories[Side]->AddEquipmentInstance(TEXT("LeatherArmor"), 2, EMMOItemRarity::Epic);
				Trades[Side]->LastOfferChangeTime = World->GetTimeSeconds() - 2.f;
				Trades[Side]->LastConfirmTime = World->GetTimeSeconds() - 2.f;
			}
			StartTradeServer(Pawns[0], Pawns[1]);
		};
		auto Add = [&](int32 Side, int32 BagIndex)
		{
			Trades[Side]->LastOfferChangeTime = World->GetTimeSeconds() - 2.f;
			Trades[Side]->ServerAddItem_Implementation(Inventories[Side]->EquipmentBag[BagIndex].InstanceId);
		};
		auto Gold = [&](int32 Side, int32 Amount)
		{ Trades[Side]->LastOfferChangeTime = World->GetTimeSeconds() - 2.f; Trades[Side]->ServerSetGold_Implementation(Amount); };
		auto AgeConfirm = [&]()
		{
			for (UMMOTradeComponent* Trade : Trades)
			{ Trade->LastTradeChangeTime = World->GetTimeSeconds() - 2.f; Trade->LastConfirmTime = World->GetTimeSeconds() - 2.f; }
		};
		auto Complete = [&]()
		{ AgeConfirm(); Trades[0]->ServerConfirm_Implementation(Trades[0]->OfferRevision); Trades[1]->ServerConfirm_Implementation(Trades[1]->OfferRevision); };
		auto Ended = [&]()
		{
			for (UMMOTradeComponent* Trade : Trades)
				if (Trade->bTradeActive || Trade->Partner.IsValid() || !Trade->MyOffer.Items.IsEmpty() || !Trade->TheirOffer.Items.IsEmpty()
					|| Trade->MyOffer.Gold || Trade->TheirOffer.Gold || Trade->MyOffer.bConfirmed || Trade->TheirOffer.bConfirmed
					|| World->GetTimerManager().IsTimerActive(Trade->ValidateTimerHandle)) return false;
			return true;
		};
		auto BothUnchanged = [&](const FMMOCharacterSaveData& A, const FMMOCharacterSaveData& B)
		{ return Equal(A, Snapshot(Inventories[0])) && Equal(B, Snapshot(Inventories[1])); };
		Reset();
		if (!Check(Inventories[0]->EquipmentBag.Num() == 3 && Inventories[1]->EquipmentBag.Num() == 3
			&& Inventories[0]->GetEquipped(EMMOEquipSlot::Weapon).InstanceId == 30 && Inventories[1]->GetEquipped(EMMOEquipSlot::Weapon).InstanceId == 30
			&& Trades[0]->GetValidPartnerComp() == Trades[1] && Trades[1]->GetValidPartnerComp() == Trades[0], TEXT("fixture setup"))) return false;
		const FMMOCharacterSaveData InitialA = Snapshot(Inventories[0]), InitialB = Snapshot(Inventories[1]);
		Complete();
		if (!Check(!Trades[0]->MyOffer.bConfirmed && !Trades[1]->MyOffer.bConfirmed && BothUnchanged(InitialA, InitialB), TEXT("empty confirmation"))) return false;
		Add(0, 0);
		if (!Check(Trades[0]->OfferRevision == 1 && Trades[1]->OfferRevision == 1 && Trades[1]->TheirOffer.Items.Num() == 1, TEXT("add revision/mirror"))) return false;
		Trades[0]->ServerAddItem_Implementation(Inventories[0]->EquipmentBag[1].InstanceId);
		if (!Check(Trades[0]->OfferRevision == 1 && Trades[0]->MyOffer.Items.Num() == 1, TEXT("offer throttle"))) return false;
		Add(0, 0); Trades[0]->ServerRemoveItem_Implementation(-1);
		if (!Check(Trades[0]->OfferRevision == 1 && Trades[0]->MyOffer.Items.Num() == 1, TEXT("duplicate/stale item request"))) return false;
		Gold(0, MAX_int32);
		if (!Check(Trades[0]->MyOffer.Gold == 1000 && Trades[1]->TheirOffer.Gold == 1000 && Trades[0]->OfferRevision == 2, TEXT("gold wallet clamp"))) return false;
		Gold(0, 1000);
		if (!Check(Trades[0]->OfferRevision == 2, TEXT("unchanged gold revision"))) return false;
		Gold(0, -1);
		if (!Check(Trades[0]->MyOffer.Gold == 0 && Trades[0]->OfferRevision == 3, TEXT("negative gold clamp"))) return false;
		AgeConfirm(); Trades[0]->LastTradeChangeTime = World->GetTimeSeconds();
		Trades[0]->ServerConfirm_Implementation(Trades[0]->OfferRevision);
		if (!Check(!Trades[0]->MyOffer.bConfirmed, TEXT("confirmation lock"))) return false;
		AgeConfirm(); Trades[0]->LastConfirmTime = World->GetTimeSeconds();
		Trades[0]->ServerConfirm_Implementation(Trades[0]->OfferRevision);
		if (!Check(!Trades[0]->MyOffer.bConfirmed, TEXT("confirmation throttle"))) return false;
		AgeConfirm(); Trades[0]->ServerConfirm_Implementation(Trades[0]->OfferRevision);
		if (!Check(Trades[0]->MyOffer.bConfirmed && Trades[1]->TheirOffer.bConfirmed && !Trades[1]->MyOffer.bConfirmed
			&& BothUnchanged(InitialA, InitialB), TEXT("first confirmation must not transfer"))) return false;
		const int32 OldRevision = Trades[0]->OfferRevision;
		Gold(1, 50);
		if (!Check(Trades[0]->OfferRevision == OldRevision + 1 && Trades[1]->OfferRevision == OldRevision + 1
			&& !Trades[0]->MyOffer.bConfirmed && !Trades[0]->TheirOffer.bConfirmed && !Trades[1]->MyOffer.bConfirmed && !Trades[1]->TheirOffer.bConfirmed,
			TEXT("offer change must clear both approvals/mirrors"))) return false;
		AgeConfirm(); Trades[0]->ServerConfirm_Implementation(OldRevision);
		if (!Check(!Trades[0]->MyOffer.bConfirmed && BothUnchanged(InitialA, InitialB), TEXT("stale revision confirmation"))) return false;
		Trades[0]->LastOfferChangeTime = World->GetTimeSeconds() - 2.f;
		Trades[0]->ServerRemoveItem_Implementation(Inventories[0]->EquipmentBag[0].InstanceId);
		if (!Check(Trades[0]->MyOffer.Items.IsEmpty() && Trades[1]->TheirOffer.Items.IsEmpty() && Trades[0]->OfferRevision == OldRevision + 2, TEXT("remove revision/mirror"))) return false;
		Reset();
		for (int32 Index = 0; Index < 2; ++Index) Inventories[0]->AddEquipmentInstance(TEXT("ShortSword"));
		for (int32 Index = 0; Index < 5; ++Index) Add(0, Index);
		if (!Check(Trades[0]->MyOffer.Items.Num() == MaxOfferItems && Trades[0]->OfferRevision == MaxOfferItems, TEXT("max four offered items"))) return false;

		// Whole inventory expectation includes allocator, unoffered items, slots, warehouse and stacks.
		Reset(); Add(0, 0); Add(0, 1); Add(1, 2); Gold(0, 120); Gold(1, 35);
		FMMOCharacterSaveData Expected[2] = { Snapshot(Inventories[0]), Snapshot(Inventories[1]) };
		const FMMOTradeOffer Offers[2] = { Trades[0]->MyOffer, Trades[1]->MyOffer };
		for (int32 Side = 0; Side < 2; ++Side)
		{
			for (const FMMOEquipmentInstance& Outgoing : Offers[Side].Items)
				Expected[Side].EquipmentBag.RemoveAll([&](const FMMOEquipmentInstance& Item) { return Item.InstanceId == Outgoing.InstanceId; });
			for (FMMOEquipmentInstance Incoming : Offers[1 - Side].Items)
			{ Incoming.InstanceId = Expected[Side].NextInstanceId++; Expected[Side].EquipmentBag.Add(Incoming); }
			Expected[Side].Gold += Offers[1 - Side].Gold - Offers[Side].Gold;
		}
		AgeConfirm(); Trades[0]->ServerConfirm_Implementation(Trades[0]->OfferRevision);
		Trades[0]->ServerConfirm_Implementation(Trades[0]->OfferRevision);
		if (!Check(Inventories[0]->Gold == 1000 && Inventories[1]->Gold == 1000 && Trades[0]->bTradeActive, TEXT("duplicate first approval"))) return false;
		Trades[1]->ServerConfirm_Implementation(Trades[1]->OfferRevision);
		if (!Check(Ended(), TEXT("completed state/timer cleanup"))) return false;
		if (!Check(BothUnchanged(Expected[0], Expected[1]) && static_cast<int64>(Inventories[0]->Gold) + Inventories[1]->Gold == 2000,
			TEXT("exact exchange payload/recipient IDs/gold/collateral"))) return false;
		Complete();
		if (!Check(Ended() && BothUnchanged(Expected[0], Expected[1]), TEXT("post completion duplicate approvals"))) return false;

		for (int32 Recipient = 0; Recipient < 2; ++Recipient)
		{
			for (int32 Scenario = 0; Scenario < 3; ++Scenario)
			{
				Reset(); Inventories[Recipient]->Gold = UMMOInventoryComponent::MaxGold - (Scenario == 2 ? 0 : 10);
				Gold(1 - Recipient, Scenario == 1 ? 11 : 10); if (Scenario == 2) Gold(Recipient, 20);
				FMMOCharacterSaveData Before[2] = { Snapshot(Inventories[0]), Snapshot(Inventories[1]) };
				if (Scenario != 1) { Before[Recipient].Gold += Scenario == 2 ? -10 : 10; Before[1 - Recipient].Gold += Scenario == 2 ? 10 : -10; }
				Complete();
				if (!Check(Ended(), TEXT("gold cap state cleanup")) || !Check(BothUnchanged(Before[0], Before[1]), TEXT("gold exact cap/overflow/net balance"))) return false;
			}
			for (int32 Exchange = 0; Exchange < 2; ++Exchange)
			{
				Reset();
				while (Inventories[Recipient]->EquipmentBag.Num() < UMMOInventoryComponent::MaxBagSize) Inventories[Recipient]->AddEquipmentInstance(TEXT("ShortSword"));
				Add(1 - Recipient, 0); if (Exchange) Add(Recipient, 0);
				FMMOCharacterSaveData Before[2] = { Snapshot(Inventories[0]), Snapshot(Inventories[1]) };
				if (Exchange)
				{
					for (int32 Side = 0; Side < 2; ++Side)
					{
						Before[Side].EquipmentBag.RemoveAt(0);
						FMMOEquipmentInstance Received = Trades[1 - Side]->MyOffer.Items[0];
						Received.InstanceId = Before[Side].NextInstanceId++; Before[Side].EquipmentBag.Add(Received);
					}
				}
				Complete();
				if (!Check(Ended(), TEXT("full bag state cleanup")) || !Check(BothUnchanged(Before[0], Before[1]), TEXT("full bag rejects receipt/allows net exchange"))) return false;
			}
		}
		// Compare AFTER intervening mutation: rejection must not undo legitimate changes either.
		for (int32 Side = 0; Side < 2; ++Side)
		{
			for (int32 Mutation = 0; Mutation < 8; ++Mutation)
			{
				Reset(); Add(Side, 0); Gold(Side, 100);
				switch (Mutation)
				{
				case 0: Inventories[Side]->EquipmentBag.RemoveAt(0); break;
				case 1: Inventories[Side]->EquipByInstanceId(Inventories[Side]->EquipmentBag[0].InstanceId); break;
				case 2: Inventories[Side]->EquipmentBag[0].ItemId = TEXT("LeatherArmor"); break;
				case 3: ++Inventories[Side]->EquipmentBag[0].Plus; break;
				case 4: Inventories[Side]->EquipmentBag[0].Rarity = EMMOItemRarity::Epic; break;
				case 5: Inventories[Side]->AddGold(-950); break;
				case 6: Pawns[Side]->GetStats()->bIsDead = true; break;
				case 7: Pawns[Side]->SetActorLocation(Origin + FVector(2000, 0, 0)); break;
				}
				const FMMOCharacterSaveData A = Snapshot(Inventories[0]), B = Snapshot(Inventories[1]);
				Complete();
				if (!Check(Ended(), TEXT("stale offer/death/range cancellation")) || !Check(BothUnchanged(A, B), TEXT("cancel changed inventory or allocator"))) return false;
			}
		}
		for (int32 Scenario = 0; Scenario < 4; ++Scenario)
		{
			Reset(); Gold(0, 5);
			if (Scenario == 0) Pawns[1]->GetStats()->bIsDead = true;
			if (Scenario == 1) Pawns[1]->SetActorLocation(Origin + FVector(CancelDistance + 1, 0, 0));
			if (Scenario == 2) Trades[0]->LastTradeChangeTime = World->GetTimeSeconds() - IdleTimeoutSeconds - 2.f;
			if (Scenario == 3) Pawns[1]->SetActorLocation(Origin + FVector(CancelDistance, 0, 0));
			const FMMOCharacterSaveData A = Snapshot(Inventories[0]), B = Snapshot(Inventories[1]);
			Trades[0]->ValidateTick();
			if (!Check(Scenario == 3 ? Trades[0]->bTradeActive && Trades[1]->bTradeActive : Ended(), TEXT("validation tick/boundary"))
				|| !Check(BothUnchanged(A, B), TEXT("validation tick changed inventory"))) return false;
		}
		return true;
	}();
	for (AMMOCharacter* Fixture : Fixtures)
	{
		if (!IsValid(Fixture)) continue;
		if (Fixture->GetTradeComponent()) Fixture->GetTradeComponent()->EndTradeServer(TEXT("fixture cleanup"));
		Fixture->Destroy();
	}
	return bPassed; // No world clock mutation, tick, player possession, load, or durable save.
}

namespace
{
	struct FMMOTradeTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestTrade"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = UMMOTradeComponent::RunTradeSelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_TRADE_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOTradeTestExec GMMOTradeTestExec;
}
#endif

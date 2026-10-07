#include "Components/MMOTradeComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOStatsComponent.h"
#include "Player/MMOPlayerController.h"
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
			Trades[Side]->ServerAddItem_Implementation(Trades[Side]->TradeSessionId, Trades[Side]->OfferRevision, Inventories[Side]->EquipmentBag[BagIndex].InstanceId);
		};
		auto Gold = [&](int32 Side, int32 Amount)
		{ Trades[Side]->LastOfferChangeTime = World->GetTimeSeconds() - 2.f; Trades[Side]->ServerSetGold_Implementation(Trades[Side]->TradeSessionId, Trades[Side]->OfferRevision, Amount); };
		auto AgeConfirm = [&]()
		{
			for (UMMOTradeComponent* Trade : Trades)
			{ Trade->LastTradeChangeTime = World->GetTimeSeconds() - 2.f; Trade->LastConfirmTime = World->GetTimeSeconds() - 2.f; }
		};
		auto Complete = [&]()
		{ AgeConfirm(); Trades[0]->ServerConfirm_Implementation(Trades[0]->TradeSessionId, Trades[0]->OfferRevision); Trades[1]->ServerConfirm_Implementation(Trades[1]->TradeSessionId, Trades[1]->OfferRevision); };
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
		Trades[0]->ServerAddItem_Implementation(Trades[0]->TradeSessionId, Trades[0]->OfferRevision, Inventories[0]->EquipmentBag[1].InstanceId);
		if (!Check(Trades[0]->OfferRevision == 1 && Trades[0]->MyOffer.Items.Num() == 1, TEXT("offer throttle"))) return false;
		Add(0, 0); Trades[0]->ServerRemoveItem_Implementation(Trades[0]->TradeSessionId, Trades[0]->OfferRevision, -1);
		if (!Check(Trades[0]->OfferRevision == 1 && Trades[0]->MyOffer.Items.Num() == 1, TEXT("duplicate/stale item request"))) return false;
		Gold(0, MAX_int32);
		if (!Check(Trades[0]->MyOffer.Gold == 1000 && Trades[1]->TheirOffer.Gold == 1000 && Trades[0]->OfferRevision == 2, TEXT("gold wallet clamp"))) return false;
		Gold(0, 1000);
		if (!Check(Trades[0]->OfferRevision == 2, TEXT("unchanged gold revision"))) return false;
		Gold(0, -1);
		if (!Check(Trades[0]->MyOffer.Gold == 0 && Trades[0]->OfferRevision == 3, TEXT("negative gold clamp"))) return false;
		AgeConfirm(); Trades[0]->LastTradeChangeTime = World->GetTimeSeconds();
		Trades[0]->ServerConfirm_Implementation(Trades[0]->TradeSessionId, Trades[0]->OfferRevision);
		if (!Check(!Trades[0]->MyOffer.bConfirmed, TEXT("confirmation lock"))) return false;
		AgeConfirm(); Trades[0]->LastConfirmTime = World->GetTimeSeconds();
		Trades[0]->ServerConfirm_Implementation(Trades[0]->TradeSessionId, Trades[0]->OfferRevision);
		if (!Check(!Trades[0]->MyOffer.bConfirmed, TEXT("confirmation throttle"))) return false;
		AgeConfirm(); Trades[0]->ServerConfirm_Implementation(Trades[0]->TradeSessionId, Trades[0]->OfferRevision);
		if (!Check(Trades[0]->MyOffer.bConfirmed && Trades[1]->TheirOffer.bConfirmed && !Trades[1]->MyOffer.bConfirmed
			&& BothUnchanged(InitialA, InitialB), TEXT("first confirmation must not transfer"))) return false;
		const int32 OldRevision = Trades[0]->OfferRevision;
		Gold(1, 50);
		if (!Check(Trades[0]->OfferRevision == OldRevision + 1 && Trades[1]->OfferRevision == OldRevision + 1
			&& !Trades[0]->MyOffer.bConfirmed && !Trades[0]->TheirOffer.bConfirmed && !Trades[1]->MyOffer.bConfirmed && !Trades[1]->TheirOffer.bConfirmed,
			TEXT("offer change must clear both approvals/mirrors"))) return false;
		AgeConfirm(); Trades[0]->ServerConfirm_Implementation(Trades[0]->TradeSessionId, OldRevision);
		if (!Check(!Trades[0]->MyOffer.bConfirmed && BothUnchanged(InitialA, InitialB), TEXT("stale revision confirmation"))) return false;
		Trades[0]->LastOfferChangeTime = World->GetTimeSeconds() - 2.f;
		Trades[0]->ServerRemoveItem_Implementation(Trades[0]->TradeSessionId, Trades[0]->OfferRevision, Inventories[0]->EquipmentBag[0].InstanceId);
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
		AgeConfirm(); Trades[0]->ServerConfirm_Implementation(Trades[0]->TradeSessionId, Trades[0]->OfferRevision);
		Trades[0]->ServerConfirm_Implementation(Trades[0]->TradeSessionId, Trades[0]->OfferRevision);
		if (!Check(Inventories[0]->Gold == 1000 && Inventories[1]->Gold == 1000 && Trades[0]->bTradeActive, TEXT("duplicate first approval"))) return false;
		Trades[1]->ServerConfirm_Implementation(Trades[1]->TradeSessionId, Trades[1]->OfferRevision);
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
		// Old offer actions must leave both complete offers, approval flags, clocks and inventory intact.
		auto SameOffer = [&](const FMMOTradeOffer& A, const FMMOTradeOffer& B)
		{
			FMMOCharacterSaveData Left, Right;
			Left.EquipmentBag = A.Items; Right.EquipmentBag = B.Items;
			return A.Gold == B.Gold && A.bConfirmed == B.bConfirmed && Equal(Left, Right);
		};
		Reset();
		const uint64 OldActionSession = Trades[0]->TradeSessionId;
		Trades[0]->EndTradeServer(TEXT("fixture old offer action"));
		StartTradeServer(Pawns[0], Pawns[1]);
		if (!Check(Trades[0]->TradeSessionId != OldActionSession && Trades[0]->OfferRevision == 0,
			TEXT("old action/new session same revision setup"))) return false;
		Trades[0]->RequestSetGoldSeen(OldActionSession, 0, 25);
		if (!Check(Trades[0]->MyOffer.Gold == 0 && Trades[0]->OfferRevision == 0,
			TEXT("previous session gold action accepted"))) return false;
		Trades[0]->LastOfferChangeTime = World->GetTimeSeconds() - 2.f;
		Trades[0]->RequestSetGoldSeen(Trades[0]->TradeSessionId, 0, 25);
		if (!Check(Trades[0]->MyOffer.Gold == 25 && Trades[1]->TheirOffer.Gold == 25
			&& Trades[0]->OfferRevision == 1 && Trades[1]->OfferRevision == 1,
			TEXT("fresh revision zero offer action"))) return false;
		Add(0, 0); Add(1, 0); Gold(0, 25); AgeConfirm();
		Trades[0]->RequestConfirm();
		const FMMOCharacterSaveData ActionA = Snapshot(Inventories[0]), ActionB = Snapshot(Inventories[1]);
		auto Action = [&](int32 Side, int32 Kind, uint64 Session, int32 Revision, bool bDifferentPayload = false)
		{
			const int32 BagIndex = bDifferentPayload ? (Kind == 1 ? 1 : 2) : (Kind == 1 ? 0 : 1);
			const int32 Id = Inventories[Side]->EquipmentBag[BagIndex].InstanceId;
			if (Kind == 0) Trades[Side]->RequestAddItemSeen(Session, Revision, Id);
			else if (Kind == 1) Trades[Side]->RequestRemoveItemSeen(Session, Revision, Id);
			else Trades[Side]->RequestSetGoldSeen(Session, Revision, bDifferentPayload ? 19 : 17);
		};
		auto RejectedAction = [&](int32 Side, int32 Kind, uint64 Session, int32 Revision, bool bDifferentPayload = false)
		{
			FMMOTradeOffer Mine[2], Theirs[2]; int32 Revisions[2]; float OfferTimes[2], ChangeTimes[2], ConfirmTimes[2];
			for (int32 I = 0; I < 2; ++I)
			{
				Mine[I] = Trades[I]->MyOffer; Theirs[I] = Trades[I]->TheirOffer; Revisions[I] = Trades[I]->OfferRevision;
				OfferTimes[I] = Trades[I]->LastOfferChangeTime; ChangeTimes[I] = Trades[I]->LastTradeChangeTime; ConfirmTimes[I] = Trades[I]->LastConfirmTime;
			}
			Action(Side, Kind, Session, Revision, bDifferentPayload);
			for (int32 I = 0; I < 2; ++I)
				if (!SameOffer(Mine[I], Trades[I]->MyOffer) || !SameOffer(Theirs[I], Trades[I]->TheirOffer)
					|| Revisions[I] != Trades[I]->OfferRevision || OfferTimes[I] != Trades[I]->LastOfferChangeTime
					|| ChangeTimes[I] != Trades[I]->LastTradeChangeTime || ConfirmTimes[I] != Trades[I]->LastConfirmTime) return false;
			return BothUnchanged(ActionA, ActionB);
		};
		for (int32 Side = 0; Side < 2; ++Side)
		{
			Trades[Side]->LastOfferChangeTime = World->GetTimeSeconds() - 2.f; // Rejection cannot rely on throttle.
			for (int32 Kind = 0; Kind < 3; ++Kind)
			{
				const uint64 Session = Trades[Side]->TradeSessionId;
				const int32 Revision = Trades[Side]->OfferRevision;
				if (!Check(RejectedAction(Side, Kind, OldActionSession, Revision), TEXT("old session offer action"))) return false;
				if (!Check(RejectedAction(Side, Kind, 0, Revision), TEXT("zero session offer action"))) return false;
				if (!Check(RejectedAction(Side, Kind, Session + 1, Revision), TEXT("future session offer action"))) return false;
				if (!Check(RejectedAction(Side, Kind, Session, Revision - 1), TEXT("old revision offer action"))) return false;
				if (!Check(RejectedAction(Side, Kind, Session, Revision + 1), TEXT("future revision offer action"))) return false;
				if (!Check(RejectedAction(Side, Kind, Session, -1), TEXT("negative revision offer action"))) return false;
				++Trades[1 - Side]->TradeSessionId;
				const bool SessionRejected = RejectedAction(Side, Kind, Session, Revision);
				Trades[1 - Side]->TradeSessionId = Session;
				if (!Check(SessionRejected, TEXT("partner session offer action"))) return false;
				++Trades[1 - Side]->OfferRevision;
				const bool RevisionRejected = RejectedAction(Side, Kind, Session, Revision);
				Trades[1 - Side]->OfferRevision = Revision;
				if (!Check(RevisionRejected, TEXT("partner revision offer action"))) return false;
			}
		}
		for (int32 Side = 0; Side < 2; ++Side)
		{
			for (int32 Kind = 0; Kind < 3; ++Kind)
			{
				const uint64 Session = Trades[Side]->TradeSessionId;
				const int32 Revision = Trades[Side]->OfferRevision;
				Trades[Side]->LastOfferChangeTime = World->GetTimeSeconds() - 2.f;
				// Seen HUD path and current-snapshot Console/C++ path must both remain usable.
				if (Side == 0) Action(Side, Kind, Session, Revision);
				else if (Kind == 0) Trades[Side]->RequestAddItem(Inventories[Side]->EquipmentBag[1].InstanceId);
				else if (Kind == 1) Trades[Side]->RequestRemoveItem(Inventories[Side]->EquipmentBag[0].InstanceId);
				else Trades[Side]->RequestSetGold(17);
				const int32 ExpectedItems = Kind == 0 ? 2 : 1;
				if (!Check(Trades[Side]->OfferRevision == Revision + 1 && Trades[1 - Side]->OfferRevision == Revision + 1
					&& Trades[Side]->MyOffer.Items.Num() == ExpectedItems && (Kind != 2 || Trades[Side]->MyOffer.Gold == 17)
					&& !Trades[0]->MyOffer.bConfirmed && !Trades[1]->MyOffer.bConfirmed
					&& SameOffer(Trades[Side]->MyOffer, Trades[1 - Side]->TheirOffer) && BothUnchanged(ActionA, ActionB),
					TEXT("current offer action/revision/mirror/approval reset"))) return false;
				Trades[Side]->LastOfferChangeTime = World->GetTimeSeconds() - 2.f;
				// A different actionable payload isolates old-revision rejection from idempotent no-ops.
				if (!Check(RejectedAction(Side, Kind, Session, Revision, true), TEXT("old revision actionable replay after accepted action"))) return false;
			}
		}
		Trades[0]->EndTradeServer(TEXT("fixture inactive offer actions"));
		for (int32 Kind = 0; Kind < 3; ++Kind)
			if (!Check(RejectedAction(0, Kind, Trades[0]->TradeSessionId, Trades[0]->OfferRevision) && Ended(), TEXT("inactive offer action"))) return false;
		// Cancellation has session identity, but deliberately does not depend on the offer revision.
		auto CancelUnchanged = [&](int32 Side, uint64 SeenSession, int32 Intent = 0)
		{
			struct FCancelSnapshot
			{
				FMMOTradeOffer Mine, Theirs; FMMOCharacterSaveData Inventory;
				TWeakObjectPtr<AMMOCharacter> Peer, Pending; FString Name;
				uint64 Session, Invite; int32 Revision; float OfferTime, ChangeTime, ConfirmTime, InviteTime, LastInvite;
				bool Active, Timer;
			} Before[2];
			for (int32 I = 0; I < 2; ++I)
			{
				const UMMOTradeComponent* T = Trades[I]; FCancelSnapshot& S = Before[I];
				S.Mine = T->MyOffer; S.Theirs = T->TheirOffer; S.Inventory = Snapshot(Inventories[I]);
				S.Peer = T->Partner; S.Pending = T->PendingInviteFrom; S.Name = T->PartnerName;
				S.Session = T->TradeSessionId; S.Invite = T->PendingInviteId; S.Revision = T->OfferRevision; S.OfferTime = T->LastOfferChangeTime;
				S.ChangeTime = T->LastTradeChangeTime; S.ConfirmTime = T->LastConfirmTime;
				S.InviteTime = T->PendingInviteTime; S.LastInvite = T->LastInviteTime; S.Active = T->bTradeActive;
				S.Timer = World->GetTimerManager().IsTimerActive(T->ValidateTimerHandle);
			}
			if (Intent == 1) Trades[Side]->RequestDeclineInviteSeen(SeenSession);
			else if (Intent == 2) Trades[Side]->RequestAcceptSeen(SeenSession);
			else Trades[Side]->RequestCancelSeen(SeenSession);
			for (int32 I = 0; I < 2; ++I)
			{
				const UMMOTradeComponent* T = Trades[I]; const FCancelSnapshot& S = Before[I];
				if (!SameOffer(S.Mine, T->MyOffer) || !SameOffer(S.Theirs, T->TheirOffer) || !Equal(S.Inventory, Snapshot(Inventories[I]))
					|| S.Peer != T->Partner || S.Pending != T->PendingInviteFrom || S.Name != T->PartnerName
					|| S.Session != T->TradeSessionId || S.Invite != T->PendingInviteId || S.Revision != T->OfferRevision || S.OfferTime != T->LastOfferChangeTime
					|| S.ChangeTime != T->LastTradeChangeTime || S.ConfirmTime != T->LastConfirmTime
					|| S.InviteTime != T->PendingInviteTime || S.LastInvite != T->LastInviteTime || S.Active != T->bTradeActive
					|| S.Timer != World->GetTimerManager().IsTimerActive(T->ValidateTimerHandle)) return false;
			}
			return true;
		};
		Reset();
		const uint64 OldCancelSession = Trades[0]->TradeSessionId;
		Trades[0]->EndTradeServer(TEXT("fixture cancel restart"));
		StartTradeServer(Pawns[0], Pawns[1]); Add(0, 0); Gold(1, 25); AgeConfirm(); Trades[1]->RequestConfirm();
		if (!Check(Trades[0]->TradeSessionId > OldCancelSession && Trades[0]->GetValidPartnerComp() == Trades[1]
			&& Trades[1]->MyOffer.bConfirmed, TEXT("cancel new session/approved offer fixture"))) return false;
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const uint64 Session = Trades[Side]->TradeSessionId;
			if (!Check(CancelUnchanged(Side, OldCancelSession), TEXT("previous session cancel ended new trade"))) return false;
			if (!Check(CancelUnchanged(Side, 0), TEXT("zero session cancel"))) return false;
			if (!Check(CancelUnchanged(Side, Session + 1), TEXT("future session cancel"))) return false;
			if (!Check(CancelUnchanged(Side, 0, true), TEXT("inactive decline must not cancel active trade"))) return false;
			++Trades[1 - Side]->TradeSessionId;
			const bool PeerRejected = CancelUnchanged(Side, Session);
			Trades[1 - Side]->TradeSessionId = Session;
			if (!Check(PeerRejected, TEXT("mismatched partner session cancel"))) return false;
		}
		const FMMOCharacterSaveData CancelA = Snapshot(Inventories[0]), CancelB = Snapshot(Inventories[1]);
		const uint64 DrawnCancelSession = Trades[0]->TradeSessionId;
		const int32 DrawnCancelRevision = Trades[0]->OfferRevision;
		Gold(0, 37); // A change since drawing cannot block a legitimate session cancellation.
		Trades[0]->RequestCancelSeen(DrawnCancelSession);
		if (!Check(Ended() && Trades[0]->OfferRevision != DrawnCancelRevision && BothUnchanged(CancelA, CancelB),
			TEXT("fresh cancel after revision change/no transfer/timer cleanup"))) return false;
		for (int32 Side = 0; Side < 2; ++Side)
		{
			Reset(); Gold(0, 29); Trades[Side]->RequestCancel();
			if (!Check(Ended() && BothUnchanged(CancelA, CancelB), TEXT("current snapshot cancel wrapper"))) return false;
		}
		// An old active cancel arriving after completion must not erase a new pending invitation.
		if (!Trades[0]->SetPendingInviteServer(Pawns[1]) || !Trades[1]->SetPendingInviteServer(Pawns[0]))
		{ OutError = TEXT("pending cancel fixture setup"); return false; }
		for (int32 Side = 0; Side < 2; ++Side)
		{
			if (!Check(CancelUnchanged(Side, Trades[Side]->TradeSessionId), TEXT("inactive old active cancel preserves pending invite"))) return false;
			const float PeerTime = Trades[1 - Side]->PendingInviteTime;
			Trades[Side]->RequestCancel();
			if (!Check(!Trades[Side]->PendingInviteFrom.IsValid() && Trades[Side]->PendingInviteTime == -1.f
				&& (Side == 1 || (Trades[1]->PendingInviteFrom.Get() == Pawns[0] && Trades[1]->PendingInviteTime == PeerTime))
				&& Ended() && BothUnchanged(CancelA, CancelB), TEXT("inactive cancel declines own pending invite only"))) return false;
		}
		Reset();
		Trades[0]->Partner.Reset(); // Broken reciprocal state still allows the caller to leave its own session.
		Trades[0]->RequestCancelSeen(Trades[0]->TradeSessionId);
		if (!Check(!Trades[0]->bTradeActive && !World->GetTimerManager().IsTimerActive(Trades[0]->ValidateTimerHandle)
			&& Trades[1]->bTradeActive && BothUnchanged(CancelA, CancelB), TEXT("broken reciprocal cancel local cleanup"))) return false;
		Trades[1]->RequestCancelSeen(Trades[1]->TradeSessionId);
		if (!Check(Ended() && BothUnchanged(CancelA, CancelB), TEXT("broken reciprocal peer cleanup"))) return false;
		// Invitation identity is recipient-generated and survives accept/decline/session cleanup.
		for (int32 Side = 0; Side < 2; ++Side)
		{
			UMMOTradeComponent* T = Trades[Side]; AMMOCharacter* From = Pawns[1 - Side];
			const uint64 BeforeInvite = T->PendingInviteId;
			if (!Check(T->SetPendingInviteServer(From) && T->PendingInviteId == BeforeInvite + 1,
				TEXT("first fresh recipient invitation generation"))) return false;
			const uint64 OldInvite = T->PendingInviteId;
			T->RequestDeclineInviteSeen(OldInvite);
			if (!Check(!T->PendingInviteFrom.IsValid() && T->PendingInviteTime == -1.f && T->PendingInviteId == OldInvite
				&& Ended() && BothUnchanged(CancelA, CancelB), TEXT("fresh decline retains generation/no transfer"))) return false;
			if (!Check(T->SetPendingInviteServer(From) && T->PendingInviteId == OldInvite + 1,
				TEXT("replacement invitation gets new generation"))) return false;
			const uint64 CurrentInvite = T->PendingInviteId;
			auto RejectInvite = [&](uint64 Seen, bool bAccept)
			{
				return CancelUnchanged(Side, Seen, bAccept ? 2 : 1) && T->PendingInviteFrom.Get() == From
					&& T->PendingInviteId == CurrentInvite && Ended() && BothUnchanged(CancelA, CancelB);
			};
			for (uint64 Bad : {OldInvite, uint64(0), CurrentInvite + 1})
			{
				if (!Check(RejectInvite(Bad, false), TEXT("old/zero/future decline preserves replacement invite"))) return false;
				if (!Check(RejectInvite(Bad, true), TEXT("old/zero/future accept preserves replacement invite"))) return false;
			}
			if (Side == 0) T->RequestAcceptSeen(CurrentInvite); else T->RequestAccept();
			if (!Check(Trades[0]->bTradeActive && Trades[1]->bTradeActive && T->GetValidPartnerComp() == Trades[1 - Side]
				&& !T->PendingInviteFrom.IsValid() && T->PendingInviteTime == -1.f && T->PendingInviteId == CurrentInvite
				&& BothUnchanged(CancelA, CancelB), TEXT("fresh Seen/current invitation accept starts trade once"))) return false;
			if (!Check(CancelUnchanged(Side, CurrentInvite, 1), TEXT("fresh invite decline cannot cancel active trade"))) return false;
			if (!Check(CancelUnchanged(Side, CurrentInvite, 2), TEXT("duplicate invite accept cannot restart trade"))) return false;
			T->RequestCancel();
			if (!Check(Ended() && T->PendingInviteId == CurrentInvite && BothUnchanged(CancelA, CancelB),
				TEXT("session cleanup retains recipient invite generation"))) return false;
		}
		const uint64 InviteBeforeOverflow = Trades[0]->PendingInviteId;
		Trades[0]->PendingInviteId = MAX_uint64 - 1;
		if (!Check(Trades[0]->SetPendingInviteServer(Pawns[1]) && Trades[0]->PendingInviteId == MAX_uint64,
			TEXT("invite max generation is usable"))) return false;
		Trades[0]->RequestCancel();
		if (!Check(!Trades[0]->SetPendingInviteServer(Pawns[1]) && Trades[0]->PendingInviteId == MAX_uint64
			&& !Trades[0]->PendingInviteFrom.IsValid() && Trades[0]->PendingInviteTime == -1.f,
			TEXT("invite generation overflow fails closed"))) return false;
		Trades[0]->PendingInviteId = InviteBeforeOverflow; // Isolate later trade-session overflow fixtures.
		if (!AMMOPlayerController::RunTradeChatContextSelfTest(World, OutError, OutChecks)) return false;
		// An old confirmation must not approve a different session with the same numeric revision.
		Reset(); Gold(0, 25);
		const int32 PreviousSessionRevision = Trades[0]->OfferRevision;
		const uint64 PreviousSessionId = Trades[0]->TradeSessionId;
		Trades[0]->EndTradeServer(TEXT("fixture cancel old session"));
		StartTradeServer(Pawns[0], Pawns[1]); Gold(0, 25); AgeConfirm();
		const uint64 CurrentSessionId = Trades[0]->TradeSessionId;
		if (!Check(Trades[0]->OfferRevision == PreviousSessionRevision && CurrentSessionId > PreviousSessionId
			&& Trades[1]->TradeSessionId == CurrentSessionId, TEXT("same revision/new shared session fixture"))) return false;
		FMMOCharacterSaveData SessionA = Snapshot(Inventories[0]), SessionB = Snapshot(Inventories[1]);
		const float BeforeInvalidConfirm = Trades[0]->LastConfirmTime;
		auto RejectedUnchanged = [&]()
		{ return !Trades[0]->MyOffer.bConfirmed && !Trades[1]->TheirOffer.bConfirmed
			&& Trades[0]->LastConfirmTime == BeforeInvalidConfirm && BothUnchanged(SessionA, SessionB); };
		Trades[0]->RequestConfirmSeen(PreviousSessionId, PreviousSessionRevision);
		if (!Check(RejectedUnchanged(),
			TEXT("previous session confirmation accepted"))) return false;
		Trades[0]->RequestConfirmSeen(0, PreviousSessionRevision);
		if (!Check(RejectedUnchanged(), TEXT("zero session confirmation"))) return false;
		Trades[0]->RequestConfirmSeen(CurrentSessionId + 1, PreviousSessionRevision);
		if (!Check(RejectedUnchanged(), TEXT("future session confirmation"))) return false;
		Trades[0]->RequestConfirmSeen(CurrentSessionId, PreviousSessionRevision - 1);
		if (!Check(RejectedUnchanged(), TEXT("current session stale revision"))) return false;
		++Trades[1]->TradeSessionId;
		Trades[0]->RequestConfirmSeen(CurrentSessionId, PreviousSessionRevision);
		Trades[1]->TradeSessionId = CurrentSessionId;
		if (!Check(RejectedUnchanged(), TEXT("mismatched partner session"))) return false;
		++Trades[1]->OfferRevision;
		Trades[0]->RequestConfirmSeen(CurrentSessionId, PreviousSessionRevision);
		Trades[1]->OfferRevision = PreviousSessionRevision;
		if (!Check(RejectedUnchanged(), TEXT("mismatched partner revision"))) return false;
		Trades[0]->RequestConfirmSeen(CurrentSessionId, PreviousSessionRevision);
		if (!Check(Trades[0]->MyOffer.bConfirmed && Trades[1]->TheirOffer.bConfirmed && BothUnchanged(SessionA, SessionB), TEXT("current seen session first approval"))) return false;
		Trades[1]->RequestConfirm(); // Console/C++ path intentionally uses current server snapshot.
		SessionA.Gold -= 25; SessionB.Gold += 25;
		if (!Check(Ended() && BothUnchanged(SessionA, SessionB), TEXT("current session exact completion"))) return false;
		StartTradeServer(Pawns[0], Pawns[1]); Gold(0, 25); AgeConfirm();
		Trades[1]->RequestConfirm();
		if (!Check(Trades[1]->MyOffer.bConfirmed && !Trades[0]->MyOffer.bConfirmed && BothUnchanged(SessionA, SessionB), TEXT("new session peer approved first"))) return false;
		Trades[0]->RequestConfirmSeen(CurrentSessionId, PreviousSessionRevision);
		if (!Check(Trades[0]->bTradeActive && Trades[1]->MyOffer.bConfirmed && !Trades[0]->MyOffer.bConfirmed
			&& BothUnchanged(SessionA, SessionB), TEXT("stale session must not complete peer-approved exchange"))) return false;
		Trades[0]->EndTradeServer(TEXT("fixture peer-first cleanup"));
		Trades[0]->TradeSessionId = 5; Trades[1]->TradeSessionId = 100;
		StartTradeServer(Pawns[0], Pawns[1]);
		if (!Check(Trades[0]->TradeSessionId == 101 && Trades[1]->TradeSessionId == 101, TEXT("advance beyond both participant generations"))) return false;
		StartTradeServer(Pawns[1], Pawns[0]);
		if (!Check(Trades[0]->TradeSessionId == 101 && Trades[1]->TradeSessionId == 101 && Trades[0]->GetValidPartnerComp() == Trades[1], TEXT("active session cannot be replaced"))) return false;
		Trades[0]->EndTradeServer(TEXT("fixture reversed histories"));
		Trades[0]->TradeSessionId = 200; Trades[1]->TradeSessionId = 5;
		StartTradeServer(Pawns[1], Pawns[0]);
		if (!Check(Trades[0]->TradeSessionId == 201 && Trades[1]->TradeSessionId == 201, TEXT("reversed participant histories"))) return false;
		Trades[0]->EndTradeServer(TEXT("fixture first participant maximum"));
		Trades[0]->TradeSessionId = 300; Trades[1]->TradeSessionId = 5;
		StartTradeServer(Pawns[0], Pawns[1]);
		if (!Check(Trades[0]->TradeSessionId == 301 && Trades[1]->TradeSessionId == 301, TEXT("first argument maximum generation"))) return false;
		Trades[0]->EndTradeServer(TEXT("fixture last session generation"));
		Trades[0]->TradeSessionId = MAX_uint64 - 1;
		StartTradeServer(Pawns[0], Pawns[1]);
		if (!Check(Trades[0]->TradeSessionId == MAX_uint64 && Trades[1]->TradeSessionId == MAX_uint64 && Trades[0]->GetValidPartnerComp() == Trades[1], TEXT("last session generation must not truncate"))) return false;
		Trades[0]->EndTradeServer(TEXT("fixture session overflow"));
		StartTradeServer(Pawns[0], Pawns[1]);
		if (!Check(Ended() && Trades[0]->TradeSessionId == MAX_uint64 && Trades[1]->TradeSessionId == MAX_uint64
			&& BothUnchanged(SessionA, SessionB), TEXT("session overflow must not wrap/reopen"))) return false;
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

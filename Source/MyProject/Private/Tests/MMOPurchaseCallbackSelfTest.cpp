#include "Components/MMOInventoryComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOStatsComponent.h"
#include "Engine/World.h"
#include "Items/MMOConsumableTypes.h"
#include "Items/MMOItemDatabase.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Save/MMOCharacterSaveJson.h"
#include "Save/MMOSaveGame.h"
#include "Tests/MMOPurchaseCallbackProbe.h"
#include "UObject/StrongObjectPtr.h"
#include "World/MMOVendorNPC.h"

bool UMMOInventoryComponent::RunPurchaseCallbackSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
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
		AMMOCharacter* C = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Origin, FRotator::ZeroRotator, Params);
		if (C) { Fixtures.Add(C); C->SetActorEnableCollision(false); }
		return C;
	};
	AMMOCharacter* Pawn = SpawnPawn();
	AMMOCharacter* Expected = SpawnPawn();
	AMMOCharacter* Restored = SpawnPawn();
	AMMOVendorNPC* Vendor = World->SpawnActor<AMMOVendorNPC>(AMMOVendorNPC::StaticClass(), Origin, FRotator::ZeroRotator, Params);
	if (Vendor) { Fixtures.Add(Vendor); Vendor->SetActorEnableCollision(false); }
	int32 Failures = 0;
	const auto Check = [&](const FString& Case, const TCHAR* Label, bool bOK)
	{
		++OutChecks;
		UE_LOG(LogTemp, Display, TEXT("[MMO_PURCHASE_CALLBACK_CASE] %s case=%s check=%s"), bOK ? TEXT("PASS") : TEXT("FAIL"), *Case, Label);
		if (!bOK) { ++Failures; if (OutError.IsEmpty()) OutError = Case + TEXT("/") + Label; }
	};
	const auto Snapshot = [](UMMOInventoryComponent* Inv)
	{
		FMMOCharacterSaveData D; Inv->FillSaveData(D); return D;
	};
	const auto Same = [](const FMMOCharacterSaveData& A, const FMMOCharacterSaveData& B)
	{
		TArray<FString> Differences; FMMOCharacterSaveJson::FindDifferences(A, B, Differences); return Differences.IsEmpty();
	};
	const bool bCompleted = [&]()
	{
		if (!Pawn || !Expected || !Restored || !Vendor) { OutError = TEXT("fixture spawn failed"); return false; }
		UMMOInventoryComponent* Inv = Pawn->GetInventory();
		UMMOInventoryComponent* Ref = Expected->GetInventory();
		const FMMOItemDef* Sword = FMMOItemDatabase::Find(TEXT("ShortSword"));
		const FMMOConsumableDef* Mana = FMMOConsumableDatabase::Find(TEXT("ManaPotion"));
		if (!Inv || !Ref || !Sword || !Mana) { OutError = TEXT("fixture definitions missing"); return false; }
		const auto Buy = [](UMMOInventoryComponent* I, int32 Kind)
		{
			if (Kind == 0) I->BuyEquipment(TEXT("ShortSword"));
			else if (Kind == 1) I->BuyPotion(1);
			else I->BuyConsumable(TEXT("ManaPotion"), 1);
		};
		for (int32 Kind = 0; Kind < 3; ++Kind)
		for (int32 Mode = 0; Mode < 2; ++Mode)
		for (int32 Budget = 1; Budget <= 2; ++Budget)
		for (int32 Tight = 0; Tight < 2; ++Tight)
		{
			const FString Case = FString::Printf(TEXT("kind%d_mode%d_budget%d_tight%d"), Kind, Mode, Budget, Tight);
			const int32 Price = Kind == 0 ? FMMOItemDatabase::GetBuyPrice(*Sword)
				: Kind == 1 ? FMMOItemDatabase::PotionPrice : Mana->Price;
			if (Price <= 0) { OutError = TEXT("fixture needs positive price"); return false; }
			FMMOCharacterSaveData Seed;
			Seed.Gold = Price * Budget;
			FMMOEquipmentInstance Armor; Armor.ItemId = TEXT("LeatherArmor"); Armor.InstanceId = 10;
			Armor.Plus = 2; Armor.Rarity = EMMOItemRarity::Rare;
			Seed.Warehouse.Add(Armor);
			Seed.NextInstanceId = 100;
			if (Kind == 0 && Tight)
			{
				for (int32 i = 0; i < MaxBagSize - 1; ++i) { Armor.InstanceId = 20 + i; Seed.EquipmentBag.Add(Armor); }
			}
			if (Kind != 0 && Tight)
			{
				FMMOItemStack Stack; Stack.ItemId = Kind == 1 ? TEXT("HealthPotion") : TEXT("ManaPotion");
				Stack.Count = MaxStackSize - 1; Seed.Items.Add(Stack);
			}
			Inv->ApplySaveData(Seed); Ref->ApplySaveData(Seed);
			Pawn->GetStats()->bIsDead = false; Expected->GetStats()->bIsDead = false;
			Buy(Ref, Kind);
			const FMMOCharacterSaveData Outer = Snapshot(Ref);
			Check(Case, TEXT("reference_price_delivery"), Outer.Gold == Seed.Gold - Price
				&& (Kind == 0 ? Outer.EquipmentBag.Num() == Seed.EquipmentBag.Num() + 1
					: Ref->GetItemCount(Kind == 1 ? TEXT("HealthPotion") : TEXT("ManaPotion")) == (Tight ? MaxStackSize : 1)));
			Buy(Ref, Kind); // Sequential oracle: second purchase uses remaining gold/capacity.
			const FMMOCharacterSaveData AfterNested = Snapshot(Ref);
			bool bOuterComplete = false, bGoldArgument = false;
			TStrongObjectPtr<UMMOPurchaseCallbackProbe> Probe(NewObject<UMMOPurchaseCallbackProbe>());
			Probe->Callback = [&](int32 PublishedGold)
			{
				bOuterComplete = Same(Snapshot(Inv), Outer);
				bGoldArgument = Mode != 0 || PublishedGold == Outer.Gold;
				Buy(Inv, Kind); // Real nested vendor API through a synchronous dynamic delegate.
			};
			if (Mode == 0) Inv->OnGoldChanged.AddDynamic(Probe.Get(), &UMMOPurchaseCallbackProbe::OnGold);
			else Inv->OnInventoryChanged.AddDynamic(Probe.Get(), &UMMOPurchaseCallbackProbe::OnInventory);
			Buy(Inv, Kind);
			Inv->OnGoldChanged.RemoveDynamic(Probe.Get(), &UMMOPurchaseCallbackProbe::OnGold);
			Inv->OnInventoryChanged.RemoveDynamic(Probe.Get(), &UMMOPurchaseCallbackProbe::OnInventory);
			Probe->Callback = nullptr;
			const int32 Purchases = Tight ? 1 : Budget;
			Check(Case, TEXT("exact_delegate_count"), Probe->Calls == Purchases);
			Check(Case, TEXT("first_callback_sees_paid_delivery"), bOuterComplete);
			Check(Case, TEXT("gold_argument"), bGoldArgument);
			bool bExactTotals = Inv->Gold == Seed.Gold - Purchases * Price;
			if (Kind == 0)
			{
				bExactTotals &= Inv->EquipmentBag.Num() == Seed.EquipmentBag.Num() + Purchases
					&& Inv->NextInstanceId == Seed.NextInstanceId + Purchases;
				for (int32 i = 0; i < Purchases; ++i)
				{
					const int32 Index = Seed.EquipmentBag.Num() + i;
					bExactTotals &= Inv->EquipmentBag.IsValidIndex(Index)
						&& Inv->EquipmentBag[Index].ItemId == TEXT("ShortSword")
						&& Inv->EquipmentBag[Index].InstanceId == Seed.NextInstanceId + i;
				}
			}
			else bExactTotals &= Inv->GetItemCount(Kind == 1 ? TEXT("HealthPotion") : TEXT("ManaPotion"))
				== (Tight ? MaxStackSize - 1 : 0) + Purchases;
			Check(Case, TEXT("nested_matches_sequential_and_totals"), Same(Snapshot(Inv), AfterNested) && bExactTotals);
			Inv->AddGold(Price); Ref->AddGold(Price); Buy(Inv, Kind); Buy(Ref, Kind);
			Check(Case, TEXT("later_purchase_matches_sequential"), Same(Snapshot(Inv), Snapshot(Ref)));
			Restored->GetInventory()->ApplySaveData(Snapshot(Inv));
			Check(Case, TEXT("memory_roundtrip"), Same(Snapshot(Inv), Snapshot(Restored->GetInventory())));
		}
		// Public loot/reward wrappers still publish; rejected additions stay silent.
		FMMOCharacterSaveData Empty; Empty.NextInstanceId = 100;
		Inv->ApplySaveData(Empty);
		TStrongObjectPtr<UMMOPurchaseCallbackProbe> Probe(NewObject<UMMOPurchaseCallbackProbe>());
		Inv->OnInventoryChanged.AddDynamic(Probe.Get(), &UMMOPurchaseCallbackProbe::OnInventory);
		Check(TEXT("wrappers"), TEXT("new_stack"), Inv->AddItem(TEXT("ManaPotion"), 2) == 2
			&& Probe->Calls == 1 && Inv->GetItemCount(TEXT("ManaPotion")) == 2);
		Check(TEXT("wrappers"), TEXT("existing_stack"), Inv->AddItem(TEXT("ManaPotion"), 3) == 3
			&& Probe->Calls == 2 && Inv->GetItemCount(TEXT("ManaPotion")) == 5);
		Inv->AddEquipmentInstance(TEXT("ShortSword"));
		Check(TEXT("wrappers"), TEXT("equipment"), Probe->Calls == 3 && Inv->EquipmentBag.Num() == 1 && Inv->NextInstanceId == 101);
		Inv->AddEquipmentInstance(TEXT("UnknownPurchaseFixture"));
		Check(TEXT("wrappers"), TEXT("unknown_equipment_silent"), Probe->Calls == 3 && Inv->EquipmentBag.Num() == 1 && Inv->NextInstanceId == 101);
		const bool bHasStack = Inv->Items.IsValidIndex(0);
		if (bHasStack) Inv->Items[0].Count = MaxStackSize;
		Check(TEXT("wrappers"), TEXT("full_stack_silent"), bHasStack && Inv->AddItem(TEXT("ManaPotion"), 1) == 0 && Probe->Calls == 3);
		Inv->EquipmentBag.SetNum(MaxBagSize);
		Inv->AddEquipmentInstance(TEXT("ShortSword"));
		Check(TEXT("wrappers"), TEXT("full_bag_silent"), Probe->Calls == 3 && Inv->EquipmentBag.Num() == MaxBagSize && Inv->NextInstanceId == 101);
		Inv->OnInventoryChanged.RemoveDynamic(Probe.Get(), &UMMOPurchaseCallbackProbe::OnInventory);
		return true;
	}();
	for (AActor* Fixture : Fixtures) if (IsValid(Fixture)) Fixture->Destroy();
	// Same-frame/controllerless: no player disk save, real RPC, or PIE coverage.
	if (!bCompleted) return false;
	if (OutChecks != 174) { ++Failures; OutError += TEXT(" check_count_mismatch"); }
	if (Failures) OutError = FString::Printf(TEXT("failures=%d first=%s"), Failures, *OutError);
	return Failures == 0;
}

namespace
{
	struct FMMOPurchaseCallbackTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestPurchaseCallbacks"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = UMMOInventoryComponent::RunPurchaseCallbackSelfTest(World, Error, Checks);
			Ar.Logf(TEXT("[MMO_PURCHASE_CALLBACK_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOPurchaseCallbackTestExec GMMOPurchaseCallbackTestExec;
}
#endif

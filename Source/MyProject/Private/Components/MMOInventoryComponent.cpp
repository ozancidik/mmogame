// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMOInventoryComponent.h"
#include "Items/MMOItemDatabase.h"
#include "Items/MMOConsumableTypes.h"
#include "Characters/MMOCharacter.h"
#include "Core/MMOMetrics.h"
#include "Components/MMOStatsComponent.h"
#include "Save/MMOSaveGame.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Core/MMONotify.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Player/MMOPlayerController.h"
#include "World/MMOVendorNPC.h"
#include "World/MMOWarehouseNPC.h"
#if !UE_BUILD_SHIPPING
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Save/MMOCharacterSaveJson.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMMOInventoryAudit, Log, All);

#if !UE_BUILD_SHIPPING
bool UMMOInventoryComponent::RunFirstEquipSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{ OutError = TEXT("server game world required"); return false; }
	TArray<AActor*> Fixtures;
	TArray<AMMOPlayerController*> Controllers;
	const bool bPassed = [&]()
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UMMOInventoryComponent* Inventories[2] = {};
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const FVector Location(1000000 + Index * 5000, 1000000, 5000);
			AMMOPlayerController* PC = World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(), Location, FRotator::ZeroRotator, Params);
			AMMOCharacter* Pawn = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
			if (PC) { Controllers.Add(PC); Fixtures.Add(PC); }
			if (Pawn) { Fixtures.Add(Pawn); Pawn->SetActorEnableCollision(false); }
			if (!PC || !Pawn || !Pawn->GetInventory()) { OutError = TEXT("fixture spawn failed"); return false; }
			PC->Possess(Pawn); Inventories[Index] = Pawn->GetInventory();
		}
		auto Check = [&](bool Condition, const TCHAR* Label)
		{
			if (!Condition) { OutError = Label; return false; }
			++OutChecks; return true;
		};
		UMMOInventoryComponent* Inv = Inventories[0];
		auto Snapshot = [&]() { FMMOCharacterSaveData Data; Inv->FillSaveData(Data); return Data; };
		auto Unchanged = [&](const FMMOCharacterSaveData& Before)
		{
			TArray<FString> Fields; FMMOCharacterSaveJson::FindDifferences(Before, Snapshot(), Fields); return Fields.IsEmpty();
		};
		Inv->AddEquipmentInstance(TEXT("ShortSword")); Inv->EquipStarterEquipment(1);
		Inv->AddEquipmentInstance(TEXT("LeatherArmor")); Inv->EquipStarterEquipment(2);
		if (!Check(Inv->GetEquipped(EMMOEquipSlot::Weapon).InstanceId == 1
			&& Inv->GetEquipped(EMMOEquipSlot::Armor).InstanceId == 2 && Inv->EquipmentBag.IsEmpty(), TEXT("automatic setup failed"))) return false;
		const FMMOCharacterSaveData Automatic = Snapshot();
		Inv->ApplySaveData(Automatic);
		if (!Check(Unchanged(Automatic), TEXT("save restore changed valid inventory"))) return false;
		Inv->EquipByInstanceId(0); Inv->EquipByInstanceId(-1); Inv->EquipByInstanceId(999);
		if (!Check(Unchanged(Automatic), TEXT("stale request changed inventory"))) return false;
		Inv->AddEquipmentInstance(TEXT("GreatSword"));
		const FMMOCharacterSaveData BeforeLevel = Snapshot();
		Inv->EquipByInstanceId(3);
		if (!Check(Unchanged(BeforeLevel), TEXT("level gate changed inventory"))) return false;
		Inv->AddEquipmentInstance(TEXT("ShortSword"));
		const FMMOCharacterSaveData BeforeSlot = Snapshot();
		const TArray<FMMOEquipmentInstance> Slots = Inv->EquippedItems;
		Inv->EquippedItems.Reset(); Inv->EquipByInstanceId(4); Inv->EquippedItems = Slots;
		if (!Check(Unchanged(BeforeSlot), TEXT("invalid slot consumed equipment"))) return false;
		Inv->EquipFromBag(Inv->FindBagIndexByInstanceId(4));
		const AMMOCharacter* OwnerChar = Cast<AMMOCharacter>(Inv->GetOwner());
		const FMMOItemDef* SwordDef = FMMOItemDatabase::Find(TEXT("ShortSword"));
		if (!Check(Inv->GetEquipped(EMMOEquipSlot::Weapon).InstanceId == 4 && Inv->FindBagIndexByInstanceId(1) != INDEX_NONE
			&& OwnerChar && OwnerChar->GetStats() && SwordDef
			&& FMath::IsNearlyEqual(OwnerChar->GetStats()->AttackPower,
				OwnerChar->GetStats()->Strength * OwnerChar->GetStats()->AttackPerStrength + OwnerChar->GetStats()->Dexterity * 0.5f
				+ FMMOItemDatabase::GetAttack(*SwordDef, 0)), TEXT("manual swap or bonus failed"))) return false;
		const FMMOCharacterSaveData AfterManual = Snapshot();
		Inv->EquipByInstanceId(4);
		const bool bDuplicateUnchanged = Unchanged(AfterManual);
		Inv->AddEquipmentInstance(TEXT("LeatherArmor")); Inv->EquipByInstanceId(5);
		if (!Check(bDuplicateUnchanged && Inv->GetEquipped(EMMOEquipSlot::Armor).InstanceId == 5
			&& Inv->FindBagIndexByInstanceId(2) != INDEX_NONE, TEXT("duplicate or second slot failed"))) return false;
		Inventories[1]->AddEquipmentInstance(TEXT("LeatherArmor")); Inventories[1]->EquipByInstanceId(1);
		return Check(Inventories[1]->GetEquipped(EMMOEquipSlot::Armor).InstanceId == 1
			&& Inventories[1]->EquipmentBag.IsEmpty(), TEXT("independent controller equip failed"));
	}();
	for (AMMOPlayerController* PC : Controllers) if (IsValid(PC)) PC->UnPossess();
	for (AActor* Fixture : Fixtures) if (IsValid(Fixture)) Fixture->Destroy();
	return bPassed; // Same frame, memory-only save application; no load/autosave/disk.
}

namespace
{
	struct FMMOFirstEquipTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestFirstEquip"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = UMMOInventoryComponent::RunFirstEquipSelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_FIRST_EQUIP_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOFirstEquipTestExec GMMOFirstEquipTestExec;
}

bool UMMOInventoryComponent::RunVendorEconomySelfTest(UWorld* World, FString& OutError, int32& OutChecks)
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
	const FVector Origin(1000000, 1000000, 5000);
	AMMOCharacter* Character = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Origin, FRotator::ZeroRotator, Params);
	AMMOVendorNPC* Vendor = World->SpawnActor<AMMOVendorNPC>(AMMOVendorNPC::StaticClass(), Origin + FVector(100, 0, 0), FRotator::ZeroRotator, Params);
	if (!Character || !Vendor)
	{
		if (Character) Character->Destroy();
		if (Vendor) Vendor->Destroy();
		OutError = TEXT("fixture spawn failed");
		return false;
	}
	Vendor->SetActorEnableCollision(false);
	const bool bPassed = [&]()
	{
		UMMOInventoryComponent* Inv = Character->GetInventory();
		UMMOStatsComponent* Stats = Character->GetStats();
		const FMMOItemDef* Sword = FMMOItemDatabase::Find(TEXT("ShortSword"));
		const FMMOConsumableDef* Mana = FMMOConsumableDatabase::Find(TEXT("ManaPotion"));
		if (!Inv || !Stats || !Sword || !Mana) { OutError = TEXT("fixture definitions missing"); return false; }
		auto Check = [&](bool Condition, const TCHAR* Label)
		{
			if (!Condition) { OutError = Label; return false; }
			++OutChecks;
			return true;
		};
		auto Reset = [&]() { Inv->Items.Reset(); Inv->EquipmentBag.Reset(); Inv->Gold = 100000; Stats->bIsDead = false; Character->SetActorLocation(Origin); };
		Reset();
		Inv->BuyEquipment(TEXT("ShortSword"));
		if (!Check(Inv->EquipmentBag.Num() == 1 && Inv->Gold == 100000 - FMMOItemDatabase::GetBuyPrice(*Sword), TEXT("equipment price or delivery wrong"))) return false;
		Reset();
		for (int32 Index = 0; Index < MaxBagSize; ++Index) Inv->AddEquipmentInstance(TEXT("ShortSword"), 0);
		Inv->BuyEquipment(TEXT("ShortSword"));
		if (!Check(Inv->EquipmentBag.Num() == MaxBagSize && Inv->Gold == 100000, TEXT("full bag charged"))) return false;
		Reset(); Inv->Gold = 0;
		Inv->BuyEquipment(TEXT("ShortSword")); Inv->BuyPotion(1); Inv->BuyConsumable(TEXT("ManaPotion"), 1);
		if (!Check(Inv->Gold == 0 && Inv->Items.IsEmpty() && Inv->EquipmentBag.IsEmpty(), TEXT("insufficient gold created items"))) return false;
		Reset();
		Inv->BuyEquipment(TEXT("UnknownVendorFixture")); Inv->BuyConsumable(TEXT("UnknownVendorFixture"), MAX_int32);
		if (!Check(Inv->Gold == 100000 && Inv->Items.IsEmpty() && Inv->EquipmentBag.IsEmpty(), TEXT("unknown purchase changed inventory"))) return false;
		Inv->AddEquipmentInstance(TEXT("ShortSword"), 0);
		const int32 SaleId = Inv->EquipmentBag[0].InstanceId;
		Stats->bIsDead = true;
		Inv->BuyEquipment(TEXT("ShortSword")); Inv->BuyPotion(1); Inv->BuyConsumable(TEXT("ManaPotion"), 1); Inv->SellByInstanceId(SaleId);
		if (!Check(Inv->Gold == 100000 && Inv->Items.IsEmpty() && Inv->EquipmentBag.Num() == 1 && Inv->EquipmentBag[0].InstanceId == SaleId, TEXT("dead vendor mutation"))) return false;
		Stats->bIsDead = false; Character->SetActorLocation(Origin + FVector(2000, 0, 0));
		Inv->BuyEquipment(TEXT("ShortSword")); Inv->BuyPotion(1); Inv->BuyConsumable(TEXT("ManaPotion"), 1); Inv->SellByInstanceId(SaleId);
		if (!Check(Inv->Gold == 100000 && Inv->Items.IsEmpty() && Inv->EquipmentBag.Num() == 1 && Inv->EquipmentBag[0].InstanceId == SaleId, TEXT("distant vendor mutation"))) return false;
		Reset(); Inv->BuyPotion(MAX_int32);
		if (!Check(Inv->Gold == 100000 - FMMOItemDatabase::PotionPrice * MaxStackSize && Inv->GetItemCount(TEXT("HealthPotion")) == MaxStackSize, TEXT("oversized potion request created gold or exceeded cap"))) return false;
		Reset(); Inv->AddItem(TEXT("HealthPotion"), MaxStackSize);
		Inv->BuyPotion(MAX_int32);
		if (!Check(Inv->Gold == 100000 && Inv->GetItemCount(TEXT("HealthPotion")) == MaxStackSize, TEXT("full potion stack charged"))) return false;
		Reset(); Inv->AddItem(TEXT("HealthPotion"), MaxStackSize - 1);
		Inv->BuyPotion(MAX_int32);
		if (!Check(Inv->Gold == 100000 - FMMOItemDatabase::PotionPrice && Inv->GetItemCount(TEXT("HealthPotion")) == MaxStackSize, TEXT("partial potion purchase not priced by delivery"))) return false;
		Reset(); Inv->AddItem(TEXT("ManaPotion"), MaxStackSize - 1);
		Inv->BuyConsumable(TEXT("ManaPotion"), MAX_int32);
		if (!Check(Inv->Gold == 100000 - Mana->Price && Inv->GetItemCount(TEXT("ManaPotion")) == MaxStackSize, TEXT("partial consumable purchase not priced by delivery"))) return false;
		Inv->Gold = 100000; Inv->BuyConsumable(TEXT("ManaPotion"), MAX_int32);
		if (!Check(Inv->Gold == 100000 && Inv->GetItemCount(TEXT("ManaPotion")) == MaxStackSize, TEXT("full consumable stack charged"))) return false;
		Reset();
		for (int32 Index = 0; Index < MaxItemStacks; ++Index) Inv->AddItem(FName(*FString::Printf(TEXT("VendorStackFixture%d"), Index)), 1);
		Inv->BuyPotion(1); Inv->BuyConsumable(TEXT("ManaPotion"), 1);
		if (!Check(Inv->Gold == 100000 && Inv->Items.Num() == MaxItemStacks && Inv->GetItemCount(TEXT("HealthPotion")) == 0 && Inv->GetItemCount(TEXT("ManaPotion")) == 0, TEXT("full stack list charged"))) return false;
		Inv->Items[0].ItemId = TEXT("HealthPotion"); Inv->Items[0].Count = MaxStackSize - 1;
		Inv->BuyPotion(MAX_int32);
		if (!Check(Inv->Gold == 100000 - FMMOItemDatabase::PotionPrice && Inv->Items.Num() == MaxItemStacks && Inv->GetItemCount(TEXT("HealthPotion")) == MaxStackSize, TEXT("existing stack at list limit rejected"))) return false;
		// Seviye kapisi: alici seviyesi + VendorLevelSlack < RequiredLevel ise reddedilir (altin ve canta degismez);
		// tam sinirda (RequiredLevel - Slack) alinir.
		const FMMOItemDef* HighItem = FMMOItemDatabase::Find(TEXT("AbyssBlade"));
		if (!HighItem || HighItem->RequiredLevel <= UMMOInventoryComponent::VendorLevelSlack + 1) { OutError = TEXT("level gate fixture item missing"); return false; }
		Reset(); Stats->Level = HighItem->RequiredLevel - UMMOInventoryComponent::VendorLevelSlack - 1;
		Inv->BuyEquipment(TEXT("AbyssBlade"));
		if (!Check(Inv->Gold == 100000 && Inv->EquipmentBag.IsEmpty(), TEXT("level gate allowed an item above the slack"))) return false;
		Stats->Level = HighItem->RequiredLevel - UMMOInventoryComponent::VendorLevelSlack;
		Inv->BuyEquipment(TEXT("AbyssBlade"));
		if (!Check(Inv->EquipmentBag.Num() == 1 && Inv->Gold == 100000 - FMMOItemDatabase::GetBuyPrice(*HighItem), TEXT("level gate rejected an item at the slack limit"))) return false;
		// bVendorSold=false (boss/odul seti): seviye yeterli ve altin bol olsa bile satilmaz.
		const FMMOItemDef* Throne = FMMOItemDatabase::Find(TEXT("ThroneBlade"));
		if (!Throne || Throne->bVendorSold) { OutError = TEXT("non-vendor fixture item missing"); return false; }
		Reset(); Stats->Level = Throne->RequiredLevel;
		Inv->BuyEquipment(TEXT("ThroneBlade"));
		if (!Check(Inv->Gold == 100000 && Inv->EquipmentBag.IsEmpty(), TEXT("vendor sold a non-vendor item"))) return false;
		Stats->Level = 1;
		// Yukseltme maliyeti esya seviyesiyle olceklenir (altin sink'i): 50 x (+N) x (100 + Lv^2) / 100; Lv0 eski sabit maliyet.
		if (!Check(FMMOItemDatabase::GetUpgradeCost(0) == 50 && FMMOItemDatabase::GetUpgradeCost(2, 0) == 150
			&& FMMOItemDatabase::GetUpgradeCost(2, 30) == 1500 && FMMOItemDatabase::GetUpgradeCost(0, 78) == 3092
			&& FMMOItemDatabase::GetUpgradeCost(8, 80) == 29250, TEXT("level-scaled upgrade cost wrong"))) return false;
		Reset(); Inv->AddEquipmentInstance(TEXT("ShortSword"), 0);
		const int32 FinalSaleId = Inv->EquipmentBag[0].InstanceId;
		const int32 ExpectedGold = 100000 + FMMOItemDatabase::GetSellPrice(*Sword, 0);
		Inv->SellByInstanceId(FinalSaleId); Inv->SellByInstanceId(FinalSaleId);
		return Check(Inv->Gold == ExpectedGold && Inv->EquipmentBag.IsEmpty(), TEXT("sale repeated or price wrong"));
	}();
	Vendor->Destroy(); Character->Destroy(); // Controllersiz; oyuncu kaydi yuklenmez/yazilmaz.
	return bPassed;
}

bool UMMOInventoryComponent::RunSavedEquipmentLevelSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
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
		UMMOInventoryComponent* Inv = Character->GetInventory();
		UMMOStatsComponent* Stats = Character->GetStats();
		if (!Inv || !Stats) { OutError = TEXT("fixture components missing"); return false; }
		auto Check = [&](bool Condition, const TCHAR* Label)
		{
			if (!Condition) { OutError = Label; return false; }
			++OutChecks;
			return true;
		};
		auto Instance = [](FName ItemId, int32 Id)
		{
			FMMOEquipmentInstance I;
			I.ItemId = ItemId; I.InstanceId = Id; I.Plus = 1; I.Rarity = EMMOItemRarity::Rare;
			return I;
		};
		const int32 Weapon = static_cast<int32>(EMMOEquipSlot::Weapon), Armor = static_cast<int32>(EMMOEquipSlot::Armor);
		FMMOCharacterSaveData Data;
		Data.bValid = true; Data.Level = 1;
		Data.EquippedItems.SetNum(static_cast<int32>(EMMOEquipSlot::MAX));
		Data.EquippedItems[Weapon] = Instance(TEXT("LongSword"), 101);
		Data.EquippedItems[Armor] = Instance(TEXT("PlateArmor"), 102);
		float BaseAttack = 0.f, BaseDefense = 0.f;
		auto Apply = [&]()
		{
			Stats->SetEquipmentBonuses(0.f, 0.f);
			Stats->ApplySaveData(Data);
			BaseAttack = Stats->AttackPower; BaseDefense = Stats->Defense;
			Inv->ApplySaveData(Data); // LoadCharacter: stats, sonra envanter.
		};
		Apply();
		if (!Check(!Inv->EquippedItems[Weapon].IsValidInstance() && !Inv->EquippedItems[Armor].IsValidInstance(), TEXT("underlevel saved equipment remained equipped"))) return false;
		if (!Check(Inv->EquipmentBag.Num() == 2 && Inv->EquipmentBag.ContainsByPredicate([](const FMMOEquipmentInstance& I) { return I.ItemId == FName(TEXT("LongSword")) && I.InstanceId == 101 && I.Plus == 1 && I.Rarity == EMMOItemRarity::Rare; }), TEXT("relocated equipment lost identity"))) return false;
		if (!Check(Stats->AttackPower == BaseAttack && Stats->Defense == BaseDefense, TEXT("underlevel saved bonus active"))) return false;
		Inv->EquipByInstanceId(101);
		if (!Check(!Inv->EquippedItems[Weapon].IsValidInstance() && Inv->EquipmentBag.Num() == 2, TEXT("underlevel re-equip accepted"))) return false;
		Data.Level = 3; Apply();
		if (!Check(Inv->EquippedItems[Weapon].InstanceId == 101 && !Inv->EquippedItems[Armor].IsValidInstance() && Stats->AttackPower > BaseAttack, TEXT("exact required level rejected"))) return false;
		Data.Level = 1;
		Data.EquippedItems[Weapon] = Instance(TEXT("RemovedFutureItemFixture"), 103);
		Data.EquippedItems[Armor] = Instance(TEXT("LeatherArmor"), 104);
		Apply();
		if (!Check(Inv->EquippedItems[Weapon].ItemId == FName(TEXT("RemovedFutureItemFixture")) && Inv->EquippedItems[Armor].InstanceId == 104 && Stats->AttackPower == BaseAttack && Stats->Defense > BaseDefense, TEXT("unknown preservation or legal armor changed"))) return false;
		Data.Level = 3; Data.EquippedItems[Weapon] = FMMOEquipmentInstance();
		Data.EquippedItems[Armor] = Instance(TEXT("LongSword"), 105); Apply();
		if (!Check(!Inv->EquippedItems[Armor].IsValidInstance() && Inv->EquipmentBag.Num() == 1, TEXT("wrong slot accepted"))) return false;
		Data.Level = 1; Data.EquippedItems[Armor] = FMMOEquipmentInstance();
		Data.EquippedItems[Weapon] = Instance(TEXT("LongSword"), 101);
		for (int32 Index = 0; Index < MaxBagSize; ++Index) Data.EquipmentBag.Add(Instance(TEXT("ShortSword"), 200 + Index));
		Apply();
		if (!Check(Inv->EquipmentBag.Num() == MaxBagSize && Inv->Warehouse.Num() == 1 && Inv->Warehouse[0].InstanceId == 101, TEXT("full bag lost relocated equipment"))) return false;
		for (int32 Index = 0; Index < MaxWarehouseSize; ++Index) Data.Warehouse.Add(Instance(TEXT("ShortSword"), 400 + Index));
		Apply();
		if (!Check(Inv->EquipmentBag.Num() == MaxBagSize && Inv->Warehouse.Num() == MaxWarehouseSize + 1, TEXT("full warehouse lost equipment"))) return false;
		FMMOCharacterSaveData Back;
		Stats->FillSaveData(Back); Inv->FillSaveData(Back);
		Stats->ApplySaveData(Back); Inv->ApplySaveData(Back);
		if (!Check(!Inv->EquippedItems[Weapon].IsValidInstance() && Inv->EquipmentBag.Num() == MaxBagSize && Inv->Warehouse.Num() == MaxWarehouseSize + 1, TEXT("roundtrip changed recovery"))) return false;
		Data.EquipmentBag.Reset(); Data.Warehouse.Reset();
		Data.EquipmentBag.Add(Instance(TEXT("ShortSword"), 101)); Apply();
		return Check(Inv->EquipmentBag.Num() == 2 && Inv->EquipmentBag[0].InstanceId != Inv->EquipmentBag[1].InstanceId, TEXT("relocation duplicate identity"));
	}();
	Character->Destroy(); // Controllersiz fixture; oyuncu diski yuklenmez/yazilmaz.
	return bPassed;
}

namespace
{
	struct FMMOSavedEquipmentLevelTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (FParse::Command(&Cmd, TEXT("MMOTestVendorEconomy")))
			{
				FString Error;
				int32 Checks = 0;
				const bool bPassed = UMMOInventoryComponent::RunVendorEconomySelfTest(InWorld, Error, Checks);
				Ar.Logf(TEXT("[MMO_VENDOR_ECONOMY_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
				return true;
			}
			if (!FParse::Command(&Cmd, TEXT("MMOTestSavedEquipmentLevel"))) return false;
			FString Error;
			int32 Checks = 0;
			const bool bPassed = UMMOInventoryComponent::RunSavedEquipmentLevelSelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_SAVED_EQUIPMENT_LEVEL_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOSavedEquipmentLevelTestExec GMMOSavedEquipmentLevelTestExec;
}
#endif

namespace
{
	FString InventoryActorName(const UMMOInventoryComponent* Inventory)
	{
		const AMMOCharacter* Character = Inventory ? Cast<AMMOCharacter>(Inventory->GetOwner()) : nullptr;
		const APlayerState* PlayerState = Character ? Character->GetPlayerState() : nullptr;
		return PlayerState ? PlayerState->GetPlayerName() : TEXT("?");
	}

	FString InventorySlotName(const UMMOInventoryComponent* Inventory)
	{
		const AMMOCharacter* Character = Inventory ? Cast<AMMOCharacter>(Inventory->GetOwner()) : nullptr;
		return Character ? Character->GetSaveSlotName() : TEXT("?");
	}
}

UMMOInventoryComponent::UMMOInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// 4 sabit ekipman slotu (Weapon/Armor/Helmet/Boots) — bos orneklerle basla.
	EquippedItems.SetNum((int32)EMMOEquipSlot::MAX);
}

void UMMOInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// NETWORK_AUDIT Y2: envanter yalniz sahibine replike edilir (gizlilik + bant).
	// Baska oyuncunun envanterini okuyan istemci kodu yok (HUD yalniz kendi pawn'ini cizer;
	// ticaret karsi teklifi UMMOTradeComponent'te ayri OwnerOnly alanlarda). EquippedItems
	// icin gorsel/isim plakasi kullanimi yok; ileride gerekirse ayri hafif alan eklenmeli.
	DOREPLIFETIME_CONDITION(UMMOInventoryComponent, Gold, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMMOInventoryComponent, Items, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMMOInventoryComponent, EquipmentBag, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMMOInventoryComponent, EquippedItems, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMMOInventoryComponent, Warehouse, COND_OwnerOnly);
}

bool UMMOInventoryComponent::HasAuthorityInternal() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->HasAuthority() : false;
}

bool UMMOInventoryComponent::IsLocallyControlledOwner() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return OwnerPawn ? OwnerPawn->IsLocallyControlled() : false;
}

void UMMOInventoryComponent::AddGold(int32 Amount)
{
	if (!HasAuthorityInternal() || Amount == 0)
	{
		return;
	}

	// GUVENLIK/BUG-HUNT (qa-kontrolcu bulgusu, hardening): Gold+Amount int32'de tasarsa
	// (Gold gercekten ~2.1 milyara yaklastiginda, normal uzun-vadeli oynanista ihtimal
	// dahilinde) sonuc negatife doner ve FMath::Max(0,...) oyuncunun TUM altinini
	// sifirlardi. int64'te topla, sonra guvenle [0, MaxGold] araligina sikistir (MaxGold kayit tavaniyla ayni).
	const int64 Sum = (int64)Gold + (int64)Amount;
	const int32 OldGold = Gold;
	Gold = (int32)FMath::Clamp<int64>(Sum, 0, MaxGold);
	OnGoldChanged.Broadcast(Gold);

	// Tavan asimi sessiz olmasin: kazanc kirpildiysa sahibe haber ver (ticarette once reddedildigi icin
	// buraya yalnizca loot/gorev/satis gibi tek tarafli kazanclar duser).
	if (Amount > 0 && Sum > MaxGold)
	{
		MMONotify::ToOwner(GetOwner(), FString::Printf(TEXT("Altin siniri (%d): fazlasi kaybedildi"), MaxGold),
			FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
	}

	// Olcum gunlugu (yalniz oyuncular): gercekte degisen miktar (+kazanc / -harcama).
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()); OwnerPawn && OwnerPawn->IsPlayerControlled() && Gold != OldGold)
	{
		UE_LOG(LogTemp, Display, TEXT("[MMOMETRIC] t=%.1f ev=gold who=%s src=%s amount=%d balance=%d"),
			MMOMetrics::Seconds(), *MMOMetrics::WhoName(OwnerPawn), GoldSourceTag ? GoldSourceTag : TEXT("diger"),
			Gold - OldGold, Gold);
	}
}

void UMMOInventoryComponent::AddGoldFrom(int32 Amount, const TCHAR* Source)
{
	GoldSourceTag = Source;
	AddGold(Amount);
	GoldSourceTag = nullptr;
}

int32 UMMOInventoryComponent::GetAddRoom(FName ItemId) const
{
	if (ItemId.IsNone())
	{
		return 0;
	}
	for (const FMMOItemStack& Stack : Items)
	{
		if (Stack.ItemId == ItemId)
		{
			return FMath::Max(0, MaxStackSize - FMath::Max(0, Stack.Count));
		}
	}
	return Items.Num() >= MaxItemStacks ? 0 : MaxStackSize;
}

int32 UMMOInventoryComponent::AddItem(FName ItemId, int32 Count)
{
	if (!HasAuthorityInternal() || ItemId.IsNone() || Count <= 0)
	{
		return 0;
	}

	// Tavan: fazlasi eklenmez (yukleme kirpmasi kalici kayip olurdu); eklenen adet dondurulur.
	const int32 ToAdd = FMath::Min(Count, GetAddRoom(ItemId));
	if (ToAdd <= 0)
	{
		return 0;
	}

	for (FMMOItemStack& Stack : Items)
	{
		if (Stack.ItemId == ItemId)
		{
			Stack.Count = FMath::Max(0, Stack.Count) + ToAdd; // GetAddRoom tavani asmamayi garanti eder
			OnInventoryChanged.Broadcast();
			return ToAdd;
		}
	}

	FMMOItemStack NewStack;
	NewStack.ItemId = ItemId;
	NewStack.Count = ToAdd;
	Items.Add(NewStack);
	OnInventoryChanged.Broadcast();
	return ToAdd;
}

bool UMMOInventoryComponent::RemoveItem(FName ItemId, int32 Count)
{
	if (!HasAuthorityInternal() || ItemId.IsNone() || Count <= 0)
	{
		return false;
	}

	for (int32 i = 0; i < Items.Num(); ++i)
	{
		if (Items[i].ItemId == ItemId)
		{
			if (Items[i].Count < Count)
			{
				return false; // yeterli yok
			}
			Items[i].Count -= Count;
			if (Items[i].Count <= 0)
			{
				Items.RemoveAt(i);
			}
			OnInventoryChanged.Broadcast();
			return true;
		}
	}
	return false;
}

int32 UMMOInventoryComponent::GetItemCount(FName ItemId) const
{
	for (const FMMOItemStack& Stack : Items)
	{
		if (Stack.ItemId == ItemId)
		{
			return Stack.Count;
		}
	}
	return 0;
}

int32 UMMOInventoryComponent::GetTotalItemCount() const
{
	int64 Total = 0;
	for (const FMMOItemStack& Stack : Items)
	{
		Total += FMath::Max(0, Stack.Count);
	}
	return static_cast<int32>(FMath::Min<int64>(Total, MAX_int32));
}

void UMMOInventoryComponent::OnRep_Gold()
{
	OnGoldChanged.Broadcast(Gold);
}

void UMMOInventoryComponent::OnRep_Items()
{
	OnInventoryChanged.Broadcast();
}

// ----------------------------------------------------------------------
// Ekipman
// ----------------------------------------------------------------------

FMMOEquipmentInstance UMMOInventoryComponent::GetEquipped(EMMOEquipSlot Slot) const
{
	const int32 Index = (int32)Slot;
	return EquippedItems.IsValidIndex(Index) ? EquippedItems[Index] : FMMOEquipmentInstance();
}

void UMMOInventoryComponent::AddEquipmentInstance(FName ItemId, int32 Plus, EMMOItemRarity Rarity)
{
	if (!HasAuthorityInternal() || !FMMOItemDatabase::Find(ItemId))
	{
		return;
	}

	// Canta dolu (bug-hunt: HUD zaten TUM cantayi gosterecek sekilde MaxBagSize'a
	// gore boyutlaniyor — bunu asmak eski "gorunmez/erisilemez item" bug'ini geri
	// getirir). Loot/odul/satin-alma sessizce kaybolur; oyuncu once bosaltmali.
	if (EquipmentBag.Num() >= MaxBagSize)
	{
		MMONotify::ToOwner(GetOwner(), TEXT("Canta dolu!"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	FMMOEquipmentInstance Inst;
	Inst.InstanceId = NextInstanceId++;
	Inst.ItemId = ItemId;
	Inst.Plus = FMath::Clamp(Plus, 0, FMMOItemDatabase::MaxPlus);
	Inst.Rarity = Rarity;
	EquipmentBag.Add(Inst);
	OnInventoryChanged.Broadcast();
}

void UMMOInventoryComponent::RecomputeEquipmentBonuses()
{
	if (!HasAuthorityInternal())
	{
		return;
	}

	float TotalAttack = 0.f;
	float TotalDefense = 0.f;
	for (const FMMOEquipmentInstance& Inst : EquippedItems)
	{
		if (!Inst.IsValidInstance())
		{
			continue;
		}
		if (const FMMOItemDef* Def = FMMOItemDatabase::Find(Inst.ItemId))
		{
			const float RarityMult = FMMOItemDatabase::GetRarityMultiplier(Inst.Rarity);
			TotalAttack += FMMOItemDatabase::GetAttack(*Def, Inst.Plus) * RarityMult;
			TotalDefense += FMMOItemDatabase::GetDefense(*Def, Inst.Plus) * RarityMult;
		}
	}

	if (const AMMOCharacter* OwnerChar = Cast<AMMOCharacter>(GetOwner()))
	{
		if (UMMOStatsComponent* S = OwnerChar->GetStats())
		{
			S->SetEquipmentBonuses(TotalAttack, TotalDefense);
		}
	}
}

int32 UMMOInventoryComponent::FindBagIndexByInstanceId(int32 InstanceId) const
{
	if (InstanceId == 0)
	{
		return INDEX_NONE;
	}
	return EquipmentBag.IndexOfByPredicate([InstanceId](const FMMOEquipmentInstance& I)
	{
		return I.InstanceId == InstanceId;
	});
}

void UMMOInventoryComponent::NotifyOwnerText(const TCHAR* Text) const
{
	// Uzak istemcinin pawn'i sunucuda IsLocallyControlled degildir: sahibin controller'ina
	// owner-only sistem sohbeti satiri olarak gonder (ClanComponent::NotifyOwner ile ayni yol).
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (AMMOPlayerController* PC = Cast<AMMOPlayerController>(OwnerPawn->GetController()))
		{
			PC->ClientReceiveChat(FString(), FString(Text), 2);
		}
	}
}

void UMMOInventoryComponent::EquipFromBag(int32 BagIndex)
{
	// Indeks yalniz bu (sahip) istemcinin kendi gorunumunde cozulur; sunucuya InstanceId gider.
	if (EquipmentBag.IsValidIndex(BagIndex))
	{
		EquipByInstanceId(EquipmentBag[BagIndex].InstanceId);
	}
}

void UMMOInventoryComponent::EquipByInstanceId(int32 InstanceId)
{
	EquipByInstanceIdInternal(InstanceId, true);
}

void UMMOInventoryComponent::EquipStarterEquipment(int32 InstanceId)
{
	if (!HasAuthorityInternal()) return;
	EquipByInstanceIdInternal(InstanceId, false);
}

void UMMOInventoryComponent::EquipByInstanceIdInternal(int32 InstanceId, bool bRecordFirstEquip)
{
	if (InstanceId == 0)
	{
		return;
	}

	if (!HasAuthorityInternal())
	{
		ServerEquipByInstanceId(InstanceId);
		return;
	}

	const int32 BagIndex = FindBagIndexByInstanceId(InstanceId);
	if (BagIndex == INDEX_NONE)
	{
		return; // bayat istek: esya artik cantada degil
	}

	const FMMOEquipmentInstance Inst = EquipmentBag[BagIndex];
	const FMMOItemDef* Def = FMMOItemDatabase::Find(Inst.ItemId);
	if (!Def)
	{
		return;
	}

	// Level sarti.
	const AMMOCharacter* OwnerChar = Cast<AMMOCharacter>(GetOwner());
	const UMMOStatsComponent* S = OwnerChar ? OwnerChar->GetStats() : nullptr;
	if (!S || S->Level < Def->RequiredLevel)
	{
		return;
	}

	const int32 SlotIndex = (int32)Def->Slot;
	if (!EquippedItems.IsValidIndex(SlotIndex))
	{
		return;
	}

	// Takas: slottaki (varsa) cantaya doner.
	EquipmentBag.RemoveAt(BagIndex);
	if (EquippedItems[SlotIndex].IsValidInstance())
	{
		EquipmentBag.Add(EquippedItems[SlotIndex]);
	}
	EquippedItems[SlotIndex] = Inst;

	RecomputeEquipmentBonuses();
	if (bRecordFirstEquip)
	{
		if (AMMOPlayerController* PC = Cast<AMMOPlayerController>(OwnerChar->GetController()))
			PC->RecordFirstEquipMetric(Inst.ItemId, Inst.InstanceId, SlotIndex);
	}
	OnInventoryChanged.Broadcast();
}

void UMMOInventoryComponent::ServerEquipByInstanceId_Implementation(int32 InstanceId)
{
	EquipByInstanceId(InstanceId);
}

void UMMOInventoryComponent::UnequipSlot(EMMOEquipSlot Slot)
{
	const int32 SlotIndex = (int32)Slot;
	if (!EquippedItems.IsValidIndex(SlotIndex) || !EquippedItems[SlotIndex].IsValidInstance())
	{
		return;
	}

	if (!HasAuthorityInternal())
	{
		ServerUnequipSlot(Slot);
		return;
	}

	// NETWORK_AUDIT O10: canta doluysa cikarma reddedilir (aksi halde canta MaxBagSize'i asar,
	// takas/geri alma/odul reddedilir ve HUD fazla esyalari gosteremez).
	if (EquipmentBag.Num() >= MaxBagSize)
	{
		NotifyOwnerText(TEXT("Canta dolu! Once bir esya sat veya depoya koy."));
		return;
	}

	EquipmentBag.Add(EquippedItems[SlotIndex]);
	EquippedItems[SlotIndex] = FMMOEquipmentInstance();

	RecomputeEquipmentBonuses();
	OnInventoryChanged.Broadcast();
}

void UMMOInventoryComponent::ServerUnequipSlot_Implementation(EMMOEquipSlot Slot)
{
	UnequipSlot(Slot);
}

void UMMOInventoryComponent::TryUpgrade(int32 BagIndex)
{
	// Indeks yalniz bu (sahip) istemcinin kendi gorunumunde cozulur; sunucuya InstanceId +
	// gorulen arti seviyesi gider.
	if (EquipmentBag.IsValidIndex(BagIndex))
	{
		UpgradeByInstanceId(EquipmentBag[BagIndex].InstanceId, EquipmentBag[BagIndex].Plus);
	}
}

void UMMOInventoryComponent::UpgradeByInstanceId(int32 InstanceId, int32 ExpectedPlus)
{
	if (InstanceId == 0)
	{
		return;
	}

	if (!HasAuthorityInternal())
	{
		ServerUpgradeByInstanceId(InstanceId, ExpectedPlus);
		return;
	}

	const int32 BagIndex = FindBagIndexByInstanceId(InstanceId);
	if (BagIndex == INDEX_NONE)
	{
		return; // bayat istek: esya yandi/satildi/tasindi
	}
	// Olu karakter yukseltme yapamaz (canlanma beklerken altin/esya harcamasin; NETWORK_AUDIT D3).
	if (const AMMOCharacter* DeadCheckChar = Cast<AMMOCharacter>(GetOwner()))
	{
		if (const UMMOStatsComponent* St = DeadCheckChar->GetStats(); St && St->bIsDead)
		{
			return;
		}
	}

	const FMMOEquipmentInstance Inst = EquipmentBag[BagIndex];
	const FMMOItemDef* Def = FMMOItemDatabase::Find(Inst.ItemId);
	if (!Def || Inst.Plus < 0 || Inst.Plus >= FMMOItemDatabase::MaxPlus)
	{
		return;
	}

	// Istemcinin gordugu arti seviyesi artik gecerli degilse (ornegin ilk tik basarili oldu,
	// hizli ikinci tik eski seviye ile geldi) sessizce reddet: kullanici yeni seviyeyi gorup
	// bilincli olarak tekrar basmali.
	if (ExpectedPlus >= 0 && ExpectedPlus != Inst.Plus)
	{
		return;
	}

	// Ust uste RPC'lere karsi kisa soguma (gecikmeli baglantida tik yigilmasi).
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now - LastUpgradeServerTime < UpgradeCooldownSeconds)
	{
		return;
	}

	const int32 Cost = FMMOItemDatabase::GetUpgradeCost(Inst.Plus, Def->RequiredLevel);
	if (Gold < Cost)
	{
		ClientUpgradeResult(false, false, NAME_None, 0); // "altin yetersiz" isareti
		return;
	}

	LastUpgradeServerTime = Now;
	const int32 BalanceAfterUpgrade = Gold - Cost;

	const FName UpgradedItemId = Inst.ItemId;
	const int32 OldPlus = Inst.Plus;
	const int32 UpgradeRarity = static_cast<int32>(Inst.Rarity);
	const int32 ChancePct = FMMOItemDatabase::GetUpgradeChancePct(Inst.Plus);
	const bool bSuccess = FMath::RandRange(1, 100) <= ChancePct;
	bool bBurned = false;
	const TCHAR* Result = TEXT("failed");
	int32 ResultPlus = OldPlus;

	if (bSuccess)
	{
		EquipmentBag[BagIndex].Plus = OldPlus + 1;
		Result = TEXT("success");
		ResultPlus = OldPlus + 1;
	}
	else if (FMath::RandRange(1, 100) <= FMMOItemDatabase::GetBurnChancePct(Inst.Plus))
	{
		// YANDI! Item yok olur (klasik KO).
		EquipmentBag.RemoveAt(BagIndex);
		bBurned = true;
		Result = TEXT("burned");
		ResultPlus = -1;
	}

	// O49: AddGoldFrom publishes OnGoldChanged synchronously. Its authority listeners
	// may equip/reorder/remove items or take a save snapshot. Settle the item outcome
	// before publication, and never use a bag reference/index after that callback.
	AddGoldFrom(-Cost, TEXT("yukseltme"));
	ClientUpgradeResult(bSuccess, bBurned, UpgradedItemId, bBurned ? 0 : ResultPlus);

	UE_LOG(LogMMOInventoryAudit, Display,
		TEXT("event=item_upgrade actor=\"%s\" slot=\"%s\" item=\"%s\" instance=%d rarity=%d from_plus=%d to_plus=%d cost=%d chance=%d result=%s balance=%d"),
		*InventoryActorName(this), *InventorySlotName(this), *UpgradedItemId.ToString(), InstanceId,
		UpgradeRarity, OldPlus, ResultPlus, Cost, ChancePct, Result, BalanceAfterUpgrade);

	OnInventoryChanged.Broadcast();
}

void UMMOInventoryComponent::ServerUpgradeByInstanceId_Implementation(int32 InstanceId, int32 ExpectedPlus)
{
	UpgradeByInstanceId(InstanceId, ExpectedPlus);
}

bool UMMOInventoryComponent::IsOwnerNearVendor(float MaxDist) const
{
	const AActor* OwnerActor = GetOwner();
	const UWorld* World = GetWorld();
	if (!OwnerActor || !World)
	{
		return false;
	}
	// Olu karakter satici ile islem yapamaz (depodaki kuralla ayni; NETWORK_AUDIT D3).
	if (const AMMOCharacter* Char = Cast<AMMOCharacter>(OwnerActor))
	{
		if (const UMMOStatsComponent* St = Char->GetStats(); St && St->bIsDead)
		{
			return false;
		}
	}
	const FVector Loc = OwnerActor->GetActorLocation();
	const float MaxDistSq = MaxDist * MaxDist;
	for (TActorIterator<AMMOVendorNPC> It(World); It; ++It)
	{
		if (*It && FVector::DistSquared(Loc, (*It)->GetActorLocation()) <= MaxDistSq)
		{
			return true;
		}
	}
	return false;
}

void UMMOInventoryComponent::BuyEquipment(FName ItemId)
{
	const FMMOItemDef* Def = FMMOItemDatabase::Find(ItemId);
	if (!Def)
	{
		return;
	}

	if (!HasAuthorityInternal())
	{
		ServerBuyEquipment(ItemId);
		return;
	}

	// Sunucu-otoriter dogrulama: alisveris ancak bir saticinin yakininda yapilabilir
	// (hileli istemci satici olmadan her yerden alisveris RPC'si atmasin).
	if (!IsOwnerNearVendor())
	{
		MMONotify::ToOwner(GetOwner(), TEXT("Satici yakininda degilsin"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	// Boss/odul setleri (bVendorSold=false, orn. Taht) satilmaz: yalniz ganimet ve gorev odulu.
	if (!Def->bVendorSold)
	{
		MMONotify::ToOwner(GetOwner(), TEXT("Bu esya saticida satilmaz"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	// Seviye kapisi (CONTENT_PLAN_LV46_80.md G6/S3): satici fiyati seviyeden bagimsizdi, model geliri set fiyatinin 100+
	// kati oldugu icin altinla tum kademeler atlanabiliyordu. Alicinin seviyesi gereken seviyenin en fazla
	// VendorLevelSlack altinda olabilir (kusanmada RequiredLevel yine tam denetlenir). PLACEHOLDER; olcumle ayarlanir.
	{
		const AMMOCharacter* Buyer = Cast<AMMOCharacter>(GetOwner());
		const UMMOStatsComponent* BuyerStats = Buyer ? Buyer->GetStats() : nullptr;
		if (BuyerStats && BuyerStats->Level + VendorLevelSlack < Def->RequiredLevel)
		{
			MMONotify::ToOwner(GetOwner(), FString::Printf(TEXT("Bu esya icin cok erken: Lv%d gerekir"), Def->RequiredLevel),
				FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
			return;
		}
	}

	const int32 Price = FMMOItemDatabase::GetBuyPrice(*Def);
	if (Gold < Price)
	{
		MMONotify::ToOwner(GetOwner(), TEXT("Altin yetersiz"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	// Canta doluysa AddEquipmentInstance sessizce hicbir sey eklemeden cikar (bilerek
	// kabul edilen loot/odul davranisi) - ama SATIN ALMADA bu, oyuncunun altinini
	// karsiliginda HICBIR SEY vermeden alir (bug-hunt). Odul/lootun aksine burada
	// oyuncu gercekten para harciyor; parayi dusmeden ONCE kontrol et.
	if (EquipmentBag.Num() >= MaxBagSize)
	{
		MMONotify::ToOwner(GetOwner(), TEXT("Canta dolu!"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	AddGoldFrom(-Price, TEXT("satin_alma"));
	AddEquipmentInstance(ItemId, 0);
	MMONotify::ToOwner(GetOwner(), FString::Printf(TEXT("Satin alindi: %s (-%d altin)"), *Def->Name, Price), FColor(120, 220, 120), 2.f);
}

void UMMOInventoryComponent::ServerBuyEquipment_Implementation(FName ItemId)
{
	BuyEquipment(ItemId);
}

void UMMOInventoryComponent::BuyPotion(int32 Count)
{
	// GUVENLIK: Count sadece alttan degil USTTEN de sinirlanmali. Sinirsiz birakilirsa
	// hileli bir istemci ServerBuyPotion'i devasa bir Count ile cagirip
	// (PotionPrice * Count) int32'yi TASIRIP negatif yapabilir - bu hem "altin yetersiz"
	// kontrolunu (Gold < NegatifFiyat hep yanlis) atlatir HEM DE AddGold(-Price) negatifin
	// eksisi pozitif oldugu icin oyuncuya BEDAVA ALTIN verir (gercek altin/esya uretme acigi).
	Count = FMath::Clamp(Count, 1, 999);

	if (!HasAuthorityInternal())
	{
		ServerBuyPotion(Count);
		return;
	}

	if (!IsOwnerNearVendor())
	{
		MMONotify::ToOwner(GetOwner(), TEXT("Satici yakininda degilsin"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	// Yigin tavani (999): toplam tavani asan kisim satin alinmaz; altin yalniz gercekten verilen kadar duser.
	const FName PotionId(TEXT("HealthPotion"));
	const int32 Room = GetAddRoom(PotionId);
	if (Room <= 0)
	{
		MMONotify::ToOwner(GetOwner(), FString::Printf(TEXT("En fazla %d tasiyabilirsin"), MaxStackSize), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}
	const int32 Wanted = Count;
	Count = FMath::Min(Count, Room);

	if (Gold < FMMOItemDatabase::PotionPrice * Count)
	{
		MMONotify::ToOwner(GetOwner(), TEXT("Altin yetersiz"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	const int32 Added = AddItem(PotionId, Count);
	if (Added <= 0)
	{
		return;
	}
	const int32 Price = FMMOItemDatabase::PotionPrice * Added;
	AddGoldFrom(-Price, TEXT("satin_alma"));
	MMONotify::ToOwner(GetOwner(), FString::Printf(TEXT("Satin alindi: %d iksir (-%d altin)%s"), Added, Price,
		Added < Wanted ? TEXT(" - tavana ulasildi") : TEXT("")), FColor(120, 220, 120), 2.f);
}

void UMMOInventoryComponent::ServerBuyPotion_Implementation(int32 Count)
{
	BuyPotion(Count);
}

void UMMOInventoryComponent::BuyConsumable(FName ItemId, int32 Count)
{
	// GUVENLIK: BuyPotion'daki ayni int32-tasma acigi (bkz. oradaki yorum) - USTTEN de
	// sinirla, yoksa (Def->Price * Count) tasip negatif fiyat -> bedava altin/esya olur.
	Count = FMath::Clamp(Count, 1, 999);

	if (!HasAuthorityInternal())
	{
		ServerBuyConsumable(ItemId, Count);
		return;
	}

	if (!IsOwnerNearVendor())
	{
		MMONotify::ToOwner(GetOwner(), TEXT("Satici yakininda degilsin"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	const FMMOConsumableDef* Def = FMMOConsumableDatabase::Find(ItemId);
	if (!Def)
	{
		return;
	}

	// Yigin tavani (999 / 64 farkli yigin): fazlasi satin alinmaz; altin yalniz verilen kadar duser.
	const int32 Room = GetAddRoom(ItemId);
	if (Room <= 0)
	{
		MMONotify::ToOwner(GetOwner(), FString::Printf(TEXT("En fazla %d tasiyabilirsin"), MaxStackSize), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}
	const int32 Wanted = Count;
	Count = FMath::Min(Count, Room);

	if (Gold < Def->Price * Count)
	{
		MMONotify::ToOwner(GetOwner(), TEXT("Altin yetersiz"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	const int32 Added = AddItem(ItemId, Count);
	if (Added <= 0)
	{
		return;
	}
	const int32 Price = Def->Price * Added;
	AddGoldFrom(-Price, TEXT("satin_alma"));
	MMONotify::ToOwner(GetOwner(), FString::Printf(TEXT("Satin alindi: %d %s (-%d altin)%s"), Added, *Def->Name, Price,
		Added < Wanted ? TEXT(" - tavana ulasildi") : TEXT("")), FColor(120, 220, 120), 2.f);
}

void UMMOInventoryComponent::ServerBuyConsumable_Implementation(FName ItemId, int32 Count)
{
	BuyConsumable(ItemId, Count);
}

void UMMOInventoryComponent::SellEquipment(int32 BagIndex)
{
	// Indeks yalniz bu (sahip) istemcinin kendi gorunumunde cozulur; sunucuya InstanceId gider.
	if (EquipmentBag.IsValidIndex(BagIndex))
	{
		SellByInstanceId(EquipmentBag[BagIndex].InstanceId);
	}
}

void UMMOInventoryComponent::SellByInstanceId(int32 InstanceId)
{
	if (InstanceId == 0)
	{
		return;
	}

	if (!HasAuthorityInternal())
	{
		ServerSellByInstanceId(InstanceId);
		return;
	}

	if (!IsOwnerNearVendor())
	{
		MMONotify::ToOwner(GetOwner(), TEXT("Satici yakininda degilsin"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	const int32 BagIndex = FindBagIndexByInstanceId(InstanceId);
	if (BagIndex == INDEX_NONE)
	{
		return; // bayat istek: esya zaten satildi/yandi/tasindi
	}

	const FMMOEquipmentInstance Inst = EquipmentBag[BagIndex];
	const FMMOItemDef* Def = FMMOItemDatabase::Find(Inst.ItemId);
	if (!Def)
	{
		return;
	}

	const int32 Price = FMath::RoundToInt(FMMOItemDatabase::GetSellPrice(*Def, Inst.Plus)
		* FMMOItemDatabase::GetRarityMultiplier(Inst.Rarity));
	EquipmentBag.RemoveAt(BagIndex);
	AddGoldFrom(Price, TEXT("satis"));
	UE_LOG(LogMMOInventoryAudit, Display,
		TEXT("event=item_sell actor=\"%s\" slot=\"%s\" item=\"%s\" instance=%d rarity=%d plus=%d price=%d balance=%d"),
		*InventoryActorName(this), *InventorySlotName(this), *Inst.ItemId.ToString(), Inst.InstanceId,
		static_cast<int32>(Inst.Rarity), Inst.Plus, Price, Gold);
	OnInventoryChanged.Broadcast();

	MMONotify::ToOwner(GetOwner(), FString::Printf(TEXT("Satildi: %s +%d (+%d altin)"), *Def->Name, Inst.Plus, Price), FColor(255, 210, 0), 2.f);
}

void UMMOInventoryComponent::ServerSellByInstanceId_Implementation(int32 InstanceId)
{
	SellByInstanceId(InstanceId);
}

bool UMMOInventoryComponent::IsOwnerNearWarehouse(float MaxDist) const
{
	const AActor* OwnerActor = GetOwner();
	const UWorld* World = GetWorld();
	if (!OwnerActor || !World)
	{
		return false;
	}
	// Olu karakter depoyu kullanamaz (canlanma beklerken esya tasimasin).
	if (const AMMOCharacter* Char = Cast<AMMOCharacter>(OwnerActor))
	{
		if (const UMMOStatsComponent* St = Char->GetStats(); St && St->bIsDead)
		{
			return false;
		}
	}
	const FVector Loc = OwnerActor->GetActorLocation();
	const float MaxDistSq = MaxDist * MaxDist;
	for (TActorIterator<AMMOWarehouseNPC> It(World); It; ++It)
	{
		if (*It && FVector::DistSquared(Loc, (*It)->GetActorLocation()) <= MaxDistSq)
		{
			return true;
		}
	}
	return false;
}

void UMMOInventoryComponent::DepositEquipment(int32 InstanceId)
{
	if (!HasAuthorityInternal())
	{
		ServerDepositEquipment(InstanceId);
		return;
	}

	auto Notify = [this](const TCHAR* Msg)
	{
		MMONotify::ToOwner(GetOwner(), Msg, FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
	};

	if (!IsOwnerNearWarehouse())
	{
		Notify(TEXT("Depocu yakininda degilsin"));
		return;
	}
	const int32 BagIndex = EquipmentBag.IndexOfByPredicate([InstanceId](const FMMOEquipmentInstance& I)
	{
		return I.InstanceId == InstanceId;
	});
	if (BagIndex == INDEX_NONE || !EquipmentBag[BagIndex].IsValidInstance())
	{
		return;
	}
	if (Warehouse.Num() >= MaxWarehouseSize)
	{
		Notify(TEXT("Depo dolu!"));
		return;
	}

	const FMMOEquipmentInstance Moved = EquipmentBag[BagIndex];
	Warehouse.Add(Moved);
	EquipmentBag.RemoveAt(BagIndex);
	UE_LOG(LogMMOInventoryAudit, Display,
		TEXT("event=warehouse_deposit actor=\"%s\" slot=\"%s\" item=\"%s\" instance=%d rarity=%d plus=%d bag_count=%d warehouse_count=%d"),
		*InventoryActorName(this), *InventorySlotName(this), *Moved.ItemId.ToString(), Moved.InstanceId,
		static_cast<int32>(Moved.Rarity), Moved.Plus, EquipmentBag.Num(), Warehouse.Num());
	OnInventoryChanged.Broadcast();
}

void UMMOInventoryComponent::ServerDepositEquipment_Implementation(int32 InstanceId)
{
	DepositEquipment(InstanceId);
}

void UMMOInventoryComponent::WithdrawEquipment(int32 InstanceId)
{
	if (!HasAuthorityInternal())
	{
		ServerWithdrawEquipment(InstanceId);
		return;
	}

	auto Notify = [this](const TCHAR* Msg)
	{
		MMONotify::ToOwner(GetOwner(), Msg, FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
	};

	if (!IsOwnerNearWarehouse())
	{
		Notify(TEXT("Depocu yakininda degilsin"));
		return;
	}
	const int32 StoreIndex = Warehouse.IndexOfByPredicate([InstanceId](const FMMOEquipmentInstance& I)
	{
		return I.InstanceId == InstanceId;
	});
	if (StoreIndex == INDEX_NONE)
	{
		return;
	}
	if (EquipmentBag.Num() >= MaxBagSize)
	{
		Notify(TEXT("Canta dolu!"));
		return;
	}

	const FMMOEquipmentInstance Moved = Warehouse[StoreIndex];
	EquipmentBag.Add(Moved);
	Warehouse.RemoveAt(StoreIndex);
	UE_LOG(LogMMOInventoryAudit, Display,
		TEXT("event=warehouse_withdraw actor=\"%s\" slot=\"%s\" item=\"%s\" instance=%d rarity=%d plus=%d bag_count=%d warehouse_count=%d"),
		*InventoryActorName(this), *InventorySlotName(this), *Moved.ItemId.ToString(), Moved.InstanceId,
		static_cast<int32>(Moved.Rarity), Moved.Plus, EquipmentBag.Num(), Warehouse.Num());
	OnInventoryChanged.Broadcast();
}

void UMMOInventoryComponent::ServerWithdrawEquipment_Implementation(int32 InstanceId)
{
	WithdrawEquipment(InstanceId);
}

bool UMMOInventoryComponent::TakeEquipmentByInstanceId(int32 InstanceId, FMMOEquipmentInstance& Out)
{
	if (!HasAuthorityInternal() || InstanceId == 0)
	{
		return false;
	}
	const int32 Index = EquipmentBag.IndexOfByPredicate([InstanceId](const FMMOEquipmentInstance& I)
	{
		return I.InstanceId == InstanceId;
	});
	if (Index == INDEX_NONE || !EquipmentBag[Index].IsValidInstance())
	{
		return false;
	}
	Out = EquipmentBag[Index];
	EquipmentBag.RemoveAt(Index);
	OnInventoryChanged.Broadcast();
	return true;
}

bool UMMOInventoryComponent::ReceiveTradedEquipment(const FMMOEquipmentInstance& Src)
{
	if (!HasAuthorityInternal() || !Src.IsValidInstance() || !FMMOItemDatabase::Find(Src.ItemId)
		|| EquipmentBag.Num() >= MaxBagSize)
	{
		return false;
	}
	FMMOEquipmentInstance Inst = Src;
	// Gonderenin kimligi bu oyuncunun mevcut bir kimligiyle cakisabilir: her zaman yeni kimlik ver.
	Inst.InstanceId = NextInstanceId++;
	EquipmentBag.Add(Inst);
	OnInventoryChanged.Broadcast();
	return true;
}

void UMMOInventoryComponent::FillSaveData(FMMOCharacterSaveData& Out) const
{
	Out.Gold = Gold;
	Out.Items = Items;
	Out.EquipmentBag = EquipmentBag;
	Out.EquippedItems = EquippedItems;
	Out.NextInstanceId = NextInstanceId;
	Out.Warehouse = Warehouse;
}

void UMMOInventoryComponent::ApplySaveData(const FMMOCharacterSaveData& In)
{
	if (!HasAuthorityInternal())
	{
		return;
	}

	// GUVENLIK (docs/SECURITY_AUDIT.md Y-2): elle duzenlenmis kayitta sinirsiz altin/yigin/
	// gecersiz nadirlik olmasin. Bilinmeyen esya kimlikleri bilerek SILINMEZ (surumler arasi
	// kaldirilan esya oyuncunun kaybi olurdu); yalniz degerler makul araliga cekilir.
	constexpr int32 SaveStackCap = MaxStackSize;   // AddItem ile ayni tavan (calisma zamani == kayit)
	constexpr int32 SaveMaxStacks = MaxItemStacks;
	Gold = FMath::Clamp(In.Gold, 0, MaxGold);   // calisma zamani tavaniyla AYNI sabit
	// Yiginlar: AddItem ItemId basina tek yigin tutar; bozuk kayittaki ayni kimlikli yiginlar
	// birlestirilir (64 sinirini bosuna doldurmasin), sonra adet kirpilir.
	Items.Reset();
	for (const FMMOItemStack& Stack : In.Items)
	{
		if (Stack.ItemId.IsNone() || Stack.Count <= 0)
		{
			continue;
		}
		FMMOItemStack* Existing = Items.FindByPredicate([&Stack](const FMMOItemStack& S) { return S.ItemId == Stack.ItemId; });
		if (Existing)
		{
			Existing->Count = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(Existing->Count) + Stack.Count, SaveStackCap));
		}
		else
		{
			Items.Add(Stack);
			Items.Last().Count = FMath::Min(Stack.Count, SaveStackCap);
		}
	}
	// 64'u asan yigin: sessizce silinmez, her biri loglanir (yigin depoya konamaz: depo yalniz ekipman tutar).
	for (int32 i = SaveMaxStacks; i < Items.Num(); ++i)
	{
		UE_LOG(LogTemp, Warning, TEXT("ApplySaveData: yigin siniri (%d) asildi, %s x%d yuklenmedi"),
			SaveMaxStacks, *Items[i].ItemId.ToString(), Items[i].Count);
	}
	if (Items.Num() > SaveMaxStacks)
	{
		Items.SetNum(SaveMaxStacks);
	}
	// Ornek kimligi bu sinirin ustundeyse (veya <= 0) yeni kimlik alir: "Id + 1" ve NextInstanceId++
	// INT32_MAX'te tasmasin. Sinir yarim aralik; calisma zamaninda yine ~1 milyar kimlik pay kalir.
	static constexpr int32 SaveInstanceIdCap = MAX_int32 / 2;
	static constexpr int32 NeedsNewId = -1;
	// Nadirlik enum araligi disindaysa Siradan'a cek (stat carpani tablosu disina cikmasin).
	auto SanitizeInstance = [](FMMOEquipmentInstance& Inst)
	{
		if (!Inst.ItemId.IsNone() && (Inst.InstanceId <= 0 || Inst.InstanceId > SaveInstanceIdCap))
		{
			Inst.InstanceId = NeedsNewId; // asagidaki Dedupe yeni kimlik verir; esya silinmez
		}
		Inst.Plus = FMath::Clamp(Inst.Plus, 0, FMMOItemDatabase::MaxPlus);
		if ((uint8)Inst.Rarity >= (uint8)EMMOItemRarity::MAX)
		{
			Inst.Rarity = EMMOItemRarity::Common;
		}
	};
	EquipmentBag = In.EquipmentBag;
	// Gecersiz (bos ItemId) girdiler atilir; kimligi bozuk ama ItemId'si olan esya tutulur (yeni kimlik alir).
	EquipmentBag.RemoveAll([](const FMMOEquipmentInstance& Inst) { return Inst.ItemId.IsNone(); });
	for (FMMOEquipmentInstance& Inst : EquipmentBag)
	{
		SanitizeInstance(Inst);
	}
	NextInstanceId = FMath::Clamp(In.NextInstanceId, 1, SaveInstanceIdCap);

	// Depo: gecersiz girdileri at, arti seviyesini sinirla. Kapasite asilmissa (bozuk/eski
	// kayit) hicbir item silinmez; yalniz yeni depolama reddedilir.
	Warehouse = In.Warehouse;
	Warehouse.RemoveAll([](const FMMOEquipmentInstance& Inst) { return Inst.ItemId.IsNone(); });
	for (FMMOEquipmentInstance& Inst : Warehouse)
	{
		SanitizeInstance(Inst);
	}

	// Kusanilan dizi her zaman MAX (4) slot olmali. Kayittaki dizi daha uzunsa (bozuk/
	// eski-surum kayit), fazla girdileri sessizce ATMA — gecerliyse cantaya geri koy
	// (aksi halde SetNum onlari kirpip kalici item kaybina yol acardi).
	EquippedItems = In.EquippedItems;
	for (FMMOEquipmentInstance& Inst : EquippedItems)
	{
		SanitizeInstance(Inst);
	}
	const int32 MaxSlots = (int32)EMMOEquipSlot::MAX;
	for (int32 i = MaxSlots; i < EquippedItems.Num(); ++i)
	{
		if (EquippedItems[i].IsValidInstance())
		{
			EquipmentBag.Add(EquippedItems[i]);
		}
	}
	EquippedItems.SetNum(MaxSlots);
	// Canta MaxBagSize (12) ile sinirli: fazlasi (bozuk kayit / yukaridaki geri koyma) depoya tasinir.
	// Depo da doluysa kapasite asilir ve loglanir; esya SILINMEZ (yukleme kurali).
	while (EquipmentBag.Num() > MaxBagSize)
	{
		const FMMOEquipmentInstance Overflow = EquipmentBag.Pop();
		if (Warehouse.Num() >= MaxWarehouseSize)
		{
			UE_LOG(LogTemp, Warning, TEXT("ApplySaveData: canta %d'yi asiyor ve depo dolu; %s depoya kapasite ustu tasindi (depo %d/%d)"),
				MaxBagSize, *Overflow.ItemId.ToString(), Warehouse.Num() + 1, MaxWarehouseSize);
		}
		Warehouse.Add(Overflow);
	}
	// Yukleme de normal kusanma gibi slot + seviye sartini denetler. Stats once
	// yuklenir (LoadCharacter); bilinmeyen esya kimligini koruma politikasi aynidir.
	const AMMOCharacter* OwnerCharacter = Cast<AMMOCharacter>(GetOwner());
	const UMMOStatsComponent* OwnerStats = OwnerCharacter ? OwnerCharacter->GetStats() : nullptr;
	for (int32 i = 0; i < MaxSlots; ++i)
	{
		if (!EquippedItems[i].IsValidInstance())
		{
			continue;
		}
		const FMMOItemDef* Def = FMMOItemDatabase::Find(EquippedItems[i].ItemId);
		if (Def && ((int32)Def->Slot != i || !OwnerStats || OwnerStats->Level < Def->RequiredLevel))
		{
			// Yer varsa cantaya (12'yi asmaz); yoksa depoya. Depo kapasitesi yuklemede asilabilir (mevcut kural:
			// bozuk/eski kayitta hicbir esya silinmez, yalniz yeni depolama reddedilir). Bonus uygulanmaz.
			if (EquipmentBag.Num() < MaxBagSize)
			{
				EquipmentBag.Add(EquippedItems[i]);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("ApplySaveData: kusanim kosulunu saglamayan esya (%s) canta dolu oldugu icin depoya tasindi (depo %d/%d)"),
					*EquippedItems[i].ItemId.ToString(), Warehouse.Num() + 1, MaxWarehouseSize);
				Warehouse.Add(EquippedItems[i]);
			}
			EquippedItems[i] = FMMOEquipmentInstance();
		}
	}

	// Bozuk/eski kayitta kayitli sayac en buyuk kimligin gerisinde kalmis olabilir: yeni
	// (ticaret/loot) kimlikleri mevcut bir esyayla cakismasin diye Max(Id)+1'e cek.
	// Kimlikler SanitizeInstance'ta SaveInstanceIdCap ile sinirli oldugu icin +1 tasmaz.
	for (const FMMOEquipmentInstance& Inst : EquipmentBag)
	{
		NextInstanceId = FMath::Max(NextInstanceId, Inst.InstanceId + 1);
	}
	for (const FMMOEquipmentInstance& Inst : EquippedItems)
	{
		NextInstanceId = FMath::Max(NextInstanceId, Inst.InstanceId + 1);
	}
	for (const FMMOEquipmentInstance& Inst : Warehouse)
	{
		NextInstanceId = FMath::Max(NextInstanceId, Inst.InstanceId + 1); // yanlis slottan/cantadan depoya tasinanlar dahil
	}

	// Yinelenen InstanceId (canta/kusanilan/depo arasinda): ilki kalir, digerlerine yeni
	// kimlik verilir; boylece kimlik tabanli kusan/sat/yukselt tek bir esyayi hedefler ve
	// duplike edilmis bir kopya ayni kimlikle ikinci kez islenemez.
	TSet<int32> SeenIds;
	auto Dedupe = [this, &SeenIds](FMMOEquipmentInstance& Inst)
	{
		if (!Inst.IsValidInstance())
		{
			return;
		}
		if (Inst.InstanceId == NeedsNewId || SeenIds.Contains(Inst.InstanceId))
		{
			Inst.InstanceId = NextInstanceId++;
		}
		SeenIds.Add(Inst.InstanceId);
	};
	for (FMMOEquipmentInstance& Inst : EquippedItems) { Dedupe(Inst); }
	for (FMMOEquipmentInstance& Inst : EquipmentBag) { Dedupe(Inst); }
	for (FMMOEquipmentInstance& Inst : Warehouse) { Dedupe(Inst); }

	RecomputeEquipmentBonuses();
	OnGoldChanged.Broadcast(Gold);
	OnInventoryChanged.Broadcast();
}

void UMMOInventoryComponent::ClientUpgradeResult_Implementation(bool bSuccess, bool bBurned, FName ItemId, int32 NewPlus)
{
	if (!GEngine)
	{
		return;
	}

	const FMMOItemDef* Def = FMMOItemDatabase::Find(ItemId);
	const FString Name = Def ? Def->Name : TEXT("");

	if (bSuccess)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor(120, 255, 120),
			FString::Printf(TEXT("BASARILI! %s artik +%d"), *Name, NewPlus));
	}
	else if (bBurned)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Red,
			FString::Printf(TEXT("YANDI!!! %s kul oldu..."), *Name));
	}
	else if (ItemId.IsNone())
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Altin yetersiz"));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor(255, 160, 60),
			FString::Printf(TEXT("Basarisiz... %s +%d olarak kaldi (altin gitti)"), *Name, NewPlus));
	}
}

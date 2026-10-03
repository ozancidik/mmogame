// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMOInventoryComponent.h"
#include "Items/MMOItemDatabase.h"
#include "Items/MMOConsumableTypes.h"
#include "Characters/MMOCharacter.h"
#include "Components/MMOStatsComponent.h"
#include "Save/MMOSaveGame.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "World/MMOVendorNPC.h"
#include "World/MMOWarehouseNPC.h"

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

	DOREPLIFETIME(UMMOInventoryComponent, Gold);
	DOREPLIFETIME(UMMOInventoryComponent, Items);
	DOREPLIFETIME(UMMOInventoryComponent, EquipmentBag);
	DOREPLIFETIME(UMMOInventoryComponent, EquippedItems);
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
	// sifirlardi. int64'te topla, sonra guvenle [0, INT32_MAX] araligina sikistir.
	const int64 Sum = (int64)Gold + (int64)Amount;
	Gold = (int32)FMath::Clamp<int64>(Sum, 0, MAX_int32);
	OnGoldChanged.Broadcast(Gold);
}

void UMMOInventoryComponent::AddItem(FName ItemId, int32 Count)
{
	if (!HasAuthorityInternal() || ItemId.IsNone() || Count <= 0)
	{
		return;
	}

	for (FMMOItemStack& Stack : Items)
	{
		if (Stack.ItemId == ItemId)
		{
			Stack.Count = static_cast<int32>(FMath::Min<int64>(
				static_cast<int64>(FMath::Max(0, Stack.Count)) + Count, MAX_int32));
			OnInventoryChanged.Broadcast();
			return;
		}
	}

	FMMOItemStack NewStack;
	NewStack.ItemId = ItemId;
	NewStack.Count = Count;
	Items.Add(NewStack);
	OnInventoryChanged.Broadcast();
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
		if (GEngine && IsLocallyControlledOwner())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Canta dolu!"));
		}
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

void UMMOInventoryComponent::EquipFromBag(int32 BagIndex)
{
	if (!EquipmentBag.IsValidIndex(BagIndex))
	{
		return;
	}

	if (!HasAuthorityInternal())
	{
		ServerEquipFromBag(BagIndex);
		return;
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
	OnInventoryChanged.Broadcast();
}

void UMMOInventoryComponent::ServerEquipFromBag_Implementation(int32 BagIndex)
{
	EquipFromBag(BagIndex);
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
	if (!EquipmentBag.IsValidIndex(BagIndex))
	{
		return;
	}

	if (!HasAuthorityInternal())
	{
		ServerTryUpgrade(BagIndex);
		return;
	}

	FMMOEquipmentInstance& Inst = EquipmentBag[BagIndex];
	const FMMOItemDef* Def = FMMOItemDatabase::Find(Inst.ItemId);
	if (!Def || Inst.Plus < 0 || Inst.Plus >= FMMOItemDatabase::MaxPlus)
	{
		return;
	}

	const int32 Cost = FMMOItemDatabase::GetUpgradeCost(Inst.Plus);
	if (Gold < Cost)
	{
		ClientUpgradeResult(false, false, NAME_None, 0); // "altin yetersiz" isareti
		return;
	}

	AddGold(-Cost);

	const int32 ChancePct = FMMOItemDatabase::GetUpgradeChancePct(Inst.Plus);
	const bool bSuccess = FMath::RandRange(1, 100) <= ChancePct;

	if (bSuccess)
	{
		Inst.Plus++;
		ClientUpgradeResult(true, false, Inst.ItemId, Inst.Plus);
	}
	else if (FMath::RandRange(1, 100) <= FMMOItemDatabase::GetBurnChancePct(Inst.Plus))
	{
		// YANDI! Item yok olur (klasik KO).
		const FName BurnedId = Inst.ItemId;
		EquipmentBag.RemoveAt(BagIndex);
		ClientUpgradeResult(false, true, BurnedId, 0);
	}
	else
	{
		ClientUpgradeResult(false, false, Inst.ItemId, Inst.Plus);
	}

	OnInventoryChanged.Broadcast();
}

void UMMOInventoryComponent::ServerTryUpgrade_Implementation(int32 BagIndex)
{
	TryUpgrade(BagIndex);
}

bool UMMOInventoryComponent::IsOwnerNearVendor(float MaxDist) const
{
	const AActor* OwnerActor = GetOwner();
	const UWorld* World = GetWorld();
	if (!OwnerActor || !World)
	{
		return false;
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
		if (GEngine && IsLocallyControlledOwner())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Satici yakininda degilsin"));
		}
		return;
	}

	const int32 Price = FMMOItemDatabase::GetBuyPrice(*Def);
	if (Gold < Price)
	{
		if (GEngine && IsLocallyControlledOwner())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Altin yetersiz"));
		}
		return;
	}

	// Canta doluysa AddEquipmentInstance sessizce hicbir sey eklemeden cikar (bilerek
	// kabul edilen loot/odul davranisi) - ama SATIN ALMADA bu, oyuncunun altinini
	// karsiliginda HICBIR SEY vermeden alir (bug-hunt). Odul/lootun aksine burada
	// oyuncu gercekten para harciyor; parayi dusmeden ONCE kontrol et.
	if (EquipmentBag.Num() >= MaxBagSize)
	{
		if (GEngine && IsLocallyControlledOwner())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Canta dolu!"));
		}
		return;
	}

	AddGold(-Price);
	AddEquipmentInstance(ItemId, 0);
	if (GEngine && IsLocallyControlledOwner())
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor(120, 220, 120),
			FString::Printf(TEXT("Satin alindi: %s (-%d altin)"), *Def->Name, Price));
	}
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
		if (GEngine && IsLocallyControlledOwner())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Satici yakininda degilsin"));
		}
		return;
	}

	const int32 Price = FMMOItemDatabase::PotionPrice * Count;
	if (Gold < Price)
	{
		if (GEngine && IsLocallyControlledOwner())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Altin yetersiz"));
		}
		return;
	}

	AddGold(-Price);
	AddItem(FName(TEXT("HealthPotion")), Count);
	if (GEngine && IsLocallyControlledOwner())
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor(120, 220, 120),
			FString::Printf(TEXT("Satin alindi: %d iksir (-%d altin)"), Count, Price));
	}
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
		if (GEngine && IsLocallyControlledOwner())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Satici yakininda degilsin"));
		}
		return;
	}

	const FMMOConsumableDef* Def = FMMOConsumableDatabase::Find(ItemId);
	if (!Def)
	{
		return;
	}

	const int32 Price = Def->Price * Count;
	if (Gold < Price)
	{
		if (GEngine && IsLocallyControlledOwner())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Altin yetersiz"));
		}
		return;
	}

	AddGold(-Price);
	AddItem(ItemId, Count);
	if (GEngine && IsLocallyControlledOwner())
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor(120, 220, 120),
			FString::Printf(TEXT("Satin alindi: %d %s (-%d altin)"), Count, *Def->Name, Price));
	}
}

void UMMOInventoryComponent::ServerBuyConsumable_Implementation(FName ItemId, int32 Count)
{
	BuyConsumable(ItemId, Count);
}

void UMMOInventoryComponent::SellEquipment(int32 BagIndex)
{
	if (!EquipmentBag.IsValidIndex(BagIndex))
	{
		return;
	}

	if (!HasAuthorityInternal())
	{
		ServerSellEquipment(BagIndex);
		return;
	}

	if (!IsOwnerNearVendor())
	{
		if (GEngine && IsLocallyControlledOwner())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Satici yakininda degilsin"));
		}
		return;
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
	AddGold(Price);
	OnInventoryChanged.Broadcast();

	if (GEngine && IsLocallyControlledOwner())
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor(255, 210, 0),
			FString::Printf(TEXT("Satildi: %s +%d (+%d altin)"), *Def->Name, Inst.Plus, Price));
	}
}

void UMMOInventoryComponent::ServerSellEquipment_Implementation(int32 BagIndex)
{
	SellEquipment(BagIndex);
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
		if (GEngine && IsLocallyControlledOwner())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, Msg);
		}
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

	Warehouse.Add(EquipmentBag[BagIndex]);
	EquipmentBag.RemoveAt(BagIndex);
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
		if (GEngine && IsLocallyControlledOwner())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, Msg);
		}
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

	EquipmentBag.Add(Warehouse[StoreIndex]);
	Warehouse.RemoveAt(StoreIndex);
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

	Gold = FMath::Max(0, In.Gold);
	Items = In.Items;
	Items.RemoveAll([](const FMMOItemStack& Stack)
	{
		return Stack.ItemId.IsNone() || Stack.Count <= 0;
	});
	EquipmentBag = In.EquipmentBag;
	for (FMMOEquipmentInstance& Inst : EquipmentBag)
	{
		Inst.Plus = FMath::Clamp(Inst.Plus, 0, FMMOItemDatabase::MaxPlus);
	}
	NextInstanceId = FMath::Max(1, In.NextInstanceId);

	// Depo: gecersiz girdileri at, arti seviyesini sinirla. Kapasite asilmissa (bozuk/eski
	// kayit) hicbir item silinmez; yalniz yeni depolama reddedilir.
	Warehouse = In.Warehouse;
	Warehouse.RemoveAll([](const FMMOEquipmentInstance& Inst) { return !Inst.IsValidInstance(); });
	for (FMMOEquipmentInstance& Inst : Warehouse)
	{
		Inst.Plus = FMath::Clamp(Inst.Plus, 0, FMMOItemDatabase::MaxPlus);
		// Ornek kimligi cakismasin: NextInstanceId depodaki en buyuk kimligin ustunde kalsin.
		NextInstanceId = FMath::Max(NextInstanceId, Inst.InstanceId + 1);
	}

	// Kusanilan dizi her zaman MAX (4) slot olmali. Kayittaki dizi daha uzunsa (bozuk/
	// eski-surum kayit), fazla girdileri sessizce ATMA — gecerliyse cantaya geri koy
	// (aksi halde SetNum onlari kirpip kalici item kaybina yol acardi).
	EquippedItems = In.EquippedItems;
	for (FMMOEquipmentInstance& Inst : EquippedItems)
	{
		Inst.Plus = FMath::Clamp(Inst.Plus, 0, FMMOItemDatabase::MaxPlus);
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

	// Bozuk/eski kayitta kayitli sayac en buyuk kimligin gerisinde kalmis olabilir: yeni
	// (ticaret/loot) kimlikleri mevcut bir esyayla cakismasin diye Max(Id)+1'e cek.
	for (const FMMOEquipmentInstance& Inst : EquipmentBag)
	{
		NextInstanceId = FMath::Max(NextInstanceId, Inst.InstanceId + 1);
	}
	for (const FMMOEquipmentInstance& Inst : EquippedItems)
	{
		NextInstanceId = FMath::Max(NextInstanceId, Inst.InstanceId + 1);
	}

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

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

DEFINE_LOG_CATEGORY_STATIC(LogMMOInventoryAudit, Log, All);

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

	FMMOEquipmentInstance& Inst = EquipmentBag[BagIndex];
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

	const int32 Cost = FMMOItemDatabase::GetUpgradeCost(Inst.Plus);
	if (Gold < Cost)
	{
		ClientUpgradeResult(false, false, NAME_None, 0); // "altin yetersiz" isareti
		return;
	}

	LastUpgradeServerTime = Now;
	AddGoldFrom(-Cost, TEXT("yukseltme"));

	const FName UpgradedItemId = Inst.ItemId;
	const int32 OldPlus = Inst.Plus;
	const int32 UpgradeRarity = static_cast<int32>(Inst.Rarity);
	const int32 ChancePct = FMMOItemDatabase::GetUpgradeChancePct(Inst.Plus);
	const bool bSuccess = FMath::RandRange(1, 100) <= ChancePct;
	const TCHAR* Result = TEXT("failed");
	int32 ResultPlus = OldPlus;

	if (bSuccess)
	{
		Inst.Plus++;
		Result = TEXT("success");
		ResultPlus = Inst.Plus;
		ClientUpgradeResult(true, false, Inst.ItemId, Inst.Plus);
	}
	else if (FMath::RandRange(1, 100) <= FMMOItemDatabase::GetBurnChancePct(Inst.Plus))
	{
		// YANDI! Item yok olur (klasik KO).
		const FName BurnedId = Inst.ItemId;
		EquipmentBag.RemoveAt(BagIndex);
		Result = TEXT("burned");
		ResultPlus = -1;
		ClientUpgradeResult(false, true, BurnedId, 0);
	}
	else
	{
		ClientUpgradeResult(false, false, Inst.ItemId, Inst.Plus);
	}

	UE_LOG(LogMMOInventoryAudit, Display,
		TEXT("event=item_upgrade actor=\"%s\" slot=\"%s\" item=\"%s\" instance=%d rarity=%d from_plus=%d to_plus=%d cost=%d chance=%d result=%s balance=%d"),
		*InventoryActorName(this), *InventorySlotName(this), *UpgradedItemId.ToString(), InstanceId,
		UpgradeRarity, OldPlus, ResultPlus, Cost, ChancePct, Result, Gold);

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
	// Yanlis slota kusanilmis esya (ornegin silah "Zirh" slotunda) cantaya geri konur;
	// normal EquipFromBag bunu zaten engeller, elle duzenlenmis kayit atlayabilirdi.
	for (int32 i = 0; i < MaxSlots; ++i)
	{
		if (!EquippedItems[i].IsValidInstance())
		{
			continue;
		}
		const FMMOItemDef* Def = FMMOItemDatabase::Find(EquippedItems[i].ItemId);
		if (Def && (int32)Def->Slot != i)
		{
			// Yer varsa cantaya (12'yi asmaz); yoksa depoya. Depo kapasitesi yuklemede asilabilir (mevcut kural:
			// bozuk/eski kayitta hicbir esya silinmez, yalniz yeni depolama reddedilir). Esya yanlis slotta KALMAZ.
			if (EquipmentBag.Num() < MaxBagSize)
			{
				EquipmentBag.Add(EquippedItems[i]);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("ApplySaveData: yanlis slottaki esya (%s) canta dolu oldugu icin depoya tasindi (depo %d/%d)"),
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

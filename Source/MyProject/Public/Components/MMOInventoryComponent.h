// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/MMOItemTypes.h"
#include "MMOInventoryComponent.generated.h"

/** Envanterdeki tek bir item yigini (item kimligi + adet). */
USTRUCT(BlueprintType)
struct FMMOItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "MMO|Inventory")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "MMO|Inventory")
	int32 Count = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMMOOnGoldChanged, int32, NewGold);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMMOOnInventoryChanged);

/**
 * Basit envanter/cuzdan bileseni: altin (Gold) ve item yiginlarini tutar.
 * Sunucu-otoriteli, replicated; OnRep handler'lari UI icin delegate tetikler.
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UMMOInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMMOInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category = "MMO|Inventory")
	FMMOOnGoldChanged OnGoldChanged;

	UPROPERTY(BlueprintAssignable, Category = "MMO|Inventory")
	FMMOOnInventoryChanged OnInventoryChanged;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Gold, Category = "MMO|Inventory")
	int32 Gold = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Items, Category = "MMO|Inventory")
	TArray<FMMOItemStack> Items;

	/** Altin ekler. Sadece otorite. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Inventory")
	void AddGold(int32 Amount);

	/** Item ekler (varsa yigina ekler, yoksa yeni yigin). Sadece otorite. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Inventory")
	void AddItem(FName ItemId, int32 Count = 1);

	/** Item harcar (yeterli adet varsa dusurur, yigin biterse siler). Sadece otorite. Basarirsa true. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Inventory")
	bool RemoveItem(FName ItemId, int32 Count = 1);

	UFUNCTION(BlueprintPure, Category = "MMO|Inventory")
	int32 GetItemCount(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "MMO|Inventory")
	int32 GetTotalItemCount() const;

	// ------------------------------------------------------------------
	// Ekipman (KO tarzi): canta + kusanilan slotlar + upgrade/yakma
	// ------------------------------------------------------------------

	// Cantadaki (kusanilmamis) ekipmanlar.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Items, Category = "MMO|Equipment")
	TArray<FMMOEquipmentInstance> EquipmentBag;

	// Kusanilan ekipman; index = (int)EMMOEquipSlot (4 slot). InstanceId=0 -> bos.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Items, Category = "MMO|Equipment")
	TArray<FMMOEquipmentInstance> EquippedItems;

	/** Yeni ekipman ornegi ekler (loot/odul). Sadece otorite. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Equipment")
	void AddEquipmentInstance(FName ItemId, int32 Plus = 0);

	/** Cantadaki ekipmani kusanir (slot doluysa takas eder). Istemciden cagrilirsa sunucuya yonlenir. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Equipment")
	void EquipFromBag(int32 BagIndex);

	/** Slottaki ekipmani cikarip cantaya koyar. Istemciden cagrilirsa sunucuya yonlenir. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Equipment")
	void UnequipSlot(EMMOEquipSlot Slot);

	/** Cantadaki ekipmani +1 yukseltmeyi dener: altin harcar, sans zar atar; yuksek artida basarisizlik YAKAR. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Equipment")
	void TryUpgrade(int32 BagIndex);

	UFUNCTION(BlueprintPure, Category = "MMO|Equipment")
	FMMOEquipmentInstance GetEquipped(EMMOEquipSlot Slot) const;

	// --- Satici islemleri (altin dongusu) ---

	/** Saticidan ekipman satin alir (altin yeterliyse). Istemciden -> Server RPC. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Shop")
	void BuyEquipment(FName ItemId);

	/** Saticidan can iksiri satin alir. Istemciden -> Server RPC. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Shop")
	void BuyPotion(int32 Count = 1);

	/** Cantadaki ekipmani saticiya satar (altin kazanir). Istemciden -> Server RPC. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Shop")
	void SellEquipment(int32 BagIndex);

	// Kalicilik: envanter/ekipman/cuzdan verisini kayda yazar / kayittan uygular (sadece otorite).
	void FillSaveData(struct FMMOCharacterSaveData& Out) const;
	void ApplySaveData(const struct FMMOCharacterSaveData& In);

protected:
	UFUNCTION()
	void OnRep_Gold();

	UFUNCTION()
	void OnRep_Items();

	bool HasAuthorityInternal() const;
	bool IsLocallyControlledOwner() const;

	// Kusanilanlarin toplam bonusunu stat bilesenine isler (sunucu).
	void RecomputeEquipmentBonuses();

	// Sunucuda artan ornek kimligi.
	int32 NextInstanceId = 1;

	// --- Server RPC'ler ---
	UFUNCTION(Server, Reliable)
	void ServerEquipFromBag(int32 BagIndex);

	UFUNCTION(Server, Reliable)
	void ServerUnequipSlot(EMMOEquipSlot Slot);

	UFUNCTION(Server, Reliable)
	void ServerTryUpgrade(int32 BagIndex);

	UFUNCTION(Server, Reliable)
	void ServerBuyEquipment(FName ItemId);

	UFUNCTION(Server, Reliable)
	void ServerBuyPotion(int32 Count);

	UFUNCTION(Server, Reliable)
	void ServerSellEquipment(int32 BagIndex);

	// Upgrade sonucunu sahibine bildir (basari / basarisiz / YANDI).
	UFUNCTION(Client, Reliable)
	void ClientUpgradeResult(bool bSuccess, bool bBurned, FName ItemId, int32 NewPlus);
};

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

#if !UE_BUILD_SHIPPING
	static bool RunSavedEquipmentLevelSelfTest(UWorld* World, FString& OutError, int32& OutChecks);
	static bool RunVendorEconomySelfTest(UWorld* World, FString& OutError, int32& OutChecks);
	static bool RunFirstEquipSelfTest(UWorld* World, FString& OutError, int32& OutChecks);
	static bool RunUpgradeCallbackSelfTest(UWorld* World, FString& OutError, int32& OutChecks);
	static bool RunPurchaseCallbackSelfTest(UWorld* World, FString& OutError, int32& OutChecks);
	static bool RunGoldMetricSelfTest(UWorld* World, FString& OutError, int32& OutChecks);
#endif

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category = "MMO|Inventory")
	FMMOOnGoldChanged OnGoldChanged;

	UPROPERTY(BlueprintAssignable, Category = "MMO|Inventory")
	FMMOOnInventoryChanged OnInventoryChanged;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Gold, Category = "MMO|Inventory")
	int32 Gold = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Items, Category = "MMO|Inventory")
	TArray<FMMOItemStack> Items;

	// Altin tavani: calisma zamani (AddGold), ticaret (tavan asimi reddi) ve kayit (ApplySaveData) AYNI
	// sabiti kullanir; boylece oyunda tutulan altin kaydet-yukle sonrasi da aynen kalir. Toplamlar
	// int64'te hesaplanir.
	static constexpr int32 MaxGold = 999999999;

	/** Altin ekler (sonuc [0, MaxGold]). Tavani asan KAZANC kirpilir ve sahibe bildirilir (ticaret
	 *  tavan asimini once reddeder, altin yutmaz). Sadece otorite. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Inventory")
	void AddGold(int32 Amount);

	/** AddGold + olcum gunlugu kaynak etiketi (loot/gorev/satis/satin_alma/yukseltme/iyilestirme/ticaret). Sadece otorite. */
	void AddGoldFrom(int32 Amount, const TCHAR* Source);

	// Sarf malzemesi tavanlari: kayit (ApplySaveData) ayni sinirlari uygular, boylece calisma
	// zamaninda tutulan her adet kaydet-yukle sonrasi da kalir (eskiden AddItem sinirsizdi, yukleme kirpardi).
	static constexpr int32 MaxStackSize = 999;
	static constexpr int32 MaxItemStacks = 64;

	/** Bu esyadan simdi en fazla kac adet eklenebilir (yigin tavani 999, farkli yigin tavani 64). */
	int32 GetAddRoom(FName ItemId) const;

	/**
	 * Item ekler (varsa yigina ekler, yoksa yeni yigin). Sadece otorite.
	 * Tavan asilirsa fazlasi EKLENMEZ ve sessizce yok edilmez: gercekten eklenen adet dondurulur,
	 * cagiran kalani yerde birakir / reddeder / bildirir.
	 */
	UFUNCTION(BlueprintCallable, Category = "MMO|Inventory")
	int32 AddItem(FName ItemId, int32 Count = 1);

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

	// Canta kapasitesi (bug-hunt): HUD envanter penceresi eskiden CANTA'yi ilk 8 satirla
	// SESSIZCE kirpiyordu (MaxRows=8) ama EquipmentBag'in kendisi sinirsiz buyuyebiliyordu —
	// normal oynanista (loot dusme oranlariyla) 8'i asan oyuncularin fazla itemleri hicbir
	// UI eyleminden (goster/sat/kusan/yukselt) erisilemez oluyordu. Artik AddEquipmentInstance
	// bu sinirda dolulukta yeni item EKLEMİYOR (var olan itemler asla silinmez); HUD da
	// TUM canta icerigini (<=MaxBagSize) her zaman gosterecek sekilde satir yuksekligini
	// dinamik kucultuyor (shop/quest penceresindeki "tasma yok" deseniyle ayni).
	static constexpr int32 MaxBagSize = 12;

	// Satici seviye kapisi: alici seviyesi + bu pay >= esyanin RequiredLevel'i olmali (BuyEquipment). PLACEHOLDER.
	static constexpr int32 VendorLevelSlack = 3;

	// Kusanilan ekipman; index = (int)EMMOEquipSlot (4 slot). InstanceId=0 -> bos.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Items, Category = "MMO|Equipment")
	TArray<FMMOEquipmentInstance> EquippedItems;

	/** Yeni ekipman ornegi ekler (loot/odul). Sadece otorite. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Equipment")
	void AddEquipmentInstance(FName ItemId, int32 Plus = 0, EMMOItemRarity Rarity = EMMOItemRarity::Common);

	// NETWORK_AUDIT Y4: Equip/Upgrade/Sell sunucuya canta INDEKSI degil InstanceId ile gider
	// (indeks, bayat gorunum/hizli cift tikta yanlis esyaya isabet edebiliyordu). Indeks alan
	// eski API'ler (EquipFromBag/TryUpgrade/SellEquipment) korundu: istemci tarafinda indeksi
	// KENDI gorunumundeki InstanceId'ye cevirip *ByInstanceId yoluna yonlendirirler.

	/** Cantadaki ekipmani kusanir (slot doluysa takas eder). Istemciden cagrilirsa sunucuya yonlenir. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Equipment")
	void EquipFromBag(int32 BagIndex);

	/** EquipFromBag'in InstanceId surumu (sunucuya giden asil yol). */
	UFUNCTION(BlueprintCallable, Category = "MMO|Equipment")
	void EquipByInstanceId(int32 InstanceId);

	// Server-only starter setup; does not count as the player's first equip request.
	void EquipStarterEquipment(int32 InstanceId);

	/** Slottaki ekipmani cikarip cantaya koyar. Istemciden cagrilirsa sunucuya yonlenir. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Equipment")
	void UnequipSlot(EMMOEquipSlot Slot);

	/** Cantadaki ekipmani +1 yukseltmeyi dener: altin harcar, sans zar atar; yuksek artida basarisizlik YAKAR. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Equipment")
	void TryUpgrade(int32 BagIndex);

	/**
	 * TryUpgrade'in InstanceId surumu. ExpectedPlus >= 0 ise sunucu, esyanin arti seviyesi
	 * istemcinin gordugu degerle ayni degilse reddeder (cift tikta ikinci istek bayat olur).
	 */
	UFUNCTION(BlueprintCallable, Category = "MMO|Equipment")
	void UpgradeByInstanceId(int32 InstanceId, int32 ExpectedPlus = -1);

	UFUNCTION(BlueprintPure, Category = "MMO|Equipment")
	FMMOEquipmentInstance GetEquipped(EMMOEquipSlot Slot) const;

	// --- Satici islemleri (altin dongusu) ---

	/** Saticidan ekipman satin alir (altin yeterliyse). Istemciden -> Server RPC. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Shop")
	void BuyEquipment(FName ItemId);

	/** Saticidan can iksiri satin alir. Istemciden -> Server RPC. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Shop")
	void BuyPotion(int32 Count = 1);

	/** Saticidan bir tuketilebilir (mana iksiri, parsomen...) satin alir. Istemciden -> Server RPC. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Shop")
	void BuyConsumable(FName ItemId, int32 Count = 1);

	/** Cantadaki ekipmani saticiya satar (altin kazanir). Istemciden -> Server RPC. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Shop")
	void SellEquipment(int32 BagIndex);

	/** SellEquipment'in InstanceId surumu (sunucuya giden asil yol). */
	UFUNCTION(BlueprintCallable, Category = "MMO|Shop")
	void SellByInstanceId(int32 InstanceId);

	// --- Depo (KO "Inn" deposu): canta disinda kalici ekipman saklama ---
	// Yalniz sahibine replike edilir. Kayitla birlikte saklanir; haritalar arasi ortaktir.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Items, Category = "MMO|Warehouse")
	TArray<FMMOEquipmentInstance> Warehouse;

	static constexpr int32 MaxWarehouseSize = 24;

	/** Cantadaki ekipmani (InstanceId ile) depoya koyar. Depocu yakininda; istemciden -> Server RPC. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Warehouse")
	void DepositEquipment(int32 InstanceId);

	/** Depodaki ekipmani (InstanceId ile) cantaya alir (canta doluysa reddeder). Istemciden -> Server RPC. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Warehouse")
	void WithdrawEquipment(int32 InstanceId);

	// --- Ticaret yardimcilari (UMMOTradeComponent kullanir; sadece otorite) ---
	/** Cantadan InstanceId ile ekipmani cikarir ve Out'a yazar. Bulunamazsa false. */
	bool TakeEquipmentByInstanceId(int32 InstanceId, FMMOEquipmentInstance& Out);

	/** Baska oyuncudan gelen ekipmani YENI bir InstanceId ile cantaya ekler (kimlik cakismasi/cogaltma olmasin). Canta doluysa false. */
	bool ReceiveTradedEquipment(const FMMOEquipmentInstance& Src);

	// Kalicilik: envanter/ekipman/cuzdan verisini kayda yazar / kayittan uygular (sadece otorite).
	void FillSaveData(struct FMMOCharacterSaveData& Out) const;
	void ApplySaveData(const struct FMMOCharacterSaveData& In);

protected:
	// Purchase commits delivery and payment before publishing either delegate.
	int32 AddItemInternal(FName ItemId, int32 Count, bool bNotify);
	bool AddEquipmentInstanceInternal(FName ItemId, int32 Plus, EMMOItemRarity Rarity, bool bNotify);

	UFUNCTION()
	void OnRep_Gold();

	UFUNCTION()
	void OnRep_Items();

	void AddGoldInternal(int32 Amount, const TCHAR* Source);

	bool HasAuthorityInternal() const;
	void EquipByInstanceIdInternal(int32 InstanceId, bool bRecordFirstEquip);
	bool IsLocallyControlledOwner() const;

	// Sahip, MaxDist icinde bir satici NPC'ye yakin mi? Satin-al/sat RPC'lerinin
	// sunucu-tarafi dogrulamasi (client satici olmadan her yerden alisveris yapamasin).
	bool IsOwnerNearVendor(float MaxDist = 800.f) const;

	// Depo RPC'lerinin sunucu dogrulamasi: sahip bir depocu NPC'ye MaxDist icinde mi.
	bool IsOwnerNearWarehouse(float MaxDist = 800.f) const;

	UFUNCTION(Server, Reliable)
	void ServerDepositEquipment(int32 InstanceId);

	UFUNCTION(Server, Reliable)
	void ServerWithdrawEquipment(int32 InstanceId);

	// Kusanilanlarin toplam bonusunu stat bilesenine isler (sunucu).
	void RecomputeEquipmentBonuses();

	// Sunucuda artan ornek kimligi.
	int32 NextInstanceId = 1;

	// Cantada InstanceId'li ekipmanin indeksi (yoksa/0 ise INDEX_NONE).
	int32 FindBagIndexByInstanceId(int32 InstanceId) const;

	// Sahibine kisa sistem satiri yollar (uzak istemciye de ulasir; yalniz sunucuda).
	void NotifyOwnerText(const TCHAR* Text) const;

	// Yukseltme istekleri arasi asgari sure (sunucu saati, saniye) ve son deneme zamani.
	static constexpr double UpgradeCooldownSeconds = 0.3;
	double LastUpgradeServerTime = -1000.0;

	// --- Server RPC'ler ---
	UFUNCTION(Server, Reliable)
	void ServerEquipByInstanceId(int32 InstanceId);

	UFUNCTION(Server, Reliable)
	void ServerUnequipSlot(EMMOEquipSlot Slot);

	UFUNCTION(Server, Reliable)
	void ServerUpgradeByInstanceId(int32 InstanceId, int32 ExpectedPlus);

	UFUNCTION(Server, Reliable)
	void ServerBuyEquipment(FName ItemId);

	UFUNCTION(Server, Reliable)
	void ServerBuyPotion(int32 Count);

	UFUNCTION(Server, Reliable)
	void ServerBuyConsumable(FName ItemId, int32 Count);

	UFUNCTION(Server, Reliable)
	void ServerSellByInstanceId(int32 InstanceId);

	// Upgrade sonucunu sahibine bildir (basari / basarisiz / YANDI).
	UFUNCTION(Client, Reliable)
	void ClientUpgradeResult(bool bSuccess, bool bBurned, FName ItemId, int32 NewPlus);
};

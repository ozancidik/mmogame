// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/MMOItemTypes.h"
#include "MMOTradeComponent.generated.h"

class AMMOCharacter;

/** Ticarette bir tarafin teklifi: ekipmanlar (cantadan kopya), altin ve onay bayragi. */
USTRUCT(BlueprintType)
struct FMMOTradeOffer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MMO|Trade")
	TArray<FMMOEquipmentInstance> Items;

	UPROPERTY(BlueprintReadOnly, Category = "MMO|Trade")
	int32 Gold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MMO|Trade")
	bool bConfirmed = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMMOOnTradeChanged);

/**
 * Oyuncular arasi ticaret bileseni (KO tarzi). Sunucu-otoriteli.
 * Akis: davet -> kabul -> iki taraf da ekipman (en fazla MaxOfferItems) + altin teklif eder ->
 * ikisi de onaylayinca SUNUCU atomik takas eder. Teklif degisirse iki onay da sifirlanir.
 * Iptal / uzaklasma / olum / bolge degisimi takasi iptal eder.
 * Gecici durum yalniz iki tarafa replike edilir (COND_OwnerOnly; sunucu iki tarafin aynasini yazar).
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UMMOTradeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMMOTradeComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Ticaret durumu degisince HUD'u tazelemek icin.
	UPROPERTY(BlueprintAssignable, Category = "MMO|Trade")
	FMMOOnTradeChanged OnTradeChanged;

	// Su an bir ticaretin icinde mi (sahibine replike).
	UPROPERTY(ReplicatedUsing = OnRep_Trade, BlueprintReadOnly, Category = "MMO|Trade")
	bool bTradeActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_Trade, BlueprintReadOnly, Category = "MMO|Trade")
	FString PartnerName;

	// Sahibin kendi teklifi ve karsi tarafin teklifi.
	UPROPERTY(ReplicatedUsing = OnRep_Trade, BlueprintReadOnly, Category = "MMO|Trade")
	FMMOTradeOffer MyOffer;

	// Teklif her degistiginde artar (iki tarafta ayni deger). Onay, gorulen revizyona baglidir:
	// gecikmeli bir onay, sonradan degisen teklife onay sayilmaz (QA YUKSEK).
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "MMO|Trade")
	int32 OfferRevision = 0;

	// Owner-only, server-generated generation; kept after cancellation to reject old-session approvals.
	// uint64 is deliberately C++ only (not exposed as a Blueprint numeric property).
	UPROPERTY(ReplicatedUsing = OnRep_Trade)
	uint64 TradeSessionId = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Trade, BlueprintReadOnly, Category = "MMO|Trade")
	FMMOTradeOffer TheirOffer;

	UFUNCTION(BlueprintPure, Category = "MMO|Trade")
	bool IsTrading() const { return bTradeActive; }

	static constexpr int32 MaxOfferItems = 4;
	static constexpr float InviteRange = 600.f;       // davet menzili
	static constexpr float CancelDistance = 800.f;    // bunu asan uzaklasma takasi iptal eder

	// --- Sahip istemciden cagrilir (sunucuya yonlenir) ---
	void RequestInviteNearest();
	void RequestAccept();
	void RequestCancel();
	void RequestAddItem(int32 InstanceId);
	void RequestRemoveItem(int32 InstanceId);
	void RequestSetGold(int32 NewGold);
	void RequestConfirm();
	// HUD supplies the last rendered offer's identity, not newer state received before the click.
	void RequestConfirmSeen(uint64 SeenSessionId, int32 SeenRevision);

#if !UE_BUILD_SHIPPING
	// Same-frame, controllerless server fixtures; no RPC transport or durable save proof.
	static bool RunTradeSelfTest(UWorld* World, FString& OutError, int32& OutChecks);
#endif

protected:
	UFUNCTION()
	void OnRep_Trade();

	bool HasAuthorityInternal() const;
	AMMOCharacter* GetOwnerCharacter() const;

	// --- Sunucu RPC'leri ---
	UFUNCTION(Server, Reliable)
	void ServerInviteNearest();

	UFUNCTION(Server, Reliable)
	void ServerAccept();

	UFUNCTION(Server, Reliable)
	void ServerCancel();

	UFUNCTION(Server, Reliable)
	void ServerAddItem(int32 InstanceId);

	UFUNCTION(Server, Reliable)
	void ServerRemoveItem(int32 InstanceId);

	UFUNCTION(Server, Reliable)
	void ServerSetGold(int32 NewGold);

	UFUNCTION(Server, Reliable)
	void ServerConfirm(uint64 SeenSessionId, int32 SeenRevision);

	// --- Sunucu durumu (replike degil) ---
	TWeakObjectPtr<AMMOCharacter> Partner;
	TWeakObjectPtr<AMMOCharacter> PendingInviteFrom;
	float PendingInviteTime = -1.f;
	float LastInviteTime = -100.f;
	float LastOfferChangeTime = -100.f;
	float LastTradeChangeTime = -100.f;   // iki taraftan herhangi birinin son teklif degisikligi
	float LastConfirmTime = -100.f;
	static constexpr float ConfirmLockSeconds = 1.0f;
	static constexpr float ConfirmCooldown = 0.5f;
	static constexpr float IdleTimeoutSeconds = 180.f;
	FTimerHandle ValidateTimerHandle;

	static constexpr float InviteCooldown = 3.f;
	static constexpr float OfferChangeCooldown = 0.3f;
	static constexpr float InviteExpirySeconds = 30.f;

	// Karsi tarafin bileseni; karsilikli eslesme gecerli degilse nullptr.
	UMMOTradeComponent* GetValidPartnerComp() const;

	AMMOCharacter* FindNearestTradeTarget() const;

	// Iki taraf icin ticareti baslatir.
	static void StartTradeServer(AMMOCharacter* A, AMMOCharacter* B);

	// Bu bilesenin ve karsi tarafin ticaret durumunu temizler; Reason bos degilse ikisine bildirir.
	void EndTradeServer(const FString& Reason);

	// Kendi durumunu temizler (kismi; EndTradeServer iki tarafa uygular).
	void ResetLocalState();

	// Sahibin teklifini karsi tarafin aynasina yazar, iki onayi sifirlar, delegeleri tetikler.
	void OnMyOfferChanged();

	// Iki taraf onayladiysa sunucuda yeniden dogrulayip takasi uygular.
	void TryExecuteTrade();

	// Periyodik sunucu dogrulamasi: uzaklasma / olum / kopma.
	void ValidateTick();

	// Sahibin ekranina sistem sohbet satiri gonderir.
	void NotifyOwner(const FString& Text) const;
};

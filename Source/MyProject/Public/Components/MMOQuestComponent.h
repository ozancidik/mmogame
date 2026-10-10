// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Quests/MMOQuestTypes.h"
#include "MMOQuestComponent.generated.h"

/**
 * Oyuncunun gorevlerini yonetir: kabul, ilerleme takibi, odul alma. Sunucu-otoriteli, replicated.
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UMMOQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMMOQuestComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "MMO|Quest")
	TArray<FMMOQuestProgress> ActiveQuests;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "MMO|Quest")
	TArray<FName> CompletedQuests;

	/** Gorevi kabul eder (istemciden -> Server RPC). */
	UFUNCTION(BlueprintCallable, Category = "MMO|Quest")
	void AcceptQuest(FName QuestId);

	/** Tamamlanmis gorevin odulunu alir (istemciden -> Server RPC). */
	UFUNCTION(BlueprintCallable, Category = "MMO|Quest")
	void ClaimReward(FName QuestId);

	/** Bir mob olunce olduren oyuncunun bilesenine cagrilir (sunucu). */
	void NotifyMobKilled(FName MobArchetypeId);

	UFUNCTION(BlueprintPure, Category = "MMO|Quest")
	bool IsQuestActive(FName QuestId) const;

	UFUNCTION(BlueprintPure, Category = "MMO|Quest")
	bool IsCompleted(FName QuestId) const;

	// Gorev su an alinabilir mi? (aktif/tamam degil, level yeterli, on kosul gorev tamamlanmis)
	UFUNCTION(BlueprintPure, Category = "MMO|Quest")
	bool IsQuestAvailable(FName QuestId) const;

	// Gorevin su anki ilerlemesini (0..hedef) dondurur.
	int32 GetProgress(const FMMOQuestDef& Def) const;

	// Gorev tamamlanma kosulu saglandi mi?
	bool IsQuestComplete(const FMMOQuestDef& Def, const FMMOQuestProgress& Prog) const;

	// Kalicilik.
	void FillSaveData(struct FMMOCharacterSaveData& Out) const;
	void ApplySaveData(const struct FMMOCharacterSaveData& In);

#if !UE_BUILD_SHIPPING
	static bool RunRewardLifeStateSelfTest(UWorld* World, FString& OutError, int32& OutChecks);
	static bool RunRewardCapacitySelfTest(UWorld* World, FString& OutError, int32& OutChecks);
	static bool RunRewardCallbackSelfTest(UWorld* World, FString& OutError, int32& OutChecks);
#endif

protected:
	FMMOQuestProgress* FindActive(FName QuestId);
	const FMMOQuestProgress* FindActive(FName QuestId) const;
	bool HasAuthorityInternal() const;
	bool IsLocallyControlledOwner() const;

	// Sahip, MaxDist icinde bir gorevli NPC'ye yakin mi? AcceptQuest/ClaimReward RPC'lerinin
	// sunucu-tarafi dogrulamasi (client, gorevliye hic yaklasmadan gorev kabul/odul-alma
	// yapamasin — vendor'daki IsOwnerNearVendor ile ayni desen, digerinde eksikti).
	bool IsOwnerNearQuestGiver(float MaxDist = 800.f) const;

	// IsOwnerNearQuestGiver onbellegi: en son menzilde bulunan gorevli (sadece hizli yol;
	// menzilde degilse tam tarama yapilir, sonuc degismez).
	mutable TWeakObjectPtr<class AMMOQuestGiverNPC> CachedNearGiver;

	// Server RPC hiz siniri: kabul ve odul istekleri ayri ayri sinirlanir; ardisik kabul/odul engellenmez.
	static constexpr float QuestRpcMinInterval = 0.25f;

public:
	// HaveGold gorevleri bu hedefin altinda birikimdir (altin kalir), bu hedef ve ustunde teslimde BAGISTIR (altin dusulur).
	// Q_Gold500/1000 erken oyunda altin kitligi yuzunden birikim kalir. DECISIONS_NEEDED madde 3(a); PLACEHOLDER.
	// public: HUD aktif gorev satirinda bagis notu icin okur.
	static constexpr int32 GoldDonationMinTarget = 5000;

protected:
	double LastAcceptRpcTime = -1.0;
	double LastClaimRpcTime = -1.0;
	bool PassesRpcRateLimit(double& LastTime);

	// Synchronous reward delegates may call ClaimReward again. Per-component,
	// server-only call guard; neither replicated nor persisted.
	bool bClaimRewardInProgress = false;

	UFUNCTION(Server, Reliable)
	void ServerAcceptQuest(FName QuestId);

	UFUNCTION(Server, Reliable)
	void ServerClaimReward(FName QuestId);

	// Odul verilemediginde nedeni yalnizca gorevi almaya calisan oyuncuya bildir.
	UFUNCTION(Client, Reliable)
	void ClientClaimRewardFailed(const FString& Reason);
};

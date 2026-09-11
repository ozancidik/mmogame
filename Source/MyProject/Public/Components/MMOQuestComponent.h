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

protected:
	FMMOQuestProgress* FindActive(FName QuestId);
	const FMMOQuestProgress* FindActive(FName QuestId) const;
	bool HasAuthorityInternal() const;
	bool IsLocallyControlledOwner() const;

	// Sahip, MaxDist icinde bir gorevli NPC'ye yakin mi? AcceptQuest/ClaimReward RPC'lerinin
	// sunucu-tarafi dogrulamasi (client, gorevliye hic yaklasmadan gorev kabul/odul-alma
	// yapamasin — vendor'daki IsOwnerNearVendor ile ayni desen, digerinde eksikti).
	bool IsOwnerNearQuestGiver(float MaxDist = 800.f) const;

	UFUNCTION(Server, Reliable)
	void ServerAcceptQuest(FName QuestId);

	UFUNCTION(Server, Reliable)
	void ServerClaimReward(FName QuestId);
};

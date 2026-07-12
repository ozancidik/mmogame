// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/MMOCharacter.h"
#include "MMOMob.generated.h"

class AMMOLootPickup;

/**
 * Basit dusman NPC'si. MMOCharacter'dan (dolayisiyla stat sisteminden) turer.
 * Sunucuda: oyuncuyu AggroRange icinde gorunce uzerine yurur, AttackRange'de periyodik vurur.
 * Oldugunde son vuran oyuncuya XPReward kadar deneyim verir ve kisa sure sonra yok olur.
 */
UCLASS()
class MYPROJECT_API AMMOMob : public AMMOCharacter
{
	GENERATED_BODY()

public:
	AMMOMob();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Hasar alinca saldirgani hedef alir (menzilli "pull" calissin diye AggroRange'i asar).
	virtual float TakeDamage(float DamageAmount, const struct FDamageEvent& DamageEvent,
		class AController* EventInstigator, AActor* DamageCauser) override;

	// Bos birakilirsa spawn'da rastgele bir arketip atanir; doldurulursa o arketip kullanilir.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Archetype, Category = "MMO|Mob")
	FName ArchetypeId;

	// Gorunen ad ve seviye (HUD plakasi/hedef bari icin; istemcilere replike).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "MMO|Mob")
	FString MobName = TEXT("Canavar");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "MMO|Mob")
	int32 MobLevel = 1;

	UFUNCTION(BlueprintPure, Category = "MMO|Mob")
	const FString& GetMobName() const { return MobName; }

	UFUNCTION(BlueprintPure, Category = "MMO|Mob")
	int32 GetMobLevel() const { return MobLevel; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	float AggroRange = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	float AttackRange = 160.f;

	// Mob spawn noktasindan bu kadar uzaklasirsa kovalamayi birakip eve doner ve tam canlanir (leash).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	float LeashRange = 1800.f;

	// Bos zamanda (hedef yokken) ev cevresinde bu yaricap icinde rastgele gezinir.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	float WanderRadius = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	float MobAttackCooldown = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	float MobAttackDamage = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	int32 XPReward = 40;

	// --- Loot ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob|Loot")
	TSubclassOf<AMMOLootPickup> LootPickupClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob|Loot")
	int32 MinGold = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob|Loot")
	int32 MaxGold = 25;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob|Loot")
	float ItemDropChance = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob|Loot")
	FName DropItemId = TEXT("HealthPotion");

	// Ekipman dusurme sansi (item veritabanindan rastgele secilir).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob|Loot")
	float EquipDropChance = 0.25f;

protected:
	virtual void BeginPlay() override;
	virtual void HandleDeath() override;

	// Arketipi mobun statlarina + gorseline uygular (sunucu).
	void ApplyArchetype(const struct FMMOMobArchetype& Arch);

	// Sadece gorsel (olcek + renk) — hem sunucu hem istemci uygular.
	void ApplyArchetypeVisuals(const struct FMMOMobArchetype& Arch);

	// Govdeyi verilen renge boyar.
	void ApplyTint(const FLinearColor& Color);

	UFUNCTION()
	void OnRep_Archetype();

	float LastMobAttackTime = -100.f;
	float LastRepathTime = -100.f;
	bool bDeathHandled = false;

	// --- Leash (eve donus) ---
	FVector HomeLocation = FVector::ZeroVector;
	bool bReturningHome = false;

	// --- Idle gezinme ---
	float NextWanderTime = 0.f;
	void IdleWander();

	// --- Hasarda aggro (menzilli pull) ---
	TWeakObjectPtr<APawn> ForcedTarget;
	float ForcedAggroUntil = -1.f;

	// Hasar alan mobun saldirgani ne kadar sure zorla hedeflemeye devam edecegi.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	float ForcedAggroDuration = 8.f;

	// --- Yetenekler (arketipten kopyalanir; sunucu tarafi) ---
	bool bIsElite = false;
	FName OnHitEffectId;
	float OnHitEffectChance = 0.f;
	FName EnrageEffectId;
	float EnrageHealthPct = 0.f;
	bool bEnraged = false;

	void DestroySelf();
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MMOCharacter.generated.h"

class UMMOStatsComponent;
class UMMOInventoryComponent;
class UMMOSkillComponent;
class UMMOQuestComponent;

/**
 * MMO karakterleri (oyuncu ve NPC/mob) için C++ temel sınıfı.
 * Stat bileşenini barındırır ve standart hasar akışını (TakeDamage) stat sistemine bağlar.
 * BP_ThirdPersonCharacter bu sınıfa reparent edilerek stat sistemine kavuşur.
 */
UCLASS()
class MYPROJECT_API AMMOCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMMOCharacter();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MMO")
	TObjectPtr<UMMOStatsComponent> Stats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MMO")
	TObjectPtr<UMMOInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MMO")
	TObjectPtr<UMMOSkillComponent> SkillComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MMO")
	TObjectPtr<UMMOQuestComponent> QuestComponent;

	// Unreal'in standart hasar akışı (UGameplayStatics::ApplyDamage) buraya düşer;
	// biz de stat bileşenine yönlendiririz. Sadece otoritede etki eder.
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "MMO")
	UMMOStatsComponent* GetStats() const { return Stats; }

	UFUNCTION(BlueprintPure, Category = "MMO")
	UMMOInventoryComponent* GetInventory() const { return Inventory; }

	UFUNCTION(BlueprintPure, Category = "MMO")
	UMMOSkillComponent* GetSkillComponent() const { return SkillComponent; }

	UFUNCTION(BlueprintPure, Category = "MMO")
	UMMOQuestComponent* GetQuestComponent() const { return QuestComponent; }

	// Sinif degistirme konsol komutu: "MMOClass warrior|archer|mage|priest".
	UFUNCTION(Exec)
	void MMOClass(const FString& ClassName);

	// --- Kalicilik (kaydet/yukle) ---
	UFUNCTION(Exec)
	void MMOSave();

	UFUNCTION(Exec)
	void MMOLoad();

	// Karakteri diske yazar / diskten yukler (sadece otorite; istemciden Exec cagrilirsa sunucuya yonlenir).
	void SaveCharacter();
	bool LoadCharacter();
	FString GetSaveSlotName() const;

	// Olum sonrasi yeniden dogusa kalan sure (HUD geri sayimi icin; olu degilse 0).
	UFUNCTION(BlueprintPure, Category = "MMO")
	float GetRespawnTimeRemaining() const;

	// Olumden kac saniye sonra yeniden dogulur (sadece oyuncu).
	UPROPERTY(EditAnywhere, Category = "MMO|Combat")
	float RespawnDelay = 5.f;

	// Envanterden bir can iksiri kullanir (F tusu / konsol).
	UFUNCTION(BlueprintCallable, Exec, Category = "MMO|Inventory")
	void UseHealthPotion();

	// Bir iksirin yenildigi can miktari.
	UPROPERTY(EditAnywhere, Category = "MMO|Inventory")
	float PotionHealAmount = 75.f;

	// Bir tuketilebilir (mana iksiri / buff parsomeni) kullanir: HP/MP yeniler ve/veya buff uygular.
	UFUNCTION(BlueprintCallable, Category = "MMO|Inventory")
	void UseConsumable(FName ItemId);

	// G tusu -> mana iksiri (F zaten can iksiri).
	void UseManaPotion();

	// Konsol: "MMOUse ScrollRage" gibi bir tuketilebilir kullanir.
	UFUNCTION(Exec)
	void MMOUse(const FString& ItemId);

	// Test: envantere item ekler (konsol: "MMOGiveItem ScrollHaste 3").
	UFUNCTION(Exec)
	void MMOGiveItem(const FString& ItemId, int32 Count = 1);

	// --- Test/debug komutlari (PIE konsolundan cagrilabilir) ---
	// Ornek: "MMODamage 30", "MMOHeal 20", "MMOGainXP 500", "MMOUseMana 10"
	UFUNCTION(Exec)
	void MMODamage(float Amount = 20.f);

	UFUNCTION(Exec)
	void MMOHeal(float Amount = 20.f);

	UFUNCTION(Exec)
	void MMOGainXP(int32 Amount = 50);

	UFUNCTION(Exec)
	void MMOUseMana(float Amount = 10.f);

	// En yakin dusmana yakin dovus vurusu yapar (konsol testi).
	UFUNCTION(BlueprintCallable, Exec, Category = "MMO|Combat")
	void MMOAttack();

	// Belirli bir hedefe vurur (Target null ise en yakin mob). Istemciden -> Server RPC.
	// Hedef oyuncuysa sunucu PvP acik mi diye kontrol eder.
	UFUNCTION(BlueprintCallable, Category = "MMO|Combat")
	void AttackTarget(AMMOCharacter* Target);

	// PvP acik/kapa (konsol: "MMOPvP 1" / "MMOPvP 0"). Sunucudaki GameMode bayragini degistirir.
	UFUNCTION(Exec)
	void MMOPvP(int32 bOn);

	bool IsPvPAllowed() const;

	// Test: kendine bir buff/debuff uygular (konsol: "MMOBuff Rage", "MMOBuff Poison", "MMOBuff Haste").
	UFUNCTION(Exec)
	void MMOBuff(const FString& EffectId);

	// Test: onune belirtilen arketipte bir mob spawn'lar (konsol: "MMOSpawnMob OrcWarlord", "MMOSpawnMob SpiderQueen").
	UFUNCTION(Exec)
	void MMOSpawnMob(const FString& ArchetypeId);

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// Sunucuda possession aninda calisir: oyuncuysa sinif kurulumunu yapar (MP-guvenli).
	virtual void PossessedBy(AController* NewController) override;

	// Sahibi istemcide input hazir oldugunda calisir: BP'nin Enhanced Input mappinglerini temizler.
	virtual void PawnClientRestart() override;

	// BP'nin ekledigi Enhanced Input mapping'lerini temizler (fare-bak/WASD cakismasin).
	// BP BeginPlay mapping'i bizden sonra tekrar ekleyebildigi icin timer'la birkac kez cagrilir.
	void ClearBPInputMappings();
	FTimerHandle ClearMappingsTimerHandle;
	int32 ClearMappingsAttempts = 0;

	// Oyundan cikarken/level bitiminde otomatik kaydet.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Possess sonrasi guvenli zamanlamada kaydi yukle (tum bilesenler hazir olsun diye timer'la).
	void AutoLoadDeferred();

	UFUNCTION(Server, Reliable)
	void ServerSaveCharacter();

	UFUNCTION(Server, Reliable)
	void ServerLoadCharacter();

	FTimerHandle AutoLoadTimerHandle;
	FTimerHandle AutoSaveTimerHandle;

	// --- Server RPC'ler (istemci istegi -> sunucu uygular) ---
	UFUNCTION(Server, Reliable)
	void ServerAttackTarget(AMMOCharacter* Target);

	UFUNCTION(Server, Reliable)
	void ServerUseHealthPotion();

	UFUNCTION(Server, Reliable)
	void ServerUseConsumable(FName ItemId);

	UFUNCTION(Server, Reliable)
	void ServerGiveItem(FName ItemId, int32 Count);

	UFUNCTION(Server, Reliable)
	void ServerSetPvP(bool bOn);

	UFUNCTION(Server, Reliable)
	void ServerApplyBuff(FName EffectId);

	UFUNCTION(Server, Reliable)
	void ServerSpawnMob(FName ArchetypeId);

	// Yakin dovus vurus efektini tum makinelerde oynat.
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastMeleeFX(FVector ImpactLocation);

	// Hotbar tuslari 1..9,0 -> slot 0..9.
	void UseHotbar0();
	void UseHotbar1();
	void UseHotbar2();
	void UseHotbar3();
	void UseHotbar4();
	void UseHotbar5();
	void UseHotbar6();
	void UseHotbar7();
	void UseHotbar8();
	void UseHotbar9();

	// En son bu aktore hasar veren (oldurme XP'sini dogru oyuncuya vermek icin).
	TWeakObjectPtr<AActor> LastDamageInstigator;

	// --- Yakin dovus ayarlari ---
	UPROPERTY(EditAnywhere, Category = "MMO|Combat")
	float MeleeRange = 220.f;

	UPROPERTY(EditAnywhere, Category = "MMO|Combat")
	float MeleeRadius = 90.f;

	UPROPERTY(EditAnywhere, Category = "MMO|Combat")
	float MeleeCooldown = 0.6f;

	float LastMeleeTime = -100.f;

	// Stat bileşeninin OnDeath delegate'ine bağlanır.
	UFUNCTION()
	virtual void HandleDeath();

	// Stat bileşeninin OnRevived delegate'ine bağlanır (istemcide de hareketi geri açar).
	UFUNCTION()
	virtual void HandleRevived();

	// Level atlayinca ekran bildirimi (sadece yerel oyuncu).
	UFUNCTION()
	void OnLevelUpFeedback(int32 NewLevel);

	// Oyuncuyu baslangic noktasina isinlayip tam can/mana ile diriltir.
	void RespawnAtStart();

	FTimerHandle RespawnTimerHandle;

	// Blueprint'te ölüm animasyonu/efekti için override edilebilir olay.
	UFUNCTION(BlueprintImplementableEvent, Category = "MMO")
	void OnCharacterDied();
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/MMOCharacter.h"
#include "MMOMob.generated.h"

class AMMOLootPickup;
class USkeletalMesh;
class UAnimSequence;

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

	// GUVENLI BOLGE: PlayerStart (hub/dogus + NPC'ler) etrafinda bu yaricap icindeki oyunculara
	// hicbir mob saldirmaz/hedef almaz (klasik MMO kasaba guvenli alani). Aggro+gezinme menzili
	// hesaplari hub'a ulasabildigi icin (QA HIGH) yapisal savunma. 0 = kapali.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	float HubSafeRadius = 700.f;

	FVector HubLocation = FVector::ZeroVector;
	bool bHasHub = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	float MobAttackCooldown = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	float MobAttackDamage = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	int32 XPReward = 40;

	/**
	 * Seviye farkina gore XP (KO tarzi, sunucu tarafi). Oyuncu mob'dan 5'ten fazla seviye
	 * yuksekse her ek seviye icin -%10 (taban %10: "gri mob" farmi); mob oyuncudan 10'dan
	 * fazla seviye yuksekse (parti ile power-leveling) her ek seviye icin -%10 (taban %20).
	 * Fark bu bantlarin icindeyse tam XP. Gorev kredisi ve altin etkilenmez.
	 */
	static int32 ScaleXPForLevelGap(int32 BaseXP, int32 PlayerLevel, int32 InMobLevel);

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

	// --- Veri-odakli ozel yaratik gorseli + AnimBP'siz single-node animasyon ---
	// Arketipin MeshPath'i doluysa mesh o arketipin verisinden yuklenir ve durum
	// (idle/kosu/saldiri/olum) her makinede tek-dugum animasyonla oynatilir (AnimBP gerekmez).
	// MeshPath bos ise varsayilan tint'li Manny davranisi surer.
	bool bCustomMeshVisual = false;

	// O an yuklenmis olan animasyon seti (arketipten degil, ApplyArchetypeVisuals'ta yuklenir).
	UPROPERTY()
	TObjectPtr<UAnimSequence> AnimIdle = nullptr;

	UPROPERTY()
	TObjectPtr<UAnimSequence> AnimRun = nullptr;

	UPROPERTY()
	TObjectPtr<UAnimSequence> AnimAttack = nullptr;

	UPROPERTY()
	TObjectPtr<UAnimSequence> AnimDeath = nullptr;

	// O an oynayan durum animasyonu (tekrar tetiklememek icin).
	UPROPERTY()
	TObjectPtr<UAnimSequence> CurrentAnim = nullptr;

	// Saldiri animasyonu su ana kadar durum animasyonunu ezmesin (oynayip bitsin).
	float AttackAnimUntil = -1.f;

	// Durum -> animasyon secip (degistiyse) oynatir. Tum makinelerde cagirilir.
	void UpdateCreatureAnimation();

	// Mob vururken saldiri animasyonunu tum makinelerde oynat (server tetikler).
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayCreatureAttack();

	// Elit/boss oldugunde TUM sunucuya duyuru yap (bug-hunt: dunya boss'u/elit dustugunde
	// hicbir sunucu-genelinde bildirim yoktu - sadece olduren ve menzildeki parti uyeleri
	// gorurdu, herkes icin onemli bir olay olmasina ragmen). Reliable: nadir/dusuk frekansli
	// bir olay, kaybolmamasi onemli.
	UFUNCTION(NetMulticast, Reliable)
	void MulticastEliteDeathAnnounce(const FString& MobDisplayName, const FString& KillerDisplayName);

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

	// --- Elit ganimet (arketipten kopyalanir; sunucu tarafi). Default => normal loot davranisi.
	float EliteEquipDropChance = 0.f;
	int32 EliteEquipDropCount = 1;
	int32 EliteMinPlus = 0;
	int32 EliteMaxPlus = 0;

	void DestroySelf();
};

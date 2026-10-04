// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Effects/MMOEffectTypes.h"
#include "MMOStatsComponent.generated.h"

// UI ve oyun mantığı bu delegate'lere bağlanır (health bar, level-up efekti, ölüm vb.).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMMOOnHealthChanged, float, NewHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMMOOnManaChanged, float, NewMana, float, MaxMana);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMMOOnExperienceChanged, int32, CurrentExp, int32, ExpToNextLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMMOOnLevelUp, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMMOOnDeath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMMOOnRevived);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMMOOnEffectsChanged);

/**
 * MMORPG karakter/mob istatistik bileşeni.
 * Ana statlar (STR/DEX/INT/VIT) türetilmiş statları (MaxHealth, MaxMana, AttackPower...) belirler.
 * Otorite (sunucu/host) statları değiştirir, değerler istemcilere replike olur, OnRep fonksiyonları UI delegate'lerini tetikler.
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UMMOStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMMOStatsComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Sahibin "buff'siz" yurume hizini SIMDIKI MaxWalkSpeed'den yeniden cache'ler.
	// BeginPlay'de otomatik cache'lenir (varsayilan hiz icin); ama BeginPlay SONRASI
	// (or. AMMOMob::ApplyArchetype ile arketipe ozgu MoveSpeed atanirken) MaxWalkSpeed
	// degistirilirse bunu MUTLAKA cagir — yoksa bir sonraki hiz-efekti (Slow/Haste)
	// devreye girdiginde/bittiginde hiz yanlis (eski/varsayilan) taban degere sicrar.
	void RefreshBaseWalkSpeed();

	// ------------------------------------------------------------------
	// Delegate'ler (Blueprint'ten bind edilebilir)
	// ------------------------------------------------------------------
	UPROPERTY(BlueprintAssignable, Category = "MMO|Events")
	FMMOOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "MMO|Events")
	FMMOOnManaChanged OnManaChanged;

	UPROPERTY(BlueprintAssignable, Category = "MMO|Events")
	FMMOOnExperienceChanged OnExperienceChanged;

	UPROPERTY(BlueprintAssignable, Category = "MMO|Events")
	FMMOOnLevelUp OnLevelUp;

	UPROPERTY(BlueprintAssignable, Category = "MMO|Events")
	FMMOOnDeath OnDeath;

	UPROPERTY(BlueprintAssignable, Category = "MMO|Events")
	FMMOOnRevived OnRevived;

	// Aktif buff/debuff listesi degisince tetiklenir (HUD ikonlarini tazelemek icin).
	UPROPERTY(BlueprintAssignable, Category = "MMO|Events")
	FMMOOnEffectsChanged OnEffectsChanged;

	// ------------------------------------------------------------------
	// Aktif durum efektleri (buff/debuff)
	// ------------------------------------------------------------------
	UPROPERTY(ReplicatedUsing = OnRep_Effects, BlueprintReadOnly, Category = "MMO|Effects")
	TArray<FMMOActiveEffect> ActiveEffects;

	// ------------------------------------------------------------------
	// Ana statlar (primary attributes) — level atladıkça artar
	// ------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_PrimaryStats, Category = "MMO|Primary")
	int32 Strength = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_PrimaryStats, Category = "MMO|Primary")
	int32 Dexterity = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_PrimaryStats, Category = "MMO|Primary")
	int32 Intelligence = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_PrimaryStats, Category = "MMO|Primary")
	int32 Vitality = 12;

	// Level başına oyuncunun dağıtabileceği serbest stat puanı (KO tarzı).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "MMO|Primary")
	int32 UnspentStatPoints = 0;

	// Level başına kazanılan, skill öğrenmek için harcanan serbest skill puanı (KO tarzı).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "MMO|Primary")
	int32 UnspentSkillPoints = 0;

	// ------------------------------------------------------------------
	// Seviye / deneyim
	// ------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Level, Category = "MMO|Progression")
	int32 Level = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Experience, Category = "MMO|Progression")
	int32 Experience = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MMO|Progression")
	int32 ExperienceToNextLevel = 100;

	// 2026-09-14: kullanici acikca MaxLevel=80 istedi (KO'nun hedef derinligine uygun
	// tavan). Not: 20'ye cekilmisti cunku mevcut icerik (12 mob, 15 gorev) tum gorevler
	// bitirildiginde oyuncuyu sadece ~Lv15-16'ya tasiyor - bu hala gecerli bir kisit,
	// yani 80 su an "hedef tavan", icerik derinligi degil; Lv16-80 arasi mob/gorev
	// eklendikce bu tavan gercek anlamda doldurulmali (bkz. skill agaci genisletme notu,
	// MMOSkillDatabase.cpp).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Progression")
	int32 MaxLevel = 80;

	// ------------------------------------------------------------------
	// Türetilmiş / anlık statlar
	// ------------------------------------------------------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "MMO|Vitals")
	float Health = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "MMO|Vitals")
	float MaxHealth = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Mana, Category = "MMO|Vitals")
	float Mana = 50.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "MMO|Vitals")
	float MaxMana = 50.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "MMO|Vitals")
	float AttackPower = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "MMO|Vitals")
	float Defense = 0.f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_IsDead, Category = "MMO|Vitals")
	bool bIsDead = false;

	// Son ApplyDamage'in gercek (uygulanan) hasari; yalniz sunucuda anlamli, replike degil. OnDeath yayininda
	// OLUMCUL vurusun hasarini okumak icin: ApplyDamage bunu Health dusurulmeden ve OnDeath'ten ONCE yazar
	// (AMMOMob::HandleDeath hasar payi icin kullanir).
	float LastAppliedDamage = 0.f;

	// ------------------------------------------------------------------
	// Ayar eğrileri (dengeleme için Blueprint/DataAsset'ten değiştirilebilir)
	// ------------------------------------------------------------------
	UPROPERTY(EditDefaultsOnly, Category = "MMO|Tuning")
	float HealthPerVitality = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "MMO|Tuning")
	float HealthPerLevel = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "MMO|Tuning")
	float ManaPerIntelligence = 5.f;

	// QA denetimi (2026-09-14, BULGU #2): MaxMana'nin eskiden seviye terimi yoktu (MaxHealth'in
	// aksine) - INT'ye hic yatirim yapmayan bir karakterin mana tavani Lv1'de de Lv80'de de
	// ayniydi (~55-85), ama Lv10-80 genisletmesindeki skiller Lv80'de 400 mana'ya kadar
	// cikan maliyetler varsayarak tasarlandi (KO'nun gercek MaxMP formulunde de seviye terimi
	// var, bkz. ko_research_report.md). Bu, INT'ye hic basmayan Warrior/Archer icin kendi
	// gec-oyun skillerinin coğunu KULLANILAMAZ hale getiriyordu - kozmetik degil, fonksiyonel
	// bir bug. HealthPerLevel'in yanina ayni desenle eklendi.
	// QA takip denetimi (2026-09-15, HIGH #1): ilk deger (4.0) YETERSIZ kaldi - INT'ye hic
	// yatirim yapmayan bir Lv80 Warrior/Archer MaxMana=375-385 cikiyordu, ama Lv70/80
	// capstone'lar 320-400 mana istiyor - en pahalisi (400) hala atilamiyordu (3 skil puanini
	// harcayip ogrenip SONRA hicbir zaman kullanamama riski). 5.5'e cikarildi: en dusuk INT'li
	// sinifin (Warrior, INT=6) Lv80'de MaxMana'si artik 495 - en pahali skil (400) icin ~95
	// pay birakiyor.
	UPROPERTY(EditDefaultsOnly, Category = "MMO|Tuning")
	float ManaPerLevel = 5.5f;

	UPROPERTY(EditDefaultsOnly, Category = "MMO|Tuning")
	float AttackPerStrength = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "MMO|Tuning")
	float DefensePerVitality = 0.5f;

	// Level başına otomatik verilen stat puanı sayısı.
	UPROPERTY(EditDefaultsOnly, Category = "MMO|Tuning")
	int32 StatPointsPerLevel = 3;

	// Level başına otomatik verilen skill puanı sayısı.
	UPROPERTY(EditDefaultsOnly, Category = "MMO|Tuning")
	int32 SkillPointsPerLevel = 2;

	// Ölünce kaybedilen exp oranı (bir sonraki seviye eşiğine göre).
	UPROPERTY(EditDefaultsOnly, Category = "MMO|Tuning")
	float DeathXPPenaltyPct = 0.1f;

	// Savaş dışı saniyelik HP/MP yenilenmesi (sadece oyuncu).
	UPROPERTY(EditDefaultsOnly, Category = "MMO|Tuning", meta = (ClampMin = "0"))
	float HealthRegenPerSec = 6.f;

	UPROPERTY(EditDefaultsOnly, Category = "MMO|Tuning", meta = (ClampMin = "0"))
	float ManaRegenPerSec = 7.f;

	// Seviye basina ek mana yenilenmesi (Lv80'de +20/sn).
	UPROPERTY(EditDefaultsOnly, Category = "MMO|Tuning", meta = (ClampMin = "0"))
	float ManaRegenPerLevel = 0.25f;

	// Savaştayken yenilenme çarpanı (son hasardan sonra CombatDuration boyunca).
	UPROPERTY(EditDefaultsOnly, Category = "MMO|Tuning")
	float CombatRegenMult = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "MMO|Tuning")
	float CombatDuration = 5.f;

	// ------------------------------------------------------------------
	// Public API — otorite (sunucu) tarafında çağrılır
	// ------------------------------------------------------------------

	/** Bu bileşene hasar uygular. Defense sonrası net hasar döner. Sadece otorite. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Combat")
	float ApplyDamage(float RawDamage, AActor* DamageInstigator);

	/** Can yeniler (clamp'li). Sadece otorite. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Combat")
	void Heal(float Amount);

	/** Mana harcar; yeterliyse true döner. Sadece otorite. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Combat")
	bool ConsumeMana(float Amount);

	/** Mana yeniler (clamp'li). Sadece otorite. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Combat")
	void RestoreMana(float Amount);

	/** Deneyim ekler, gerekiyorsa (birden fazla) level atlatır. Sadece otorite. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Progression")
	void GainExperience(int32 Amount);

	/** Ölüm cezası: mevcut seviyedeki exp'in bir kısmını kaybettirir (level düşmez). Sadece otorite. Kaybedilen exp'i döndürür. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Progression")
	int32 ApplyDeathPenalty();

	/** Serbest stat puanı harcayarak bir ana statı artırır. Sadece otorite. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Progression")
	bool SpendStatPoint(FName StatName, int32 Amount = 1);

	/** Karakteri diriltir (tam can/mana). Sadece otorite. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Combat")
	void Revive();

	/** Ana statlari topluca ayarlar ve turetilmis statlari yeniden hesaplayip can/manayi doldurur (sinif secimi icin). Sadece otorite. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Primary")
	void InitializePrimaryStats(int32 NewStrength, int32 NewDexterity, int32 NewIntelligence, int32 NewVitality);

	/** Skill puani harcar (yeterliyse true). Sadece otorite. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Primary")
	bool SpendSkillPoints(int32 Cost);

	/**
	 * Sinif degisimi (QA MEDIUM-5/6): ana statlari yeni sinifin baslangic dagilimina ceker,
	 * level-up'ta kazanilan tum stat puanlarini ve SpentSkillPoints kadar skill puanini iade eder.
	 * Can/mana DOLDURULMAZ (mevcut deger korunur, sadece yeni maksimuma kirpilir). Sadece otorite.
	 */
	void ResetPointsForClassChange(int32 NewStrength, int32 NewDexterity, int32 NewIntelligence, int32 NewVitality, int32 SpentSkillPoints);

	/** Sunucuda yapilan dusmanca eylemi savas zamanlayicisina isler. */
	void MarkCombatActivity();

	/** Son alinan hasar veya dusmanca eylemden beri CombatDuration gecmediyse true. */
	bool IsInCombat() const;

	// Belirli tipteki tum aktif efektlerin Magnitude toplami (turetilmis stat/hiz hesabi, mob vurusu icin).
	float SumEffectMagnitude(EMMOEffectType InType) const;

	/** Kusanilan ekipmanin toplam bonuslarini ayarlar ve turetilmis statlari tazeler. Sadece otorite. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Vitals")
	void SetEquipmentBonuses(float AttackBonus, float DefenseBonus);

	// ------------------------------------------------------------------
	// Durum efektleri API — otorite (sunucu) tarafinda cagrilir
	// ------------------------------------------------------------------

	/** EffectId'li buff/debuff'i uygular. Ayni efekt varsa suresini tazeler. Sadece otorite. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Effects")
	void ApplyEffect(FName EffectId, AActor* EffectInstigator);

	/** Belirli bir aktif efekti kaldirir. Sadece otorite. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Effects")
	void RemoveEffect(FName EffectId);

	/** Tum aktif efektleri temizler (olum/dirilme). Sadece otorite. */
	UFUNCTION(BlueprintCallable, Category = "MMO|Effects")
	void ClearAllEffects();

	UFUNCTION(BlueprintPure, Category = "MMO|Effects")
	bool HasEffect(FName EffectId) const;

	const TArray<FMMOActiveEffect>& GetActiveEffects() const { return ActiveEffects; }

	// Kalicilik: bu bilesenin verisini kayda yazar / kayittan uygular (sadece otorite).
	void FillSaveData(struct FMMOCharacterSaveData& Out) const;
	void ApplySaveData(const struct FMMOCharacterSaveData& In);

	UFUNCTION(BlueprintPure, Category = "MMO|Vitals")
	float GetHealthPercent() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

	UFUNCTION(BlueprintPure, Category = "MMO|Vitals")
	float GetManaPercent() const { return MaxMana > 0.f ? Mana / MaxMana : 0.f; }

	UFUNCTION(BlueprintPure, Category = "MMO|Progression")
	float GetExperiencePercent() const { return ExperienceToNextLevel > 0 ? (float)Experience / (float)ExperienceToNextLevel : 0.f; }

protected:
	// Ana statlardan türetilmiş statları yeniden hesaplar (MaxHealth/MaxMana/AttackPower/Defense).
	// bRefill true ise anlık can/manayı maksimuma çeker (level-up / spawn anında).
	void RecalculateDerivedStats(bool bRefill, bool bPreserveCurrentVitals = false);

	// Bir sonraki seviye için gereken deneyimi hesaplar.
	int32 ComputeExperienceForLevel(int32 InLevel) const;

	void HandleLevelUp();
	bool HasAuthorityInternal() const;

	// --- Durum efekti isleme (sunucu) ---
	// Aktif efektleri her tick isler: sureleri azaltir, periyodik hasar/sifa uygular, bitenleri kaldirir.
	void ProcessEffects(float DeltaTime);

	// Aktif SpeedBuff efektlerine gore sahibin hareket hizini gunceller (sunucu + istemci).
	void ApplyMovementSpeedFromEffects();

	// Sahibin varsayilan yurume hizi (ilk okundugunda cache'lenir; hiz buff'i buradan olceklenir).
	float BaseWalkSpeed = -1.f;

	// Istemcide OnRep_Health'te hasar farkini hesaplamak icin son bilinen can (replike degil).
	float LastKnownHealth = -1.f;

	// Ekipmandan gelen bonuslar (sunucuda hesaplanir; AttackPower/Defense'e islenir ve onlar replike olur).
	float EquipmentAttackBonus = 0.f;
	float EquipmentDefenseBonus = 0.f;

	// Son alinan hasar veya saldiri zamani (savas ici/disi yenilenme icin; replike degil).
	float LastCombatActivityTime = -100.f;

	// Yenilenme birikimi (replike degil): Health/Mana yalniz >=1 puanlik adimlarla yazilir; her karede
	// kirlenip replike edilmesin (NETWORK_AUDIT Y3). Toplam yenilenme miktari degismez.
	float HealthRegenAccum = 0.f;
	float ManaRegenAccum = 0.f;

	// --- OnRep fonksiyonları: istemcide değer değişince UI delegate'lerini tetikler ---
	UFUNCTION()
	void OnRep_Health();

	UFUNCTION()
	void OnRep_Mana();

	UFUNCTION()
	void OnRep_Experience();

	UFUNCTION()
	void OnRep_Level();

	UFUNCTION()
	void OnRep_PrimaryStats();

	UFUNCTION()
	void OnRep_IsDead();

	UFUNCTION()
	void OnRep_Effects();
};

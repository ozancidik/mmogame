// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MMOCharacter.generated.h"

class UMMOStatsComponent;
class UMMOInventoryComponent;
class UMMOSkillComponent;
class UMMOQuestComponent;
class UMMOPartyComponent;
class UMMOTradeComponent;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MMO")
	TObjectPtr<UMMOPartyComponent> PartyComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MMO")
	TObjectPtr<UMMOTradeComponent> TradeComponent;

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

	UFUNCTION(BlueprintPure, Category = "MMO")
	UMMOPartyComponent* GetPartyComponent() const { return PartyComponent; }

	UFUNCTION(BlueprintPure, Category = "MMO")
	UMMOTradeComponent* GetTradeComponent() const { return TradeComponent; }

	// Sinif degistirme konsol komutu: "MMOClass savasci|okcu|buyucu|rahip"
	// (Ingilizce warrior|archer|mage|priest de calisir).
	UFUNCTION(Exec)
	void MMOClass(const FString& ClassName);

	// Yeni karakter sinif secimi ipucu (HUD paneli). Sunucu kayitsiz (yeni) karakterde set
	// eder, sinif degisince temizler; KAYDA YAZILMAZ. Sadece sahibine replike.
	UPROPERTY(Replicated)
	bool bClassChoicePending = false;

	bool ShouldShowClassHint() const { return bClassChoicePending; }

	// --- Kalicilik (kaydet/yukle) ---
	UFUNCTION(Exec)
	void MMOSave();

	UFUNCTION(Exec)
	void MMOLoad();

	// Kayit dosyasi adi PlayerState'in adina dayanir (GetSaveSlotName). Bu proje
	// login/kimlik sistemi kullanmiyor: motor varsayilaninda oyuncu adi baglanti
	// sirasina gore "Player0"/"Player1".. olur - AYNI ISIMDIR farkli gecelerde farkli
	// kisilerin ayni kayit dosyasini paylasip birbirinin karakterini EZMESI riski var
	// (host her zaman Player0 oldugu icin guvenli, ama misafirler degil). Kalici bir
	// kimlik sistemi olmadan tam duzeltilemez, ama oyuncuya EN AZINDAN kendi adini
	// sabitleyecek bir arac ver - MMOSave/MMOLoad'dan ONCE cagrilmali.
	UFUNCTION(Exec)
	void MMOSetName(const FString& NewName);

	// Karakteri diske yazar / diskten yukler (sadece otorite; istemciden Exec cagrilirsa sunucuya yonlenir).
	void SaveCharacter();
	// Harita degisimi oncesi zorunlu kayit: yukleme bitmediyse veya disk yazimi
	// basarisizsa false doner; GameMode bu durumda seyahati iptal eder.
	bool SaveCharacterForTravel();
	void SetTravelSaveCommitted(bool bCommitted);
	bool LoadCharacter();
	FString GetSaveSlotName() const;

	// Olum sonrasi yeniden dogusa kalan sure (HUD geri sayimi icin; olu degilse 0).
	UFUNCTION(BlueprintPure, Category = "MMO")
	float GetRespawnTimeRemaining() const;

	// Olumden kac saniye sonra yeniden dogulur (sadece oyuncu).
	UPROPERTY(EditAnywhere, Category = "MMO|Combat")
	float RespawnDelay = 5.f;

	// --- Canlanma korumasi (respawn protection) ---
	// Oyuncu canlandiktan / oyuna girdikten (possess) sonra kac saniye hasar almaz ve
	// moblar onu hedef almaz. 0 = koruma yok. Oyuncu saldirirsa koruma erken biter.
	UPROPERTY(EditDefaultsOnly, Category = "MMO|Combat", meta = (ClampMin = "0.0"))
	float SpawnProtectionDuration = 4.f;

	// Koruma aktif mi (sunucu yazar, istemcilere replike olur; HUD 'KORUMA' etiketi bunu okur).
	UPROPERTY(ReplicatedUsing = OnRep_SpawnProtected, BlueprintReadOnly, Category = "MMO|Combat")
	bool bSpawnProtected = false;

	UFUNCTION(BlueprintPure, Category = "MMO|Combat")
	bool IsSpawnProtected() const { return bSpawnProtected; }

	// Korumayi hemen bitirir (sure doldu ya da oyuncu saldirdi). Sadece otoritede etki eder.
	void EndSpawnProtection();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- Saldiri animasyonlari (KO'daki gibi: her vurus/skill bir animasyon oynatir) ---
	// 0..2 = temel vurus varyasyonlari, 3 = buyuk vurus / buyu okuma (charged).
	// Constructor'da Mannequin Attack animasyonlariyla doldurulur; BP'den degistirilebilir.
	UPROPERTY(EditDefaultsOnly, Category = "MMO|Anim")
	TArray<TObjectPtr<class UAnimSequenceBase>> AttackAnims;

	// Buyuk vurus / buyu okuma animasyonunun AttackAnims icindeki indeksi.
	static constexpr int32 ChargedAnimIndex = 3;

	/** Verilen indeksteki saldiri animasyonunu BU makinede oynatir (ABP'deki "DefaultSlot" uzerinden). */
	void PlayAttackAnimLocal(int32 AnimIndex);

	// --- Kamera ayarlari (BP'deki Camera Boom uzerine runtime'da uygulanir) ---
	// Karaktere uzaklik (spring arm boyu). Buyudukce daha uzaktan gorunur.
	UPROPERTY(EditAnywhere, Category = "MMO|Camera")
	float CameraDistance = 850.f;

	// Baslangic kamera egimi (derece; negatif = yukaridan/tepeden bakis). Oyuncu suruklemeyle degistirebilir.
	UPROPERTY(EditAnywhere, Category = "MMO|Camera")
	float CameraStartPitch = -45.f;

	// Spring arm boyunu + baslangic bakis acisini uygular.
	void ApplyCameraSettings();

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

	// --- Parti (grup) konsol komutlari ---
	UFUNCTION(Exec)
	void MMOInvite();        // en yakin oyuncuyu partiye davet et

	UFUNCTION(Exec)
	void MMOPartyAccept();   // gelen daveti kabul et

	UFUNCTION(Exec)
	void MMOPartyLeave();    // partiden ayril

	UFUNCTION(Exec)
	void MMOPartyKick();     // secili parti uyesini partiden cikar (sadece lider)

	// --- Oyuncular arasi ticaret konsol komutlari ---
	UFUNCTION(Exec)
	void MMOTrade();         // en yakin oyuncuya ticaret daveti gonder

	UFUNCTION(Exec)
	void MMOTradeAccept();   // gelen ticaret davetini kabul et

	UFUNCTION(Exec)
	void MMOTradeCancel();   // ticareti iptal et

	UFUNCTION(Exec)
	void MMOTradeGold(int32 Amount);   // teklifteki altini ayarla

	UFUNCTION(Exec)
	void MMOTradeConfirm();  // teklifi onayla

	// Bir ana stata 1 puan harcar (stat penceresi / konsol). Istemciden -> Server RPC.
	// StatName: "STR"/"DEX"/"INT"/"VIT" veya tam ad.
	UFUNCTION(BlueprintCallable, Category = "MMO|Progression")
	void RequestSpendStat(FName StatName);

	// Konsol: "MMOStat str" gibi.
	UFUNCTION(Exec)
	void MMOStat(const FString& StatName);

	// Konsol: "MMONpcTalk" — LLM'den ornek bir NPC replik ister, ekrana/loga basar.
	// (Oyun ici LLM entegrasyonu testi. NVIDIA_API_KEY/ANTHROPIC_API_KEY tanimli olmali.)
	// NOT: bu Exec komutu client-side calisir (RPC degil, HasAuthority kontrolu yok) —
	// spam'i onlemek icin kendi cooldown'unu tutar (bkz. LastNpcTalkDebugTime).
	UFUNCTION(Exec)
	void MMONpcTalk();

	// Sunucu mob XP'si verdiginde sahibi istemciye bildirir: karakterin ustunde "+N EXP"
	// ucar; Percent < 100 ise seviye farki indirimi "(%P)" olarak eklenir. Yalniz gorsel.
	UFUNCTION(Client, Unreliable)
	void ClientNotifyXPGain(int32 Amount, int32 Percent);

protected:
	// "MMONpcTalk" debug komutunun spam korumasi (client-side, RPC olmadigi icin
	// sunucu-tarafi mesafe/cooldown guard'lari devreye girmiyor — bkz. MMONpcTalk).
	float LastNpcTalkDebugTime = -100.f;
	static constexpr float NpcTalkDebugCooldown = 2.f;

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
	void RetrySavedLocationRestore();
	bool TryRestoreSavedLocation(const FVector& SavedLocation, const FRotator& SavedRotation, bool& bNavPending);
	void MoveToPlayerStart();

	UFUNCTION(Server, Reliable)
	void ServerSaveCharacter();

	UFUNCTION(Server, Reliable)
	void ServerLoadCharacter();

	UFUNCTION(Server, Reliable)
	void ServerSetName(const FString& NewName);

	FTimerHandle AutoLoadTimerHandle;
	FTimerHandle AutoSaveTimerHandle;
	FTimerHandle SavedLocationRetryTimerHandle;
	FVector PendingSavedLocation = FVector::ZeroVector;
	FRotator PendingSavedRotation = FRotator::ZeroRotator;
	int32 SavedLocationRetryCount = 0;
	static constexpr int32 MaxSavedLocationRetries = 15;

	// Yalniz kayit yoksa veya var olan kayit basariyla okunduysa true olur.
	// Basarisiz/bozuk yuklemede otomatik, manuel ve EndPlay kaydi mevcut dosyayi
	// varsayilan karakter verisiyle ezmemelidir.
	bool bInitialLoadComplete = false;
	bool bTravelSaveCommitted = false;
	float LastRemoteManualSaveTime = -100.f;
	static constexpr float RemoteManualSaveCooldown = 10.f;

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

	UFUNCTION(Server, Reliable)
	void ServerSpendStat(FName StatName);

	// Yakin dovus vurus efektini + saldiri animasyonunu tum makinelerde oynat.
	// AnimIndex sunucuda secilir ki herkes ayni varyasyonu gorsun.
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastMeleeFX(FVector ImpactLocation, int32 AnimIndex);

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

	// Canlanma korumasini SpawnProtectionDuration saniyeligine baslatir (sadece otorite).
	void StartSpawnProtection();

	UFUNCTION()
	void OnRep_SpawnProtected();

	FTimerHandle SpawnProtectionTimerHandle;

	// Blueprint'te ölüm animasyonu/efekti için override edilebilir olay.
	UFUNCTION(BlueprintImplementableEvent, Category = "MMO")
	void OnCharacterDied();
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MMOGameMode.generated.h"

/**
 * MMO oyun modu. �imdilik tek g�revi HUD s�n�f�n� (AMMOHUD) devreye almak.
 * BP_ThirdPersonGameMode bu s�n�fa reparent edilir; ileride respawn / oyun kurallar� buraya eklenir.
 */
UCLASS()
class MYPROJECT_API AMMOGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMMOGameMode();

	virtual void StartPlay() override;

	// Y-5 (SECURITY_AUDIT): baslangic adi "<isletim sistemi kullanici adi>-<hex>" kalibindaysa (Null OSS
	// varsayilani) herkese gorunmesin diye rastgele "Gezgin######" ile degistirilir. Kasitli secilen adlar
	// (?Name=Ozan, MMOSetName) ve sonraki ad degisiklikleri (bNameChange) etkilenmez.
	virtual void ChangeName(AController* Controller, const FString& NewName, bool bNameChange) override;

	// Kimlik korumasi (SECURITY_AUDIT K-1, kismi): baglanti URL'indeki ?Name= degeri su an
	// cevrimici bir oyuncunun adiyla (kayit yuvasi esdegeri: buyuk/kucuk harf ve bosluk/_ ayni)
	// cakisiyorsa baglanti reddedilir. Cevrimdisi birinin adini kapmayi ENGELLEMEZ (hesap sistemi gerekir).
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	// Gelistirici araci: spawner dogrulamasini (zemin izi + navmesh) verilen konumda 64 kez calistirir ve
	// sonucu "[MMOPROBE]" satiri olarak loglar. Smoke test (Scripts/SmokeTest.ps1 -Probe) kullanir.
	// Konsol: MMOProbeSpawn X Y Z [Radius]. Yalniz sunucuda anlamlidir; oyunu etkilemez.
	// Shipping derlemesinde govdeleri bos (UHT, UFUNCTION'i #if icinde kabul etmez).
	UFUNCTION(Exec)
	void MMOProbeSpawn(float X, float Y, float Z, float Radius = 225.f);

	// Z'yi kendisi bulur (yukaridan asagi ilk yuzey + 50) ve MMOProbeSpawn calistirir. Grid taramasi icin.
	UFUNCTION(Exec)
	void MMOProbeSurface(float X, float Y, float Radius = 225.f);

	// Headless otomasyon: ilk karakter yuklemesi bittikten sonra gercek bolge gecisini tetikler.
	UFUNCTION(Exec)
	void MMOTestZoneTravel();

	// Headless otomasyon: bir kayit callback'ini dusurup gecisin timeout ile acilmasini dogrular.
	UFUNCTION(Exec)
	void MMOTestZoneTravelTimeout();

	// Kayit onayi beklenirken savasa giren oyuncunun gecisinin iptalini headless dogrular.
	UFUNCTION(Exec)
	void MMOTestZoneTravelStateChange();

	// Ertelenen ek kayit hatasinda gecis iptali ve sonraki basarili kayitta isaret temizligi.
	UFUNCTION(Exec)
	void MMOTestZoneTravelDeferredSaveFailure();

	// Listen server hostunun tum bagli oyunculari Summer/Winter arasinda tasimasi.
	// Her karakter basariyla kaydedilmeden harita degismez.
	bool RequestZoneTravel(APlayerController* Requester, FString& OutFailureReason);

	// Bu karakterin bolge gecisi kaydi su an UCUSTA mi (istek verildi, onay callback'i gelmedi)? Ucustayken
	// ayni yuvaya yapilan baska bir yazim (otomatik kayit, klan/ticaret) gecis yazimini "eski" yapip gecisi
	// iptal ettirirdi: AMMOCharacter::SaveCharacterForTravel bu durumda yazimi ERTELER, gecis bitince
	// (basari ya da iptal) tek bir yazimda birlestirilip yapilir. Yerel (senkron) magazada pencere yoktur.
	bool IsZoneTravelSaveInFlight(const class AMMOCharacter* Character) const;

	// PvP acik mi? Acikken oyuncular birbirine vurabilir (sunucuda dogrulanir). Varsayilan KAPALI:
	// tikla-saldir oyuncu hedefini AttackTarget'a gecirmiyor, PvP bilincli acilana kadar kapali kalir.
	// Host konsolundan "MMOPvP 1" ile acilir.
	// BlueprintReadOnly: yalniz C++ SetPvPAllowed yazar (BP dogrudan yazarsa replike ayna bayatlardi).
	// Content taramasi (ham bayt): hicbir Blueprint/harita bu alani kullanmiyor.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MMO|GameMode")
	bool bAllowPvP = false;

	// PvP anahtarini degistirmenin TEK yolu: bAllowPvP'yi (tek kaynak) yazar ve replike aynayi
	// (AMMOGameState::bPvPEnabled) gunceller. Dogrudan bAllowPvP yazmayin; istemci durumu bilemez.
	void SetPvPAllowed(bool bOn);

	// GameState olustugunda bAllowPvP'nin baslangic degerini aynaya yansitir (CDO/BP varsayilani dahil).
	virtual void InitGameState() override;

protected:
	bool bZoneTravelInProgress = false;
	uint64 ZoneTravelRequestSerial = 0;
	int32 PendingZoneTravelSaves = 0;
	// Kaydi TAMAMLANAN karakterler: ayni karakterin cift callback'i (bulut tekrar-deneme) bariyeri erken acmasin.
	TSet<TWeakObjectPtr<class AMMOCharacter>> CompletedZoneTravelSaves;
	// Son iptalin nedeni: RequestZoneTravel ayni karede iptal olursa cagirana false + bu neden doner.
	FString LastZoneTravelAbortReason;
	FString PendingZoneTravelTargetMap;
	FString PendingZoneTravelSourceMap;
	TArray<TWeakObjectPtr<class AMMOCharacter>> PendingZoneTravelCharacters;
	FTimerHandle ZoneTravelSaveTimeoutHandle;
	static constexpr float ZoneTravelSaveTimeoutSeconds = 15.f;
	bool bTestDropNextZoneTravelSaveCallback = false;
	bool bTestRetryAfterZoneTravelTimeout = false;
	float TestZoneTravelTimeoutOverrideSeconds = 0.f;

	void ScheduleZoneTravelTest(bool bExpectTimeout, bool bExpectStateChange = false, bool bExpectFlushFailure = false);
	void HandleZoneTravelSaveComplete(uint64 RequestSerial, const TWeakObjectPtr<class AMMOCharacter>& Character, bool bSuccess);
	void ClearZoneTravelTestHooks();
	void HandleZoneTravelSaveTimeout(uint64 RequestSerial);
	void FinishZoneTravel(uint64 RequestSerial);
	void AbortZoneTravel(uint64 RequestSerial, const FString& Reason);
	// Gecis penceresinde ertelenen kayitlari (bkz. IsZoneTravelSaveInFlight) birlestirip yazar.
	bool FlushDeferredCharacterSaves(const TArray<TWeakObjectPtr<class AMMOCharacter>>& Characters);
	// Oyun basladiginda oyuncu dogus noktasinin yakinina bir mob spawner yerlestirir.
	void SpawnMobArea();

	// Spawner'in oyuncu dogus noktasindan ne kadar onune konacagi.
	UPROPERTY(EditDefaultsOnly, Category = "MMO|GameMode")
	float MobAreaDistance = 700.f;
};

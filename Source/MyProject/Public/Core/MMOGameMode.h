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

	// Listen server hostunun tum bagli oyunculari Summer/Winter arasinda tasimasi.
	// Her karakter basariyla kaydedilmeden harita degismez.
	bool RequestZoneTravel(APlayerController* Requester, FString& OutFailureReason);

	// PvP acik mi? Acikken oyuncular birbirine vurabilir (sunucuda dogrulanir). Varsayilan KAPALI:
	// tikla-saldir oyuncu hedefini AttackTarget'a gecirmiyor, PvP bilincli acilana kadar kapali kalir.
	// Host konsolundan "MMOPvP 1" ile acilir.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "MMO|GameMode")
	bool bAllowPvP = false;

protected:
	bool bZoneTravelInProgress = false;
	// Oyun basladiginda oyuncu dogus noktasinin yakinina bir mob spawner yerlestirir.
	void SpawnMobArea();

	// Spawner'in oyuncu dogus noktasindan ne kadar onune konacagi.
	UPROPERTY(EditDefaultsOnly, Category = "MMO|GameMode")
	float MobAreaDistance = 700.f;
};

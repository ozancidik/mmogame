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

	// PvP acik mi? Acikken oyuncular birbirine vurabilir (sunucuda dogrulanir). Test icin varsayilan acik.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "MMO|GameMode")
	bool bAllowPvP = true;

protected:
	bool bZoneTravelInProgress = false;
	// Oyun basladiginda oyuncu dogus noktasinin yakinina bir mob spawner yerlestirir.
	void SpawnMobArea();

	// Spawner'in oyuncu dogus noktasindan ne kadar onune konacagi.
	UPROPERTY(EditDefaultsOnly, Category = "MMO|GameMode")
	float MobAreaDistance = 700.f;
};

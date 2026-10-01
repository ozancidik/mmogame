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

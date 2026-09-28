// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MMOMobSpawner.generated.h"

class AMMOMob;

/**
 * Belirli sayida mob'u alanda tutar. Sunucuda basladiginda MaxMobs kadar spawn eder ve
 * periyodik "top-up" ile olen/yok olan moblarin yerine yenisini uretir (respawn).
 */
UCLASS()
class MYPROJECT_API AMMOMobSpawner : public AActor
{
	GENERATED_BODY()

public:
	AMMOMobSpawner();

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Spawner")
	TSubclassOf<AMMOMob> MobClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Spawner")
	int32 MaxMobs = 3;

	// Bos ise rastgele arketip; doldurulursa tum moblar bu arketiple spawn olur (or. world boss).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Spawner")
	FName ForcedArchetypeId;

	// Opsiyonel seviye-bandi filtresi (yalniz ForcedArchetypeId bosken rastgele secimde etkili).
	// 0 = o yonde sinir yok (varsayilan; mevcut davranis aynen korunur). Ikisinden biri sifir degilse
	// GetRandomInLevelRange kullanilir.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Spawner")
	int32 MinMobLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Spawner")
	int32 MaxMobLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Spawner")
	float SpawnRadius = 500.f;

	// Yalniz acildiginda rastgele noktayi yakin zemin ve navmesh uzerinde dogrular.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Spawner")
	bool bValidateSpawnOnNavGround = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Spawner")
	float TopUpInterval = 4.f;

protected:
	UPROPERTY()
	TArray<TWeakObjectPtr<AMMOMob>> LiveMobs;

	FTimerHandle TopUpTimer;
	bool bInvalidBandReported = false;
	bool bSpawnLocationWarningReported = false;

	void TopUp();
	void SpawnOne();
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/MMOItemTypes.h"
#include "MMOLootPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;

/**
 * Yerde duran, donen/ziplayan toplanabilir loot. Altin veya item olabilir.
 * Oyuncu (player-controlled MMOCharacter) uzerine gelince otorite tarafinda envanterine eklenir ve yok olur.
 * Spawn'dan once SetupAsGold / SetupAsItem ile yapilandirilir (SpawnActorDeferred).
 */
UCLASS()
class MYPROJECT_API AMMOLootPickup : public AActor
{
	GENERATED_BODY()

public:
	AMMOLootPickup();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Loot")
	bool bIsGold = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Loot")
	int32 GoldAmount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Loot")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Loot")
	int32 ItemCount = 1;

	// Kendini yok etmeden once yerde kalma suresi (saniye).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Loot")
	float LifeSpanSeconds = 30.f;

	// Ekipman lootu (silah/zirh...): toplayinca ekipman cantasina girer.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Loot")
	bool bIsEquipment = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Loot")
	FName EquipItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Loot")
	int32 EquipPlus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Loot")
	EMMOItemRarity EquipRarity = EMMOItemRarity::Common;

	void SetupAsGold(int32 Amount);
	void SetupAsItem(FName InItemId, int32 Count);
	void SetupAsEquipment(FName InItemId, int32 InPlus, EMMOItemRarity InRarity = EMMOItemRarity::Common);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "MMO|Loot")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "MMO|Loot")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// Tur/renge gore gorseli ayarlar (BeginPlay'de cagrilir).
	void ApplyVisual();

	float BobTime = 0.f;
};

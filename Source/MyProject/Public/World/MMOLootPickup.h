// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Actor.h"
#include "Items/MMOItemTypes.h"
#include "MMOLootPickup.generated.h"

class AMMOCharacter;
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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_LootVisual, Category = "MMO|Loot")
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_LootVisual, Category = "MMO|Loot")
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

	// Ganimet rezervasyonu (yalniz sunucu): Seconds boyunca yalniz Owners toplayabilir; sure dolunca herkes.
	// Owners bos ise rezervasyon yok. Spawner'dan FinishSpawning'den ONCE cagrilmali (BeginPlay zamanlayiciyi kurar).
	void SetReserved(const TArray<AMMOCharacter*>& Owners, float Seconds);

#if !UE_BUILD_SHIPPING
	static bool RunLifeStateSelfTest(UWorld* World, FString& OutError, int32& OutChecks);
	static bool RunFirstLootSelfTest(UWorld* World, FString& OutError, int32& OutChecks);
#endif

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
	UFUNCTION()
	void OnRep_LootVisual();

	float BobTime = 0.f;

	// Tek-sefer toplama guard'i: Destroy() ayni frame'de senkron olmadigindan, ayni
	// frame'de gelen ikinci bir overlap (iki oyuncu veya cok-govde) lootu DUPE edebilir.
	// grant'tan ONCE set edilir; ikinci overlap erken doner. (Sunucu-tarafi.)
	bool bCollected = false;

	// Rezervasyon durumu (yalniz sunucu, replike degil).
	TArray<TWeakObjectPtr<AMMOCharacter>> ReservedFor;
	float ReservedSeconds = 0.f;
	double ReservedUntilTime = 0.0;
	FTimerHandle ReservationTimer;

	// "Canta dolu" bildirimi spam onlemi (yalniz sunucu): oyuncu pickup'in uzerinde gidip gelirken her
	// overlap'te mesaj yagmasin; ayni oyuncuya bu pickup'tan en cok BagFullNotifyInterval saniyede bir.
	TMap<TWeakObjectPtr<AMMOCharacter>, double> LastBagFullNotifyTime;
	static constexpr double BagFullNotifyInterval = 3.0;

	// OnOverlap ve rezervasyon bitiminde ortadaki oyuncular icin ortak toplama mantigi.
	void TryCollect(AMMOCharacter* Char);
	void OnReservationExpired();
};

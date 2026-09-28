// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MMOItemTypes.generated.h"

/** Ekipman slotu. */
UENUM(BlueprintType)
enum class EMMOEquipSlot : uint8
{
	Weapon UMETA(DisplayName = "Silah"),
	Armor  UMETA(DisplayName = "Zirh"),
	Helmet UMETA(DisplayName = "Kask"),
	Boots  UMETA(DisplayName = "Bot"),
	MAX    UMETA(Hidden)
};

/** Esya nadirligi (kademe). Stat carpani + isim rengi bunlardan turer. */
UENUM(BlueprintType)
enum class EMMOItemRarity : uint8
{
	Common    UMETA(DisplayName = "Siradan"),
	Uncommon  UMETA(DisplayName = "Nadir"),
	Rare      UMETA(DisplayName = "Ender"),
	Epic      UMETA(DisplayName = "Destansi"),
	Legendary UMETA(DisplayName = "Efsanevi"),
	MAX       UMETA(Hidden)
};

/** Bir ekipman turunun tanimi (KO'nun ITEM tablosunun karsiligi). */
USTRUCT(BlueprintType)
struct FMMOItemDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Item")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Item")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Item")
	EMMOEquipSlot Slot = EMMOEquipSlot::Weapon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Item")
	float BaseAttack = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Item")
	float BaseDefense = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Item")
	int32 RequiredLevel = 1;

	bool IsValidDef() const { return !ItemId.IsNone(); }
};

/** Oyuncunun sahip oldugu somut bir ekipman ornegi (upgrade seviyesiyle). */
USTRUCT(BlueprintType)
struct FMMOEquipmentInstance
{
	GENERATED_BODY()

	// Oyuncu icinde benzersiz kimlik (0 = bos/yok).
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "MMO|Item")
	int32 InstanceId = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "MMO|Item")
	FName ItemId;

	// Upgrade seviyesi: +0 .. +8.
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "MMO|Item")
	int32 Plus = 0;

	// Nadirlik kademesi (stat carpani + isim rengi). Kayitta saklanir.
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "MMO|Item")
	EMMOItemRarity Rarity = EMMOItemRarity::Common;

	bool IsValidInstance() const { return InstanceId != 0 && !ItemId.IsNone(); }
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MMOConsumableTypes.generated.h"

/**
 * Tuketilebilir bir esyanin tanimi (mana iksiri, buff parsomeni...).
 * Envanterde normal item yigini olarak (ItemId + adet) tutulur; kullanilinca
 * bu tanimdaki aninda HP/MP yenileme ve/veya bir buff efekti uygulanir.
 */
USTRUCT(BlueprintType)
struct FMMOConsumableDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Consumable")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Consumable")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Consumable")
	int32 Price = 10;

	// Aninda yenilenen HP (0 = yok).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Consumable")
	float Heal = 0.f;

	// Aninda yenilenen MP (0 = yok).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Consumable")
	float ManaRestore = 0.f;

	// Kullaninca uygulanacak buff/debuff (MMOEffectDatabase EffectId; bos = yok).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Consumable")
	FName EffectId;

	bool IsValidDef() const { return !ItemId.IsNone(); }
};

/** Tum tuketilebilirlerin statik veritabani. */
class MYPROJECT_API FMMOConsumableDatabase
{
public:
	static const TArray<FMMOConsumableDef>& GetAll();
	static const FMMOConsumableDef* Find(FName ItemId);
};

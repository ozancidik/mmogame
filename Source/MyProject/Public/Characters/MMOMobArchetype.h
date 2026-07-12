// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MMOMobArchetype.generated.h"

/** Bir mob turunun tanimi (Goblin, Ork, Trol...). Seviye + stat + odul + gorsel. */
USTRUCT(BlueprintType)
struct FMMOMobArchetype
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	FName ArchetypeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	int32 Level = 1;

	// Ana statlar (MaxHealth/AttackPower bunlardan turer).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	int32 Strength = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	int32 Vitality = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	float AttackDamage = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	int32 XPReward = 40;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	int32 MinGold = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	int32 MaxGold = 25;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	float MoveSpeed = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	float AggroRange = 900.f;

	// Gorsel: govde olcegi ve renk.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	float Scale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob")
	FLinearColor Tint = FLinearColor(0.7f, 0.08f, 0.08f);

	bool IsValidArchetype() const { return !ArchetypeId.IsNone(); }
};

/** Tum mob arketiplerinin merkezi kayit defteri. */
class MYPROJECT_API FMMOMobDatabase
{
public:
	static const TArray<FMMOMobArchetype>& GetAll();
	static const FMMOMobArchetype* Find(FName ArchetypeId);
	static const FMMOMobArchetype& GetRandom();
};

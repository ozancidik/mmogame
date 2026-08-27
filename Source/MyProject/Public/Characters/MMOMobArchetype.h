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

	// --- Yetenekler (buff/debuff sistemi) ---
	// Elit/boss mu? (daha guclu; ozel muamele/gorsel vurgu icin).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob|Ability")
	bool bIsElite = false;

	// Vurusta oyuncuya uygulanabilecek debuff (MMOEffectDatabase EffectId; bos = yok).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob|Ability")
	FName OnHitEffectId;

	// Vurusta OnHitEffectId'nin uygulanma sansi (0..1).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob|Ability")
	float OnHitEffectChance = 0.f;

	// Can bu orana (0..1) dusunce mob kendine bir defa bu buff'i uygular (or. Rage). 0 = enrage yok.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob|Ability")
	FName EnrageEffectId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob|Ability")
	float EnrageHealthPct = 0.f;

	// --- Elit ganimet (yalnizca bIsElite icin anlamli; hepsi default => normal loot davranisi DEGISMEZ) ---
	// >0 ise elitin ekipman dusurme sansi (1.0 = garantili). 0 = kullanilmaz (normal EquipDropChance gecerli).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob|Loot")
	float EliteEquipDropChance = 0.f;

	// Elit kac ekipman dusurur (boss icin 2 vb.). En az 1 uygulanir.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob|Loot")
	int32 EliteEquipDropCount = 1;

	// Elit dropunun +N araligi (yuksek plus daha degerli). Default 0..0 = +0.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob|Loot")
	int32 EliteMinItemPlus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Mob|Loot")
	int32 EliteMaxItemPlus = 0;

	bool IsValidArchetype() const { return !ArchetypeId.IsNone(); }
};

/** Tum mob arketiplerinin merkezi kayit defteri. */
class MYPROJECT_API FMMOMobDatabase
{
public:
	static const TArray<FMMOMobArchetype>& GetAll();
	static const FMMOMobArchetype* Find(FName ArchetypeId);
	static const FMMOMobArchetype& GetRandom();

	// Seviye bandina (MinLv..MaxLv, dahil) uyan non-elit moblardan rastgele sec.
	// 0 = o yonde sinir yok. Banda uyan mob yoksa GUVENLE GetRandom()'a duser (crash yok).
	static const FMMOMobArchetype& GetRandomInLevelRange(int32 MinLv, int32 MaxLv);
};

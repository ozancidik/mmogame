// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MMOSkillTypes.generated.h"

/** Karakter sinifi (KO tarzi). */
UENUM(BlueprintType)
enum class EMMOClass : uint8
{
	Warrior UMETA(DisplayName = "Savasci"),
	Archer  UMETA(DisplayName = "Okcu"),
	Mage    UMETA(DisplayName = "Buyucu"),
	Priest  UMETA(DisplayName = "Priest")
};

/** Skill agaci mastery kategorisi (KO: BASIC + SPECIAL0..3). */
UENUM(BlueprintType)
enum class EMMOSkillTree : uint8
{
	Basic    UMETA(DisplayName = "Temel"),
	Special0 UMETA(DisplayName = "Ustalik 1"),
	Special1 UMETA(DisplayName = "Ustalik 2"),
	Special2 UMETA(DisplayName = "Ustalik 3"),
	Special3 UMETA(DisplayName = "Ustalik 4")
};

/** Bir yetenegin etkisi. */
UENUM(BlueprintType)
enum class EMMOSkillEffect : uint8
{
	MeleeDamage  UMETA(DisplayName = "Yakin Hasar"),
	RangedDamage UMETA(DisplayName = "Menzilli Hasar"),
	AoESelf      UMETA(DisplayName = "Etrafina Alan Hasar"),
	AoETarget    UMETA(DisplayName = "Hedefte Alan Hasar"),
	HealSelf     UMETA(DisplayName = "Kendini Iyilestir"),
	BuffSelf     UMETA(DisplayName = "Kendine Buff")        // AppliedEffectId'yi kastere uygular (hasar yok)
};

/**
 * Tek bir skill tanimi. KO'daki __TABLE_UPC_SKILL + type tablolarinin sadelestirilmis karsiligi.
 * FTableRowBase'den turedigi icin ileride bir UE DataTable'a tasinabilir.
 */
USTRUCT(BlueprintType)
struct FMMOSkillDef : public FTableRowBase
{
	GENERATED_BODY()

	// --- Kimlik / agac ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Skill")
	FName SkillId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Skill")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Skill")
	EMMOClass RequiredClass = EMMOClass::Warrior;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Skill")
	EMMOSkillTree Tree = EMMOSkillTree::Basic;

	// --- Ogrenme kosullari ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Skill")
	int32 RequiredLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Skill")
	int32 SkillPointCost = 1;

	// Ogrenmek icin once ogrenilmesi gereken skill (bos = yok).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Skill")
	FName PrereqSkillId;

	// --- Kullanim / efekt ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Skill")
	EMMOSkillEffect Effect = EMMOSkillEffect::MeleeDamage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Skill")
	float ManaCost = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Skill")
	float Cooldown = 1.f;

	// Hasar veya iyilestirme miktari (duz deger).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Skill")
	float Power = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Skill")
	float Range = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Skill")
	float Radius = 0.f;

	// Bu skill kullaninca uygulanacak buff/debuff (MMOEffectDatabase'deki EffectId).
	// BuffSelf'te kastere; hasar skillerinde vurulan hedefe/hedeflere uygulanir. Bos = efekt yok.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Skill")
	FName AppliedEffectId;

	// Kullaninca oynatilacak animasyon (AMMOCharacter::AttackAnims indeksi; KO'nun skill tablosundaki
	// animasyon id'sinin karsiligi). -1 = otomatik: yakin dovus -> vurus, digerleri -> buyu okuma.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Skill")
	int32 AnimIndex = -1;

	bool IsValidDef() const { return !SkillId.IsNone(); }
};

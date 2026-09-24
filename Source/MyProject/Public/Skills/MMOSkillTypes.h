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
	Priest  UMETA(DisplayName = "Rahip")
};

/** Sinifin oyuncuya gosterilen Turkce adi — HUD ve ekran mesajlari tek yerden okur. */
inline FString GetMMOClassDisplayName(EMMOClass Class)
{
	switch (Class)
	{
	case EMMOClass::Warrior: return TEXT("Savasci");
	case EMMOClass::Archer:  return TEXT("Okcu");
	case EMMOClass::Mage:    return TEXT("Buyucu");
	case EMMOClass::Priest:  return TEXT("Rahip");
	}
	return TEXT("?");
}

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
	BuffSelf     UMETA(DisplayName = "Kendine Buff"),       // AppliedEffectId'yi kastere uygular (hasar yok)
	// Secili hedef canli bir MUTTEFIK (baska oyuncu, mob DEGIL) ise ONU iyilestirir;
	// hedef yoksa/gecersizse kendini iyilestirir (bug-hunt: Priest'in TUM iyilestirme
	// skilleri HealSelf idi - parti icinde BASKA kimseyi iyilestirmenin hicbir yolu
	// yoktu, "sifaci" sinifinin asil islevi eksikti). Hedef gerektirmedigi icin
	// (self-fallback'i sayesinde) UseSkill'in bNeedsTarget kontrolune eklenmedi.
	HealTarget   UMETA(DisplayName = "Hedefi Iyilestir (yoksa kendini)")
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

	// KO mastery rutbesi: bu skilin acilmasi icin karakterin kendi Tree'sinde sahip olmasi
	// gereken rutbe (KO: m_bstrSkill[modulator] >= skill.bSkillLevel). Skill puani tek tek
	// skillere degil, agacin rutbesine yatirilir; rutbe yukseldikce o agactaki skiller acilir.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Skill")
	int32 RequiredMasteryRank = 1;

	// ESKI sistemin prereq zinciri. Artik gating'de KULLANILMIYOR (yerini RequiredMasteryRank aldi);
	// eski kayit verisi/geri uyumluluk ve tasarim referansi icin duruyor.
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

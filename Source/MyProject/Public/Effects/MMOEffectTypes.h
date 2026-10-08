// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MMOEffectTypes.generated.h"

/** Buff/Debuff (durum efekti) turu. */
UENUM(BlueprintType)
enum class EMMOEffectType : uint8
{
	PeriodicDamage UMETA(DisplayName = "Zamanla Hasar"),   // DoT: her tick Magnitude hasar (zehir/kanama)
	PeriodicHeal   UMETA(DisplayName = "Zamanla Iyilesme"), // HoT: her tick Magnitude iyilesme
	AttackBuff     UMETA(DisplayName = "Saldiri Gucu"),      // Sure boyunca AttackPower += Magnitude
	DefenseBuff    UMETA(DisplayName = "Savunma"),           // Sure boyunca Defense += Magnitude
	SpeedBuff      UMETA(DisplayName = "Hiz")                // Hareket hizi %Magnitude (negatif = yavaslatma)
};

/**
 * Bir durum efektinin tanimi (buff veya debuff). KO'daki magic/skill "duration" tipi efektlerin
 * sadelestirilmis karsiligi. FTableRowBase'den turedigi icin ileride DataTable'a tasinabilir.
 */
USTRUCT(BlueprintType)
struct FMMOEffectDef : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Effect")
	FName EffectId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Effect")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Effect")
	EMMOEffectType Type = EMMOEffectType::AttackBuff;

	// Periyodik efektlerde tick basina miktar; stat buff'larda duz bonus; SpeedBuff'ta yuzde.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Effect")
	float Magnitude = 0.f;

	// Toplam sure (saniye).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Effect")
	float Duration = 10.f;

	// Periyodik efektler icin tick araligi (saniye). Stat/hiz buff'lari icin kullanilmaz.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Effect")
	float TickInterval = 1.f;

	// HUD renklendirmesi/gruplama icin: debuff mu?
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Effect")
	bool bIsDebuff = false;

	// HUD ikonunda gosterilen kisa etiket (2-3 harf).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Effect")
	FString ShortLabel;

	bool IsPeriodic() const { return Type == EMMOEffectType::PeriodicDamage || Type == EMMOEffectType::PeriodicHeal; }
	bool IsValidDef() const { return !EffectId.IsNone(); }
};

/**
 * Bir karaktere/moba etki eden AKTIF durum efekti (calisma zamani ornegi).
 * Stat bileseninde replike bir dizide tutulur; sadece UPROPERTY alanlari istemcilere replike olur.
 * Periyodik tick alanlari ve instigator sadece sunucuda anlamlidir (istemcide efekt islenmez).
 */
USTRUCT(BlueprintType)
struct FMMOActiveEffect
{
	GENERATED_BODY()

	UPROPERTY()
	FName EffectId;

	UPROPERTY()
	EMMOEffectType Type = EMMOEffectType::AttackBuff;

	UPROPERTY()
	float Magnitude = 0.f;

	// Yalniz sunucuda azalir ve REPLIKE EDILMEZ (NETWORK_AUDIT Y3: her karede degisen alan butun
	// ActiveEffects dizisini her karede yeniden gonderiyordu). Istemci kalan sureyi EndServerTime'dan hesaplar.
	UPROPERTY(NotReplicated)
	float RemainingTime = 0.f;

	// Bitis zamani (AGameStateBase::GetServerWorldTimeSeconds bazli). Yalniz efekt uygulanirken/tazelenirken degisir.
	UPROPERTY()
	float EndServerTime = 0.f;

	UPROPERTY()
	float Duration = 0.f;

	UPROPERTY()
	bool bIsDebuff = false;

	// --- Sadece sunucu (replike edilmez) ---
	float TickInterval = 0.f;
	float TimeSinceLastTick = 0.f;
	TWeakObjectPtr<AActor> Instigator;
	// Preserve player provenance when the weak source expires; never downgrade to environmental damage.
	bool bPlayerSource = false;
	// Mob yok edilse bile kaynak moba ait hub korumasini DoT suresince uygula.
	FVector MobSourceHubLocation = FVector::ZeroVector;
	float MobSourceHubRadius = 0.f;
};

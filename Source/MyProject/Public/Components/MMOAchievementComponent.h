// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Achievements/MMOAchievementTypes.h"
#include "MMOAchievementComponent.generated.h"

class AMMOCharacter;

/** Sunucuda tutulan ve diske yazilan basarim sayaclari (replike degil). */
struct FMMOAchievementCounters
{
	int32 KillTotal = 0;
	int32 Deaths = 0;
	int32 PeakGold = 0;
	TMap<FName, int32> Kills; // arketip -> oldurme sayisi
};

/**
 * Basarimlar ve unvanlar. Tamamen sunucu-otoriteli: sayaclar yalniz sunucuda artar, acilan basarimlar ve secili unvan
 * sahibine/herkese replike olur. Seviye/altin/gorev sayisi her 5 sn'de okunarak, oldurme ve olum olaylarla izlenir.
 * Kalicilik: Saved/MMOAchievements/<karakterin ActiveSaveSlot'u>.json, yol yukleme aninda sabitlenir; karakter yuklenmeden
 * hic yazilmaz (eski surum EndPlay'de MMOSave_Default.json'a yaziyordu). Karakter kayit semasindan bagimsiz; kimlik = oyuncu adi,
 * bilinen hesap acigi docs/SECURITY_AUDIT.md'de). Sohbet komutlari: /basarim, /basarim unvan <ad>.
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UMMOAchievementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMMOAchievementComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Acilmis basarim kimlikleri. Yalniz sahibine replike.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "MMO|Achievement")
	TArray<FName> Unlocked;

	// Secili unvan (bos = yok). Herkese replike (isim etiketi icin).
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "MMO|Achievement")
	FString ActiveTitle;

	// Sunucu: mob oldugunda UMMOQuestComponent::NotifyMobKilled'dan cagrilir.
	void NotifyMobKilled(FName ArchetypeId);

	// Sunucu: olum olayi (UMMOStatsComponent::OnDeath).
	UFUNCTION()
	void OnOwnerDied();

	// Sunucu: seviye/altin/gorev sayisini okuyup yeni acilan basarimlari verir. Yeniden girise karsi korumali.
	void EvaluateAll();

	// Sohbet komutlari (yalniz sunucuda): ozet liste ve unvan secimi.
	void CmdList();
	void CmdTitle(const FString& TitleName);

	bool IsUnlocked(FName Id) const { return Unlocked.Contains(Id); }
	const FMMOAchievementCounters& GetCounters() const { return Counters; }

	// Saf mantik (test edilebilir, bilesen durumundan bagimsiz).
	static bool IsSatisfied(const FMMOAchievementDef& Def, const FMMOAchievementCounters& C, int32 Level, int32 QuestsDone);
	static int32 ProgressOf(const FMMOAchievementDef& Def, const FMMOAchievementCounters& C, int32 Level, int32 QuestsDone);

	// Kalicilik bicimi (JSON). FromJson girdiyi dogrular: negatifleri 0'a, asiri buyukleri tavana ceker,
	// bilinmeyen/yinelenen basarim kimliklerini ve sahip olunmayan unvani atar. Gecersiz JSON'da false doner.
	static bool ToJson(const FMMOAchievementCounters& C, const TArray<FName>& Unlocked, const FString& Title, FString& OutJson);
	static bool FromJson(const FString& Json, FMMOAchievementCounters& OutC, TArray<FName>& OutUnlocked, FString& OutTitle);

#if !UE_BUILD_SHIPPING
	// Testler: kalici yazmayi kapatir ve yuklenmis sayar (PlayerState/disk gerekmez).
	void InitForTest() { bPersistenceEnabled = false; bLoaded = true; }
	// Kayit hatasi enjeksiyonu: SaveNow false doner (acilma geri alinir, odul verilmez).
	void SetSaveFailForTest(bool bFail) { bTestFailSaves = bFail; }
	static bool RunSelfTest(UWorld* World, FString& OutError, int32& OutChecks);
#endif

	static constexpr int32 MaxCounter = 1000000000;

private:
	bool HasAuthorityInternal() const;
	AMMOCharacter* GetOwnerCharacter() const;
	// Yuklu degilse yukler (karakter kaydi yuklendikten sonra, karakterin ActiveSaveSlot'una gore; canli ada gore DEGIL).
	// Karakter yeniden yuklendiyse (LoadSerial) kirli durumu ESKI yola yazar, sifirlar ve yeniden yukler. Ayni karakter
	// verisi ad degisimi + kayitla yeni yuvaya tasindiysa durumu da tasir (sifirlamaz: ad degistirip odul tekrari yok).
	bool TryLoad();
	// Basariyla diske yazildiysa (veya test modunda) true.
	bool SaveNow();
	void SaveIfDirty();
	// Karakterin ActiveSaveSlot'u; bos (yuklenmedi) veya Default ise false.
	bool GetBoundSlot(FString& OutSlot) const;
	void ResetLoadedState();
	void Grant(const FMMOAchievementDef& Def);
	void NotifyChat(const FString& Text) const;
	void OnPeriodic();

	FMMOAchievementCounters Counters;
	// Yukleme aninda BIR KEZ belirlenen dosya yolu ve yuva; kayit hep buraya gider (EndPlay'de PlayerState kopmus olsa da).
	FString StoragePath;
	FString LoadedSlot;
	uint32 LoadedSerial = 0;
	bool bTestFailSaves = false;
	bool bLoaded = false;
	bool bDirty = false;
	bool bEvaluating = false;
	bool bPersistenceEnabled = true;
	double LastSaveTime = 0.0;
	FTimerHandle PeriodicTimer;
};

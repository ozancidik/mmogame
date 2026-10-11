// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MMODailyComponent.generated.h"

class AMMOCharacter;

enum class EMMODailyKind : uint8
{
	KillAny,      // herhangi bir mob
	KillBand,     // seviyesi [oyuncu-8, oyuncu+3] penceresinde olan mob (ContentMap'teki uygun-mob penceresiyle ayni)
	ClaimQuests   // gun icinde odulu alinan (tamamlanan) gorev sayisi
};

/** Gunun tek gorevi. Hedef ve oduller gorev OLUSTURULDUGU andaki seviyeye gore sabitlenir (gun ici seviye atlama degistirmez). */
struct FMMODailyBounty
{
	EMMODailyKind Kind = EMMODailyKind::KillAny;
	int32 Target = 1;
	int32 Progress = 0;
	int32 RewardGold = 0;
	int32 RewardXP = 0;
	bool bDone = false;
};

/**
 * Gunluk gorevler: UTC gunu degisince yeniden olusturulan 3 gorev (avlanma, uygun-seviye avlanma, 1 gorev tamamlama).
 * Sunucu-otoriteli, replikasyon YOK (yalniz sohbet komutu /gunluk ve bildirimler). Tamamlaninca odul otomatik verilir.
 * Kalicilik: Saved/MMODaily/<kayit-yuvasi>.json, yuva = karakterin ActiveSaveSlot'u, yol yukleme aninda sabitlenir, karakter yuklenmeden yazilmaz (karakter kayit semasindan bagimsiz; kimlik = oyuncu adi, bilinen aciklar SECURITY_AUDIT.md).
 * Oduller kucuk: altin 15 x seviye (x2 son gorevde), XP = sonraki seviye esiginin %2'si (x2 son gorevde).
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UMMODailyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMMODailyComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Sunucu: mob oldugunda (UMMOQuestComponent::NotifyMobKilled, basarimlardan sonra) cagrilir.
	void NotifyMobKilled(FName ArchetypeId);

	// Sunucu: gun degisimi + gorev sayisi taramasi (zamanlayici cagirir); tamamlananlari odullendirir.
	void EvaluateAll();

	// Sohbet komutu /gunluk (yalniz sunucuda).
	void CmdList();

	const TArray<FMMODailyBounty>& GetBounties() const { return Bounties; }
	int32 GetDayKey() const { return StoredDayKey; }

	// Saf mantik (test edilebilir).
	static int32 DayKeyOf(const FDateTime& UtcNow) { return UtcNow.GetYear() * 10000 + UtcNow.GetMonth() * 100 + UtcNow.GetDay(); }
	static void BuildBounties(int32 Level, int32 ExpToNextLevel, TArray<FMMODailyBounty>& Out);
	static bool IsKillBandMatch(int32 PlayerLevel, int32 MobLevel) { return MobLevel >= PlayerLevel - 8 && MobLevel <= PlayerLevel + 3; }
	static bool ToJson(int32 DayKey, int32 QuestBaseline, const TArray<FMMODailyBounty>& B, FString& OutJson);
	// Dogrular: tam 3 gorev, bilinen tur, hedef 1..10000, ilerleme [0,hedef], oduller [0,1e7]; aksi halde false (yeniden olustur).
	static bool FromJson(const FString& Json, int32& OutDayKey, int32& OutQuestBaseline, TArray<FMMODailyBounty>& OutB);

#if !UE_BUILD_SHIPPING
	// Testler: kalici yazmayi kapatir, yuklenmis sayar ve gun anahtarini sabitler (PlayerState/disk/saat gerekmez).
	void InitForTest(int32 ForcedDay) { bPersistenceEnabled = false; bLoaded = true; ForcedDayKey = ForcedDay; }
	void SetDayForTest(int32 ForcedDay) { ForcedDayKey = ForcedDay; }
	// Kayit hatasi enjeksiyonu: SaveNow false doner (bDone geri alinir, odul verilmez).
	void SetSaveFailForTest(bool bFail) { bTestFailSaves = bFail; }
	static bool RunSelfTest(UWorld* World, FString& OutError, int32& OutChecks);
#endif

	static constexpr int32 BountyCount = 3;

private:
	bool HasAuthorityInternal() const;
	AMMOCharacter* GetOwnerCharacter() const;
	int32 CurrentDayKey() const;
	// Yuklu degilse yukler (karakter kaydi yuklendikten sonra, karakterin ActiveSaveSlot'una gore; canli ada gore DEGIL).
	// Karakter yeniden yuklendiyse (LoadSerial) kirli durumu ESKI yola yazar, sifirlar, yeniden yukler ve gorev referansini
	// (QuestBaseline) yeniden alir. Ayni karakter ad degisimi + kayitla yeni yuvaya tasindiysa durumu da tasir (sifirlamaz).
	bool TryLoad();
	void RollIfNewDay(const AMMOCharacter& Owner);
	// Basariyla diske yazildiysa (veya test modunda) true.
	bool SaveNow();
	// Karakterin ActiveSaveSlot'u; bos (yuklenmedi) veya Default ise false.
	bool GetBoundSlot(FString& OutSlot) const;
	void ResetLoadedState();
	void Grant(int32 Index);
	void NotifyChat(const FString& Text) const;
	void OnPeriodic();

	// Yukleme aninda BIR KEZ belirlenen dosya yolu ve yuva; kayit hep buraya gider (EndPlay'de PlayerState kopmus olsa da).
	FString StoragePath;
	FString LoadedSlot;
	uint32 LoadedSerial = 0;
	bool bTestFailSaves = false;
	TArray<FMMODailyBounty> Bounties;
	int32 StoredDayKey = 0;
	int32 QuestBaseline = 0;
	bool bLoaded = false;
	bool bDirty = false;
	bool bEvaluating = false;
	bool bPersistenceEnabled = true;
	bool bRebaseQuestsOnLoad = false; // karakter yeniden yuklendikten sonraki ilk yuklemede QuestBaseline yeniden alinir
	int32 ForcedDayKey = 0;
	FTimerHandle PeriodicTimer;
};

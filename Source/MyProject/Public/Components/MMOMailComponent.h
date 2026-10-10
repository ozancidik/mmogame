// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Mail/MMOMailStore.h"
#include "MMOMailComponent.generated.h"

class AMMOCharacter;

enum class EMMOMailResult : uint8
{
	Ok,
	BadTarget,     // bos/gecersiz ad
	Self,          // kendine posta
	UnknownTarget, // bu adda karakter yok
	NoGold,        // altin + ucret karsilanmiyor
	BadGold,       // negatif veya ust sinir asildi
	EmptyMail,     // metin ve altin yok
	TextTooLong,
	Cooldown,
	BoxFull,
	StoreError
};

/**
 * Posta: cevrimdisi/cevrimici oyuncuya kisa mesaj + altin eki. Sunucu-otoriteli, replikasyonsuz; komut /posta.
 * Gonderen once altin+ucreti oder (ucret = bir altin SINK'i), sonra kutuya yazilir; yazma basarisizsa iade edilir.
 * Alici kutusu her 10 sn ve giriste bosaltilir (once diske bos yazilir, sonra altin verilir: cift odeme yok).
 * Kimlik oyuncu adi; alici varligi = karakter kayit dosyasi (yerel depo) veya cevrimici oyuncu. Bulut depoya gecince guncellenmeli.
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UMMOMailComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMMOMailComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	static constexpr int32 FlatFee = 10;          // gonderim ucreti: 10 altin + eke %2
	static constexpr float CooldownSeconds = 5.f; // iki gonderim arasi
	static constexpr float CollectSeconds = 10.f;

	// Ucret: sabit 10 + eke %2 (yukari yuvarla). Saf fonksiyon.
	static int32 FeeFor(int32 Gold) { return FlatFee + (Gold > 0 ? (Gold * 2 + 99) / 100 : 0); }

	// Sunucu: gonderim. bRecipientExists cagiran tarafindan cozulur (cevrimici ya da kayit dosyasi var).
	EMMOMailResult SendMail(const FString& ToName, int32 Gold, const FString& Text, bool bRecipientExists);

	// Sunucu: kendi kutumu bosalt, altinlari ver, bildir; alinan posta sayisini dondurur.
	int32 CollectMail();

	// Sohbet komutlari (yalniz sunucuda).
	void CmdSend(const FString& Args);
	void CmdInfo();

	static FString ResultText(EMMOMailResult R);

#if !UE_BUILD_SHIPPING
	// Testler: ad ve zamanlayici/PlayerState bagimliligini kaldirir; cooldown saatini elle ilerletir.
	void InitForTest(const FString& Name) { TestName = Name; bTestMode = true; }
	void AdvanceTimeForTest(double Seconds) { TestClock += Seconds; }
	static bool RunSelfTest(UWorld* World, FString& OutError, int32& OutChecks);
#endif

private:
	bool HasAuthorityInternal() const;
	AMMOCharacter* GetOwnerCharacter() const;
	FString GetMyName() const;
	bool RecipientExists(const FString& Name) const;
	double Now() const;
	void NotifyChat(const FString& Text) const;
	void OnPeriodic();

	double LastSendTime = -1000.0;
	bool bCollecting = false;
	bool bTestMode = false;
	FString TestName;
	double TestClock = 0.0;
	FTimerHandle PeriodicTimer;
};

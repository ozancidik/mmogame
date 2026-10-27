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
	StoreError,
	NameChanged,   // gonderen ad degistirdi ama karakter henuz yeni yuvaya yazilmadi (catallama/altin katlama riski)
	Maintenance    // planli kapanis Flushing'e girdi: yeni posta gonderimi kapali (MMOServerLifecycle::IsFlushingOrLater)
};

/**
 * Posta: cevrimdisi/cevrimici oyuncuya kisa mesaj + altin eki. Sunucu-otoriteli, replikasyonsuz; komut /posta.
 * Gonderim sirasi (altin asla KOPYALANMAZ, en kotu durumda kaybolur): ucret dusulur -> gonderen karakter diske kaydedilir
 * (basarisizsa iade) -> kutuya yazilir (basarisizsa iade + tekrar kayit). Kayit yuklenmeden / bolge gecisi onaylandiktan sonra gonderim yok.
 * Alici kutusu her 10 sn bosaltilir: once teslim edilecekler kutudan silinir (diske), sonra altin verilir, sonra karakter kaydedilir.
 * Altin tavanini (UMMOInventoryComponent::MaxGold) asacak posta kutuda kalir.
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

	// "/posta gonder" argumanlari (saf, test edilebilir): "isim [<sayi>g] mesaj". Altin YALNIZ acik bicimde (rakamlar + 'g'
	// soneki, ornek "500g"); aksi halde isimden sonraki her sey mesajdir ("100 kere tesekkurler" altin DEGILDIR).
	// Cok buyuk sayi MaxGoldPerMail+1 olur (SendMail BadGold ile reddeder).
	static void ParseSendArgs(const FString& Args, FString& OutName, int32& OutGold, FString& OutText);

#if !UE_BUILD_SHIPPING
	// Testler: ad ve zamanlayici/PlayerState/kayit-yukleme bagimliligini kaldirir; cooldown saatini elle ilerletir.
	// Test modunda karakter kaydi cagrilmaz; sonucu SetSaveResultForTest belirler (varsayilan basarili) ve sayilir.
	void InitForTest(const FString& Name) { TestName = Name; bTestMode = true; }
	void AdvanceTimeForTest(double Seconds) { TestClock += Seconds; }
	void SetSaveResultForTest(bool bOk) { bTestSaveResult = bOk; }
	void SetTravelLockedForTest(bool bLocked) { bTestTravelLocked = bLocked; }
	void SetNameChangedForTest(bool bChanged) { bTestNameChanged = bChanged; }
	int32 GetSaveCallsForTest() const { return TestSaveCalls; }
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
	// Kayit yuklenmedi veya bolge gecisi kaydi onaylandi: altin degisikligi kalici olmaz, posta islemi yapilmaz.
	bool IsOwnerLocked(const AMMOCharacter& Owner) const;
	// Canli ad ile karakterin kayit yuvasi (ActiveSaveSlot) farkli: ad degisti, karakter yeni ada henuz yazilmadi.
	bool IsNameChanged(const AMMOCharacter& Owner) const;
	// Sahip karakteri diske yazar (senkron); test modunda sahte sonuc.
	bool SaveOwner();

	double LastSendTime = -1000.0;
	bool bCollecting = false;
	bool bTestMode = false;
	bool bTestSaveResult = true;
	bool bTestTravelLocked = false;
	bool bTestNameChanged = false;
	int32 TestSaveCalls = 0;
	int32 LastCapWarnKept = 0; // altin tavani yuzunden kutuda kalan posta sayisi (uyari tekrarini onler)
	FString TestName;
	double TestClock = 0.0;
	FTimerHandle PeriodicTimer;
};

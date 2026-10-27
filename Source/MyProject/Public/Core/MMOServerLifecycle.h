// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/MMOShutdownPlan.h"
#include "MMOServerLifecycle.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMMOLifecycle, Log, All);

class AGameModeBase;
class FOutputDevice;

/**
 * Planli kapatma / bakim modu, dilim L1 (docs/DESIGN_SERVER_LIFECYCLE.md 4-5): FMMOShutdownPlan'i (saf durum makinesi)
 * gercek zamanla surer, olaylari oyunculara sohbet sistem satiri (ClientReceiveChat, kanal 2) olarak duyurur ve
 * Closing/sonrasinda yeni girisi motorun FGameModeEvents::OnGameModePreLoginEvent() olayindan reddeder
 * (AMMOGameMode::PreLogin'e dokunulmaz). Kayit turu, atma ve cikis L2/L3'tur: burada yoktur.
 * Yeni RPC / replike alan yok; yalniz sunucuda calisir.
 *
 * Tetikleyiciler (yetki: yalniz sunucu sahibi, uzak istemci DEGIL):
 *  - komut satiri -MMOShutdownAfterSeconds=N [-MMOMaintenanceReason=bakim|guncelleme|acil] (yalniz !UE_BUILD_SHIPPING: L1'de
 *    cikis kodu yok, plan Exiting'e varinca surec SONLANMAZ -> Shipping'de L3 gelene kadar kapali, bkz. IsCmdLineTriggerSupported),
 *  - sunucu konsolu / -ExecCmds: "MMOShutdown <saniye> [neden]" ve "MMOShutdownCancel" (yalniz !UE_BUILD_SHIPPING).
 *    R1: uzak istemcinin ServerExec ile bu komuta ulasamadigi motor kaynaginda dogrulandi (bkz. cpp basindaki not);
 *    ek olarak komut IsExecAuthorized(NetMode) kapisindan gecer (yalniz Dedicated/Listen sunucu dunyasi).
 *
 * Plan durumu SUREC-genelidir (statik): bolge gecisinde (ServerTravel) dunya/alt sistem yeniden olusur, geri sayim sifirlanmaz.
 * Istisna: WorldType==PIE dunyasinin Initialize/Deinitialize'inda plan, son tick zamani ve komut satiri bayragi sifirlanir
 * (editor oturumlari arasi sizinti olmasin). Duyuru, GEngine dunya baglamlarindaki TUM sunucu (istemci olmayan) oyun dunyalarina gider.
 */
UCLASS()
class MYPROJECT_API UMMOServerLifecycleSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	/** Geri sayimi baslatir; olaylari isler (duyuru + PreLogin kapisi). Basarisizsa OutError sabit metindir. */
	bool RequestShutdown(double Seconds, EMMOShutdownReason Reason, FString& OutError);
	/** Yalniz Counting/Closing'de gecer. */
	bool RequestCancel(FString& OutError);

	/** Tick'in gercek zaman farkiyla cagirdigi giris; testler sahte sure verir. Olaylari isler. */
	void AdvanceSeconds(double DeltaSeconds);
	/** Tick'in cekirdegi: duvar saati NowSeconds. Ilk cagri (son tick zamani 0) yalniz saati kaydeder, bosta gecen sureyi saymaz. */
	void TickWithWallClock(double NowSeconds);
	/**
	 * Komut satiri tetikleyicisi (surec basina bir kez; anahtar varsa gecersiz de olsa tuketilir). bTriggerSupported=false
	 * (Shipping): plan kurulmaz, Warning loglanir. Gecersiz deger / reddedilen plan: log Error.
	 */
	void TryScheduleFromCommandLine(const TCHAR* CmdLine, bool bTriggerSupported);

	bool IsLoginClosed() const;
	EMMOShutdownPhase GetPhase() const;
	double GetRemainingSeconds() const;
	EMMOShutdownReason GetReason() const;

	/** Bu alt sistem sunucu modunda (Dedicated/Listen) mi? OnWorldBeginPlay'de belirlenir (listen surucusu o ana dek kurulur). */
	bool IsServerActive() const { return bServerActive; }

	/** Son duyuru metinleri ve alici sayisi (test/denetim; Shipping'de de derlenir, kucuk). */
	const TArray<FString>& GetSentMessages() const { return SentMessages; }
	int32 GetLastRecipientCount() const { return LastRecipientCount; }

#if !UE_BUILD_SHIPPING
	/** Sunucu konsolu komutlari. NetMode dunyadan okunur. true = komut bu sinifindi (tuketildi). */
	static bool HandleExecCommand(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar);
	/** Ayni, ama NetMode disaridan verilir (R1 yetki kapisini uzak/istemci/standalone modlari icin sinamak). */
	static bool HandleExecCommandForNetMode(UWorld* World, ENetMode NetMode, const TCHAR* Cmd, FOutputDevice& Ar);
	/** Surec-geneli plani ve test kayitlarini sifirlar (yalniz testler). */
	static void ResetPlanForTest();
	/** Son tick zamanini ayarlar (TickWithWallClock testi icin). */
	static void SetLastTickTimeForTest(double Seconds);
#endif

private:
	void ProcessEvents(const TArray<FMMOShutdownEvent>& Events);
	void BroadcastToPlayers(const FString& Message);
	void HandlePreLogin(AGameModeBase* GameMode, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage);

	bool bServerActive = false;
	FDelegateHandle PreLoginHandle;
	TArray<FString> SentMessages;
	int32 LastRecipientCount = 0;
};

/** Saf yardimcilar (dunyasiz test edilebilir). Oyuncuya gorunen metinler sabit Turkce ASCII; kullanici girdisi yankilanmaz. */
namespace MMOServerLifecycle
{
	/** "Bakim: sunucu N dakika icinde kapanacak. Ilerlemen kaydedilecek." (60 sn ve tam dakikalar) ya da "Bakim: sunucu N saniye icinde kapaniyor." */
	MYPROJECT_API FString AnnounceText(int32 Seconds);
	MYPROJECT_API const TCHAR* LoginClosedText();   // "Bakim: yeni girisler kapandi."
	MYPROJECT_API const TCHAR* CancelledText();     // "Bakim iptal edildi."
	MYPROJECT_API const TCHAR* LoginRejectText();   // PreLogin ret metni
	/** Plan olayi -> oyuncuya gosterilecek metin; gosterilmeyen olay (Flushing/Kicking/Exiting: L2/L3) icin false. */
	MYPROJECT_API bool EventText(const FMMOShutdownEvent& Event, FString& OutText);
	/** bakim | guncelleme | acil (buyuk/kucuk harf farksiz); baska bir sey false. */
	MYPROJECT_API bool ParseReason(const FString& Text, EMMOShutdownReason& OutReason);
	MYPROJECT_API const TCHAR* ReasonName(EMMOShutdownReason Reason);
	/** "<saniye> [neden]": saniye yalniz rakam ve en cok bir nokta; fazladan sozcuk ret. Aralik denetimi plan'indir. */
	MYPROJECT_API bool ParseShutdownArgs(const TCHAR* Args, double& OutSeconds, EMMOShutdownReason& OutReason, FString& OutError);
	/** R1 yetki kapisi: komut yalniz sunucu dunyasinda (Dedicated/Listen) calisir; Client/Standalone ret. */
	MYPROJECT_API bool IsExecAuthorized(ENetMode NetMode);

	enum class ECmdLineShutdown : uint8
	{
		NotRequested,  // ne -MMOShutdownAfterSeconds= ne -MMOMaintenanceReason= var
		Ok,
		Invalid        // anahtar var ama bos/gecersiz/aralik disi/yalniz neden: OutError dolu
	};
	/**
	 * Saf komut satiri ayristirma. Tirnakli deger kabul (FParse::Value); bos deger, yalniz neden, bilinmeyen neden, sayi disi
	 * ya da [MinDelaySeconds, MaxDelaySeconds] disi saniye -> Invalid. Hicbir anahtar yoksa NotRequested.
	 */
	MYPROJECT_API ECmdLineShutdown ParseCmdLineShutdown(const TCHAR* CmdLine, double& OutSeconds, EMMOShutdownReason& OutReason, FString& OutError);
	/** Komut satiri tetikleyicisi bu surumde acik mi? Shipping'de false (L1'de cikis yok -> kapanmayan sunucu riski; L3 kaldirir). */
	MYPROJECT_API bool IsCmdLineTriggerSupported();
	/** Bu dunya turunde (PIE) plan durumu dunya omruyle sifirlanir; Game/Dedicated'de surec-geneli kalir (ServerTravel). */
	MYPROJECT_API bool ResetsPlanWithWorldType(EWorldType::Type WorldType);
}

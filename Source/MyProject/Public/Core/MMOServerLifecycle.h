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
class AMMOCharacter;
class APlayerController;
class FOutputDevice;

/** L2: tek oyuncunun kapanis kaydi sonucu. Yalniz Saved "kaydedildi"dir; Skipped* kayit YAZILMADI demektir (kasitli). */
enum class EMMOLifecycleSaveResult : uint8
{
	Saved,
	Failed,                 // yazim basarisiz (dizin yazilamaz, ad cakismasi, gecis penceresinde ertelenen yazim basarisiz...)
	SkippedUnloaded,        // IsInitialLoadComplete()==false: bos karakterle mevcut kaydi EZME korumasi
	SkippedTravelCommitted, // bolge gecisi kaydi onayli: gecis kaydi zaten yazildi, altin/esya kalici olmaz
	SkippedNoPawn,          // pawn yok (olu/spectator/possess yok)
	SkippedGone             // denetleyici kayit adimindan once yok oldu (oyuncu ayrildi; EndPlay kendi kaydini yazar)
};

/** L2: kayit turu / atma ozeti (test ve denetim). */
struct FMMOLifecycleFlushSummary
{
	bool bRoundStarted = false;
	bool bFlushDone = false;
	int32 Players = 0;
	int32 Pending = 0;
	int32 Saved = 0;
	int32 Failed = 0;
	int32 SkippedUnloaded = 0;
	int32 SkippedTravelCommitted = 0;
	int32 SkippedNoPawn = 0;
	int32 SkippedGone = 0;
	int32 Kicked = 0;
	int32 Skipped() const { return SkippedUnloaded + SkippedTravelCommitted + SkippedNoPawn + SkippedGone; }
};

/**
 * Planli kapatma / bakim modu, dilim L1-L2 (docs/DESIGN_SERVER_LIFECYCLE.md 4-5): FMMOShutdownPlan'i (saf durum makinesi)
 * gercek zamanla surer, olaylari oyunculara sohbet sistem satiri (ClientReceiveChat, kanal 2) olarak duyurur ve
 * Closing/sonrasinda yeni girisi motorun FGameModeEvents::OnGameModePreLoginEvent() olayindan reddeder
 * (AMMOGameMode::PreLogin'e dokunulmaz).
 * L2: Flushing'e girince TEK SEFERLIK toplu kayit turu (oyuncu basina bir Tick: SaveCharacterForTravel / FlushDeferredSave),
 * tur bitince plana NotifyFlushDone; Kicking'de her oyuncu YALNIZ kendi kayit sonucu belli olduktan sonra atilir. Exiting'e
 * gelindiginde kalan is senkron tamamlanir (once kayit, sonra atma).
 * L3: Exiting'de Drain'den sonra tum (listen ev sahibi disindaki) PlayerController'lar yok olana kadar en cok ExitWaitMaxSeconds
 * beklenir, sonra FPlatformMisc::RequestExitWithStatus: kod 0 = kayip yok, 2 = en az bir kayip: atma aninda yeniden denemeden sonra da
 * kaydi basarisiz oyuncu, kayit turu bitmedigi icin kararsiz kalan, 3 atma denemesinden sonra atilamayan ya da 5 sn sonunda hala bagli
 * oyuncu (K3: surec yoneticisi kod 2'de OTOMATIK YENIDEN BASLATMAMALI). Atma aninda Saved sonuclu oyuncu da bir kez daha yazilir
 * (tur ile atma arasindaki degisiklik kaybolmasin); "kaydedildi" metni SON yazima gore. Kayip sayilarinin dokumu ev=shutdown_exit satirinda
 * ve Saved/Logs/MMOShutdownResult.json'da (Windows kod notu: MMOServerLifecycle::NeedsForcedExitCode). Flushing'e girince aktif ticaretler
 * iptal edilir; ticaret (davet/kabul/baslatma/onay/esya/altin), envanter (satin alma, satis, depo, yukseltme/yakma), gorev odulu, posta
 * ve birlestirme reddedilir (IsFlushingOrLater / RefuseWhenFlushing).
 * Yan dosyalar (basarim/gunluk/yaris): bilesenlerin SaveNow'u private, bu sistem onlari ayrica CAGIRMAZ; EndPlay kaydina guvenilir.
 * Yeni RPC / replike alan yok; yalniz sunucuda calisir.
 *
 * Tetikleyiciler (yetki: yalniz sunucu sahibi, uzak istemci DEGIL):
 *  - komut satiri -MMOShutdownAfterSeconds=N [-MMOMaintenanceReason=bakim|guncelleme|acil] (L3'ten beri Shipping dahil TUM yapilarda),
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
	/** Komut satiri tetikleyicisi (surec basina bir kez; anahtar varsa gecersiz de olsa tuketilir). Gecersiz deger / reddedilen plan: log Error. */
	void TryScheduleFromCommandLine(const TCHAR* CmdLine);

	bool IsLoginClosed() const;
	EMMOShutdownPhase GetPhase() const;
	double GetRemainingSeconds() const;
	EMMOShutdownReason GetReason() const;

	/** Bu alt sistem sunucu modunda (Dedicated/Listen) mi? OnWorldBeginPlay'de belirlenir (listen surucusu o ana dek kurulur). */
	bool IsServerActive() const { return bServerActive; }

	/** Son duyuru metinleri ve alici sayisi (test/denetim; Shipping'de de derlenir, kucuk). */
	const TArray<FString>& GetSentMessages() const { return SentMessages; }
	int32 GetLastRecipientCount() const { return LastRecipientCount; }

	/**
	 * L2: Flushing'e giriste Tick yolundan baslar. Tur bir kez calisir (ikinci cagri false, kimseye ikinci yazim yok).
	 * Kayit turu yalniz sunucuda; sunucu degilse false.
	 */
	bool BeginFlushRound();
	/** L2: bir Tick'lik is: sirada bekleyen TEK oyuncuyu kaydeder + atma kapisi aciksa sonucu belli en cok MaxKicksPerTick oyuncuyu atar. */
	void StepShutdownWork();
	/** L2: Exiting'de (zaman doldu) kalan tum isi senkron bitirir: once kalan kayitlar, sonra atmalar (Tick siniri yok). */
	void DrainShutdownWork();
	/**
	 * L3: Exiting'de bir adim (Tick cagirir; testler sahte duvar saati verir). Gec gelenler icin Drain'i tekrarlar; listen ev sahibi
	 * disindaki tum denetleyiciler yok olunca ya da bekleme (ilk adimdan ExitWaitMaxSeconds) dolunca sureci kapatir. Bir kez.
	 */
	void StepExit(double NowSeconds);
	/** L3: cikis istendi mi (surec-geneli)? Istendikten sonra alt sistem tick etmez. */
	static bool WasExitRequested();
	FMMOLifecycleFlushSummary GetFlushSummary() const;

#if !UE_BUILD_SHIPPING
	/** Test: kayit yazimini (SaveCharacterForTravel yerine) degistirir; false/bos = gercek yazim. true donerse "Saved". */
	static void SetSaveHookForTest(TFunction<bool(AMMOCharacter*)> Hook);
	/** Test: atmayi (AGameSession::KickPlayer yerine) degistirir; true donerse atildi sayilir. */
	static void SetKickHookForTest(TFunction<bool(APlayerController*, const FText&)> Hook);
	/** Test: kayit turu / atma yalniz filtreden gecen denetleyicileri kapsar (canli dunyadaki ev sahibi vb. disarida kalir). */
	static void SetPlayerFilterForTest(TFunction<bool(const APlayerController*)> Filter);
	/** Test: RequestExit YERINE cagrilir (kod, basarisiz oyuncu sayisi). Atanmamissa gercek cikis istenir. */
	static void SetExitHookForTest(TFunction<void(int32 /*ExitCode*/, int32 /*Failed*/)> Hook);
#endif
	/** L2: bu denetleyicinin kayit sonucu ve atilip atilmadigi; tur onu henuz bilmiyorsa ya da sonucu belli degilse false. */
	bool GetFlushResultFor(const APlayerController* PC, EMMOLifecycleSaveResult& OutResult, bool& bOutKicked) const;

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
	void GatherServerWorlds(TArray<UWorld*>& OutWorlds) const;
	bool FlushNextPlayer();
	void FinishFlushRound();
	/** Serial: ayni Serial ile bir oyuncuya en cok BIR atma denemesi (Drain'in gecisleri ayni Serial'i kullanir; yeniden denemeler Tick'lere yayilir). */
	void KickResolvedPlayers(int32 MaxKicks, uint32 Serial);
	void FinishExit(bool bTimedOut);
	void GatherExitWaitingPlayers(TArray<const APlayerController*>& OutPlayers) const;
	int32 CountExitWaitingPlayers() const;
	void CancelActiveTrades();
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
	/** Atma nedeni: yalniz kayit GERCEKTEN basarili olduysa "kaydedildi" denir; digerlerinde yalan soylenmez. */
	MYPROJECT_API const TCHAR* KickText(EMMOLifecycleSaveResult Result);
	/** Tur kaydi Saved idi ama atma aninda yeniden yazim Failed oldu: onceki kayit gecerli, son degisiklikler kaybolmus olabilir ("kaydedildi" denmez). */
	MYPROJECT_API const TCHAR* KickTextRewriteFailed();
	MYPROJECT_API const TCHAR* SaveResultName(EMMOLifecycleSaveResult Result);
	/**
	 * L2 kopru: tek karakterin kapanis kaydi. Kapilar: pawn yok -> SkippedNoPawn; IsInitialLoadComplete degilse YAZMA -> SkippedUnloaded;
	 * IsTravelSaveCommitted ise YAZMA -> SkippedTravelCommitted; ertelenmis kayit varsa FlushDeferredSave(true); yoksa
	 * SaveCharacterForTravel(false), o gecis penceresinde ertelerse FlushDeferredSave(true) bir kez; basarisiz -> Failed.
	 */
	MYPROJECT_API EMMOLifecycleSaveResult SaveCharacterNow(AMMOCharacter* Character);
	/** L2: nazik atma (AGameSession::KickPlayer -> istemci ana menuye doner). Sunucunun yerel oyuncusu (listen ev sahibi) ya da ag baglantisi olmayan denetleyici atilmaz: false. */
	MYPROJECT_API bool KickPlayerNow(APlayerController* PC, const FText& Reason);
	/** Plan olayi -> oyuncuya gosterilecek metin; gosterilmeyen olay (Flushing/Kicking/Exiting: L2/L3) icin false. */
	MYPROJECT_API bool EventText(const FMMOShutdownEvent& Event, FString& OutText);
	/** bakim | guncelleme | acil (buyuk/kucuk harf farksiz); baska bir sey false. */
	MYPROJECT_API bool ParseReason(const FString& Text, EMMOShutdownReason& OutReason);
	MYPROJECT_API const TCHAR* ReasonName(EMMOShutdownReason Reason);
	/** "<saniye> [neden]": saniye yalniz rakam ve en cok bir nokta; fazladan sozcuk ret. Aralik denetimi plan'indir. */
	MYPROJECT_API bool ParseShutdownArgs(const TCHAR* Args, double& OutSeconds, EMMOShutdownReason& OutReason, FString& OutError);
	/** R1 yetki kapisi: komut yalniz sunucu dunyasinda (Dedicated/Listen) calisir; Client/Standalone ret. */
	MYPROJECT_API bool IsExecAuthorized(ENetMode NetMode);

	// [PLACEHOLDER] L3 sayilari.
	constexpr double ExitWaitMaxSeconds = 5.0;  // Exiting'de denetleyicilerin yok olmasi icin ust sinir
	constexpr int32 MaxKicksPerTick = 4;        // Tick basina atma siniri (Drain haric)
	constexpr int32 MaxKickAttempts = 3;        // uzak oyuncu icin KickPlayer false donerse en cok deneme
	constexpr int32 ExitCodeOk = 0;
	constexpr int32 ExitCodeSaveFailed = 2;     // K3: surec yoneticisi bu kodda otomatik yeniden baslatmamali

	/** Kapanis plani Flushing ya da sonrasinda mi (surec-geneli)? Ticaret/posta/birlestirme RPC girisleri bunu okuyup reddeder. */
	MYPROJECT_API bool IsFlushingOrLater();
	/**
	 * Altin/esya/XP degistiren RPC girislerinin ortak kapisi: IsFlushingOrLater() ise true (cagiran reddeder) ve
	 * "event=maintenance_refused gate=<Gate>" satiri LogMMOLifecycle'a yazilir (ilk MaxRefusedLogLines satir; RPC dolduran istemci logu sisirmesin).
	 * Gate yalniz kod sabiti olmali (kullanici girdisi degil).
	 */
	MYPROJECT_API bool RefuseWhenFlushing(const TCHAR* Gate);
	constexpr int32 MaxRefusedLogLines = 200;
	/** Cikis kodu: kaydi atma anindaki yeniden denemeden sonra da basarisiz kalan oyuncu varsa 2, yoksa 0. */
	MYPROJECT_API int32 ExitCodeForFailures(int32 FailedPlayers);

	/**
	 * Cikis kodu platform notu (UE 5.7 kaynagi): FPlatformMisc::RequestExitWithStatus(false, kod) Windows'ta kodu ISLETIM SISTEMINE
	 * TASIMAZ (yalniz PostQuitMessage; ana dongu IsEngineExitRequested ile biter, WinMain 0 doner). Linux'ta GOverriddenReturnCode ile tasinir.
	 * Cozum: kod != 0 ise Windows'ta motor kapanis zinciri (dunya yikimi, EndPlay kayitlari) normal calissin, FCoreDelegates::OnPreExit'te
	 * (GEngine->PreExit SONRASI) log yazilip RequestExitWithStatus(true, kod) -> TerminateProcess(kod) cagrilir. Her durumda
	 * Saved/Logs/MMOShutdownResult.json yazilir (surec yoneticisi bunu da okuyabilir).
	 */
	MYPROJECT_API bool NeedsForcedExitCode(int32 ExitCode);
	/** Durum dosyasi icerigi: {"run_id":"...","exit_code":N,"failed":N,"gone":N,"still_connected":N,"kick_failed":N,"time_utc":"..."}. */
	MYPROJECT_API FString BuildResultJson(const FString& RunId, int32 ExitCode, int32 Failed, int32 Gone, int32 StillConnected, int32 KickFailed, const FDateTime& TimeUtc);
	/**
	 * Bu surecin kimligi: "<surec baslangic UTC yyyyMMddTHHmmssZ>-<PID>" (ilk cagrida bir kez uretilir, surec boyunca sabit).
	 * Yalniz [0-9TZ-] karakterleri. Yonetici yalniz KENDI baslattigi calismanin run_id degerini gecerli sayar; dosya yoksa cikis temiz bitmemistir.
	 */
	MYPROJECT_API const FString& RunId();
	/** run_id bicimi: bos olmayan, en cok 40 karakter, yalniz rakam, 'T', 'Z' ve '-'. */
	MYPROJECT_API bool IsValidRunId(const FString& Id);
	/** Bayat durum dosyasini (onceki calismadan kalan) ve .tmp yarimini siler; dosya sonunda yoksa true. */
	MYPROJECT_API bool DeleteStaleResultFile(const FString& Path);
	/** Surec basina bir kez (ilk sunucu OnWorldBeginPlay): ResultFilePath() bayat dosyasini siler. Ilk cagri true, sonrakiler no-op false. */
	MYPROJECT_API bool ClearStaleResultOnce();
	/** ClearStaleResultOnce bu surecte calisti mi? */
	MYPROJECT_API bool StaleResultCleanupDone();
	/** Saved/Logs/MMOShutdownResult.json (ProjectLogDir). */
	MYPROJECT_API FString ResultFilePath();
	/** Gecici dosya + tasima ile yazar (yarim dosya kalmaz). Basarisizsa false ve LogMMOLifecycle Warning. */
	MYPROJECT_API bool WriteResultFile(const FString& Path, const FString& Json);

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
	/** Bu dunya turunde (PIE) plan durumu dunya omruyle sifirlanir; Game/Dedicated'de surec-geneli kalir (ServerTravel). */
	MYPROJECT_API bool ResetsPlanWithWorldType(EWorldType::Type WorldType);
}

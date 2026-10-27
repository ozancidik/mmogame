// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/MMOServerLifecycle.h"
#include "Core/MMOMetrics.h"
#include "Characters/MMOCharacter.h"
#include "Components/MMOTradeComponent.h"
#include "Player/MMOExecGuard.h"
#include "Player/MMOPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Internationalization/Text.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Stats/Stats.h"

DEFINE_LOG_CATEGORY(LogMMOLifecycle);

/*
 * R1 (docs/DESIGN_SERVER_LIFECYCLE.md 8): uzak istemci ServerExec ile bu konsol komutuna ulasabilir mi? UE 5.7 motor kaynagi (2026-10-27):
 *  - APlayerController::ServerExec (PlayerController.cpp:1886-1896, yalniz !UE_BUILD_SHIPPING) -> ServerExecRPC -> ServerExecRPC_Implementation
 *    (:1879-1884) -> ClientMessage(ConsoleCommand(Msg)).
 *  - APlayerController::ConsoleCommand (:536-562) -> Player->ConsoleCommand; sunucuda uzak PC'nin Player'i UNetConnection'dir.
 *  - UPlayer::ConsoleCommand (Player.cpp:30-50) -> UPlayer::Exec (Player.cpp:98-160). Zincir: FExec::Exec (UPlayer'in kendi Exec_*'i),
 *    UWorld::Exec, PlayerInput, PC, Pawn, HUD, GameMode, CheatManager, GameState, CameraManager ProcessConsoleExec. Bunlarin hicbiri
 *    GEngine->Exec / FSelfRegisteringExec::StaticExec'e inmez: StaticExec yalniz UEngine::Exec'ten (UnrealEngine.cpp:5219) cagrilir; ona da
 *    yerel yollar gider (UGameViewportClient::Exec GameViewportClient.cpp:3381, konsol stdin, -ExecCmds). UWorld::Exec "else return 0" ile biter.
 *  => Kayitli FSelfRegisteringExec komutlarina uzak istemci ULASAMAZ ("Command not recognized"). Test bunu ayni motor zinciriyle benzetir
 *     (MMOServerLifecycleSelfTest: UPlayer'li sahte PC -> ConsoleCommand). Yine de savunma derinligi: komut IsExecAuthorized kapisindan gecer.
 */

namespace
{
	// Surec-geneli plan: ServerTravel (bolge gecisi) dunyayi/alt sistemi yeniden olusturur, geri sayim kaybolmasin.
	FMMOShutdownPlan LifecyclePlan;
	double LifecycleLastTickTime = 0.0;
	bool bLifecycleCommandLineHandled = false;

	// L2 kayit turu durumu (surec-geneli, plan gibi; PC'ler zayif isaretli). Tur Flushing'e girince bir kez kurulur.
	struct FLifecycleFlushEntry
	{
		TWeakObjectPtr<APlayerController> PC;
		FString Who;                 // MMOExecGuard::SanitizeToken ile temiz (log izin listesi)
		bool bResolved = false;      // kayit sonucu belli (Saved/Failed/Skipped*)
		EMMOLifecycleSaveResult Result = EMMOLifecycleSaveResult::Failed;
		bool bRetried = false;       // atma aninda yeniden deneme/degerlendirme yapildi (bir kez)
		int32 KickAttempts = 0;      // KickPlayer denemeleri (uzak oyuncu icin en cok MaxKickAttempts)
		bool bKickDone = false;      // atma tamamlandi (basarili, ev sahibi ya da deneme hakki bitti)
		bool bKicked = false;        // atma gercekten yapildi
	};
	TArray<FLifecycleFlushEntry> LifecycleEntries;
	int32 LifecycleNextFlush = 0;
	bool bLifecycleRoundStarted = false;
	// L3 cikis durumu (surec-geneli): bekleme baslangici (<0 = henuz baslamadi) ve "cikis istendi" bayragi.
	double LifecycleExitWaitStart = -1.0;
	bool bLifecycleExitRequested = false;

#if !UE_BUILD_SHIPPING
	TFunction<void(int32, int32)> LifecycleExitHook;
	TFunction<bool(AMMOCharacter*)> LifecycleSaveHook;
	TFunction<bool(APlayerController*, const FText&)> LifecycleKickHook;
	TFunction<bool(const APlayerController*)> LifecyclePlayerFilter;
#endif

	bool LifecycleIncludesPlayer(const APlayerController* PC)
	{
#if !UE_BUILD_SHIPPING
		if (LifecyclePlayerFilter) return LifecyclePlayerFilter(PC);
#endif
		return PC != nullptr;
	}

	void LifecycleResetState()
	{
		LifecyclePlan = FMMOShutdownPlan();
		LifecycleLastTickTime = 0.0;
		bLifecycleCommandLineHandled = false;
		LifecycleEntries.Reset();
		LifecycleNextFlush = 0;
		bLifecycleRoundStarted = false;
		LifecycleExitWaitStart = -1.0;
		bLifecycleExitRequested = false;
	}

	// Listen sunucunun kendi (yerel) oyuncusu: atilacak baglanti yok, cikista beklenmez.
	bool LifecycleIsHost(const APlayerController* PC)
	{
		return PC && PC->Player && PC->Player->IsA<ULocalPlayer>();
	}

	FLifecycleFlushEntry* LifecycleFindEntry(const APlayerController* PC)
	{
		for (FLifecycleFlushEntry& E : LifecycleEntries)
		{
			if (E.PC.Get() == PC) return &E;
		}
		return nullptr;
	}

	FString LifecycleWhoName(const APlayerController* PC)
	{
		FString Raw;
		if (PC)
		{
			Raw = PC->PlayerState ? PC->PlayerState->GetPlayerName() : PC->GetName();
		}
		const FString Safe = MMOExecGuard::SanitizeToken(Raw, 32);
		return Safe.IsEmpty() ? FString(TEXT("(bos)")) : Safe;
	}

	const TCHAR* LifecyclePhaseName(EMMOShutdownPhase Phase)
	{
		switch (Phase)
		{
		case EMMOShutdownPhase::Idle: return TEXT("Idle");
		case EMMOShutdownPhase::Counting: return TEXT("Counting");
		case EMMOShutdownPhase::Closing: return TEXT("Closing");
		case EMMOShutdownPhase::Flushing: return TEXT("Flushing");
		case EMMOShutdownPhase::Kicking: return TEXT("Kicking");
		case EMMOShutdownPhase::Exiting: return TEXT("Exiting");
		}
		return TEXT("?");
	}

	int32 LifecycleCountPlayers(const UWorld* World)
	{
		int32 Count = 0;
		if (World)
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (It->Get()) ++Count;
			}
		}
		return Count;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Saf yardimcilar
// ---------------------------------------------------------------------------------------------------------------------

FString MMOServerLifecycle::AnnounceText(int32 Seconds)
{
	if (Seconds >= 60 && Seconds % 60 == 0)
	{
		return FString::Printf(TEXT("Bakim: sunucu %d dakika icinde kapanacak. Ilerlemen kaydedilecek."), Seconds / 60);
	}
	return FString::Printf(TEXT("Bakim: sunucu %d saniye icinde kapaniyor."), Seconds);
}

const TCHAR* MMOServerLifecycle::LoginClosedText() { return TEXT("Bakim: yeni girisler kapandi."); }
const TCHAR* MMOServerLifecycle::CancelledText() { return TEXT("Bakim iptal edildi."); }
const TCHAR* MMOServerLifecycle::LoginRejectText() { return TEXT("Sunucu bakima giriyor, daha sonra tekrar dene."); }

const TCHAR* MMOServerLifecycle::KickText(EMMOLifecycleSaveResult Result)
{
	// "Kaydedildi" yalnizca gercekten yazilmis KARAKTER kaydi (.sav) icin; basarim/gunluk yan dosyalari hakkinda bir sey denmez.
	// Yeniden denemeden sonra da yazilamayan kayitta durust uyari; atlananlarda (kaydedilecek durum yok) yalnizca atma nedeni.
	if (Result == EMMOLifecycleSaveResult::Saved) return TEXT("Sunucu bakima girdi. Ilerlemen kaydedildi.");
	if (Result == EMMOLifecycleSaveResult::Failed) return TEXT("Sunucu bakima girdi. Ilerleme kaydedilemedi, yoneticiye bildir.");
	return TEXT("Sunucu bakima girdi.");
}

const TCHAR* MMOServerLifecycle::SaveResultName(EMMOLifecycleSaveResult Result)
{
	switch (Result)
	{
	case EMMOLifecycleSaveResult::Saved: return TEXT("saved");
	case EMMOLifecycleSaveResult::Failed: return TEXT("failed");
	case EMMOLifecycleSaveResult::SkippedUnloaded: return TEXT("skipped_unloaded");
	case EMMOLifecycleSaveResult::SkippedTravelCommitted: return TEXT("skipped_travel");
	case EMMOLifecycleSaveResult::SkippedNoPawn: return TEXT("skipped_nopawn");
	case EMMOLifecycleSaveResult::SkippedGone: return TEXT("gone");
	}
	return TEXT("?");
}

EMMOLifecycleSaveResult MMOServerLifecycle::SaveCharacterNow(AMMOCharacter* Character)
{
	if (!Character) return EMMOLifecycleSaveResult::SkippedNoPawn;
	// Yuklenmemis karakter: gercek kayit dosyasini bos karakterle EZME (AMMOCharacter::EndPlay ile ayni koruma).
	if (!Character->IsInitialLoadComplete()) return EMMOLifecycleSaveResult::SkippedUnloaded;
	// Bolge gecisi onayli: gecis kaydi zaten yazildi, bu andan sonra altin/esya degisikligi kalici olmaz.
	if (Character->IsTravelSaveCommitted()) return EMMOLifecycleSaveResult::SkippedTravelCommitted;
	bool bOk = false;
#if !UE_BUILD_SHIPPING
	if (LifecycleSaveHook)
	{
		return LifecycleSaveHook(Character) ? EMMOLifecycleSaveResult::Saved : EMMOLifecycleSaveResult::Failed;
	}
#endif
	if (Character->IsSaveDeferredByTravel())
	{
		bOk = Character->FlushDeferredSave(true); // R7: gecis yazimi ucustayken ertelenen kayit; basarisizsa failed
	}
	else
	{
		bOk = Character->SaveCharacterForTravel(false);
		if (!bOk && Character->IsSaveDeferredByTravel())
		{
			bOk = Character->FlushDeferredSave(true); // gecis penceresi simdi bittiyse tek deneme daha
		}
	}
	return bOk ? EMMOLifecycleSaveResult::Saved : EMMOLifecycleSaveResult::Failed;
}

bool MMOServerLifecycle::KickPlayerNow(APlayerController* PC, const FText& Reason)
{
	if (!PC) return false;
#if !UE_BUILD_SHIPPING
	if (LifecycleKickHook) return LifecycleKickHook(PC, Reason);
#endif
	if (PC->IsLocalController()) return false; // listen sunucunun kendi oyuncusu: atilacak baglanti yok
	const UWorld* World = PC->GetWorld();
	const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	AGameSession* Session = GameMode ? GameMode->GameSession : nullptr;
	return Session && Session->KickPlayer(PC, Reason);
}

bool MMOServerLifecycle::EventText(const FMMOShutdownEvent& Event, FString& OutText)
{
	switch (Event.Type)
	{
	case EMMOShutdownEventType::Announce:
		OutText = AnnounceText(Event.Seconds);
		return true;
	case EMMOShutdownEventType::Cancelled:
		OutText = CancelledText();
		return true;
	case EMMOShutdownEventType::PhaseChanged:
		if (Event.Phase == EMMOShutdownPhase::Closing)
		{
			OutText = LoginClosedText();
			return true;
		}
		return false; // Flushing/Kicking/Exiting: kayit turu, atma ve cikis L2/L3.
	}
	return false;
}

bool MMOServerLifecycle::ParseReason(const FString& Text, EMMOShutdownReason& OutReason)
{
	if (Text.Equals(TEXT("bakim"), ESearchCase::IgnoreCase)) { OutReason = EMMOShutdownReason::Maintenance; return true; }
	if (Text.Equals(TEXT("guncelleme"), ESearchCase::IgnoreCase)) { OutReason = EMMOShutdownReason::Update; return true; }
	if (Text.Equals(TEXT("acil"), ESearchCase::IgnoreCase)) { OutReason = EMMOShutdownReason::Emergency; return true; }
	return false;
}

const TCHAR* MMOServerLifecycle::ReasonName(EMMOShutdownReason Reason)
{
	switch (Reason)
	{
	case EMMOShutdownReason::Maintenance: return TEXT("bakim");
	case EMMOShutdownReason::Update: return TEXT("guncelleme");
	case EMMOShutdownReason::Emergency: return TEXT("acil");
	}
	return TEXT("?");
}

bool MMOServerLifecycle::ParseShutdownArgs(const TCHAR* Args, double& OutSeconds, EMMOShutdownReason& OutReason, FString& OutError)
{
	OutReason = EMMOShutdownReason::Maintenance;
	OutSeconds = 0.0;
	const TCHAR* Stream = Args ? Args : TEXT("");
	FString SecondsToken, ReasonToken, ExtraToken;
	FParse::Token(Stream, SecondsToken, false);
	FParse::Token(Stream, ReasonToken, false);
	FParse::Token(Stream, ExtraToken, false);
	if (SecondsToken.IsEmpty())
	{
		OutError = TEXT("Kullanim: MMOShutdown <saniye> [bakim|guncelleme|acil]");
		return false;
	}
	// Yalniz rakam ve en cok bir nokta (ilk karakter rakam): "1e3", "0x10", "-5", "nan", "inf" ret.
	bool bDot = false;
	bool bValid = SecondsToken.Len() <= 12 && FChar::IsDigit(SecondsToken[0]);
	for (int32 i = 0; bValid && i < SecondsToken.Len(); ++i)
	{
		const TCHAR C = SecondsToken[i];
		if (C == TEXT('.')) { if (bDot) bValid = false; bDot = true; }
		else if (!FChar::IsDigit(C)) bValid = false;
	}
	if (!bValid)
	{
		OutError = TEXT("Saniye gecerli bir sayi olmali.");
		return false;
	}
	if (!ExtraToken.IsEmpty())
	{
		OutError = TEXT("Fazla arguman.");
		return false;
	}
	if (!ReasonToken.IsEmpty() && !ParseReason(ReasonToken, OutReason))
	{
		OutError = TEXT("Neden bakim, guncelleme ya da acil olmali.");
		return false;
	}
	OutSeconds = FCString::Atod(*SecondsToken);
	return true;
}

bool MMOServerLifecycle::IsExecAuthorized(ENetMode NetMode)
{
	return NetMode == NM_DedicatedServer || NetMode == NM_ListenServer;
}

MMOServerLifecycle::ECmdLineShutdown MMOServerLifecycle::ParseCmdLineShutdown(const TCHAR* CmdLine, double& OutSeconds, EMMOShutdownReason& OutReason, FString& OutError)
{
	OutSeconds = 0.0;
	OutReason = EMMOShutdownReason::Maintenance;
	OutError.Reset();
	const TCHAR* Line = CmdLine ? CmdLine : TEXT("");
	FString SecondsText, ReasonText;
	const bool bHasSeconds = FParse::Value(Line, TEXT("MMOShutdownAfterSeconds="), SecondsText);
	const bool bHasReason = FParse::Value(Line, TEXT("MMOMaintenanceReason="), ReasonText);
	if (!bHasSeconds && !bHasReason) return ECmdLineShutdown::NotRequested;
	if (!bHasSeconds)
	{
		OutError = TEXT("MMOMaintenanceReason verildi ama MMOShutdownAfterSeconds yok.");
		return ECmdLineShutdown::Invalid;
	}
	if (SecondsText.IsEmpty())
	{
		OutError = TEXT("MMOShutdownAfterSeconds degeri bos.");
		return ECmdLineShutdown::Invalid;
	}
	for (int32 i = 0; i < SecondsText.Len(); ++i)
	{
		if (FChar::IsWhitespace(SecondsText[i]))
		{
			OutError = TEXT("MMOShutdownAfterSeconds tek bir sayi olmali.");
			return ECmdLineShutdown::Invalid;
		}
	}
	if (!ParseShutdownArgs(*SecondsText, OutSeconds, OutReason, OutError))
	{
		return ECmdLineShutdown::Invalid;
	}
	if (bHasReason && !ParseReason(ReasonText, OutReason))
	{
		OutError = TEXT("MMOMaintenanceReason bakim, guncelleme ya da acil olmali.");
		return ECmdLineShutdown::Invalid;
	}
	if (OutSeconds < FMMOShutdownPlan::MinDelaySeconds || OutSeconds > FMMOShutdownPlan::MaxDelaySeconds)
	{
		OutError = FString::Printf(TEXT("Sure %d..%d saniye araliginda olmali."), static_cast<int32>(FMMOShutdownPlan::MinDelaySeconds), static_cast<int32>(FMMOShutdownPlan::MaxDelaySeconds));
		return ECmdLineShutdown::Invalid;
	}
	return ECmdLineShutdown::Ok;
}

bool MMOServerLifecycle::IsFlushingOrLater()
{
	return LifecyclePlan.GetPhase() >= EMMOShutdownPhase::Flushing;
}

int32 MMOServerLifecycle::ExitCodeForFailures(int32 FailedPlayers)
{
	return FailedPlayers > 0 ? ExitCodeSaveFailed : ExitCodeOk;
}

bool MMOServerLifecycle::ResetsPlanWithWorldType(EWorldType::Type WorldType)
{
	return WorldType == EWorldType::PIE;
}

// ---------------------------------------------------------------------------------------------------------------------
// Alt sistem
// ---------------------------------------------------------------------------------------------------------------------

bool UMMOServerLifecycleSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer)) return false;
	// Listen surucusu InitWorld'den SONRA kurulur: burada NM_Standalone gorunebilir. Istemci dunyasi hic olusmasin;
	// asil Dedicated/Listen karari OnWorldBeginPlay'de verilir (bServerActive).
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_Client;
}

void UMMOServerLifecycleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Yeni bir PIE oturumu onceki oturumdan kalan statik plani miras almasin (gercek ServerTravel'da dunya turu Game: korunur).
	const UWorld* World = GetWorld();
	if (World && MMOServerLifecycle::ResetsPlanWithWorldType(World->WorldType))
	{
		LifecycleResetState();
	}
}

void UMMOServerLifecycleSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bServerActive = MMOServerLifecycle::IsExecAuthorized(InWorld.GetNetMode());
	if (!bServerActive) return;
	// Cift abonelik olmasin (OnWorldBeginPlay tekrar gelirse eski handle once kaldirilir).
	if (PreLoginHandle.IsValid())
	{
		FGameModeEvents::OnGameModePreLoginEvent().Remove(PreLoginHandle);
		PreLoginHandle.Reset();
	}
	PreLoginHandle = FGameModeEvents::OnGameModePreLoginEvent().AddUObject(this, &UMMOServerLifecycleSubsystem::HandlePreLogin);
	UE_LOG(LogMMOLifecycle, Display, TEXT("event=lifecycle_ready netmode=%d phase=%s"), static_cast<int32>(InWorld.GetNetMode()), LifecyclePhaseName(LifecyclePlan.GetPhase()));
	TryScheduleFromCommandLine(FCommandLine::Get());
}

void UMMOServerLifecycleSubsystem::Deinitialize()
{
	if (PreLoginHandle.IsValid())
	{
		FGameModeEvents::OnGameModePreLoginEvent().Remove(PreLoginHandle);
		PreLoginHandle.Reset();
	}
	bServerActive = false;
	// PIE dunyasi ortadan kalkinca plan da gider; ServerTravel'da (Game dunyasi) surec-geneli plan korunur.
	const UWorld* World = GetWorld();
	if (World && MMOServerLifecycle::ResetsPlanWithWorldType(World->WorldType))
	{
		LifecycleResetState();
	}
	Super::Deinitialize();
}

bool UMMOServerLifecycleSubsystem::IsTickable() const
{
	// Exiting'de de tick eder (oyuncularin yok olmasini bekleyip sureci kapatir); cikis istendikten sonra durur.
	return bServerActive && LifecyclePlan.GetPhase() != EMMOShutdownPhase::Idle && !bLifecycleExitRequested;
}

TStatId UMMOServerLifecycleSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMMOServerLifecycleSubsystem, STATGROUP_Tickables);
}

void UMMOServerLifecycleSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// Gercek (duvar) saati: oyun zamani dilation'i ya da dunya gecisi yukleme suresi geri sayimi bozmaz; buyuk fark plan'da olay atlatmaz.
	TickWithWallClock(FPlatformTime::Seconds());
	StepShutdownWork(); // L2: Flushing/Kicking'te Tick basina bir oyuncu kaydi + sonucu belli olanlarin atilmasi
	StepExit(FPlatformTime::Seconds()); // L3: Exiting'de oyuncularin yok olmasini bekle, sonra sureci kapat
}

void UMMOServerLifecycleSubsystem::TickWithWallClock(double NowSeconds)
{
	if (!bServerActive || LifecyclePlan.GetPhase() == EMMOShutdownPhase::Idle) return;
	// Son tick zamani yoksa (0) ilk tick yalniz saati kaydeder: iki oturum arasi bosta gecen sure tek hamlede sayilmaz.
	const double Delta = LifecycleLastTickTime > 0.0 ? NowSeconds - LifecycleLastTickTime : 0.0;
	LifecycleLastTickTime = NowSeconds;
	AdvanceSeconds(Delta);
}

void UMMOServerLifecycleSubsystem::AdvanceSeconds(double DeltaSeconds)
{
	if (!bServerActive) return;
	TArray<FMMOShutdownEvent> Events;
	LifecyclePlan.Advance(DeltaSeconds, Events);
	ProcessEvents(Events);
}

bool UMMOServerLifecycleSubsystem::RequestShutdown(double Seconds, EMMOShutdownReason Reason, FString& OutError)
{
	if (!bServerActive)
	{
		OutError = TEXT("Bu dunya bir sunucu degil.");
		return false;
	}
	TArray<FMMOShutdownEvent> Events;
	const EMMOShutdownScheduleResult Result = LifecyclePlan.Schedule(Seconds, Reason, Events);
	if (Result == EMMOShutdownScheduleResult::AlreadyActive)
	{
		OutError = TEXT("Zaten bir kapatma planli; once MMOShutdownCancel.");
		return false;
	}
	if (Result == EMMOShutdownScheduleResult::OutOfRange)
	{
		OutError = FString::Printf(TEXT("Sure %d..%d saniye araliginda olmali."), static_cast<int32>(FMMOShutdownPlan::MinDelaySeconds), static_cast<int32>(FMMOShutdownPlan::MaxDelaySeconds));
		return false;
	}
	LifecycleLastTickTime = FPlatformTime::Seconds();
	UE_LOG(LogMMOLifecycle, Display, TEXT("event=shutdown_scheduled seconds=%.0f reason=%s"), Seconds, MMOServerLifecycle::ReasonName(Reason));
	ProcessEvents(Events);
	return true;
}

bool UMMOServerLifecycleSubsystem::RequestCancel(FString& OutError)
{
	if (!bServerActive)
	{
		OutError = TEXT("Bu dunya bir sunucu degil.");
		return false;
	}
	TArray<FMMOShutdownEvent> Events;
	if (!LifecyclePlan.Cancel(Events))
	{
		OutError = TEXT("Iptal yalniz geri sayim ya da giris kapatma asamasinda mumkun.");
		return false;
	}
	ProcessEvents(Events);
	return true;
}

bool UMMOServerLifecycleSubsystem::IsLoginClosed() const { return LifecyclePlan.LoginClosed(); }
EMMOShutdownPhase UMMOServerLifecycleSubsystem::GetPhase() const { return LifecyclePlan.GetPhase(); }
double UMMOServerLifecycleSubsystem::GetRemainingSeconds() const { return LifecyclePlan.GetRemainingSeconds(); }
EMMOShutdownReason UMMOServerLifecycleSubsystem::GetReason() const { return LifecyclePlan.GetReason(); }

void UMMOServerLifecycleSubsystem::ProcessEvents(const TArray<FMMOShutdownEvent>& Events)
{
	const int32 Players = LifecycleCountPlayers(GetWorld());
	for (const FMMOShutdownEvent& E : Events)
	{
		if (E.Type == EMMOShutdownEventType::PhaseChanged)
		{
			UE_LOG(LogMMOLifecycle, Display, TEXT("event=shutdown_phase phase=%s remaining=%.0f players=%d"), LifecyclePhaseName(E.Phase), LifecyclePlan.GetRemainingSeconds(), Players);
			UE_LOG(LogMMOLifecycle, Display, TEXT("[MMOMETRIC] t=%.1f ev=shutdown phase=%s players=%d"), MMOMetrics::Seconds(), LifecyclePhaseName(E.Phase), Players);
			if (E.Phase == EMMOShutdownPhase::Flushing)
			{
				BeginFlushRound(); // L2: tek seferlik toplu kayit turu (Tick basina bir oyuncu)
			}
			else if (E.Phase == EMMOShutdownPhase::Exiting)
			{
				UE_LOG(LogMMOLifecycle, Display, TEXT("event=shutdown_exiting players=%d"), Players);
				DrainShutdownWork(); // zaman doldu: kalan kayitlar senkron biter, sonra atilir (kayitsiz atma yok); cikis StepExit'te
			}
		}
		else if (E.Type == EMMOShutdownEventType::Cancelled)
		{
			UE_LOG(LogMMOLifecycle, Display, TEXT("event=shutdown_cancelled players=%d"), Players);
		}
		FString Text;
		if (MMOServerLifecycle::EventText(E, Text))
		{
			BroadcastToPlayers(Text);
		}
	}
}

void UMMOServerLifecycleSubsystem::GatherServerWorlds(TArray<UWorld*>& OutWorlds) const
{
	// Bolge gecisinde (ServerTravel) eski ve yeni dunya bir sure birlikte yasar, oyuncular hangisinde olursa olsun duyuruyu/kaydi almali:
	// bu alt sistemin dunyasi + tum dunya baglamlarindaki istemci olmayan oyun dunyalari (tekrar yok).
	if (UWorld* Own = GetWorld()) OutWorlds.Add(Own);
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* W = Context.World();
			if (W && W->IsGameWorld() && W->GetNetMode() != NM_Client) OutWorlds.AddUnique(W);
		}
	}
}

bool UMMOServerLifecycleSubsystem::BeginFlushRound()
{
	if (!bServerActive || bLifecycleRoundStarted) return false; // tur bir kez: ikinci cagri kimseye ikinci yazim yapmaz
	bLifecycleRoundStarted = true;
	LifecycleEntries.Reset();
	LifecycleNextFlush = 0;
	// Aktif ticaretler kayittan ONCE iptal edilir: kayit turu ile takas tamamlanmasi yarismasin (yeni ticaret IsFlushingOrLater ile reddedilir).
	CancelActiveTrades();
	UE_LOG(LogMMOLifecycle, Display, TEXT("event=shutdown_sidefiles mode=endplay_trust note=basarim-gunluk-yaris-SaveNow-private-bu-sistem-cagirmaz"));
	TArray<UWorld*> Worlds;
	GatherServerWorlds(Worlds);
	for (UWorld* World : Worlds)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (!PC || !LifecycleIncludesPlayer(PC) || LifecycleFindEntry(PC)) continue;
			FLifecycleFlushEntry& Entry = LifecycleEntries.AddDefaulted_GetRef();
			Entry.PC = PC;
			Entry.Who = LifecycleWhoName(PC);
		}
	}
	UE_LOG(LogMMOLifecycle, Display, TEXT("event=shutdown_flush_begin players=%d"), LifecycleEntries.Num());
	if (LifecycleEntries.IsEmpty())
	{
		FinishFlushRound(); // kimse yok: Flushing erken biter
	}
	return true;
}

bool UMMOServerLifecycleSubsystem::FlushNextPlayer()
{
	while (LifecycleNextFlush < LifecycleEntries.Num() && LifecycleEntries[LifecycleNextFlush].bResolved) ++LifecycleNextFlush;
	if (LifecycleNextFlush >= LifecycleEntries.Num()) return false;
	FLifecycleFlushEntry& Entry = LifecycleEntries[LifecycleNextFlush];
	const APlayerController* PC = Entry.PC.Get();
	AMMOCharacter* Character = PC ? Cast<AMMOCharacter>(PC->GetPawn()) : nullptr;
	// Denetleyici kayit adiminda yok olmus (oyuncu ayrildi): pawn'siz oyuncudan ayri neden; kendi EndPlay kaydini yazmistir.
	Entry.Result = PC ? MMOServerLifecycle::SaveCharacterNow(Character) : EMMOLifecycleSaveResult::SkippedGone;
	Entry.bResolved = true;
	++LifecycleNextFlush;
	UE_LOG(LogMMOLifecycle, Display, TEXT("event=shutdown_save player=%s result=%s"), *Entry.Who, MMOServerLifecycle::SaveResultName(Entry.Result));
	if (Entry.Result == EMMOLifecycleSaveResult::Failed)
	{
		// Ilk deneme basarisiz; atma aninda bir kez daha denenir (event=shutdown_save_retry), son sonuc cikis kodunu belirler.
		UE_LOG(LogMMOLifecycle, Warning, TEXT("[MMOMETRIC] t=%.1f ev=shutdown_save_failed player=%s"), MMOMetrics::Seconds(), *Entry.Who);
	}
	if (LifecycleNextFlush >= LifecycleEntries.Num() && !LifecyclePlan.IsFlushDone())
	{
		FinishFlushRound();
	}
	return true;
}

void UMMOServerLifecycleSubsystem::FinishFlushRound()
{
	if (!LifecyclePlan.NotifyFlushDone()) return;
	const FMMOLifecycleFlushSummary S = GetFlushSummary();
	UE_LOG(LogMMOLifecycle, Display, TEXT("[MMOMETRIC] t=%.1f ev=shutdown phase=flush_done players=%d saved=%d failed=%d skipped=%d"),
		MMOMetrics::Seconds(), S.Players, S.Saved, S.Failed, S.Skipped());
}

void UMMOServerLifecycleSubsystem::KickResolvedPlayers(int32 MaxKicks)
{
	// Atma kapisi: kayit turu bitmeden kimse atilmaz. Ek olarak her oyuncu kendi kayit sonucu belli olmadan atilmaz.
	if (!LifecyclePlan.KickAllowed()) return;
	TArray<UWorld*> Worlds;
	GatherServerWorlds(Worlds);
	TArray<TWeakObjectPtr<APlayerController>> Targets;
	for (UWorld* World : Worlds)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && LifecycleIncludesPlayer(PC)) Targets.AddUnique(PC);
		}
	}
	// KickPlayer denetleyiciyi yok eder: once listeyi topla, sonra at.
	for (const TWeakObjectPtr<APlayerController>& Target : Targets)
	{
		APlayerController* PC = Target.Get();
		if (!PC) continue;
		FLifecycleFlushEntry* Entry = LifecycleFindEntry(PC);
		if (!Entry)
		{
			// Tur kurulduktan sonra gelen oyuncu: once kayit sirasina girer, bir sonraki adimda atilir.
			FLifecycleFlushEntry& Late = LifecycleEntries.AddDefaulted_GetRef();
			Late.PC = PC;
			Late.Who = LifecycleWhoName(PC);
			continue;
		}
		if (!Entry->bResolved || Entry->bKickDone) continue;
		if (MaxKicks <= 0) break; // Tick basina sinir: kalanlar sonraki adimda (kayit/atma yuku tek karede yigilmasin)
		// Atma aninda yeniden dene/degerlendir (bir kez): ilk kayit basarisizsa tekrar yaz; pawn yoktu / yuklenmemisti ise simdi
		// olusmus olabilir. Sonuc (ve cikis kodu) bu son duruma gore belirlenir; atmadan sonraki EndPlay kaydi sayilmaz.
		if (!Entry->bRetried)
		{
			Entry->bRetried = true;
			if (Entry->Result == EMMOLifecycleSaveResult::Failed || Entry->Result == EMMOLifecycleSaveResult::SkippedUnloaded || Entry->Result == EMMOLifecycleSaveResult::SkippedNoPawn)
			{
				const EMMOLifecycleSaveResult Previous = Entry->Result;
				Entry->Result = MMOServerLifecycle::SaveCharacterNow(Cast<AMMOCharacter>(PC->GetPawn()));
				UE_LOG(LogMMOLifecycle, Display, TEXT("event=shutdown_save_retry player=%s previous=%s result=%s"), *Entry->Who, MMOServerLifecycle::SaveResultName(Previous), MMOServerLifecycle::SaveResultName(Entry->Result));
			}
		}
		const bool bHost = LifecycleIsHost(PC);
		++Entry->KickAttempts;
		--MaxKicks;
		Entry->bKicked = MMOServerLifecycle::KickPlayerNow(PC, FText::FromString(MMOServerLifecycle::KickText(Entry->Result)));
		// Uzak oyuncuda KickPlayer false donerse sonraki adimda yeniden dene; ev sahibi atilamaz (yeniden deneme anlamsiz).
		if (Entry->bKicked || bHost || Entry->KickAttempts >= MMOServerLifecycle::MaxKickAttempts) Entry->bKickDone = true;
		UE_LOG(LogMMOLifecycle, Display, TEXT("event=shutdown_kick player=%s result=%s kicked=%d attempt=%d"), *Entry->Who, MMOServerLifecycle::SaveResultName(Entry->Result), Entry->bKicked ? 1 : 0, Entry->KickAttempts);
	}
}

void UMMOServerLifecycleSubsystem::StepShutdownWork()
{
	if (!bServerActive) return;
	if (bLifecycleRoundStarted) FlushNextPlayer(); // oyuncu basina bir kare (hitch olmasin)
	KickResolvedPlayers(MMOServerLifecycle::MaxKicksPerTick);
}

void UMMOServerLifecycleSubsystem::DrainShutdownWork()
{
	if (!bServerActive) return;
	if (!bLifecycleRoundStarted) BeginFlushRound();
	// Gec gelenler her gecis birkac yeni kayit ekleyebilir; uzak oyuncunun atmasi en cok MaxKickAttempts gecis ister: sinirli sayida gecis.
	for (int32 Pass = 0; Pass < 6; ++Pass)
	{
		while (FlushNextPlayer()) {}
		KickResolvedPlayers(MAX_int32); // Drain'de Tick siniri yok (zaman doldu)
		bool bPending = false;
		for (const FLifecycleFlushEntry& E : LifecycleEntries)
		{
			if (!E.bResolved || (!E.bKickDone && E.PC.IsValid())) { bPending = true; break; }
		}
		if (!bPending) break;
	}
}

int32 UMMOServerLifecycleSubsystem::CountExitWaitingPlayers() const
{
	TArray<UWorld*> Worlds;
	GatherServerWorlds(Worlds);
	TArray<const APlayerController*> Seen;
	for (UWorld* World : Worlds)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			if (PC && LifecycleIncludesPlayer(PC) && !LifecycleIsHost(PC)) Seen.AddUnique(PC);
		}
	}
	return Seen.Num();
}

void UMMOServerLifecycleSubsystem::CancelActiveTrades()
{
	TArray<UWorld*> Worlds;
	GatherServerWorlds(Worlds);
	int32 Cancelled = 0;
	for (UWorld* World : Worlds)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			const AMMOCharacter* Character = PC ? Cast<AMMOCharacter>(PC->GetPawn()) : nullptr;
			UMMOTradeComponent* Trade = Character ? Character->GetTradeComponent() : nullptr;
			if (Trade && Trade->IsTrading())
			{
				Trade->RequestCancelSeen(Trade->TradeSessionId); // sunucuda ServerCancel_Implementation: iki tarafi da temizler
				++Cancelled;
			}
		}
	}
	UE_LOG(LogMMOLifecycle, Display, TEXT("event=shutdown_trades_cancelled sides=%d"), Cancelled);
}

void UMMOServerLifecycleSubsystem::StepExit(double NowSeconds)
{
	if (!bServerActive || bLifecycleExitRequested || LifecyclePlan.GetPhase() != EMMOShutdownPhase::Exiting) return;
	if (LifecycleExitWaitStart < 0.0) LifecycleExitWaitStart = NowSeconds;
	DrainShutdownWork(); // Exiting'de gelen/kalan oyuncular: once kayit, sonra atma
	const int32 Waiting = CountExitWaitingPlayers();
	const bool bTimedOut = NowSeconds - LifecycleExitWaitStart >= MMOServerLifecycle::ExitWaitMaxSeconds;
	if (Waiting > 0 && !bTimedOut) return;
	if (Waiting > 0)
	{
		UE_LOG(LogMMOLifecycle, Warning, TEXT("event=shutdown_exit_wait_timeout waiting=%d max_seconds=%.0f"), Waiting, MMOServerLifecycle::ExitWaitMaxSeconds);
	}
	FinishExit();
}

bool UMMOServerLifecycleSubsystem::WasExitRequested()
{
	return bLifecycleExitRequested;
}

void UMMOServerLifecycleSubsystem::FinishExit()
{
	if (bLifecycleExitRequested) return;
	bLifecycleExitRequested = true; // bir kez; ayrica IsTickable'i kapatir
	// Karar: yalniz tur + atma aninda yeniden deneme sonucu sayilir. Atmadan sonra karakterin EndPlay'inde yapilan kayit KATILMAZ
	// (iyi niyetli ek yazim; basarisi/basarisizligi cikis kodunu degistirmez). Kararsiz (Pending) oyuncu da basarisiz sayilir.
	const FMMOLifecycleFlushSummary S = GetFlushSummary();
	const int32 Failed = S.Failed + S.Pending;
	const int32 Code = MMOServerLifecycle::ExitCodeForFailures(Failed);
	UE_LOG(LogMMOLifecycle, Display, TEXT("[MMOMETRIC] t=%.1f ev=shutdown_exit code=%d failed=%d"), MMOMetrics::Seconds(), Code, Failed);
#if !UE_BUILD_SHIPPING
	if (LifecycleExitHook)
	{
		LifecycleExitHook(Code, Failed);
		return;
	}
	if (GIsEditor)
	{
		// Editor/PIE oturumunu kapatma: komut-konsolu denemelerinde editor surecini oldurmeyelim.
		UE_LOG(LogMMOLifecycle, Warning, TEXT("event=shutdown_exit_skipped_editor code=%d"), Code);
		return;
	}
#endif
	FPlatformMisc::RequestExitWithStatus(false, static_cast<uint8>(Code), TEXT("MMOServerLifecycle"));
}

FMMOLifecycleFlushSummary UMMOServerLifecycleSubsystem::GetFlushSummary() const
{
	FMMOLifecycleFlushSummary S;
	S.bRoundStarted = bLifecycleRoundStarted;
	S.bFlushDone = LifecyclePlan.IsFlushDone();
	S.Players = LifecycleEntries.Num();
	for (const FLifecycleFlushEntry& E : LifecycleEntries)
	{
		if (!E.bResolved) { ++S.Pending; continue; }
		switch (E.Result)
		{
		case EMMOLifecycleSaveResult::Saved: ++S.Saved; break;
		case EMMOLifecycleSaveResult::Failed: ++S.Failed; break;
		case EMMOLifecycleSaveResult::SkippedUnloaded: ++S.SkippedUnloaded; break;
		case EMMOLifecycleSaveResult::SkippedTravelCommitted: ++S.SkippedTravelCommitted; break;
		case EMMOLifecycleSaveResult::SkippedNoPawn: ++S.SkippedNoPawn; break;
		case EMMOLifecycleSaveResult::SkippedGone: ++S.SkippedGone; break;
		}
		if (E.bKicked) ++S.Kicked;
	}
	return S;
}

bool UMMOServerLifecycleSubsystem::GetFlushResultFor(const APlayerController* PC, EMMOLifecycleSaveResult& OutResult, bool& bOutKicked) const
{
	for (const FLifecycleFlushEntry& E : LifecycleEntries)
	{
		if (E.PC.Get() == PC && E.bResolved)
		{
			OutResult = E.Result;
			bOutKicked = E.bKicked;
			return true;
		}
	}
	return false;
}

void UMMOServerLifecycleSubsystem::BroadcastToPlayers(const FString& Message)
{
	LastRecipientCount = 0;
	TArray<UWorld*> Worlds;
	GatherServerWorlds(Worlds);
	for (UWorld* World : Worlds)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			// Mevcut sistem sohbet satiri (kanal 2); mob dunya duyurusu (MMOMob.cpp) ile ayni yol.
			if (AMMOPlayerController* PC = Cast<AMMOPlayerController>(It->Get()))
			{
				PC->ClientReceiveChat(FString(), Message, 2);
				++LastRecipientCount;
			}
		}
	}
	SentMessages.Add(Message);
	if (SentMessages.Num() > 32) SentMessages.RemoveAt(0);
}

void UMMOServerLifecycleSubsystem::HandlePreLogin(AGameModeBase* GameMode, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	if (!bServerActive || !ErrorMessage.IsEmpty()) return;
	if (GameMode && GameMode->GetWorld() != GetWorld()) return;
	if (LifecyclePlan.LoginClosed())
	{
		ErrorMessage = MMOServerLifecycle::LoginRejectText();
		UE_LOG(LogMMOLifecycle, Display, TEXT("event=prelogin_rejected phase=%s"), LifecyclePhaseName(LifecyclePlan.GetPhase()));
	}
}

void UMMOServerLifecycleSubsystem::TryScheduleFromCommandLine(const TCHAR* CmdLine)
{
	if (bLifecycleCommandLineHandled) return; // surec basina bir kez (bolge gecisinde yeni dunya yeniden planlamaz)
	double Seconds = 0.0;
	EMMOShutdownReason Reason = EMMOShutdownReason::Maintenance;
	FString Error;
	const MMOServerLifecycle::ECmdLineShutdown Parsed = MMOServerLifecycle::ParseCmdLineShutdown(CmdLine, Seconds, Reason, Error);
	if (Parsed == MMOServerLifecycle::ECmdLineShutdown::NotRequested) return;
	bLifecycleCommandLineHandled = true; // anahtar varsa gecersiz de olsa tuketilir (yeniden denenmez)
	if (Parsed == MMOServerLifecycle::ECmdLineShutdown::Invalid || !RequestShutdown(Seconds, Reason, Error))
	{
		UE_LOG(LogMMOLifecycle, Error, TEXT("event=shutdown_cmdline_rejected error=%s"), *Error);
	}
}

#if !UE_BUILD_SHIPPING

void UMMOServerLifecycleSubsystem::SetSaveHookForTest(TFunction<bool(AMMOCharacter*)> Hook)
{
	LifecycleSaveHook = MoveTemp(Hook);
}

void UMMOServerLifecycleSubsystem::SetKickHookForTest(TFunction<bool(APlayerController*, const FText&)> Hook)
{
	LifecycleKickHook = MoveTemp(Hook);
}

void UMMOServerLifecycleSubsystem::SetExitHookForTest(TFunction<void(int32, int32)> Hook)
{
	LifecycleExitHook = MoveTemp(Hook);
}

void UMMOServerLifecycleSubsystem::SetPlayerFilterForTest(TFunction<bool(const APlayerController*)> Filter)
{
	LifecyclePlayerFilter = MoveTemp(Filter);
}

void UMMOServerLifecycleSubsystem::ResetPlanForTest()
{
	LifecycleResetState();
}

void UMMOServerLifecycleSubsystem::SetLastTickTimeForTest(double Seconds)
{
	LifecycleLastTickTime = Seconds;
}

bool UMMOServerLifecycleSubsystem::HandleExecCommand(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Komut metni bu sinifa ait degilse dunya aramadan don.
	const TCHAR* ProbeA = Cmd;
	const TCHAR* ProbeB = Cmd;
	if (!FParse::Command(&ProbeA, TEXT("MMOShutdown")) && !FParse::Command(&ProbeB, TEXT("MMOShutdownCancel"))) return false;
	// Editor Cmd cubugundan gelen dunya editor dunyasi olabilir (game world degil): o zaman PIE/oyun dunyasini ara.
	if ((!World || !World->IsGameWorld()) && GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World() && Context.World()->IsGameWorld() && Context.World()->GetSubsystem<UMMOServerLifecycleSubsystem>())
			{
				World = Context.World();
				break;
			}
		}
	}
	return HandleExecCommandForNetMode(World, World ? World->GetNetMode() : NM_Standalone, Cmd, Ar);
}

bool UMMOServerLifecycleSubsystem::HandleExecCommandForNetMode(UWorld* World, ENetMode NetMode, const TCHAR* Cmd, FOutputDevice& Ar)
{
	const TCHAR* Stream = Cmd;
	const bool bCancel = FParse::Command(&Stream, TEXT("MMOShutdownCancel"));
	if (!bCancel)
	{
		Stream = Cmd;
		if (!FParse::Command(&Stream, TEXT("MMOShutdown"))) return false;
	}
	// R1 yetki kapisi: yalniz sunucu dunyasi.
	if (!MMOServerLifecycle::IsExecAuthorized(NetMode))
	{
		Ar.Log(TEXT("MMOShutdown: yalniz sunucuda calisir."));
		UE_LOG(LogMMOLifecycle, Warning, TEXT("event=exec_refused netmode=%d"), static_cast<int32>(NetMode));
		return true;
	}
	UMMOServerLifecycleSubsystem* Subsystem = World ? World->GetSubsystem<UMMOServerLifecycleSubsystem>() : nullptr;
	if (!Subsystem || !Subsystem->IsServerActive())
	{
		Ar.Log(TEXT("MMOShutdown: sunucu yasam dongusu alt sistemi yok."));
		return true;
	}
	FString Error;
	if (bCancel)
	{
		Ar.Log(Subsystem->RequestCancel(Error) ? TEXT("MMOShutdown: iptal edildi.") : *(FString(TEXT("MMOShutdown: ")) + Error));
		return true;
	}
	double Seconds = 0.0;
	EMMOShutdownReason Reason = EMMOShutdownReason::Maintenance;
	if (!MMOServerLifecycle::ParseShutdownArgs(Stream, Seconds, Reason, Error) || !Subsystem->RequestShutdown(Seconds, Reason, Error))
	{
		Ar.Log(*(FString(TEXT("MMOShutdown: ")) + Error));
		return true;
	}
	Ar.Logf(TEXT("MMOShutdown: %.0f saniye sonra kapanacak (%s)."), Seconds, MMOServerLifecycle::ReasonName(Reason));
	return true;
}

namespace
{
	struct FMMOLifecycleExec final : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			return UMMOServerLifecycleSubsystem::HandleExecCommand(World, Cmd, Ar);
		}
	};
	FMMOLifecycleExec GMMOLifecycleExec;
}

#endif

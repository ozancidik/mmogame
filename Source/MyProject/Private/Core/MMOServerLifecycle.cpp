// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/MMOServerLifecycle.h"
#include "Core/MMOMetrics.h"
#include "Player/MMOPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
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

	void LifecycleResetState()
	{
		LifecyclePlan = FMMOShutdownPlan();
		LifecycleLastTickTime = 0.0;
		bLifecycleCommandLineHandled = false;
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

bool MMOServerLifecycle::IsCmdLineTriggerSupported()
{
#if UE_BUILD_SHIPPING
	return false; // L3 (cikis kodu) gelene kadar: plan Exiting'e varsa surec sonlanmaz, giris sonsuza dek kapali kalir.
#else
	return true;
#endif
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
	TryScheduleFromCommandLine(FCommandLine::Get(), MMOServerLifecycle::IsCmdLineTriggerSupported());
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
	// Exiting son durumdur ve L1'de cikis yoktur: bosuna tick etme (Exiting'e giriste Warning loglanir).
	const EMMOShutdownPhase Phase = LifecyclePlan.GetPhase();
	return bServerActive && Phase != EMMOShutdownPhase::Idle && Phase != EMMOShutdownPhase::Exiting;
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
			if (E.Phase == EMMOShutdownPhase::Exiting)
			{
				UE_LOG(LogMMOLifecycle, Warning, TEXT("event=shutdown_exiting_not_implemented note=cikis-kodu-L3-yok-surec-SONLANMAZ-giris-kapali-kalir players=%d"), Players);
			}
			else if (E.Phase >= EMMOShutdownPhase::Flushing)
			{
				UE_LOG(LogMMOLifecycle, Display, TEXT("event=shutdown_phase_note phase=%s note=kayit-turu-atma-cikis-L2-L3-yok-surec-sonlanmaz"), LifecyclePhaseName(E.Phase));
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

void UMMOServerLifecycleSubsystem::BroadcastToPlayers(const FString& Message)
{
	LastRecipientCount = 0;
	// Bolge gecisinde (ServerTravel) eski ve yeni dunya bir sure birlikte yasar, oyuncular hangisinde olursa olsun duyuruyu almali:
	// bu alt sistemin dunyasi + tum dunya baglamlarindaki istemci olmayan oyun dunyalari (tekrar yok).
	TArray<UWorld*> Worlds;
	if (UWorld* Own = GetWorld()) Worlds.Add(Own);
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* W = Context.World();
			if (W && W->IsGameWorld() && W->GetNetMode() != NM_Client) Worlds.AddUnique(W);
		}
	}
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

void UMMOServerLifecycleSubsystem::TryScheduleFromCommandLine(const TCHAR* CmdLine, bool bTriggerSupported)
{
	if (bLifecycleCommandLineHandled) return; // surec basina bir kez (bolge gecisinde yeni dunya yeniden planlamaz)
	double Seconds = 0.0;
	EMMOShutdownReason Reason = EMMOShutdownReason::Maintenance;
	FString Error;
	const MMOServerLifecycle::ECmdLineShutdown Parsed = MMOServerLifecycle::ParseCmdLineShutdown(CmdLine, Seconds, Reason, Error);
	if (Parsed == MMOServerLifecycle::ECmdLineShutdown::NotRequested) return;
	bLifecycleCommandLineHandled = true; // anahtar varsa gecersiz de olsa tuketilir (yeniden denenmez)
	if (!bTriggerSupported)
	{
		UE_LOG(LogMMOLifecycle, Warning, TEXT("event=shutdown_cmdline_unsupported note=bu-surumde-desteklenmez-L3-bekleniyor"));
		return;
	}
	if (Parsed == MMOServerLifecycle::ECmdLineShutdown::Invalid || !RequestShutdown(Seconds, Reason, Error))
	{
		UE_LOG(LogMMOLifecycle, Error, TEXT("event=shutdown_cmdline_rejected error=%s"), *Error);
	}
}

#if !UE_BUILD_SHIPPING

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

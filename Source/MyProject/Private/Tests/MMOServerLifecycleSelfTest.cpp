// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/MMOServerLifecycle.h"

#if !UE_BUILD_SHIPPING
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Player/MMOPlayerController.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Misc/StringOutputDevice.h"

// Sunucu yasam dongusu L1 native testi (docs/DESIGN_SERVER_LIFECYCLE.md 7, dilim L1). CANLI dunya ister: listen sunucu
// (Map?listen -game; Scripts/TestServerLifecycle.ps1). Kapsam: duyuru metinleri, komut ayristirma, PreLogin reddi (gercek
// AGameModeBase::PreLogin -> motor olayi), R1 yetki kapisi ve uzak istemci ServerExec benzetimi. Kayit turu/atma/cikis L2/L3.
// Her Check benzersiz case/check cifti basar. Anonim namespace adlari LifecycleTest onekli (unity-build tuzagi).
namespace
{
	constexpr int32 LifecycleTestExpectedChecks = 90;

	UWorld* LifecycleTestFindWorld(UWorld* Hint)
	{
		if (Hint && Hint->IsGameWorld()) return Hint;
		if (GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.World() && Context.World()->IsGameWorld()) return Context.World();
			}
		}
		return nullptr;
	}

	FString LifecycleTestPreLogin(AGameModeBase* GameMode)
	{
		FString Error;
		GameMode->PreLogin(TEXT("?Name=LifecycleProbe"), TEXT("127.0.0.1"), FUniqueNetIdRepl(), Error);
		return Error;
	}

	bool LifecycleTestRun(UWorld* InWorld, FString& OutError, int32& OutChecks)
	{
		OutChecks = 0;
		OutError.Reset();
		int32 Failures = 0;
		const auto Check = [&](const TCHAR* Case, const TCHAR* Label, bool bOK)
		{
			++OutChecks;
			UE_LOG(LogTemp, Display, TEXT("[MMO_LIFECYCLE_CASE] %s case=%s check=%s"), bOK ? TEXT("PASS") : TEXT("FAIL"), Case, Label);
			if (!bOK) { ++Failures; if (OutError.IsEmpty()) OutError = FString(Case) + TEXT("/") + Label; }
		};
		using P = EMMOShutdownPhase;
		using R = EMMOShutdownReason;
		using Ev = FMMOShutdownEvent;
		using ET = EMMOShutdownEventType;
		namespace L = MMOServerLifecycle;

		// 1) Oyuncuya gorunen metinler (sabit sozluk).
		{
			Check(TEXT("text"), TEXT("announce_60"), L::AnnounceText(60) == TEXT("Bakim: sunucu 1 dakika icinde kapanacak. Ilerlemen kaydedilecek."));
			Check(TEXT("text"), TEXT("announce_3600"), L::AnnounceText(3600) == TEXT("Bakim: sunucu 60 dakika icinde kapanacak. Ilerlemen kaydedilecek."));
			Check(TEXT("text"), TEXT("announce_120"), L::AnnounceText(120) == TEXT("Bakim: sunucu 2 dakika icinde kapanacak. Ilerlemen kaydedilecek."));
			Check(TEXT("text"), TEXT("announce_30"), L::AnnounceText(30) == TEXT("Bakim: sunucu 30 saniye icinde kapaniyor."));
			Check(TEXT("text"), TEXT("announce_125"), L::AnnounceText(125) == TEXT("Bakim: sunucu 125 saniye icinde kapaniyor."));
			Check(TEXT("text"), TEXT("announce_1"), L::AnnounceText(1) == TEXT("Bakim: sunucu 1 saniye icinde kapaniyor."));
			FString T;
			Ev Closing; Closing.Type = ET::PhaseChanged; Closing.Phase = P::Closing;
			Check(TEXT("text"), TEXT("event_closing"), L::EventText(Closing, T) && T == TEXT("Bakim: yeni girisler kapandi."));
			Ev Cancelled; Cancelled.Type = ET::Cancelled;
			Check(TEXT("text"), TEXT("event_cancelled"), L::EventText(Cancelled, T) && T == TEXT("Bakim iptal edildi."));
			Ev Announce; Announce.Type = ET::Announce; Announce.Seconds = 10;
			Check(TEXT("text"), TEXT("event_announce"), L::EventText(Announce, T) && T == TEXT("Bakim: sunucu 10 saniye icinde kapaniyor."));
			Ev Flushing; Flushing.Type = ET::PhaseChanged; Flushing.Phase = P::Flushing;
			Ev Kicking = Flushing; Kicking.Phase = P::Kicking;
			Ev Exiting = Flushing; Exiting.Phase = P::Exiting;
			Check(TEXT("text"), TEXT("no_text_flushing_kicking_exiting"), !L::EventText(Flushing, T) && !L::EventText(Kicking, T) && !L::EventText(Exiting, T));
		}

		// 2) Neden sozlugu.
		{
			R Out = R::Emergency;
			Check(TEXT("reason"), TEXT("bakim"), L::ParseReason(TEXT("bakim"), Out) && Out == R::Maintenance);
			Check(TEXT("reason"), TEXT("guncelleme"), L::ParseReason(TEXT("guncelleme"), Out) && Out == R::Update);
			Check(TEXT("reason"), TEXT("acil"), L::ParseReason(TEXT("acil"), Out) && Out == R::Emergency);
			Check(TEXT("reason"), TEXT("case_insensitive"), L::ParseReason(TEXT("GUNCELLEME"), Out) && Out == R::Update);
			Check(TEXT("reason"), TEXT("unknown_rejected"), !L::ParseReason(TEXT("serbest metin"), Out));
			Check(TEXT("reason"), TEXT("empty_rejected"), !L::ParseReason(TEXT(""), Out));
			Check(TEXT("reason"), TEXT("name_roundtrip"), FString(L::ReasonName(R::Update)) == TEXT("guncelleme") && FString(L::ReasonName(R::Emergency)) == TEXT("acil"));
		}

		// 3) Komut argumanlari.
		{
			double S = -1.0; R Reason = R::Emergency; FString Err;
			Check(TEXT("args"), TEXT("ok_seconds_default_reason"), L::ParseShutdownArgs(TEXT(" 60"), S, Reason, Err) && S == 60.0 && Reason == R::Maintenance);
			Check(TEXT("args"), TEXT("ok_with_reason"), L::ParseShutdownArgs(TEXT(" 600 acil"), S, Reason, Err) && S == 600.0 && Reason == R::Emergency);
			Check(TEXT("args"), TEXT("ok_fractional"), L::ParseShutdownArgs(TEXT("45.5"), S, Reason, Err) && S == 45.5);
			Check(TEXT("args"), TEXT("empty_rejected"), !L::ParseShutdownArgs(TEXT(""), S, Reason, Err) && !Err.IsEmpty());
			Check(TEXT("args"), TEXT("null_rejected"), !L::ParseShutdownArgs(nullptr, S, Reason, Err));
			Check(TEXT("args"), TEXT("letters_rejected"), !L::ParseShutdownArgs(TEXT("abc"), S, Reason, Err));
			Check(TEXT("args"), TEXT("negative_rejected"), !L::ParseShutdownArgs(TEXT("-5"), S, Reason, Err));
			Check(TEXT("args"), TEXT("exponent_rejected"), !L::ParseShutdownArgs(TEXT("1e3"), S, Reason, Err));
			Check(TEXT("args"), TEXT("hex_rejected"), !L::ParseShutdownArgs(TEXT("0x10"), S, Reason, Err));
			Check(TEXT("args"), TEXT("nan_rejected"), !L::ParseShutdownArgs(TEXT("nan"), S, Reason, Err));
			Check(TEXT("args"), TEXT("inf_rejected"), !L::ParseShutdownArgs(TEXT("inf"), S, Reason, Err));
			Check(TEXT("args"), TEXT("two_dots_rejected"), !L::ParseShutdownArgs(TEXT("1.2.3"), S, Reason, Err));
			Check(TEXT("args"), TEXT("too_long_rejected"), !L::ParseShutdownArgs(TEXT("1234567890123"), S, Reason, Err));
			Check(TEXT("args"), TEXT("bad_reason_rejected"), !L::ParseShutdownArgs(TEXT("60 serbest"), S, Reason, Err));
			Check(TEXT("args"), TEXT("extra_arg_rejected"), !L::ParseShutdownArgs(TEXT("60 acil fazla"), S, Reason, Err));
		}

		// 4) R1 yetki kapisi (saf).
		{
			Check(TEXT("auth"), TEXT("dedicated"), L::IsExecAuthorized(NM_DedicatedServer));
			Check(TEXT("auth"), TEXT("listen"), L::IsExecAuthorized(NM_ListenServer));
			Check(TEXT("auth"), TEXT("standalone_refused"), !L::IsExecAuthorized(NM_Standalone));
			Check(TEXT("auth"), TEXT("client_refused"), !L::IsExecAuthorized(NM_Client));
		}

		// 5) Canli dunya on kosullari; saglanmazsa kalan vakalar anlamsiz (cokme riski) -> durur.
		UWorld* World = LifecycleTestFindWorld(InWorld);
		UMMOServerLifecycleSubsystem* Sub = World ? World->GetSubsystem<UMMOServerLifecycleSubsystem>() : nullptr;
		AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
		Check(TEXT("world"), TEXT("world_found"), World != nullptr);
		Check(TEXT("world"), TEXT("netmode_listen"), World && World->GetNetMode() == NM_ListenServer);
		Check(TEXT("world"), TEXT("subsystem_exists"), Sub != nullptr);
		Check(TEXT("world"), TEXT("server_active"), Sub && Sub->IsServerActive());
		Check(TEXT("world"), TEXT("gamemode_exists"), GameMode != nullptr);
		if (!World || !Sub || !Sub->IsServerActive() || !GameMode)
		{
			if (Failures) OutError = FString::Printf(TEXT("failures=%d first=%s"), Failures, *OutError);
			return false;
		}
		UMMOServerLifecycleSubsystem::ResetPlanForTest();
		Check(TEXT("world"), TEXT("idle_at_start"), Sub->GetPhase() == P::Idle && !Sub->IsLoginClosed());
		Check(TEXT("world"), TEXT("prelogin_open_at_start"), LifecycleTestPreLogin(GameMode).IsEmpty());

		// 6) Konsol komutu ile 60 sn: duyuru + Closing + PreLogin reddi (gercek GameMode->PreLogin -> motor olayi).
		{
			FStringOutputDevice Out;
			const bool bConsumed = UMMOServerLifecycleSubsystem::HandleExecCommand(World, TEXT("MMOShutdown 60 guncelleme"), Out);
			Check(TEXT("sched60"), TEXT("command_consumed"), bConsumed);
			Check(TEXT("sched60"), TEXT("output_ack"), Out.Contains(TEXT("60 saniye sonra kapanacak (guncelleme)")));
			Check(TEXT("sched60"), TEXT("phase_closing"), Sub->GetPhase() == P::Closing && Sub->GetReason() == R::Update);
			Check(TEXT("sched60"), TEXT("login_closed"), Sub->IsLoginClosed());
			const TArray<FString>& Sent = Sub->GetSentMessages();
			Check(TEXT("sched60"), TEXT("message_count"), Sent.Num() == 2);
			Check(TEXT("sched60"), TEXT("message_announce"), Sent.Num() == 2 && Sent[0] == TEXT("Bakim: sunucu 1 dakika icinde kapanacak. Ilerlemen kaydedilecek."));
			Check(TEXT("sched60"), TEXT("message_closed"), Sent.Num() == 2 && Sent[1] == TEXT("Bakim: yeni girisler kapandi."));
			int32 MMOControllers = 0;
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (Cast<AMMOPlayerController>(It->Get())) ++MMOControllers;
			}
			Check(TEXT("sched60"), TEXT("recipient_count_matches_mmo_controllers"), Sub->GetLastRecipientCount() == MMOControllers);
			UE_LOG(LogTemp, Display, TEXT("[MMO_LIFECYCLE_INFO] mmo_controllers=%d"), MMOControllers);
			Check(TEXT("sched60"), TEXT("prelogin_rejected_text"), LifecycleTestPreLogin(GameMode) == MMOServerLifecycle::LoginRejectText());
			// Baska bir hata zaten doluysa bizim ret ustune yazmaz (motor ApproveLogin/diger abone hatasi korunur).
			FString Preset = TEXT("baska_hata");
			auto& Event = FGameModeEvents::OnGameModePreLoginEvent();
			Event.Broadcast(GameMode, FUniqueNetIdRepl(), Preset);
			Check(TEXT("sched60"), TEXT("existing_error_preserved"), Preset == TEXT("baska_hata"));
			FStringOutputDevice Second;
			UMMOServerLifecycleSubsystem::HandleExecCommand(World, TEXT("MMOShutdown 600"), Second);
			Check(TEXT("sched60"), TEXT("second_schedule_refused"), Second.Contains(TEXT("once MMOShutdownCancel")) && Sub->GetRemainingSeconds() <= 60.0);
		}

		// 7) Iptal: Closing'de gecer, giris tekrar acilir.
		{
			FStringOutputDevice Out;
			UMMOServerLifecycleSubsystem::HandleExecCommand(World, TEXT("MMOShutdownCancel"), Out);
			Check(TEXT("cancel"), TEXT("output_ack"), Out.Contains(TEXT("iptal edildi")));
			Check(TEXT("cancel"), TEXT("phase_idle"), Sub->GetPhase() == P::Idle && !Sub->IsLoginClosed());
			Check(TEXT("cancel"), TEXT("message_cancelled"), Sub->GetSentMessages().Num() >= 1 && Sub->GetSentMessages().Last() == TEXT("Bakim iptal edildi."));
			Check(TEXT("cancel"), TEXT("prelogin_open_again"), LifecycleTestPreLogin(GameMode).IsEmpty());
			FStringOutputDevice Again;
			UMMOServerLifecycleSubsystem::HandleExecCommand(World, TEXT("MMOShutdownCancel"), Again);
			Check(TEXT("cancel"), TEXT("idle_cancel_refused"), Again.Contains(TEXT("Iptal yalniz")) && Sub->GetPhase() == P::Idle);
		}

		// 8) 600 sn zaman cizelgesi (sahte sure): esikler sirayla, Closing'de ret, Flushing'de iptal yok.
		{
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			FString Err;
			const int32 B = Sub->GetSentMessages().Num();
			Check(TEXT("timeline"), TEXT("schedule_ok"), Sub->RequestShutdown(600.0, R::Maintenance, Err));
			Check(TEXT("timeline"), TEXT("counting_login_open"), Sub->GetPhase() == P::Counting && LifecycleTestPreLogin(GameMode).IsEmpty());
			Check(TEXT("timeline"), TEXT("first_message_600"), Sub->GetSentMessages().Num() == B + 1 && Sub->GetSentMessages()[B] == TEXT("Bakim: sunucu 10 dakika icinde kapanacak. Ilerlemen kaydedilecek."));
			Sub->AdvanceSeconds(541.0);
			const TArray<FString>& Sent = Sub->GetSentMessages();
			Check(TEXT("timeline"), TEXT("closing_after_541"), Sub->GetPhase() == P::Closing && Sub->IsLoginClosed());
			Check(TEXT("timeline"), TEXT("messages_after_541"), Sent.Num() == B + 5
				&& Sent[B + 1] == TEXT("Bakim: sunucu 5 dakika icinde kapanacak. Ilerlemen kaydedilecek.")
				&& Sent[B + 2] == TEXT("Bakim: sunucu 2 dakika icinde kapanacak. Ilerlemen kaydedilecek.")
				&& Sent[B + 3] == TEXT("Bakim: sunucu 1 dakika icinde kapanacak. Ilerlemen kaydedilecek.")
				&& Sent[B + 4] == TEXT("Bakim: yeni girisler kapandi."));
			Check(TEXT("timeline"), TEXT("prelogin_rejected_in_closing"), LifecycleTestPreLogin(GameMode) == MMOServerLifecycle::LoginRejectText());
			Sub->AdvanceSeconds(40.0);
			Check(TEXT("timeline"), TEXT("flushing_after_581"), Sub->GetPhase() == P::Flushing && Sub->IsLoginClosed());
			Check(TEXT("timeline"), TEXT("message_30s"), Sent.Num() == B + 6 && Sent[B + 5] == TEXT("Bakim: sunucu 30 saniye icinde kapaniyor."));
			Check(TEXT("timeline"), TEXT("cancel_refused_in_flushing"), !Sub->RequestCancel(Err) && Sub->GetPhase() == P::Flushing && Sub->IsLoginClosed());
			Check(TEXT("timeline"), TEXT("prelogin_rejected_in_flushing"), LifecycleTestPreLogin(GameMode) == MMOServerLifecycle::LoginRejectText());
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			Check(TEXT("timeline"), TEXT("reset_reopens_login"), !Sub->IsLoginClosed() && LifecycleTestPreLogin(GameMode).IsEmpty());
		}

		// 9) R1 yetki kapisi komut yolunda: NetMode disaridan verilir; sunucu olmayan modlar plani degistiremez.
		{
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			FStringOutputDevice ClientOut, StandaloneOut, ListenOut, ClientCancelOut;
			UMMOServerLifecycleSubsystem::HandleExecCommandForNetMode(World, NM_Client, TEXT("MMOShutdown 600"), ClientOut);
			Check(TEXT("gate"), TEXT("client_refused_text"), ClientOut.Contains(TEXT("yalniz sunucuda")));
			Check(TEXT("gate"), TEXT("client_no_schedule"), Sub->GetPhase() == P::Idle);
			UMMOServerLifecycleSubsystem::HandleExecCommandForNetMode(World, NM_Standalone, TEXT("MMOShutdown 600"), StandaloneOut);
			Check(TEXT("gate"), TEXT("standalone_refused_text"), StandaloneOut.Contains(TEXT("yalniz sunucuda")));
			Check(TEXT("gate"), TEXT("standalone_no_schedule"), Sub->GetPhase() == P::Idle);
			UMMOServerLifecycleSubsystem::HandleExecCommandForNetMode(World, NM_ListenServer, TEXT("MMOShutdown 600"), ListenOut);
			Check(TEXT("gate"), TEXT("listen_accepted"), Sub->GetPhase() == P::Counting);
			UMMOServerLifecycleSubsystem::HandleExecCommandForNetMode(World, NM_Client, TEXT("MMOShutdownCancel"), ClientCancelOut);
			Check(TEXT("gate"), TEXT("client_cancel_refused"), ClientCancelOut.Contains(TEXT("yalniz sunucuda")) && Sub->GetPhase() == P::Counting);
			Check(TEXT("gate"), TEXT("unrelated_command_not_consumed"), !UMMOServerLifecycleSubsystem::HandleExecCommandForNetMode(World, NM_Client, TEXT("stat fps"), ClientOut));
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
		}

		// 10) Gecersiz argumanlar komut yolunda plani degistirmez.
		{
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			FStringOutputDevice A, B, C;
			UMMOServerLifecycleSubsystem::HandleExecCommand(World, TEXT("MMOShutdown abc"), A);
			Check(TEXT("badargs"), TEXT("letters_idle"), Sub->GetPhase() == P::Idle && A.Contains(TEXT("MMOShutdown:")));
			UMMOServerLifecycleSubsystem::HandleExecCommand(World, TEXT("MMOShutdown 10"), B);
			Check(TEXT("badargs"), TEXT("below_min_idle"), Sub->GetPhase() == P::Idle && B.Contains(TEXT("30..86400")));
			UMMOServerLifecycleSubsystem::HandleExecCommand(World, TEXT("MMOShutdown 86401"), C);
			Check(TEXT("badargs"), TEXT("above_max_idle"), Sub->GetPhase() == P::Idle && C.Contains(TEXT("30..86400")));
			FStringOutputDevice D;
			UMMOServerLifecycleSubsystem::HandleExecCommand(World, TEXT("MMOShutdown"), D);
			Check(TEXT("badargs"), TEXT("no_args_usage"), Sub->GetPhase() == P::Idle && D.Contains(TEXT("Kullanim")));
		}

		// 11) R1 uzak istemci benzetimi: ServerExecRPC_Implementation == PC->ConsoleCommand (PlayerController.cpp:1879-1884).
		// Sunucudaki uzak PC'nin Player'i UNetConnection'dir (ULocalPlayer degil); burada ayni motor zinciri UPlayer ile calistirilir.
		{
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			APlayerController* RemotePC = World->SpawnActor<APlayerController>();
			UPlayer* FakeRemote = NewObject<UPlayer>(World);
			Check(TEXT("remote"), TEXT("setup"), RemotePC != nullptr && FakeRemote != nullptr);
			if (RemotePC && FakeRemote)
			{
				RemotePC->Player = FakeRemote;
				FakeRemote->PlayerController = RemotePC;
				// Kontrol: ayni zincir bilinen bir motor komutunu isler (benzetim gercekten Exec'e ulasiyor).
				const FString Control = RemotePC->ConsoleCommand(TEXT("FLUSHPERSISTENTDEBUGLINES"), false);
				Check(TEXT("remote"), TEXT("control_engine_command_recognized"), !Control.Contains(TEXT("Command not recognized")));
				const FString Schedule = RemotePC->ConsoleCommand(TEXT("MMOShutdown 600"), false);
				Check(TEXT("remote"), TEXT("schedule_not_recognized"), Schedule.Contains(TEXT("Command not recognized")));
				Check(TEXT("remote"), TEXT("schedule_no_effect"), Sub->GetPhase() == P::Idle && !Sub->IsLoginClosed());
				// Yerel yol (GEngine->Exec == konsol/-ExecCmds): komut kabul edilir.
				FStringOutputDevice LocalOut;
				const bool bLocal = GEngine->Exec(World, TEXT("MMOShutdown 600 acil"), LocalOut);
				Check(TEXT("remote"), TEXT("local_exec_accepted"), bLocal && Sub->GetPhase() == P::Counting && Sub->GetReason() == R::Emergency);
				const FString Cancel = RemotePC->ConsoleCommand(TEXT("MMOShutdownCancel"), false);
				Check(TEXT("remote"), TEXT("cancel_not_recognized"), Cancel.Contains(TEXT("Command not recognized")));
				Check(TEXT("remote"), TEXT("cancel_no_effect"), Sub->GetPhase() == P::Counting);
				FStringOutputDevice LocalCancel;
				GEngine->Exec(World, TEXT("MMOShutdownCancel"), LocalCancel);
				Check(TEXT("remote"), TEXT("local_cancel_accepted"), Sub->GetPhase() == P::Idle);
				RemotePC->Player = nullptr;
				FakeRemote->PlayerController = nullptr;
				RemotePC->Destroy();
			}
		}

		UMMOServerLifecycleSubsystem::ResetPlanForTest();
		Check(TEXT("end"), TEXT("plan_reset"), Sub->GetPhase() == P::Idle && !Sub->IsLoginClosed());
		if (OutChecks != LifecycleTestExpectedChecks) { ++Failures; OutError += TEXT(" check_count_mismatch"); }
		if (Failures) OutError = FString::Printf(TEXT("failures=%d first=%s"), Failures, *OutError);
		return Failures == 0;
	}

	struct FMMOLifecycleTestExec final : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestServerLifecycle"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = LifecycleTestRun(World, Error, Checks);
			Ar.Logf(TEXT("[MMO_LIFECYCLE_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOLifecycleTestExec GMMOLifecycleTestExec;
}
#endif

// Kapsam notu (L1): duyuru metni/sirasi, komut ayristirma, PreLogin reddi, R1 kapisi ve uzak-ServerExec benzetimi. Kayit turu, atma, cikis kodu
// ve gercek iki surecli dedicated dogrulama (TestShutdownSmoke) L2-L4'tur; burada kanitlanmaz.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/MMOServerLifecycle.h"

#if !UE_BUILD_SHIPPING
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Player/MMOPlayerController.h"
#include "Characters/MMOCharacter.h"
#include "GameFramework/PlayerState.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Parse.h"
#include "Misc/StringOutputDevice.h"

// Sunucu yasam dongusu L1-L2 native testi (docs/DESIGN_SERVER_LIFECYCLE.md 7, dilim L1-L2). CANLI dunya ister: listen sunucu
// (Map?listen -game; Scripts/TestServerLifecycle.ps1). Kapsam: duyuru metinleri, komut ayristirma, PreLogin reddi (gercek
// AGameModeBase::PreLogin -> motor olayi), R1 yetki kapisi ve uzak istemci ServerExec benzetimi; L2: toplu kayit turu (oyuncu basina
// bir adim), atma kapisi, idempotans, yuklenmemis/gecis-onayli/pawn'siz atlama, kayit hatasi, gercek disk (gecici LCTDisk* yuvalari).
// Cikis kodu L3.
// Her Check benzersiz case/check cifti basar. Anonim namespace adlari LifecycleTest onekli (unity-build tuzagi).
namespace
{
	constexpr int32 LifecycleTestExpectedChecks = 180;

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

	int32 LifecycleTestCountMMOControllers(UWorld* World)
	{
		int32 Count = 0;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (Cast<AMMOPlayerController>(It->Get())) ++Count;
		}
		return Count;
	}

	// LogMMOLifecycle satirlarini yakalar (Warning/Error beklentileri icin). Okumadan once Sync().
	class FLifecycleTestLogCapture final : public FOutputDevice
	{
	public:
		FLifecycleTestLogCapture() { GLog->AddOutputDevice(this); }
		virtual ~FLifecycleTestLogCapture() override { GLog->RemoveOutputDevice(this); }
		void Sync() { GLog->Flush(); }
		int32 Count(ELogVerbosity::Type Verbosity, const TCHAR* Needle) const
		{
			int32 N = 0;
			for (const FLine& L : Lines)
			{
				if (L.Verbosity == Verbosity && L.Text.Contains(Needle)) ++N;
			}
			return N;
		}
		virtual void Serialize(const TCHAR* Text, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if (Category == LogMMOLifecycle.GetCategoryName())
			{
				Lines.Add({ static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask), FString(Text) });
			}
		}
	private:
		struct FLine { ELogVerbosity::Type Verbosity; FString Text; };
		TArray<FLine> Lines;
	};

	// Ek dunya (ServerTravel/PIE benzetimi). bAddContext: GEngine dunya baglamina da eklenir (duyuru taramasi gorsun).
	UWorld* LifecycleTestCreateWorld(EWorldType::Type Type, const TCHAR* Name, bool bAddContext)
	{
		UWorld* W = UWorld::CreateWorld(Type, false, FName(Name));
		if (W && bAddContext && GEngine)
		{
			FWorldContext& Context = GEngine->CreateNewWorldContext(Type);
			Context.SetCurrentWorld(W);
			// Aktorler baslatilmadan SpawnActor bilesenleri baslatmaz (denetleyici dunya listesine girmez).
			W->InitializeActorsForPlay(FURL());
		}
		return W;
	}

	void LifecycleTestDestroyWorld(UWorld* W, bool bHadContext)
	{
		if (!W) return;
		if (bHadContext && GEngine) GEngine->DestroyWorldContext(W);
		W->DestroyWorld(false);
	}

	FString LifecycleTestPreLogin(AGameModeBase* GameMode)
	{
		FString Error;
		GameMode->PreLogin(TEXT("?Name=LifecycleProbe"), TEXT("127.0.0.1"), FUniqueNetIdRepl(), Error);
		return Error;
	}

	// L2 sahte oyuncu: gercek AMMOPlayerController + (istege bagli) possess edilmis AMMOCharacter. Kayit kimligi diskten bagimsiz kurulur.
	struct FLifecycleTestPlayer
	{
		AMMOPlayerController* PC = nullptr;
		AMMOCharacter* Pawn = nullptr;
	};

	FLifecycleTestPlayer LifecycleTestMakePlayer(UWorld* World, const FString& Name, bool bWithPawn, bool bLoaded, int32 Index)
	{
		FLifecycleTestPlayer P;
		P.PC = World->SpawnActor<AMMOPlayerController>();
		if (P.PC && P.PC->PlayerState) P.PC->PlayerState->SetPlayerName(Name);
		if (bWithPawn && P.PC)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			P.Pawn = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(1900000.0 + Index * 3000.0, 1900000.0, 5000.0), FRotator::ZeroRotator, Params);
			if (P.Pawn)
			{
				P.PC->Possess(P.Pawn);
				P.Pawn->SetSaveIdentityForTest(bLoaded, P.Pawn->GetSaveSlotName(), 1);
			}
		}
		return P;
	}

	void LifecycleTestDestroyPlayer(FLifecycleTestPlayer& P)
	{
		// EndPlay/UnPossessed yuklenmis karakteri kaydeder: once "yuklenmemis" yap ki test yok ederken diske yazmasin.
		if (P.Pawn) P.Pawn->SetSaveIdentityForTest(false, FString(), 0);
		if (P.PC) { P.PC->UnPossess(); P.PC->Destroy(); }
		if (P.Pawn) P.Pawn->Destroy();
		P = FLifecycleTestPlayer();
	}

	FString LifecycleTestSlotPath(const FString& Slot)
	{
		return FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Slot + TEXT(".sav"));
	}

	void LifecycleTestCleanSlot(const FString& Slot)
	{
		const FString Path = LifecycleTestSlotPath(Slot);
		IFileManager::Get().DeleteDirectory(*Path, false, true); // engelleyici dizin benzetimi
		IFileManager::Get().Delete(*Path, false, true);
	}

	bool LifecycleTestSlotPresent(const FString& Slot)
	{
		return IFileManager::Get().FileExists(*LifecycleTestSlotPath(Slot)) || IFileManager::Get().DirectoryExists(*LifecycleTestSlotPath(Slot));
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
		// Alici sayisi vakasi 0==0 ile bos gecmesin: canli dunyada MMO denetleyicisi yoksa bir tane dogurulur (sonda yok edilir).
		AMMOPlayerController* TestMMOPC = nullptr;
		if (LifecycleTestCountMMOControllers(World) == 0)
		{
			TestMMOPC = World->SpawnActor<AMMOPlayerController>();
		}
		// L2 kuyrugu: kayit ve atma tum calisma boyunca kancali (Exiting'e varan eski vakalar ev sahibinin gercek kaydini DISKE YAZMASIN);
		// gercek disk vakalari kendi bolumunde kancayi gecici olarak kaldirir ve yalniz gecici LCTDisk* yuvalarina yazar.
		TArray<FString> HookLog; // "S:<ad>" kayit, "K:<ad>" atma (sirayla)
		TMap<FString, FString> KickTexts;
		TSet<FString> HookFailNames;
		const auto HookName = [](const APlayerController* PC) { return PC && PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString(); };
		UMMOServerLifecycleSubsystem::SetSaveHookForTest([&](AMMOCharacter* Character)
		{
			const APlayerState* PS = Character ? Character->GetPlayerState() : nullptr;
			const FString Name = PS ? PS->GetPlayerName() : FString();
			HookLog.Add(TEXT("S:") + Name);
			return !HookFailNames.Contains(Name);
		});
		UMMOServerLifecycleSubsystem::SetKickHookForTest([&](APlayerController* PC, const FText& Reason)
		{
			const FString Name = HookName(PC);
			HookLog.Add(TEXT("K:") + Name);
			KickTexts.Add(Name, Reason.ToString());
			return true;
		});

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
			const int32 MMOControllers = LifecycleTestCountMMOControllers(World);
			Check(TEXT("sched60"), TEXT("recipient_precondition_has_mmo_controller"), MMOControllers >= 1);
			Check(TEXT("sched60"), TEXT("recipient_count_matches_mmo_controllers"), MMOControllers >= 1 && Sub->GetLastRecipientCount() == MMOControllers);
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

		// 12) Tick yolu: duvar saati farki aynen uygulanir, son tick zamani guncellenir, ilk tick bosta sureyi saymaz.
		{
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			FString Err;
			Sub->RequestShutdown(600.0, R::Maintenance, Err);
			Check(TEXT("tick"), TEXT("counting_is_tickable"), Sub->IsTickable());
			Sub->SetLastTickTimeForTest(1000.0);
			Sub->TickWithWallClock(1010.0);
			Check(TEXT("tick"), TEXT("delta_applied"), FMath::IsNearlyEqual(Sub->GetRemainingSeconds(), 590.0, 1e-6));
			Sub->TickWithWallClock(1020.0);
			Check(TEXT("tick"), TEXT("last_tick_time_updated"), FMath::IsNearlyEqual(Sub->GetRemainingSeconds(), 580.0, 1e-6));
			Sub->SetLastTickTimeForTest(0.0);
			Sub->TickWithWallClock(9000.0);
			Check(TEXT("tick"), TEXT("first_tick_counts_no_idle_gap"), FMath::IsNearlyEqual(Sub->GetRemainingSeconds(), 580.0, 1e-6));
			Sub->TickWithWallClock(9010.0);
			Check(TEXT("tick"), TEXT("first_tick_records_clock"), FMath::IsNearlyEqual(Sub->GetRemainingSeconds(), 570.0, 1e-6));
			// Exiting'e varinca: Warning loglanir, tick durur (L1'de cikis yok).
			FLifecycleTestLogCapture Capture;
			Sub->TickWithWallClock(9700.0);
			Capture.Sync();
			Check(TEXT("tick"), TEXT("exiting_reached"), Sub->GetPhase() == P::Exiting);
			Check(TEXT("tick"), TEXT("exiting_not_tickable"), !Sub->IsTickable());
			Check(TEXT("tick"), TEXT("exiting_warning_logged"), Capture.Count(ELogVerbosity::Warning, TEXT("shutdown_exiting_not_implemented")) == 1);
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
		}

		// 13) Komut satiri ayristirma (saf).
		{
			double S = -1.0; R Reason = R::Emergency; FString Err;
			using CL = L::ECmdLineShutdown;
			Check(TEXT("cmdline"), TEXT("not_requested"), L::ParseCmdLineShutdown(TEXT("-game -log -port=7777"), S, Reason, Err) == CL::NotRequested);
			Check(TEXT("cmdline"), TEXT("null_not_requested"), L::ParseCmdLineShutdown(nullptr, S, Reason, Err) == CL::NotRequested);
			Check(TEXT("cmdline"), TEXT("ok_seconds_default_reason"), L::ParseCmdLineShutdown(TEXT("-game -MMOShutdownAfterSeconds=60"), S, Reason, Err) == CL::Ok && S == 60.0 && Reason == R::Maintenance);
			Check(TEXT("cmdline"), TEXT("ok_with_reason"), L::ParseCmdLineShutdown(TEXT("-MMOShutdownAfterSeconds=300 -MMOMaintenanceReason=acil"), S, Reason, Err) == CL::Ok && S == 300.0 && Reason == R::Emergency);
			Check(TEXT("cmdline"), TEXT("ok_quoted_values"), L::ParseCmdLineShutdown(TEXT("-MMOShutdownAfterSeconds=\"120\" -MMOMaintenanceReason=\"guncelleme\""), S, Reason, Err) == CL::Ok && S == 120.0 && Reason == R::Update);
			Check(TEXT("cmdline"), TEXT("empty_value_invalid"), L::ParseCmdLineShutdown(TEXT("-MMOShutdownAfterSeconds= -log"), S, Reason, Err) == CL::Invalid && !Err.IsEmpty());
			Check(TEXT("cmdline"), TEXT("only_reason_invalid"), L::ParseCmdLineShutdown(TEXT("-MMOMaintenanceReason=acil"), S, Reason, Err) == CL::Invalid && !Err.IsEmpty());
			Check(TEXT("cmdline"), TEXT("below_min_invalid"), L::ParseCmdLineShutdown(TEXT("-MMOShutdownAfterSeconds=10"), S, Reason, Err) == CL::Invalid && Err.Contains(TEXT("30..86400")));
			Check(TEXT("cmdline"), TEXT("above_max_invalid"), L::ParseCmdLineShutdown(TEXT("-MMOShutdownAfterSeconds=86401"), S, Reason, Err) == CL::Invalid);
			Check(TEXT("cmdline"), TEXT("min_boundary_ok"), L::ParseCmdLineShutdown(TEXT("-MMOShutdownAfterSeconds=30"), S, Reason, Err) == CL::Ok && S == 30.0);
			Check(TEXT("cmdline"), TEXT("max_boundary_ok"), L::ParseCmdLineShutdown(TEXT("-MMOShutdownAfterSeconds=86400"), S, Reason, Err) == CL::Ok && S == 86400.0);
			Check(TEXT("cmdline"), TEXT("bad_reason_invalid"), L::ParseCmdLineShutdown(TEXT("-MMOShutdownAfterSeconds=60 -MMOMaintenanceReason=serbest"), S, Reason, Err) == CL::Invalid);
			Check(TEXT("cmdline"), TEXT("empty_reason_invalid"), L::ParseCmdLineShutdown(TEXT("-MMOShutdownAfterSeconds=60 -MMOMaintenanceReason= -log"), S, Reason, Err) == CL::Invalid);
			Check(TEXT("cmdline"), TEXT("letters_invalid"), L::ParseCmdLineShutdown(TEXT("-MMOShutdownAfterSeconds=abc"), S, Reason, Err) == CL::Invalid);
			Check(TEXT("cmdline"), TEXT("negative_invalid"), L::ParseCmdLineShutdown(TEXT("-MMOShutdownAfterSeconds=-5"), S, Reason, Err) == CL::Invalid);
			Check(TEXT("cmdline"), TEXT("spaced_value_invalid"), L::ParseCmdLineShutdown(TEXT("-MMOShutdownAfterSeconds=\"60 acil\""), S, Reason, Err) == CL::Invalid);
		}

		// 14) Komut satiri tetikleyicisi: Shipping kapisi, Error logu, anahtar tuketimi. (Beklenen 2 Error satiri: TestServerLifecycle.ps1 sayar.)
		{
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			FLifecycleTestLogCapture Capture;
			Sub->TryScheduleFromCommandLine(TEXT("-game -log"), true);
			Check(TEXT("cmdapply"), TEXT("unrelated_cmdline_ignored"), Sub->GetPhase() == P::Idle);
			Sub->TryScheduleFromCommandLine(TEXT("-MMOShutdownAfterSeconds=60"), false);
			Capture.Sync();
			Check(TEXT("cmdapply"), TEXT("unsupported_gate_no_plan"), Sub->GetPhase() == P::Idle && !Sub->IsLoginClosed());
			Check(TEXT("cmdapply"), TEXT("unsupported_warning_logged"), Capture.Count(ELogVerbosity::Warning, TEXT("shutdown_cmdline_unsupported")) == 1);
			Sub->TryScheduleFromCommandLine(TEXT("-MMOShutdownAfterSeconds=60"), true);
			Check(TEXT("cmdapply"), TEXT("unsupported_consumes_flag"), Sub->GetPhase() == P::Idle);

			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			Sub->TryScheduleFromCommandLine(TEXT("-MMOShutdownAfterSeconds=5"), true);
			Capture.Sync();
			Check(TEXT("cmdapply"), TEXT("range_rejected_no_plan"), Sub->GetPhase() == P::Idle);
			Check(TEXT("cmdapply"), TEXT("range_error_logged"), Capture.Count(ELogVerbosity::Error, TEXT("shutdown_cmdline_rejected")) == 1 && Capture.Count(ELogVerbosity::Warning, TEXT("shutdown_cmdline_rejected")) == 0);
			Sub->TryScheduleFromCommandLine(TEXT("-MMOShutdownAfterSeconds=60"), true);
			Check(TEXT("cmdapply"), TEXT("range_consumes_flag"), Sub->GetPhase() == P::Idle);

			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			Sub->TryScheduleFromCommandLine(TEXT("-MMOShutdownAfterSeconds=abc"), true);
			Capture.Sync();
			Check(TEXT("cmdapply"), TEXT("invalid_no_plan"), Sub->GetPhase() == P::Idle);
			Check(TEXT("cmdapply"), TEXT("invalid_error_logged"), Capture.Count(ELogVerbosity::Error, TEXT("shutdown_cmdline_rejected")) == 2);

			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			Sub->TryScheduleFromCommandLine(TEXT("-MMOShutdownAfterSeconds=90 -MMOMaintenanceReason=guncelleme"), true);
			Check(TEXT("cmdapply"), TEXT("valid_schedules"), Sub->GetPhase() == P::Counting && Sub->GetReason() == R::Update);
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
		}

		// 15) Duyuru, bu alt sistemin dunyasi disindaki sunucu dunyalarina da gider (ServerTravel penceresi).
		{
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			const int32 Primary = LifecycleTestCountMMOControllers(World);
			UWorld* Extra = LifecycleTestCreateWorld(EWorldType::Game, TEXT("LifecycleTestExtraGame"), true);
			AMMOPlayerController* ExtraPC = Extra ? Extra->SpawnActor<AMMOPlayerController>() : nullptr;
			Check(TEXT("multiworld"), TEXT("extra_world_created"), Extra != nullptr);
			Check(TEXT("multiworld"), TEXT("extra_pc_spawned"), ExtraPC != nullptr && LifecycleTestCountMMOControllers(Extra) == 1);
			FString Err;
			Sub->RequestShutdown(600.0, R::Maintenance, Err);
			Check(TEXT("multiworld"), TEXT("recipients_include_extra_world"), Primary >= 1 && Sub->GetLastRecipientCount() == Primary + 1);
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			LifecycleTestDestroyWorld(Extra, true);
			Sub->RequestShutdown(600.0, R::Maintenance, Err);
			Check(TEXT("multiworld"), TEXT("recipients_back_to_primary_after_destroy"), Sub->GetLastRecipientCount() == Primary);
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
		}

		// 16) PIE sizintisi: PIE dunyasinin Initialize/Deinitialize'i statik plani sifirlar; Game dunyasininki (ServerTravel) korur.
		{
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			FString Err;
			Sub->RequestShutdown(600.0, R::Maintenance, Err);
			UWorld* Pie = LifecycleTestCreateWorld(EWorldType::PIE, TEXT("LifecycleTestPie"), false);
			Check(TEXT("pie"), TEXT("pie_world_created"), Pie != nullptr);
			Check(TEXT("pie"), TEXT("new_pie_subsystem_starts_idle"), Sub->GetPhase() == P::Idle && !Sub->IsLoginClosed() && LifecycleTestPreLogin(GameMode).IsEmpty());
			Sub->RequestShutdown(60.0, R::Maintenance, Err);
			Check(TEXT("pie"), TEXT("plan_active_before_teardown"), Sub->GetPhase() == P::Closing);
			LifecycleTestDestroyWorld(Pie, false);
			Check(TEXT("pie"), TEXT("pie_teardown_resets_plan"), Sub->GetPhase() == P::Idle && !Sub->IsLoginClosed() && LifecycleTestPreLogin(GameMode).IsEmpty());
			Sub->RequestShutdown(600.0, R::Maintenance, Err);
			UWorld* Travel = LifecycleTestCreateWorld(EWorldType::Game, TEXT("LifecycleTestTravelGame"), false);
			Check(TEXT("pie"), TEXT("game_world_created"), Travel != nullptr);
			LifecycleTestDestroyWorld(Travel, false);
			Check(TEXT("pie"), TEXT("game_world_teardown_keeps_plan"), Sub->GetPhase() == P::Counting);
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
		}

		// 17) Konsol komutu editor (game olmayan) dunyasindan gelirse oyun/PIE dunyasi aranir.
		{
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			UWorld* NonGame = LifecycleTestCreateWorld(EWorldType::Inactive, TEXT("LifecycleTestInactive"), false);
			Check(TEXT("execworld"), TEXT("non_game_world_created"), NonGame != nullptr && !NonGame->IsGameWorld());
			FStringOutputDevice Out;
			UMMOServerLifecycleSubsystem::HandleExecCommand(NonGame, TEXT("MMOShutdown 600"), Out);
			Check(TEXT("execworld"), TEXT("falls_back_to_game_world"), Sub->GetPhase() == P::Counting && Out.Contains(TEXT("600 saniye sonra kapanacak")));
			LifecycleTestDestroyWorld(NonGame, false);
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
		}

		// ---- L2: toplu kayit turu + atma. Kayit/atma kancali; yalniz "LCT" onekli sahte oyuncular kapsama girer (ev sahibi disarida). ----
		const auto CountLog = [&](const FString& Entry) { int32 N = 0; for (const FString& S : HookLog) { if (S == Entry) ++N; } return N; };
		const auto CountPrefix = [&](const TCHAR* Prefix) { int32 N = 0; for (const FString& S : HookLog) { if (S.StartsWith(Prefix)) ++N; } return N; };
		const auto SavedBeforeKick = [&](const FString& Name) { const int32 S = HookLog.IndexOfByKey(TEXT("S:") + Name); const int32 K = HookLog.IndexOfByKey(TEXT("K:") + Name); return S != INDEX_NONE && K != INDEX_NONE && S < K; };
		const auto StepN = [&](int32 N) { for (int32 i = 0; i < N; ++i) Sub->StepShutdownWork(); };
		const auto NameFilter = [](const TCHAR* Prefix)
		{
			const FString P(Prefix);
			return [P](const APlayerController* PC) { return PC && PC->PlayerState && PC->PlayerState->GetPlayerName().StartsWith(P); };
		};

		// 18) Tam tur: 3 kayitli + 1 kayit-hatali + 1 yuklenmemis + 1 gecis-onayli + 1 pawn'siz. Kayit oyuncu basina bir adim, atma ancak tur bitince.
		{
			UMMOServerLifecycleSubsystem::SetPlayerFilterForTest(NameFilter(TEXT("LCT")));
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			const FString FailName = TEXT("LCTF ev=bad\"x");
			HookFailNames.Add(FailName);
			HookLog.Reset();
			KickTexts.Reset();
			FLifecycleTestPlayer A = LifecycleTestMakePlayer(World, TEXT("LCTA"), true, true, 0);
			FLifecycleTestPlayer B = LifecycleTestMakePlayer(World, TEXT("LCTB"), true, true, 1);
			FLifecycleTestPlayer C = LifecycleTestMakePlayer(World, TEXT("LCTC"), true, true, 2);
			FLifecycleTestPlayer F = LifecycleTestMakePlayer(World, FailName, true, true, 3);
			FLifecycleTestPlayer U = LifecycleTestMakePlayer(World, TEXT("LCTU"), true, false, 4);
			FLifecycleTestPlayer T = LifecycleTestMakePlayer(World, TEXT("LCTT"), true, true, 5);
			FLifecycleTestPlayer N = LifecycleTestMakePlayer(World, TEXT("LCTN"), false, false, 6);
			if (T.Pawn) T.Pawn->SetTravelSaveCommitted(true);
			FLifecycleTestPlayer* All[] = { &A, &B, &C, &F, &U, &T, &N };
			bool bSetup = true;
			for (const FLifecycleTestPlayer* P : All) { bSetup &= P->PC && P->PC->PlayerState; }
			bSetup &= A.Pawn && B.Pawn && C.Pawn && F.Pawn && U.Pawn && T.Pawn && !N.Pawn;
			Check(TEXT("l2_round"), TEXT("setup_players"), bSetup);

			FLifecycleTestLogCapture Capture;
			FString Err;
			Sub->RequestShutdown(600.0, R::Maintenance, Err);
			Sub->AdvanceSeconds(596.0); // kalan 4: Closing, Flushing (tur kurulur), Kicking
			FMMOLifecycleFlushSummary S = Sub->GetFlushSummary();
			Check(TEXT("l2_round"), TEXT("round_begun_at_flushing"), Sub->GetPhase() == P::Kicking && S.bRoundStarted && S.Players == 7 && S.Pending == 7 && !S.bFlushDone);
			Check(TEXT("l2_round"), TEXT("no_kick_before_any_step"), CountPrefix(TEXT("K:")) == 0 && CountPrefix(TEXT("S:")) == 0);
			Sub->StepShutdownWork();
			S = Sub->GetFlushSummary();
			Check(TEXT("l2_round"), TEXT("one_player_per_step"), S.Players == 7 && S.Pending == 6);
			Check(TEXT("l2_round"), TEXT("no_kick_while_flush_pending"), CountPrefix(TEXT("K:")) == 0);
			StepN(5);
			S = Sub->GetFlushSummary();
			Check(TEXT("l2_round"), TEXT("pending_until_last_step"), S.Pending == 1 && !S.bFlushDone && CountPrefix(TEXT("K:")) == 0);
			Sub->StepShutdownWork();
			S = Sub->GetFlushSummary();
			Check(TEXT("l2_round"), TEXT("flush_done_after_seventh"), S.Pending == 0 && S.bFlushDone);
			Check(TEXT("l2_round"), TEXT("result_counts"), S.Saved == 3 && S.Failed == 1 && S.SkippedUnloaded == 1 && S.SkippedTravelCommitted == 1 && S.SkippedNoPawn == 1);
			Check(TEXT("l2_round"), TEXT("all_kicked_after_flush"), S.Kicked == 7 && CountPrefix(TEXT("K:")) == 7);
			Check(TEXT("l2_round"), TEXT("saved_once_each"), CountLog(TEXT("S:LCTA")) == 1 && CountLog(TEXT("S:LCTB")) == 1 && CountLog(TEXT("S:LCTC")) == 1 && CountLog(TEXT("S:") + FailName) == 1);
			Check(TEXT("l2_round"), TEXT("unloaded_never_written"), CountLog(TEXT("S:LCTU")) == 0);
			Check(TEXT("l2_round"), TEXT("travel_committed_never_written"), CountLog(TEXT("S:LCTT")) == 0 && CountLog(TEXT("S:LCTN")) == 0);
			Check(TEXT("l2_round"), TEXT("save_before_kick_each"), SavedBeforeKick(TEXT("LCTA")) && SavedBeforeKick(TEXT("LCTB")) && SavedBeforeKick(TEXT("LCTC")) && SavedBeforeKick(FailName));
			Check(TEXT("l2_round"), TEXT("kick_text_claims_saved_only_when_saved"),
				KickTexts.FindRef(TEXT("LCTA")).Contains(TEXT("kaydedildi")) && KickTexts.FindRef(TEXT("LCTB")).Contains(TEXT("kaydedildi")) && KickTexts.FindRef(TEXT("LCTC")).Contains(TEXT("kaydedildi")));
			Check(TEXT("l2_round"), TEXT("kick_text_no_lie_when_not_saved"),
				KickTexts.FindRef(FailName) == TEXT("Sunucu bakima girdi.") && KickTexts.FindRef(TEXT("LCTU")) == TEXT("Sunucu bakima girdi.")
				&& KickTexts.FindRef(TEXT("LCTT")) == TEXT("Sunucu bakima girdi.") && KickTexts.FindRef(TEXT("LCTN")) == TEXT("Sunucu bakima girdi."));
			// Idempotent: tur bir kez; ikinci baslatma reddedilir ve sonraki adimlar/bosaltma kimseye ikinci yazim yapmaz.
			const bool bAgain = Sub->BeginFlushRound();
			StepN(3);
			Sub->DrainShutdownWork();
			Check(TEXT("l2_round"), TEXT("second_round_refused"), !bAgain);
			Check(TEXT("l2_round"), TEXT("no_double_write"), CountPrefix(TEXT("S:")) == 4 && CountLog(TEXT("S:LCTA")) == 1 && CountPrefix(TEXT("K:")) == 7);
			Capture.Sync();
			Check(TEXT("l2_round"), TEXT("metric_failed_line_once_sanitized"),
				Capture.Count(ELogVerbosity::Warning, TEXT("ev=shutdown_save_failed player=LCTF?evbad?x")) == 1 && Capture.Count(ELogVerbosity::Warning, TEXT("ev=shutdown_save_failed")) == 1);
			Check(TEXT("l2_round"), TEXT("metric_flush_done_line"), Capture.Count(ELogVerbosity::Display, TEXT("ev=shutdown phase=flush_done players=7 saved=3 failed=1 skipped=3")) == 1);
			Check(TEXT("l2_round"), TEXT("raw_name_not_in_log"), Capture.Count(ELogVerbosity::Display, TEXT("ev=bad")) == 0 && Capture.Count(ELogVerbosity::Warning, TEXT("ev=bad")) == 0);

			// 19) Tur kurulduktan sonra gelen 2 oyuncu: once kayit sirasina girer; kaydi belli olmayan atilmaz.
			FLifecycleTestPlayer L1 = LifecycleTestMakePlayer(World, TEXT("LCTL1"), true, true, 10);
			FLifecycleTestPlayer L2 = LifecycleTestMakePlayer(World, TEXT("LCTL2"), true, true, 11);
			Sub->StepShutdownWork();
			Check(TEXT("l2_late"), TEXT("late_not_kicked_first_step"), CountLog(TEXT("K:LCTL1")) == 0 && CountLog(TEXT("K:LCTL2")) == 0 && Sub->GetFlushSummary().Pending == 2);
			Sub->StepShutdownWork();
			const int32 K1 = CountLog(TEXT("K:LCTL1")), K2 = CountLog(TEXT("K:LCTL2")), S1 = CountLog(TEXT("S:LCTL1")), S2 = CountLog(TEXT("S:LCTL2"));
			Check(TEXT("l2_late"), TEXT("one_resolved_one_kicked_second_step"), K1 + K2 == 1 && S1 + S2 == 1);
			Check(TEXT("l2_late"), TEXT("kicked_one_was_saved_first"), (K1 == 1 && SavedBeforeKick(TEXT("LCTL1"))) || (K2 == 1 && SavedBeforeKick(TEXT("LCTL2"))));
			Sub->StepShutdownWork();
			Check(TEXT("l2_late"), TEXT("both_kicked_third_step"), CountLog(TEXT("K:LCTL1")) == 1 && CountLog(TEXT("K:LCTL2")) == 1 && SavedBeforeKick(TEXT("LCTL1")) && SavedBeforeKick(TEXT("LCTL2")));
			LifecycleTestDestroyPlayer(L1);
			LifecycleTestDestroyPlayer(L2);

			// 20) Zaman doldu (tek buyuk adim Exiting'e kadar): Flushing'de kurulan tur Exiting'de senkron bitirilir, sonra atilir.
			HookLog.Reset();
			KickTexts.Reset();
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			Sub->RequestShutdown(600.0, R::Maintenance, Err);
			Sub->AdvanceSeconds(700.0);
			S = Sub->GetFlushSummary();
			Check(TEXT("l2_drain"), TEXT("jump_to_exiting_flushes_all"), Sub->GetPhase() == P::Exiting && S.bFlushDone && S.Pending == 0 && S.Saved == 3 && S.Failed == 1 && S.Skipped() == 3);
			Check(TEXT("l2_drain"), TEXT("jump_to_exiting_kicks_all_after_save"), S.Kicked == 7 && SavedBeforeKick(TEXT("LCTA")) && SavedBeforeKick(TEXT("LCTB")) && SavedBeforeKick(TEXT("LCTC")) && SavedBeforeKick(FailName));
			Check(TEXT("l2_drain"), TEXT("exiting_nothing_left_not_tickable"), !Sub->IsTickable());

			// 21) Hic oyuncu yok: tur kurulur kurulmaz biter (Flushing erken biter).
			UMMOServerLifecycleSubsystem::SetPlayerFilterForTest(NameFilter(TEXT("LCTNobodyMatchesThis")));
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			Sub->RequestShutdown(600.0, R::Maintenance, Err);
			Sub->AdvanceSeconds(581.0); // kalan 19 = Flushing
			S = Sub->GetFlushSummary();
			Check(TEXT("l2_empty"), TEXT("flush_finishes_early_without_players"), Sub->GetPhase() == P::Flushing && S.bRoundStarted && S.Players == 0 && S.bFlushDone);

			// 22) Gercek Tick yolu bir oyuncuyu kaydeder (StepShutdownWork'u Tick cagirir).
			HookLog.Reset();
			UMMOServerLifecycleSubsystem::SetPlayerFilterForTest(NameFilter(TEXT("LCTA")));
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			Sub->RequestShutdown(600.0, R::Maintenance, Err);
			Sub->AdvanceSeconds(581.0);
			Check(TEXT("l2_tick"), TEXT("before_tick_one_pending"), Sub->GetFlushSummary().Pending == 1 && CountPrefix(TEXT("S:")) == 0);
			Sub->Tick(0.016f);
			S = Sub->GetFlushSummary();
			Check(TEXT("l2_tick"), TEXT("tick_runs_flush_step"), S.Saved == 1 && S.bFlushDone && CountLog(TEXT("S:LCTA")) == 1);

			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			for (FLifecycleTestPlayer* P : All) LifecycleTestDestroyPlayer(*P);
			HookFailNames.Reset();
		}

		// 23) Gercek disk (kancasiz): yalniz gecici LCTDisk* yuvalari; engelleyici dizin = yazilamaz kayit dizini benzetimi.
		{
			UMMOServerLifecycleSubsystem::SetSaveHookForTest(nullptr);
			UMMOServerLifecycleSubsystem::SetPlayerFilterForTest(NameFilter(TEXT("LCTDisk")));
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			HookLog.Reset();
			KickTexts.Reset();
			const FString SlotOk = TEXT("MMOSave_LCTDiskA"), SlotBlocked = TEXT("MMOSave_LCTDiskF"), SlotUnloaded = TEXT("MMOSave_LCTDiskU"), SlotDeferred = TEXT("MMOSave_LCTDiskD");
			for (const FString& Slot : { SlotOk, SlotBlocked, SlotUnloaded, SlotDeferred }) LifecycleTestCleanSlot(Slot);
			FLifecycleTestPlayer D1 = LifecycleTestMakePlayer(World, TEXT("LCTDiskA"), true, true, 20);
			FLifecycleTestPlayer DF = LifecycleTestMakePlayer(World, TEXT("LCTDiskF"), true, true, 21);
			FLifecycleTestPlayer DU = LifecycleTestMakePlayer(World, TEXT("LCTDiskU"), true, false, 22);
			FLifecycleTestPlayer DD = LifecycleTestMakePlayer(World, TEXT("LCTDiskD"), true, true, 23);
			const bool bTempSlots = D1.Pawn && DF.Pawn && DU.Pawn && DD.Pawn
				&& D1.Pawn->GetSaveSlotName() == SlotOk && DF.Pawn->GetSaveSlotName() == SlotBlocked
				&& DU.Pawn->GetSaveSlotName() == SlotUnloaded && DD.Pawn->GetSaveSlotName() == SlotDeferred;
			Check(TEXT("l2_disk"), TEXT("slots_are_temp"), bTempSlots);
			if (bTempSlots)
			{
				IFileManager::Get().MakeDirectory(*LifecycleTestSlotPath(SlotBlocked), true); // dizin dosyanin yerini tutar -> yazim basarisiz
				DD.Pawn->FailDeferredSavesForTest(2);
				const bool bDeferredOpen = !DD.Pawn->FlushDeferredSave(true); // basarisiz, ertelenmis isaret kalir (R7)
				Check(TEXT("l2_disk"), TEXT("deferred_flag_set_by_failed_flush"), bDeferredOpen && DD.Pawn->IsSaveDeferredByTravel());
				FString Err;
				Sub->RequestShutdown(600.0, R::Maintenance, Err);
				Sub->AdvanceSeconds(596.0);
				StepN(4);
				EMMOLifecycleSaveResult R1 = EMMOLifecycleSaveResult::Saved, R2 = R1, R3 = R1, R4 = R1;
				bool K1b = false, K2b = false, K3b = false, K4b = false;
				const bool bAllKnown = Sub->GetFlushResultFor(D1.PC, R1, K1b) && Sub->GetFlushResultFor(DF.PC, R2, K2b) && Sub->GetFlushResultFor(DU.PC, R3, K3b) && Sub->GetFlushResultFor(DD.PC, R4, K4b);
				Check(TEXT("l2_disk"), TEXT("all_results_known"), bAllKnown);
				Check(TEXT("l2_disk"), TEXT("saved_file_written"), R1 == EMMOLifecycleSaveResult::Saved && LifecycleTestSlotPresent(SlotOk) && IFileManager::Get().FileExists(*LifecycleTestSlotPath(SlotOk)));
				Check(TEXT("l2_disk"), TEXT("unwritable_dir_counts_failed"), R2 == EMMOLifecycleSaveResult::Failed && !IFileManager::Get().FileExists(*LifecycleTestSlotPath(SlotBlocked)));
				Check(TEXT("l2_disk"), TEXT("unloaded_skipped_and_no_file"), R3 == EMMOLifecycleSaveResult::SkippedUnloaded && !LifecycleTestSlotPresent(SlotUnloaded));
				Check(TEXT("l2_disk"), TEXT("deferred_save_failure_counts_failed"), R4 == EMMOLifecycleSaveResult::Failed && !LifecycleTestSlotPresent(SlotDeferred) && DD.Pawn->IsSaveDeferredByTravel());
				Check(TEXT("l2_disk"), TEXT("failed_players_kicked_without_saved_claim"),
					K1b && K2b && K3b && K4b && KickTexts.FindRef(TEXT("LCTDiskF")) == TEXT("Sunucu bakima girdi.") && KickTexts.FindRef(TEXT("LCTDiskD")) == TEXT("Sunucu bakima girdi.")
					&& KickTexts.FindRef(TEXT("LCTDiskA")).Contains(TEXT("kaydedildi")));
				// Hata enjeksiyonu bitti: ayni oyuncu bu kez kaydedilir ve ertelenmis isaret kalkar.
				Check(TEXT("l2_disk"), TEXT("deferred_recovers_after_failures"),
					L::SaveCharacterNow(DD.Pawn) == EMMOLifecycleSaveResult::Saved && IFileManager::Get().FileExists(*LifecycleTestSlotPath(SlotDeferred)) && !DD.Pawn->IsSaveDeferredByTravel());
			}
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			LifecycleTestDestroyPlayer(D1);
			LifecycleTestDestroyPlayer(DF);
			LifecycleTestDestroyPlayer(DU);
			LifecycleTestDestroyPlayer(DD);
			for (const FString& Slot : { SlotOk, SlotBlocked, SlotUnloaded, SlotDeferred }) LifecycleTestCleanSlot(Slot);
			Check(TEXT("l2_disk"), TEXT("temp_slots_cleaned"), !LifecycleTestSlotPresent(SlotOk) && !LifecycleTestSlotPresent(SlotBlocked) && !LifecycleTestSlotPresent(SlotUnloaded) && !LifecycleTestSlotPresent(SlotDeferred));
		}

		// 24) Saf kopru kapilari + gercek atma yolu (ag baglantisi olmayan denetleyici atilamaz ama cokmez).
		{
			Check(TEXT("l2_bridge"), TEXT("null_pawn_skipped_nopawn"), L::SaveCharacterNow(nullptr) == EMMOLifecycleSaveResult::SkippedNoPawn);
			Check(TEXT("l2_bridge"), TEXT("kick_text_only_saved_claims_saved"),
				FString(L::KickText(EMMOLifecycleSaveResult::Saved)).Contains(TEXT("kaydedildi"))
				&& !FString(L::KickText(EMMOLifecycleSaveResult::Failed)).Contains(TEXT("kaydedildi"))
				&& !FString(L::KickText(EMMOLifecycleSaveResult::SkippedUnloaded)).Contains(TEXT("kaydedildi"))
				&& !FString(L::KickText(EMMOLifecycleSaveResult::SkippedTravelCommitted)).Contains(TEXT("kaydedildi"))
				&& !FString(L::KickText(EMMOLifecycleSaveResult::SkippedNoPawn)).Contains(TEXT("kaydedildi")));
			UMMOServerLifecycleSubsystem::SetKickHookForTest(nullptr);
			AMMOPlayerController* Plain = World->SpawnActor<AMMOPlayerController>();
			const bool bKicked = L::KickPlayerNow(Plain, FText::FromString(TEXT("test")));
			Check(TEXT("l2_bridge"), TEXT("real_kick_without_connection_false_and_alive"), Plain && !bKicked && IsValid(Plain));
			if (Plain) Plain->Destroy();
		}

		UMMOServerLifecycleSubsystem::SetSaveHookForTest(nullptr);
		UMMOServerLifecycleSubsystem::SetKickHookForTest(nullptr);
		UMMOServerLifecycleSubsystem::SetPlayerFilterForTest(nullptr);
		if (TestMMOPC) TestMMOPC->Destroy();
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

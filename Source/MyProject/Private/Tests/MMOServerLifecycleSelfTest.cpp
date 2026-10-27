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
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOMailComponent.h"
#include "Components/MMOTradeComponent.h"
#include "Components/MMOQuestComponent.h"
#include "Components/MMOStatsComponent.h"
#include "Core/MMOGameMode.h"
#include "Items/MMOFusion.h"
#include "Items/MMOItemDatabase.h"
#include "Items/MMOConsumableTypes.h"
#include "World/MMOVendorNPC.h"
#include "World/MMOWarehouseNPC.h"
#include "World/MMOQuestGiverNPC.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerState.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Parse.h"
#include "Misc/StringOutputDevice.h"

// Sunucu yasam dongusu L1-L3 native testi (docs/DESIGN_SERVER_LIFECYCLE.md 7, dilim L1-L3). CANLI dunya ister: listen sunucu
// (Map?listen -game; Scripts/TestServerLifecycle.ps1). Kapsam: duyuru metinleri, komut ayristirma, PreLogin reddi (gercek
// AGameModeBase::PreLogin -> motor olayi), R1 yetki kapisi ve uzak istemci ServerExec benzetimi; L2: toplu kayit turu (oyuncu basina
// bir adim), atma kapisi, idempotans, yuklenmemis/gecis-onayli/pawn'siz atlama, kayit hatasi, gercek disk (gecici LCTDisk* yuvalari).
// L3: Exiting'de cikis (kanca ile, gercek RequestExit CAGRILMAZ): kod 0/2, denetleyiciler yok olana kadar bekleme + 5 sn sinir, tek cikis;
// atma aninda yeniden deneme / yeniden degerlendirme, KickPlayer yeniden denemesi, Tick basina atma siniri, kayit turunda 'gone',
// ZoneTravel ucusunda kayit (GameMode korumali alanlarina turetilmis erisimci ile), Flushing'de ticaret/posta/birlestirme kapisi.
// L3 QA turu 2: cikis durum dosyasi + Windows kod notu (saf), cikis kodu dokumu (hala bagli / atilamayan / gone), atma aninda Saved icin
// yeniden yazim, Drain'de atma yeniden denemelerinin Tick'lere yayilmasi, listen ev sahibi dallari (filtre disinda birakilmaz), Flushing'de
// envanter/depo/yukseltme/gorev kapilari + 6 ticaret kapisi (her biri kendi log etiketiyle), birlestirme icin ayri Maintenance sonucu.
// Her Check benzersiz case/check cifti basar. Anonim namespace adlari LifecycleTest onekli (unity-build tuzagi).
namespace
{
	constexpr int32 LifecycleTestExpectedChecks = 290;

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

	// Gercek AMMOGameMode'un bolge gecisi "ucusta" durumunu kurar (korumali alanlara turetilmis sinif uzerinden isaretci-uye erisimi; GameMode'a dokunulmaz).
	struct FLifecycleTestGameModeAccess : public AMMOGameMode
	{
		static void SetTravelFlight(AMMOGameMode* GM, AMMOCharacter* Character, bool bOn)
		{
			GM->*(&FLifecycleTestGameModeAccess::bZoneTravelInProgress) = bOn;
			TArray<TWeakObjectPtr<AMMOCharacter>>& Pending = GM->*(&FLifecycleTestGameModeAccess::PendingZoneTravelCharacters);
			TSet<TWeakObjectPtr<AMMOCharacter>>& Completed = GM->*(&FLifecycleTestGameModeAccess::CompletedZoneTravelSaves);
			Pending.Reset();
			Completed.Reset();
			if (bOn && Character) Pending.Add(Character);
		}
		static void AddPending(AMMOGameMode* GM, AMMOCharacter* Character)
		{
			(GM->*(&FLifecycleTestGameModeAccess::PendingZoneTravelCharacters)).AddUnique(Character);
		}
		static void RemovePending(AMMOGameMode* GM, AMMOCharacter* Character)
		{
			(GM->*(&FLifecycleTestGameModeAccess::PendingZoneTravelCharacters)).Remove(Character);
		}
	};

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
		// IsValid: atma kancasi (HookKickDestroyNames) ya da test onceden yok etmis olabilir.
		if (IsValid(P.Pawn)) P.Pawn->SetSaveIdentityForTest(false, FString(), 0);
		if (IsValid(P.PC)) { P.PC->UnPossess(); P.PC->Destroy(); }
		if (IsValid(P.Pawn)) P.Pawn->Destroy();
		P = FLifecycleTestPlayer();
	}

	// Birlestirme / ticaret / envanter vakalari icin yardimcilar.
	struct FLifecycleTestTradeAccess : public UMMOTradeComponent
	{
		static void StartTrade(AMMOCharacter* A, AMMOCharacter* B) { StartTradeServer(A, B); }
	};

	FString LifecycleTestNormalizedSlashes(FString Path)
	{
		Path.ReplaceInline(TEXT("\\"), TEXT("/"));
		return Path;
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
		TMap<FString, int32> HookSaveFailBudget; // ilk N yazim basarisiz, sonra basarili
		TMap<FString, int32> HookKickFailBudget; // ilk N atma denemesi false ("KF:" kaydi), sonra basarili
		TMap<FString, int32> HookSaveOkBudget;   // ilk N yazim basarili, sonra hep basarisiz (atma aninda yeniden yazim hatasi benzetimi)
		TSet<FString> HookKickDestroyNames;      // basarili atma denetleyiciyi gercekten yok eder (gercek KickPlayer gibi); bu adlar icin
		TArray<TPair<int32, int32>> ExitCalls;   // kancaya gelen (cikis kodu, basarisiz oyuncu sayisi); gercek RequestExit CAGRILMAZ
		const auto HookName = [](const APlayerController* PC) { return PC && PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString(); };
		const TFunction<bool(AMMOCharacter*)> SaveHookFn = [&](AMMOCharacter* Character)
		{
			const APlayerState* PS = Character ? Character->GetPlayerState() : nullptr;
			const FString Name = PS ? PS->GetPlayerName() : FString();
			HookLog.Add(TEXT("S:") + Name);
			bool bFail = HookFailNames.Contains(Name);
			if (int32* Budget = HookSaveFailBudget.Find(Name))
			{
				if (*Budget > 0) { --*Budget; bFail = true; }
			}
			if (int32* OkBudget = HookSaveOkBudget.Find(Name))
			{
				if (*OkBudget > 0) --*OkBudget; else bFail = true;
			}
			return !bFail;
		};
		const TFunction<bool(APlayerController*, const FText&)> KickHookFn = [&](APlayerController* PC, const FText& Reason)
		{
			const FString Name = HookName(PC);
			if (int32* Budget = HookKickFailBudget.Find(Name))
			{
				if (*Budget > 0) { --*Budget; HookLog.Add(TEXT("KF:") + Name); return false; }
			}
			HookLog.Add(TEXT("K:") + Name);
			KickTexts.Add(Name, Reason.ToString());
			if (HookKickDestroyNames.Contains(Name))
			{
				// Gercek KickPlayer baglantiyi kapatir ve denetleyici yok olur; yuklenmis karakterin EndPlay kaydi diske yazmasin diye once kimlik kapatilir.
				if (AMMOCharacter* Pawn = Cast<AMMOCharacter>(PC->GetPawn())) Pawn->SetSaveIdentityForTest(false, FString(), 0);
				PC->Destroy();
			}
			return true;
		};
		UMMOServerLifecycleSubsystem::SetSaveHookForTest(SaveHookFn);
		UMMOServerLifecycleSubsystem::SetKickHookForTest(KickHookFn);
		UMMOServerLifecycleSubsystem::SetExitHookForTest([&](int32 Code, int32 Failed) { ExitCalls.Add(TPair<int32, int32>(Code, Failed)); });

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
			// Exiting'e varinca: tek Display logu (eski "uygulanmadi" Warning'i yok); tick cikis istenene kadar surer (L3).
			FLifecycleTestLogCapture Capture;
			Sub->TickWithWallClock(9700.0);
			Capture.Sync();
			Check(TEXT("tick"), TEXT("exiting_reached"), Sub->GetPhase() == P::Exiting);
			Check(TEXT("tick"), TEXT("exiting_still_tickable_until_exit"), Sub->IsTickable() && !UMMOServerLifecycleSubsystem::WasExitRequested() && ExitCalls.Num() == 0);
			Check(TEXT("tick"), TEXT("exiting_logged_once_no_stub_warning"), Capture.Count(ELogVerbosity::Display, TEXT("event=shutdown_exiting")) == 1 && Capture.Count(ELogVerbosity::Warning, TEXT("shutdown_exiting_not_implemented")) == 0);
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
			Sub->TryScheduleFromCommandLine(TEXT("-game -log"));
			Check(TEXT("cmdapply"), TEXT("unrelated_cmdline_ignored"), Sub->GetPhase() == P::Idle);
			// L3: Shipping kapisi kaldirildi; tetikleyici tum yapilarda ayni kod yolunu kullanir (yapi dali yok) ve plan kurar.
			Sub->TryScheduleFromCommandLine(TEXT("-MMOShutdownAfterSeconds=90"));
			Capture.Sync();
			Check(TEXT("cmdapply"), TEXT("all_builds_schedules_plan"), Sub->GetPhase() == P::Counting && Sub->GetReason() == R::Maintenance && !Sub->IsLoginClosed());
			Check(TEXT("cmdapply"), TEXT("no_unsupported_warning"), Capture.Count(ELogVerbosity::Warning, TEXT("shutdown_cmdline_unsupported")) == 0);
			FString CancelErr;
			Sub->RequestCancel(CancelErr);
			Sub->TryScheduleFromCommandLine(TEXT("-MMOShutdownAfterSeconds=60"));
			Check(TEXT("cmdapply"), TEXT("schedule_consumes_flag"), Sub->GetPhase() == P::Idle);

			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			Sub->TryScheduleFromCommandLine(TEXT("-MMOShutdownAfterSeconds=5"));
			Capture.Sync();
			Check(TEXT("cmdapply"), TEXT("range_rejected_no_plan"), Sub->GetPhase() == P::Idle);
			Check(TEXT("cmdapply"), TEXT("range_error_logged"), Capture.Count(ELogVerbosity::Error, TEXT("shutdown_cmdline_rejected")) == 1 && Capture.Count(ELogVerbosity::Warning, TEXT("shutdown_cmdline_rejected")) == 0);
			Sub->TryScheduleFromCommandLine(TEXT("-MMOShutdownAfterSeconds=60"));
			Check(TEXT("cmdapply"), TEXT("range_consumes_flag"), Sub->GetPhase() == P::Idle);

			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			Sub->TryScheduleFromCommandLine(TEXT("-MMOShutdownAfterSeconds=abc"));
			Capture.Sync();
			Check(TEXT("cmdapply"), TEXT("invalid_no_plan"), Sub->GetPhase() == P::Idle);
			Check(TEXT("cmdapply"), TEXT("invalid_error_logged"), Capture.Count(ELogVerbosity::Error, TEXT("shutdown_cmdline_rejected")) == 2);

			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			Sub->TryScheduleFromCommandLine(TEXT("-MMOShutdownAfterSeconds=90 -MMOMaintenanceReason=guncelleme"));
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
			// L3: atma Tick basina MaxKicksPerTick ile sinirli (Drain haric); kalanlar sonraki adimda.
			Check(TEXT("l2_round"), TEXT("kick_limited_per_tick"), S.Kicked == L::MaxKicksPerTick && CountPrefix(TEXT("K:")) == L::MaxKicksPerTick);
			Sub->StepShutdownWork();
			S = Sub->GetFlushSummary();
			Check(TEXT("l2_round"), TEXT("all_kicked_after_flush"), S.Kicked == 7 && CountPrefix(TEXT("K:")) == 7);
			// Basarisiz kayit atma aninda BIR kez yeniden denenir (2 yazim); digerleri bir kez.
			Check(TEXT("l2_round"), TEXT("saved_again_at_kick_failed_retried_once"), CountLog(TEXT("S:LCTA")) == 2 && CountLog(TEXT("S:LCTB")) == 2 && CountLog(TEXT("S:LCTC")) == 2 && CountLog(TEXT("S:") + FailName) == 2);
			Check(TEXT("l2_round"), TEXT("unloaded_never_written"), CountLog(TEXT("S:LCTU")) == 0);
			Check(TEXT("l2_round"), TEXT("travel_committed_never_written"), CountLog(TEXT("S:LCTT")) == 0 && CountLog(TEXT("S:LCTN")) == 0);
			Check(TEXT("l2_round"), TEXT("save_before_kick_each"), SavedBeforeKick(TEXT("LCTA")) && SavedBeforeKick(TEXT("LCTB")) && SavedBeforeKick(TEXT("LCTC")) && SavedBeforeKick(FailName));
			Check(TEXT("l2_round"), TEXT("kick_text_claims_saved_only_when_saved"),
				KickTexts.FindRef(TEXT("LCTA")).Contains(TEXT("kaydedildi")) && KickTexts.FindRef(TEXT("LCTB")).Contains(TEXT("kaydedildi")) && KickTexts.FindRef(TEXT("LCTC")).Contains(TEXT("kaydedildi")));
			Check(TEXT("l2_round"), TEXT("kick_text_no_lie_when_not_saved"),
				KickTexts.FindRef(TEXT("LCTU")) == TEXT("Sunucu bakima girdi.")
				&& KickTexts.FindRef(TEXT("LCTT")) == TEXT("Sunucu bakima girdi.") && KickTexts.FindRef(TEXT("LCTN")) == TEXT("Sunucu bakima girdi."));
			Check(TEXT("l2_round"), TEXT("kick_text_failed_after_retry_tells_admin"),
				KickTexts.FindRef(FailName) == TEXT("Sunucu bakima girdi. Ilerleme kaydedilemedi, yoneticiye bildir.") && !KickTexts.FindRef(FailName).Contains(TEXT("kaydedildi")));
			// Idempotent: tur bir kez; ikinci baslatma reddedilir ve sonraki adimlar/bosaltma kimseye ikinci yazim yapmaz.
			const bool bAgain = Sub->BeginFlushRound();
			StepN(3);
			Sub->DrainShutdownWork();
			Check(TEXT("l2_round"), TEXT("second_round_refused"), !bAgain);
			Check(TEXT("l2_round"), TEXT("no_double_write"), CountPrefix(TEXT("S:")) == 8 && CountLog(TEXT("S:LCTA")) == 2 && CountPrefix(TEXT("K:")) == 7);
			Capture.Sync();
			Check(TEXT("l2_round"), TEXT("metric_failed_line_once_sanitized"),
				Capture.Count(ELogVerbosity::Warning, TEXT("ev=shutdown_save_failed player=LCTF?evbad?x")) == 1 && Capture.Count(ELogVerbosity::Warning, TEXT("ev=shutdown_save_failed")) == 1);
			Check(TEXT("l2_round"), TEXT("metric_flush_done_line"), Capture.Count(ELogVerbosity::Display, TEXT("ev=shutdown phase=flush_done players=7 saved=3 failed=1 skipped=3")) == 1);
			Check(TEXT("l2_round"), TEXT("retry_logged_for_failed_saved_and_skipped"),
				Capture.Count(ELogVerbosity::Display, TEXT("event=shutdown_save_retry player=LCTF?evbad?x previous=failed result=failed")) == 1
				&& Capture.Count(ELogVerbosity::Display, TEXT("event=shutdown_save_retry player=LCTU previous=skipped_unloaded result=skipped_unloaded")) == 1
				&& Capture.Count(ELogVerbosity::Display, TEXT("event=shutdown_save_retry player=LCTA previous=saved result=saved")) == 1
					&& Capture.Count(ELogVerbosity::Display, TEXT("event=shutdown_save_retry player=LCTT")) == 0);
			Check(TEXT("l2_round"), TEXT("raw_name_not_in_log"), Capture.Count(ELogVerbosity::Display, TEXT("ev=bad")) == 0 && Capture.Count(ELogVerbosity::Warning, TEXT("ev=bad")) == 0);

			// 19) Tur kurulduktan sonra gelen 2 oyuncu: once kayit sirasina girer; kaydi belli olmayan atilmaz.
			FLifecycleTestPlayer L1 = LifecycleTestMakePlayer(World, TEXT("LCTL1"), true, true, 10);
			FLifecycleTestPlayer L2 = LifecycleTestMakePlayer(World, TEXT("LCTL2"), true, true, 11);
			Sub->StepShutdownWork();
			Check(TEXT("l2_late"), TEXT("late_not_kicked_first_step"), CountLog(TEXT("K:LCTL1")) == 0 && CountLog(TEXT("K:LCTL2")) == 0 && Sub->GetFlushSummary().Pending == 2);
			Sub->StepShutdownWork();
			const int32 K1 = CountLog(TEXT("K:LCTL1")), K2 = CountLog(TEXT("K:LCTL2")), S1 = CountLog(TEXT("S:LCTL1")), S2 = CountLog(TEXT("S:LCTL2"));
			Check(TEXT("l2_late"), TEXT("one_resolved_one_kicked_second_step"), K1 + K2 == 1 && S1 + S2 == 2); // yazilan oyuncu kick aninda bir kez daha yazilir
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
			Check(TEXT("l2_drain"), TEXT("exiting_tickable_waiting_for_exit"), Sub->IsTickable() && ExitCalls.Num() == 0 && !UMMOServerLifecycleSubsystem::WasExitRequested());

			// L3 cikis: denetleyiciler (kanca atmasi onlari yok etmez) varken beklenir; 5 sn dolunca kod 2 (1 oyuncunun kaydi yeniden denemeden sonra da basarisiz).
			{
				FLifecycleTestLogCapture ExitCapture;
				const double T0 = 5000.0;
				// Kanca atmasi PC'leri yok etmez; gercek KickPlayer'da atilanlar kapanir. Burada B,C,U,T,N "ayrildi"; A (atildi ama hala bagli)
				// ve F (kaydi basarisiz) kalir: 5 sn sonunda A "hala bagli" (+1), F zaten Failed (iki kez sayilmaz).
				for (FLifecycleTestPlayer* Leaver : { &B, &C, &U, &T, &N }) LifecycleTestDestroyPlayer(*Leaver);
				IFileManager::Get().Delete(*L::ResultFilePath(), false, true, true);
				Sub->StepExit(T0);
				Check(TEXT("l3_exit"), TEXT("waits_while_players_remain"), ExitCalls.Num() == 0 && !UMMOServerLifecycleSubsystem::WasExitRequested());
				Sub->StepExit(T0 + 4.0);
				Check(TEXT("l3_exit"), TEXT("still_waiting_before_limit"), ExitCalls.Num() == 0);
				Sub->StepExit(T0 + L::ExitWaitMaxSeconds);
				ExitCapture.Sync();
				Check(TEXT("l3_exit"), TEXT("limit_exits_with_failed_code"), ExitCalls.Num() == 1 && ExitCalls[0].Key == L::ExitCodeSaveFailed && ExitCalls[0].Value == 2 && UMMOServerLifecycleSubsystem::WasExitRequested());
				Check(TEXT("l3_exit"), TEXT("exit_metric_line_logged"), ExitCapture.Count(ELogVerbosity::Display, TEXT("ev=shutdown_exit code=2 failed=2 gone=0 still_connected=1 kick_failed=0")) == 1 && ExitCapture.Count(ELogVerbosity::Warning, TEXT("event=shutdown_exit_wait_timeout")) == 1);
				FString ResultJson;
				FFileHelper::LoadFileToString(ResultJson, *L::ResultFilePath());
				Check(TEXT("l3_exit"), TEXT("result_file_written_with_breakdown"),
					ResultJson.Contains(TEXT("\"exit_code\":2")) && ResultJson.Contains(TEXT("\"failed\":2")) && ResultJson.Contains(TEXT("\"still_connected\":1")) && ResultJson.Contains(TEXT("\"time_utc\":\"")));
				Check(TEXT("l3_exit"), TEXT("not_tickable_after_exit"), !Sub->IsTickable());
				Sub->StepExit(T0 + 60.0);
				Check(TEXT("l3_exit"), TEXT("exit_requested_only_once"), ExitCalls.Num() == 1);
			}

			// 21) Hic oyuncu yok: tur kurulur kurulmaz biter (Flushing erken biter).
			UMMOServerLifecycleSubsystem::SetPlayerFilterForTest(NameFilter(TEXT("LCTNobodyMatchesThis")));
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			Sub->RequestShutdown(600.0, R::Maintenance, Err);
			Sub->AdvanceSeconds(581.0); // kalan 19 = Flushing
			S = Sub->GetFlushSummary();
			Check(TEXT("l2_empty"), TEXT("flush_finishes_early_without_players"), Sub->GetPhase() == P::Flushing && S.bRoundStarted && S.Players == 0 && S.bFlushDone);
			// L3: kimse yokken cikis beklemeden, kod 0.
			ExitCalls.Reset();
			Sub->AdvanceSeconds(30.0);
			Sub->StepExit(9000.0);
			Check(TEXT("l3_exit"), TEXT("nobody_left_exits_immediately_code_zero"), Sub->GetPhase() == P::Exiting && ExitCalls.Num() == 1 && ExitCalls[0].Key == L::ExitCodeOk && ExitCalls[0].Value == 0);

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
			// Flushing'de tur bitse bile atma YOK: atma kapisi Kicking'i bekler (yalniz flush-bitti yeterli degil).
			Check(TEXT("l2_tick"), TEXT("no_kick_in_flushing_after_round_done"), Sub->GetPhase() == P::Flushing && S.bFlushDone && CountPrefix(TEXT("K:")) == 0 && S.Kicked == 0);

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
				DD.Pawn->FailDeferredSavesForTest(2); // L3: atma aninda yeniden deneme var -> tur denemesi + yeniden deneme = 2 basarisiz yazim
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
					K1b && K2b && K3b && K4b && KickTexts.FindRef(TEXT("LCTDiskF")) == TEXT("Sunucu bakima girdi. Ilerleme kaydedilemedi, yoneticiye bildir.")
					&& KickTexts.FindRef(TEXT("LCTDiskD")) == TEXT("Sunucu bakima girdi. Ilerleme kaydedilemedi, yoneticiye bildir.")
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

		// 25) L3 atma aninda yeniden deneme / yeniden degerlendirme, KickPlayer yeniden denemesi, 'gone', cikis kodu 0.
		{
			UMMOServerLifecycleSubsystem::SetSaveHookForTest(SaveHookFn); // 23/24 kancalari kaldirdi: kayit + atma kancalarini yeniden kur
			UMMOServerLifecycleSubsystem::SetKickHookForTest(KickHookFn);
			UMMOServerLifecycleSubsystem::SetPlayerFilterForTest(NameFilter(TEXT("LCTX")));
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			HookLog.Reset();
			KickTexts.Reset();
			ExitCalls.Reset();
			HookSaveFailBudget.Add(TEXT("LCTXR"), 1);   // ilk yazim basarisiz, atma aninda yeniden deneme basarili
			HookKickFailBudget.Add(TEXT("LCTXK"), 2);   // ilk 2 atma denemesi false, 3. basarili
			HookKickFailBudget.Add(TEXT("LCTXB"), 10);  // hep false: MaxKickAttempts'te durur
			FLifecycleTestPlayer XR = LifecycleTestMakePlayer(World, TEXT("LCTXR"), true, true, 40);
			FLifecycleTestPlayer XK = LifecycleTestMakePlayer(World, TEXT("LCTXK"), true, true, 41);
			FLifecycleTestPlayer XB = LifecycleTestMakePlayer(World, TEXT("LCTXB"), true, true, 42);
			FLifecycleTestPlayer XG = LifecycleTestMakePlayer(World, TEXT("LCTXG"), true, true, 43);
			Check(TEXT("l3_retry"), TEXT("setup_players"), XR.Pawn && XK.Pawn && XB.Pawn && XG.Pawn && XR.PC && XK.PC && XB.PC && XG.PC);
			FString Err;
			Sub->RequestShutdown(600.0, R::Maintenance, Err);
			Sub->AdvanceSeconds(596.0); // Kicking, tur kurulu: 4 oyuncu bekliyor
			FMMOLifecycleFlushSummary S = Sub->GetFlushSummary();
			Check(TEXT("l3_retry"), TEXT("round_has_four_pending"), S.Players == 4 && S.Pending == 4);
			LifecycleTestDestroyPlayer(XG); // kayit adimindan ONCE ayrildi: denetleyici yok
			StepN(3); // R, K, B kaydedilir; XG adiminda denetleyici yok -> gone (tur biter)
			Check(TEXT("l3_retry"), TEXT("pending_one_before_gone_step"), Sub->GetFlushSummary().SkippedGone == 0 && Sub->GetFlushSummary().Pending == 1);
			StepN(1);
			S = Sub->GetFlushSummary();
			Check(TEXT("l3_retry"), TEXT("gone_is_own_reason_not_nopawn"), S.SkippedGone == 1 && S.SkippedNoPawn == 0 && S.bFlushDone && S.Pending == 0 && CountLog(TEXT("S:LCTXG")) == 0);
			EMMOLifecycleSaveResult RR = EMMOLifecycleSaveResult::Failed;
			bool bRK = false;
			Check(TEXT("l3_retry"), TEXT("retry_recovers_failed_save"), Sub->GetFlushResultFor(XR.PC, RR, bRK) && RR == EMMOLifecycleSaveResult::Saved && bRK && CountLog(TEXT("S:LCTXR")) == 2);
			Check(TEXT("l3_retry"), TEXT("kick_text_after_recovered_retry_says_saved"), KickTexts.FindRef(TEXT("LCTXR")).Contains(TEXT("kaydedildi")));
			Check(TEXT("l3_retry"), TEXT("remote_kick_false_retried_next_step_first_attempt_failed"), CountLog(TEXT("KF:LCTXK")) >= 1 && CountLog(TEXT("K:LCTXK")) == 0);
			StepN(8);
			EMMOLifecycleSaveResult RK = EMMOLifecycleSaveResult::Failed, RB = EMMOLifecycleSaveResult::Failed;
			bool bKK = false, bKB = false;
			Sub->GetFlushResultFor(XK.PC, RK, bKK);
			Sub->GetFlushResultFor(XB.PC, RB, bKB);
			Check(TEXT("l3_retry"), TEXT("kick_retried_until_success"), bKK && CountLog(TEXT("KF:LCTXK")) == 2 && CountLog(TEXT("K:LCTXK")) == 1);
			Check(TEXT("l3_retry"), TEXT("kick_attempts_bounded_by_max"), !bKB && CountLog(TEXT("KF:LCTXB")) == L::MaxKickAttempts && CountLog(TEXT("K:LCTXB")) == 0);
			// Cikis (L3 QA2): kanca atmasi PC'leri yok etmez; bkz. asagidaki yorum.
			Sub->AdvanceSeconds(30.0);
			FLifecycleTestLogCapture ExitCapture;
			Sub->StepExit(7000.0);
			Check(TEXT("l3_retry"), TEXT("waits_for_remaining_controllers"), ExitCalls.Num() == 0);
			// XR ve XK atildi (gercek KickPlayer'da kapanirlar): "ayrildi". XB 3 denemede de atilamadi (bagli kaldi): hem atma hakki bitmis hem hala
			// bagli ama IKI KEZ sayilmaz; XG gone sayilir, kayip degil.
			LifecycleTestDestroyPlayer(XR);
			LifecycleTestDestroyPlayer(XK);
			Sub->StepExit(7000.0 + L::ExitWaitMaxSeconds);
			ExitCapture.Sync();
			Check(TEXT("l3_retry"), TEXT("only_unkickable_player_fails_exit"), ExitCalls.Num() == 1 && ExitCalls[0].Key == L::ExitCodeSaveFailed && ExitCalls[0].Value == 1);
			Check(TEXT("l3_retry"), TEXT("kick_failed_breakdown_logged"), ExitCapture.Count(ELogVerbosity::Display, TEXT("ev=shutdown_exit code=2 failed=1 gone=1 still_connected=0 kick_failed=1")) == 1);
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			LifecycleTestDestroyPlayer(XR);
			LifecycleTestDestroyPlayer(XK);
			LifecycleTestDestroyPlayer(XB);
			LifecycleTestDestroyPlayer(XG);
			HookSaveFailBudget.Reset();
			HookKickFailBudget.Reset();
		}

		// 26) L3 bolge gecisi kaydi UCUSTAYKEN kapanis kaydi: gercek AMMOGameMode durumu (kancasiz, gecici LCTDiskZ* yuvalari).
		{
			UMMOServerLifecycleSubsystem::SetSaveHookForTest(nullptr);
			UMMOServerLifecycleSubsystem::SetPlayerFilterForTest(NameFilter(TEXT("LCTDiskZ")));
			AMMOGameMode* GM = Cast<AMMOGameMode>(World->GetAuthGameMode());
			Check(TEXT("l3_zone"), TEXT("gamemode_is_mmo"), GM != nullptr);
			const FString SlotA = TEXT("MMOSave_LCTDiskZA"), SlotB = TEXT("MMOSave_LCTDiskZB");
			for (const FString& Slot : { SlotA, SlotB }) LifecycleTestCleanSlot(Slot);
			HookLog.Reset();
			KickTexts.Reset();
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			FLifecycleTestPlayer ZA = LifecycleTestMakePlayer(World, TEXT("LCTDiskZA"), true, true, 50);
			FLifecycleTestPlayer ZB = LifecycleTestMakePlayer(World, TEXT("LCTDiskZB"), true, true, 51);
			if (GM && ZA.Pawn && ZB.Pawn)
			{
				// Iki pawn da ucustaki gecis kaydi listesinde (tek GameMode alani; B pawn'i da eklenir).
				FLifecycleTestGameModeAccess::SetTravelFlight(GM, ZA.Pawn, true);
				FLifecycleTestGameModeAccess::AddPending(GM, ZB.Pawn);
				Check(TEXT("l3_zone"), TEXT("travel_in_flight_state_real"), GM->IsZoneTravelSaveInFlight(ZA.Pawn) && GM->IsZoneTravelSaveInFlight(ZB.Pawn)
					&& ZA.Pawn->GetSaveSlotName() == SlotA && ZB.Pawn->GetSaveSlotName() == SlotB);
				FString Err;
				Sub->RequestShutdown(600.0, R::Maintenance, Err);
				Sub->AdvanceSeconds(596.0);
				StepN(1); // ZA kaydi: ucustaki gecis yazimi varken ikinci yazim YAPILMAZ (yeni numara gecisi iptal ettirirdi) -> failed
				EMMOLifecycleSaveResult RA = EMMOLifecycleSaveResult::Saved, RB = EMMOLifecycleSaveResult::Saved;
				bool KA = false, KB = false;
				Check(TEXT("l3_zone"), TEXT("flight_blocks_write_counts_failed"), Sub->GetFlushResultFor(ZA.PC, RA, KA) && RA == EMMOLifecycleSaveResult::Failed && !KA
					&& !LifecycleTestSlotPresent(SlotA) && ZA.Pawn->IsSaveDeferredByTravel() && CountPrefix(TEXT("K:")) == 0);
				FLifecycleTestGameModeAccess::RemovePending(GM, ZA.Pawn); // ZA'nin gecisi bitti (kayit onayi geldi); ZB hala ucusta
				Check(TEXT("l3_zone"), TEXT("flight_cleared_for_a_only"), !GM->IsZoneTravelSaveInFlight(ZA.Pawn) && GM->IsZoneTravelSaveInFlight(ZB.Pawn));
				StepN(1); // ZB kaydi failed -> tur biter -> atma aninda ZA yeniden denenir (artik ucusta degil), ZB hala ucusta
				const bool bKnown = Sub->GetFlushResultFor(ZA.PC, RA, KA) && Sub->GetFlushResultFor(ZB.PC, RB, KB);
				Check(TEXT("l3_zone"), TEXT("results_known"), bKnown);
				Check(TEXT("l3_zone"), TEXT("retry_after_flight_ends_writes_via_deferred_flush"), RA == EMMOLifecycleSaveResult::Saved && IFileManager::Get().FileExists(*LifecycleTestSlotPath(SlotA)) && !ZA.Pawn->IsSaveDeferredByTravel());
				Check(TEXT("l3_zone"), TEXT("still_in_flight_after_retry_counts_failed"), RB == EMMOLifecycleSaveResult::Failed && !LifecycleTestSlotPresent(SlotB) && ZB.Pawn->IsSaveDeferredByTravel());
				Check(TEXT("l3_zone"), TEXT("kick_texts_follow_final_result"), KA && KB && KickTexts.FindRef(TEXT("LCTDiskZB")) == TEXT("Sunucu bakima girdi. Ilerleme kaydedilemedi, yoneticiye bildir.")
					&& KickTexts.FindRef(TEXT("LCTDiskZA")).Contains(TEXT("kaydedildi")));
				// Cikis kodu: yalniz tur + yeniden deneme sonucu sayilir (ZB basarisiz); atmadan sonraki EndPlay kaydi KATILMAZ -> 2.
				ExitCalls.Reset();
				Sub->AdvanceSeconds(30.0);
				UMMOServerLifecycleSubsystem::SetPlayerFilterForTest(NameFilter(TEXT("LCTDiskZB"))); // ZA atildi ve kapandi (kapsam disi); ZB bagli + Failed (bir kez sayilir)
				Sub->StepExit(8000.0);
				Sub->StepExit(8000.0 + L::ExitWaitMaxSeconds);
				Check(TEXT("l3_zone"), TEXT("flight_failure_exit_code_two"), ExitCalls.Num() == 1 && ExitCalls[0].Key == L::ExitCodeSaveFailed && ExitCalls[0].Value == 1);
				FLifecycleTestGameModeAccess::SetTravelFlight(GM, nullptr, false);
				Check(TEXT("l3_zone"), TEXT("flight_state_cleared"), !GM->IsZoneTravelSaveInFlight(ZA.Pawn) && !GM->IsZoneTravelSaveInFlight(ZB.Pawn));
			}
			else
			{
				Check(TEXT("l3_zone"), TEXT("gamemode_or_pawns_missing"), false);
			}
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			LifecycleTestDestroyPlayer(ZA);
			LifecycleTestDestroyPlayer(ZB);
			for (const FString& Slot : { SlotA, SlotB }) LifecycleTestCleanSlot(Slot);
			Check(TEXT("l3_zone"), TEXT("temp_slots_cleaned"), !LifecycleTestSlotPresent(SlotA) && !LifecycleTestSlotPresent(SlotB));
		}

		// 27) L3 Flushing'de ticaret / posta / birlestirme kapisi + aktif ticaretin iptali (gercek bilesenler). QA2: her ticaret kapisi KENDI
		// log etiketiyle (event=maintenance_refused gate=...) ayri sinanir; biri silinirse bir digeri onu maskelemez (davet/kabul/baslatma/onay/esya/altin).
		{
			UMMOServerLifecycleSubsystem::SetSaveHookForTest(SaveHookFn);
			UMMOServerLifecycleSubsystem::SetKickHookForTest(KickHookFn);
			UMMOServerLifecycleSubsystem::SetPlayerFilterForTest(NameFilter(TEXT("LCTG")));
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			HookLog.Reset();
			FLifecycleTestLogCapture GateLog;
			const auto GateCount = [&](const TCHAR* Gate)
			{
				GateLog.Sync();
				return GateLog.Count(ELogVerbosity::Display, *(FString(TEXT("event=maintenance_refused gate=")) + Gate));
			};
			FLifecycleTestPlayer G1 = LifecycleTestMakePlayer(World, TEXT("LCTG1"), true, true, 60);
			FLifecycleTestPlayer G2 = LifecycleTestMakePlayer(World, TEXT("LCTG2"), true, true, 61);
			FLifecycleTestPlayer G3 = LifecycleTestMakePlayer(World, TEXT("LCTG3"), true, true, 62);
			FLifecycleTestPlayer G4 = LifecycleTestMakePlayer(World, TEXT("LCTG4"), true, true, 63);
			FLifecycleTestPlayer GU = LifecycleTestMakePlayer(World, TEXT("LCTGU"), true, false, 64);
			UMMOTradeComponent* T1 = G1.Pawn ? G1.Pawn->GetTradeComponent() : nullptr;
			UMMOTradeComponent* T2 = G2.Pawn ? G2.Pawn->GetTradeComponent() : nullptr;
			UMMOTradeComponent* T3 = G3.Pawn ? G3.Pawn->GetTradeComponent() : nullptr;
			UMMOTradeComponent* T4 = G4.Pawn ? G4.Pawn->GetTradeComponent() : nullptr;
			UMMOMailComponent* M1 = G1.Pawn ? G1.Pawn->GetMailComponent() : nullptr;
			UMMOInventoryComponent* I1 = G1.Pawn ? G1.Pawn->GetInventory() : nullptr;
			UMMOInventoryComponent* IU = GU.Pawn ? GU.Pawn->GetInventory() : nullptr;
			Check(TEXT("l3_gate"), TEXT("setup_components"), T1 && T2 && T3 && T4 && M1 && I1 && IU);
			if (T1 && T2 && T3 && T4 && M1 && I1 && IU)
			{
				G2.Pawn->SetActorLocation(G1.Pawn->GetActorLocation() + FVector(100.0, 0.0, 0.0));
				G4.Pawn->SetActorLocation(G3.Pawn->GetActorLocation() + FVector(100.0, 0.0, 0.0));
				Check(TEXT("l3_gate"), TEXT("idle_is_not_flushing_or_later"), !L::IsFlushingOrLater());
				// Idle: ticaret kurulabilir (on kosul), posta/birlestirme kapi disi nedenlerle reddedilir (bakim nedeniyle DEGIL).
				T1->RequestInviteNearest();
				T2->RequestAccept();
				Check(TEXT("l3_gate"), TEXT("idle_trade_starts_precondition"), T1->IsTrading() && T2->IsTrading());
				// Kabul edilmemis bekleyen davet (G3 -> G4): kabul kapisi Flushing'de bunu reddetmeli.
				T3->RequestInviteNearest();
				Check(TEXT("l3_gate"), TEXT("idle_pending_invite_precondition"), T4->PendingInviteId != 0 && !T4->IsTrading());
				Check(TEXT("l3_gate"), TEXT("idle_mail_not_maintenance"), M1->SendMail(TEXT("LCTGNobody"), 0, TEXT("x"), false) != EMMOMailResult::Maintenance);
				Check(TEXT("l3_gate"), TEXT("idle_fusion_not_locked"), FMMOFusion::Check(*I1, 1, 2, 3).Result != EMMOFusionResult::Locked);
				Check(TEXT("l3_gate"), TEXT("idle_unloaded_fusion_locked"), FMMOFusion::Check(*IU, 1, 2, 3).Result == EMMOFusionResult::Locked);
				FString Err;
				Sub->RequestShutdown(600.0, R::Maintenance, Err);
				Sub->AdvanceSeconds(581.0); // Flushing: tur kurulur, aktif ticaret iptal edilir
				Check(TEXT("l3_gate"), TEXT("flushing_reached"), Sub->GetPhase() == P::Flushing && L::IsFlushingOrLater());
				Check(TEXT("l3_gate"), TEXT("flushing_cancels_active_trade_both_sides"), !T1->IsTrading() && !T2->IsTrading());
				// Davet kapisi: T2 davet eder (nearest = T1); T1'in davet numarasi ARTMAMALI (kabul kapisi bunu maskelemez: numara dogrudan okunur).
				const uint64 InviteIdBefore = T1->PendingInviteId;
				T2->RequestInviteNearest();
				Check(TEXT("l3_gate"), TEXT("flushing_trade_invite_refused_logged"), GateCount(TEXT("trade_invite")) == 1);
				Check(TEXT("l3_gate"), TEXT("flushing_invite_creates_no_pending_id"), T1->PendingInviteId == InviteIdBefore);
				T1->RequestAccept();
				Check(TEXT("l3_gate"), TEXT("flushing_refuses_new_trade"), !T1->IsTrading() && !T2->IsTrading());
				// Kabul kapisi: Idle'da kurulmus bekleyen davet (G3 -> G4) Flushing'de kabul edilemez; kendi etiketi.
				T4->RequestAccept();
				Check(TEXT("l3_gate"), TEXT("flushing_trade_accept_refused_logged"), GateCount(TEXT("trade_accept")) == 1 && !T4->IsTrading() && !T3->IsTrading());
				// Baslatma kapisi: kabul/davet yolundan BAGIMSIZ, dogrudan baslatma.
				FLifecycleTestTradeAccess::StartTrade(G3.Pawn, G4.Pawn);
				Check(TEXT("l3_gate"), TEXT("flushing_trade_start_refused_logged"), GateCount(TEXT("trade_start")) == 1 && !T3->IsTrading() && !T4->IsTrading());
				// Teklif degisikligi / onay kapilari: oturum numarasi gecersiz olsa da kapi ilk satirda log basar.
				T1->RequestAddItemSeen(1, 0, 1);
				Check(TEXT("l3_gate"), TEXT("flushing_trade_additem_refused_logged"), GateCount(TEXT("trade_additem")) == 1);
				T1->RequestSetGoldSeen(1, 0, 5);
				Check(TEXT("l3_gate"), TEXT("flushing_trade_setgold_refused_logged"), GateCount(TEXT("trade_setgold")) == 1);
				T1->RequestConfirmSeen(1, 0);
				Check(TEXT("l3_gate"), TEXT("flushing_trade_confirm_refused_logged"), GateCount(TEXT("trade_confirm")) == 1);
				Check(TEXT("l3_gate"), TEXT("flushing_refuses_mail"), M1->SendMail(TEXT("LCTGNobody"), 0, TEXT("x"), false) == EMMOMailResult::Maintenance);
				// Birlestirme: bakim icin AYRI sonuc (Locked'a dusmez); yuklenmemis karakter hala Locked (siralama: once yukleme kilidi).
				Check(TEXT("l3_gate"), TEXT("flushing_refuses_fusion"), FMMOFusion::Check(*I1, 1, 2, 3).Result == EMMOFusionResult::Maintenance);
				Check(TEXT("l3_gate"), TEXT("flushing_unloaded_fusion_still_locked"), FMMOFusion::Check(*IU, 1, 2, 3).Result == EMMOFusionResult::Locked);
				Check(TEXT("l3_gate"), TEXT("fusion_maintenance_text_is_maintenance"), FString(FMMOFusion::ResultText(EMMOFusionResult::Maintenance)).Contains(TEXT("bakim"))
					&& !FString(FMMOFusion::ResultText(EMMOFusionResult::Maintenance)).Contains(TEXT("yukleniyor")));
				// Iptal edilemeyen faz (Flushing) sonrasi plan sifirlaninca kapi acilir.
				UMMOServerLifecycleSubsystem::ResetPlanForTest();
				Check(TEXT("l3_gate"), TEXT("reset_reopens_gates"), !L::IsFlushingOrLater() && FMMOFusion::Check(*I1, 1, 2, 3).Result != EMMOFusionResult::Maintenance);
			}
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			LifecycleTestDestroyPlayer(G1);
			LifecycleTestDestroyPlayer(G2);
			LifecycleTestDestroyPlayer(G3);
			LifecycleTestDestroyPlayer(G4);
			LifecycleTestDestroyPlayer(GU);
		}

		// 28) L3 QA2: Flushing'de envanter (satin alma/satis/depo/yukseltme-yakma) ve gorev odulu kapilari. Ayni islemler Idle'da calisir (kontrol).
		// Once Flushing'de (yukseltme beklemesi/cooldown hicbir kapiyi maskelemesin), sonra Idle'da; her islem icin durum imzasi + kendi log etiketi.
		{
			UMMOServerLifecycleSubsystem::SetSaveHookForTest(SaveHookFn);
			UMMOServerLifecycleSubsystem::SetKickHookForTest(KickHookFn);
			UMMOServerLifecycleSubsystem::SetPlayerFilterForTest(NameFilter(TEXT("LCTE")));
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			FLifecycleTestLogCapture EconLog;
			const auto GateCount = [&](const TCHAR* Gate)
			{
				EconLog.Sync();
				return EconLog.Count(ELogVerbosity::Display, *(FString(TEXT("event=maintenance_refused gate=")) + Gate));
			};
			FLifecycleTestPlayer E1 = LifecycleTestMakePlayer(World, TEXT("LCTE1"), true, true, 70);
			UMMOInventoryComponent* Inv = E1.Pawn ? E1.Pawn->GetInventory() : nullptr;
			UMMOQuestComponent* Quests = E1.Pawn ? E1.Pawn->GetQuestComponent() : nullptr;
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FVector Spot = E1.Pawn ? E1.Pawn->GetActorLocation() + FVector(100.0, 0.0, 0.0) : FVector::ZeroVector;
			AMMOVendorNPC* Vendor = World->SpawnActor<AMMOVendorNPC>(AMMOVendorNPC::StaticClass(), Spot, FRotator::ZeroRotator, SpawnParams);
			AMMOWarehouseNPC* Warehouse = World->SpawnActor<AMMOWarehouseNPC>(AMMOWarehouseNPC::StaticClass(), Spot, FRotator::ZeroRotator, SpawnParams);
			AMMOQuestGiverNPC* Giver = World->SpawnActor<AMMOQuestGiverNPC>(AMMOQuestGiverNPC::StaticClass(), Spot, FRotator::ZeroRotator, SpawnParams);
			if (Vendor) Vendor->SetActorEnableCollision(false);
			if (Warehouse) Warehouse->SetActorEnableCollision(false);
			if (Giver) Giver->SetActorEnableCollision(false);
			const FName QuestId(TEXT("Q_Gold5000"));
			const bool bEconSetup = Inv && Quests && Vendor && Warehouse && Giver && FMMOItemDatabase::Find(TEXT("ShortSword"))
				&& FMMOConsumableDatabase::Find(TEXT("ManaPotion")) && FMMOQuestDatabase::Find(QuestId);
			Check(TEXT("l3_econ"), TEXT("setup_fixtures"), bEconSetup);
			// 18 etiket her durumda basilir (kurulum bozuksa hepsi basarisiz): sayi sabit kalir.
			const TCHAR* const EconOps[] = { TEXT("buy_potion"), TEXT("buy_consumable"), TEXT("buy_equipment"), TEXT("sell"), TEXT("deposit"), TEXT("withdraw"), TEXT("upgrade"), TEXT("quest_claim") };
			if (bEconSetup)
			{
				const auto Seed = [&]()
				{
					Inv->Items.Reset(); Inv->EquipmentBag.Reset(); Inv->Warehouse.Reset();
					Inv->Gold = 1000000;
					Inv->AddEquipmentInstance(TEXT("ShortSword"));
					Inv->AddEquipmentInstance(TEXT("ShortSword"));
					const FMMOEquipmentInstance Moved = Inv->EquipmentBag.Pop();
					Inv->Warehouse.Add(Moved);
					Quests->ActiveQuests.Reset();
					Quests->CompletedQuests.Reset();
					FMMOQuestProgress Ready;
					Ready.QuestId = QuestId;
					Quests->ActiveQuests.Add(Ready);
				};
				const auto Sig = [&]()
				{
					FString S = FString::Printf(TEXT("g%d|b%d|w%d|i%d|a%d|c%d"), Inv->Gold, Inv->EquipmentBag.Num(), Inv->Warehouse.Num(), Inv->GetTotalItemCount(), Quests->ActiveQuests.Num(), Quests->CompletedQuests.Num());
					for (const FMMOEquipmentInstance& I : Inv->EquipmentBag) S += FString::Printf(TEXT("|%d:%d"), I.InstanceId, I.Plus);
					for (const FMMOEquipmentInstance& I : Inv->Warehouse) S += FString::Printf(TEXT("|w%d"), I.InstanceId);
					return S;
				};
				const auto RunOp = [&](int32 Op)
				{
					switch (Op)
					{
					case 0: Inv->BuyPotion(1); break;
					case 1: Inv->BuyConsumable(TEXT("ManaPotion"), 1); break;
					case 2: Inv->BuyEquipment(TEXT("ShortSword")); break;
					case 3: Inv->SellByInstanceId(Inv->EquipmentBag[0].InstanceId); break;
					case 4: Inv->DepositEquipment(Inv->EquipmentBag[0].InstanceId); break;
					case 5: Inv->WithdrawEquipment(Inv->Warehouse[0].InstanceId); break;
					case 6: Inv->UpgradeByInstanceId(Inv->EquipmentBag[0].InstanceId, Inv->EquipmentBag[0].Plus); break;
					default: Quests->ClaimReward(QuestId); break;
					}
				};
				FString Err;
				Seed();
				Sub->RequestShutdown(600.0, R::Maintenance, Err);
				Sub->AdvanceSeconds(581.0); // Flushing
				const bool bFlushingNow = Sub->GetPhase() == P::Flushing && L::IsFlushingOrLater();
				Check(TEXT("l3_econ"), TEXT("flushing_reached"), bFlushingNow);
				for (int32 Op = 0; Op < 8; ++Op)
				{
					Seed();
					const FString Before = Sig();
					RunOp(Op);
					const bool bUnchanged = Sig() == Before;
					const int32 Logged = GateCount(*((Op < 7 ? FString(TEXT("inv_")) : FString()) + EconOps[Op]));
					Check(TEXT("l3_econ"), *(FString(TEXT("flushing_refuses_")) + EconOps[Op]), bFlushingNow && bUnchanged && Logged == 1);
				}
				UMMOServerLifecycleSubsystem::ResetPlanForTest();
				Check(TEXT("l3_econ"), TEXT("idle_after_reset"), !L::IsFlushingOrLater());
				for (int32 Op = 0; Op < 8; ++Op)
				{
					Seed();
					const FString Before = Sig();
					RunOp(Op);
					Check(TEXT("l3_econ"), *(FString(TEXT("idle_works_")) + EconOps[Op]), Sig() != Before);
				}
			}
			else
			{
				for (int32 Op = 0; Op < 8; ++Op)
				{
					Check(TEXT("l3_econ"), *(FString(TEXT("flushing_refuses_")) + EconOps[Op]), false);
				}
				Check(TEXT("l3_econ"), TEXT("flushing_reached"), false);
				Check(TEXT("l3_econ"), TEXT("idle_after_reset"), false);
				for (int32 Op = 0; Op < 8; ++Op)
				{
					Check(TEXT("l3_econ"), *(FString(TEXT("idle_works_")) + EconOps[Op]), false);
				}
			}
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			if (Vendor) Vendor->Destroy();
			if (Warehouse) Warehouse->Destroy();
			if (Giver) Giver->Destroy();
			LifecycleTestDestroyPlayer(E1);
		}

		// 29) L3 QA2: listen ev sahibi dallari. Oyuncu filtresi ev sahibini KAPSAR (filtre disinda birakmaz): cikista beklenmez, atma bir kez denenir.
		{
			APlayerController* HostPC = nullptr;
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* Candidate = It->Get();
				if (Candidate && Candidate->Player && Candidate->Player->IsA<ULocalPlayer>()) { HostPC = Candidate; break; }
			}
			Check(TEXT("l3_host"), TEXT("host_controller_precondition"), HostPC != nullptr);
			bool bScope = false, bSingleAttempt = false, bNotWaiting = false;
			if (HostPC)
			{
				UMMOServerLifecycleSubsystem::SetSaveHookForTest(SaveHookFn);
				UMMOServerLifecycleSubsystem::SetKickHookForTest(KickHookFn);
				UMMOServerLifecycleSubsystem::SetPlayerFilterForTest([](const APlayerController* PC) { return PC && PC->Player && PC->Player->IsA<ULocalPlayer>(); });
				UMMOServerLifecycleSubsystem::ResetPlanForTest();
				HookLog.Reset();
				ExitCalls.Reset();
				const FString HostName = HookName(HostPC);
				HookKickFailBudget.Add(HostName, 5); // ev sahibine atma hep "false": yeniden deneme anlamsiz, bir kez denenir
				FString Err;
				Sub->RequestShutdown(600.0, R::Maintenance, Err);
				Sub->AdvanceSeconds(700.0); // Exiting: Drain
				bScope = Sub->GetFlushSummary().Players == 1;
				Sub->StepExit(9500.0); // ev sahibi beklenmez: 5 sn gecmeden hemen cikis, kayip yok (StepExit de Drain'i bir kez calistirir)
				bSingleAttempt = CountLog(TEXT("KF:") + HostName) == 1 && CountLog(TEXT("K:") + HostName) == 0;
				bNotWaiting = ExitCalls.Num() == 1 && ExitCalls[0].Key == L::ExitCodeOk && ExitCalls[0].Value == 0;
				HookKickFailBudget.Remove(HostName);
				UMMOServerLifecycleSubsystem::ResetPlanForTest();
			}
			Check(TEXT("l3_host"), TEXT("host_in_round_scope"), bScope);
			Check(TEXT("l3_host"), TEXT("host_kick_attempted_once_not_retried"), bSingleAttempt);
			Check(TEXT("l3_host"), TEXT("exit_does_not_wait_for_host"), bNotWaiting);
		}

		// 30) L3 QA2: Drain'deki KickPlayer yeniden denemeleri ayni cagri icinde degil, StepExit'in Tick'lerine yayilir.
		{
			UMMOServerLifecycleSubsystem::SetSaveHookForTest(SaveHookFn);
			UMMOServerLifecycleSubsystem::SetKickHookForTest(KickHookFn);
			UMMOServerLifecycleSubsystem::SetPlayerFilterForTest(NameFilter(TEXT("LCTW")));
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			HookLog.Reset();
			ExitCalls.Reset();
			HookKickFailBudget.Add(TEXT("LCTW1"), 2);
			HookKickDestroyNames.Add(TEXT("LCTW1"));
			FLifecycleTestPlayer W1 = LifecycleTestMakePlayer(World, TEXT("LCTW1"), true, true, 80);
			FString Err;
			Sub->RequestShutdown(600.0, R::Maintenance, Err);
			Sub->AdvanceSeconds(700.0); // Exiting: Drain (tek cagri)
			Check(TEXT("l3_drain"), TEXT("first_drain_single_attempt"), W1.PC && CountLog(TEXT("KF:LCTW1")) == 1 && CountLog(TEXT("K:LCTW1")) == 0);
			Sub->StepExit(9800.0);
			Check(TEXT("l3_drain"), TEXT("retry_spread_next_tick_second_attempt"), CountLog(TEXT("KF:LCTW1")) == 2 && CountLog(TEXT("K:LCTW1")) == 0 && ExitCalls.Num() == 0);
			Sub->StepExit(9801.0);
			Check(TEXT("l3_drain"), TEXT("third_attempt_succeeds_and_player_gone"), CountLog(TEXT("KF:LCTW1")) == 2 && CountLog(TEXT("K:LCTW1")) == 1 && !IsValid(W1.PC));
			Check(TEXT("l3_drain"), TEXT("exit_clean_after_spread_retries"), ExitCalls.Num() == 1 && ExitCalls[0].Key == L::ExitCodeOk && ExitCalls[0].Value == 0);
			HookKickFailBudget.Remove(TEXT("LCTW1"));
			HookKickDestroyNames.Remove(TEXT("LCTW1"));
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			LifecycleTestDestroyPlayer(W1);
		}

		// 31) L3 QA2: temiz cikis. Yeniden denemeyle kurtulan, atma yeniden denemesi basarili, kayit adimindan once ayrilan (gone) ve tur sonrasi
		// kendi istegiyle ayrilan (Failed kalmis ama atma aninda denetleyici yok -> gone) oyuncular kayip SAYILMAZ: kod 0, gone=2.
		{
			UMMOServerLifecycleSubsystem::SetSaveHookForTest(SaveHookFn);
			UMMOServerLifecycleSubsystem::SetKickHookForTest(KickHookFn);
			UMMOServerLifecycleSubsystem::SetPlayerFilterForTest(NameFilter(TEXT("LCTY")));
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			HookLog.Reset();
			KickTexts.Reset();
			ExitCalls.Reset();
			HookSaveFailBudget.Add(TEXT("LCTYR"), 1);
			HookFailNames.Add(TEXT("LCTYL"));
			HookKickFailBudget.Add(TEXT("LCTYK"), 2);
			HookKickDestroyNames.Add(TEXT("LCTYR"));
			HookKickDestroyNames.Add(TEXT("LCTYK"));
			FLifecycleTestPlayer YR = LifecycleTestMakePlayer(World, TEXT("LCTYR"), true, true, 90);
			FLifecycleTestPlayer YL = LifecycleTestMakePlayer(World, TEXT("LCTYL"), true, true, 91);
			FLifecycleTestPlayer YK = LifecycleTestMakePlayer(World, TEXT("LCTYK"), true, true, 92);
			FLifecycleTestPlayer YG = LifecycleTestMakePlayer(World, TEXT("LCTYG"), true, true, 93);
			FLifecycleTestLogCapture CleanLog;
			FString Err;
			Sub->RequestShutdown(600.0, R::Maintenance, Err);
			Sub->AdvanceSeconds(596.0); // Kicking, tur kurulu: 4 oyuncu bekliyor (sira: R, L, K, G)
			LifecycleTestDestroyPlayer(YG); // kayit adimindan ONCE ayrildi
			StepN(2);                       // R ve L kaydedilir (ikisi de ilk yazimda Failed)
			LifecycleTestDestroyPlayer(YL); // L tur bitmeden, kendi istegiyle ayrildi (atilmadi)
			StepN(1);                       // K
			StepN(1);                       // G gone, tur biter; atma gecisi: R yeniden yazilir + atilir, L gone, K ilk atma false
			StepN(3);                       // K yeniden denemeleri
			FMMOLifecycleFlushSummary CS = Sub->GetFlushSummary();
			Check(TEXT("l3_clean"), TEXT("gone_counts_two_failed_zero"), CS.SkippedGone == 2 && CS.Failed == 0 && CS.Pending == 0 && CS.Saved == 2);
			Check(TEXT("l3_clean"), TEXT("leaver_never_kicked_or_resaved"), CountLog(TEXT("S:LCTYL")) == 1 && CountPrefix(TEXT("K:LCTYL")) == 0 && CountLog(TEXT("KF:LCTYL")) == 0);
			Sub->AdvanceSeconds(30.0);
			Sub->StepExit(9900.0);
			CleanLog.Sync();
			Check(TEXT("l3_clean"), TEXT("exit_code_zero_with_gone_players"), ExitCalls.Num() == 1 && ExitCalls[0].Key == L::ExitCodeOk && ExitCalls[0].Value == 0);
			Check(TEXT("l3_clean"), TEXT("exit_line_reports_gone"), CleanLog.Count(ELogVerbosity::Display, TEXT("ev=shutdown_exit code=0 failed=0 gone=2 still_connected=0 kick_failed=0")) == 1);
			HookSaveFailBudget.Remove(TEXT("LCTYR"));
			HookFailNames.Remove(TEXT("LCTYL"));
			HookKickFailBudget.Remove(TEXT("LCTYK"));
			HookKickDestroyNames.Reset();
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			LifecycleTestDestroyPlayer(YR);
			LifecycleTestDestroyPlayer(YL);
			LifecycleTestDestroyPlayer(YK);
			LifecycleTestDestroyPlayer(YG);
		}

		// 32) L3 QA2: atma aninda Saved sonuclu oyuncu bir kez daha yazilir (bayat snapshot); o yazim basarisizsa Failed + durust metin + kod 2.
		{
			UMMOServerLifecycleSubsystem::SetSaveHookForTest(SaveHookFn);
			UMMOServerLifecycleSubsystem::SetKickHookForTest(KickHookFn);
			UMMOServerLifecycleSubsystem::SetPlayerFilterForTest(NameFilter(TEXT("LCTV")));
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			HookLog.Reset();
			KickTexts.Reset();
			ExitCalls.Reset();
			HookSaveOkBudget.Add(TEXT("LCTVS"), 1); // tur yazimi basarili, atma aninda yeniden yazim basarisiz
			HookKickDestroyNames.Add(TEXT("LCTVM"));
			FLifecycleTestPlayer VM = LifecycleTestMakePlayer(World, TEXT("LCTVM"), true, true, 100);
			FLifecycleTestPlayer VS = LifecycleTestMakePlayer(World, TEXT("LCTVS"), true, true, 101);
			FLifecycleTestLogCapture ResaveLog;
			FString Err;
			Sub->RequestShutdown(600.0, R::Maintenance, Err);
			Sub->AdvanceSeconds(596.0);
			StepN(3);
			ResaveLog.Sync();
			int32 SavesBeforeKick = 0;
			for (const FString& Entry : HookLog)
			{
				if (Entry == TEXT("K:LCTVM")) break;
				if (Entry == TEXT("S:LCTVM")) ++SavesBeforeKick;
			}
			Check(TEXT("l2_resave"), TEXT("saved_player_written_again_before_kick"), CountLog(TEXT("S:LCTVM")) == 2 && SavesBeforeKick == 2 && CountLog(TEXT("K:LCTVM")) == 1);
			EMMOLifecycleSaveResult RS = EMMOLifecycleSaveResult::Saved;
			bool bKS = false;
			Check(TEXT("l2_resave"), TEXT("resave_failure_demotes_saved_to_failed"),
				VS.PC && Sub->GetFlushResultFor(VS.PC, RS, bKS) && RS == EMMOLifecycleSaveResult::Failed && bKS
				&& KickTexts.FindRef(TEXT("LCTVS")) == TEXT("Sunucu bakima girdi. Son degisiklikler kaydedilemeyebilir, onceki kayit gecerli.")
				&& KickTexts.FindRef(TEXT("LCTVS")) == FString(L::KickTextRewriteFailed())
				&& !KickTexts.FindRef(TEXT("LCTVS")).Contains(TEXT("kaydedildi")) && !KickTexts.FindRef(TEXT("LCTVS")).Contains(TEXT("Ilerleme kaydedilemedi")));
			Check(TEXT("l2_resave"), TEXT("metric_resave_failed_line"), ResaveLog.Count(ELogVerbosity::Warning, TEXT("ev=shutdown_rewrite_failed player=LCTVS")) == 1);
			Check(TEXT("l2_resave"), TEXT("metric_old_stage_absent_for_rewrite"), ResaveLog.Count(ELogVerbosity::Warning, TEXT("stage=kick_resave")) == 0);
			Check(TEXT("l2_resave"), TEXT("rewrite_text_is_distinct_and_honest"),
				FString(L::KickTextRewriteFailed()) != FString(L::KickText(EMMOLifecycleSaveResult::Failed)) && FString(L::KickTextRewriteFailed()) != FString(L::KickText(EMMOLifecycleSaveResult::Saved))
				&& !FString(L::KickTextRewriteFailed()).Contains(TEXT("kaydedildi")));
			Sub->AdvanceSeconds(30.0);
			Sub->StepExit(9950.0);
			Sub->StepExit(9950.0 + L::ExitWaitMaxSeconds);
			Check(TEXT("l2_resave"), TEXT("exit_code_two_when_resave_fails"), ExitCalls.Num() == 1 && ExitCalls[0].Key == L::ExitCodeSaveFailed && ExitCalls[0].Value == 1);
			HookSaveOkBudget.Reset();
			HookKickDestroyNames.Reset();
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			LifecycleTestDestroyPlayer(VM);
			LifecycleTestDestroyPlayer(VS);
		}

		// 33) L3 QA2: cikis durum dosyasi + Windows kod tasima notu (saf yardimcilar; gercek TerminateProcess CAGRILMAZ).
		{
			Check(TEXT("exit_file"), TEXT("forced_code_not_needed_for_zero"), !L::NeedsForcedExitCode(0));
			Check(TEXT("exit_file"), TEXT("forced_code_for_nonzero_matches_platform"), L::NeedsForcedExitCode(2) == (PLATFORM_WINDOWS != 0));
			const FDateTime FixedTime(2026, 10, 27, 12, 30, 45);
			const FString Json = L::BuildResultJson(TEXT("20261027T123045Z-4242"), 2, 5, 1, 3, 1, FixedTime);
			Check(TEXT("exit_file"), TEXT("json_content_exact"), Json == TEXT("{\"run_id\":\"20261027T123045Z-4242\",\"exit_code\":2,\"failed\":5,\"gone\":1,\"still_connected\":3,\"kick_failed\":1,\"time_utc\":\"2026-10-27T12:30:45.000Z\"}\n"));
			const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectLogDir() / TEXT("LCTResult"));
			const FString File = Dir / TEXT("r.json");
			IFileManager::Get().DeleteDirectory(*Dir, false, true);
			FString Loaded;
			const bool bFirst = L::WriteResultFile(File, Json);
			FFileHelper::LoadFileToString(Loaded, *File);
			Check(TEXT("exit_file"), TEXT("write_creates_dir_and_file_no_temp_left"), bFirst && Loaded == Json && !IFileManager::Get().FileExists(*(File + TEXT(".tmp"))));
			const FString Json2 = L::BuildResultJson(L::RunId(), 0, 0, 0, 0, 0, FixedTime);
			const bool bSecond = L::WriteResultFile(File, Json2);
			FFileHelper::LoadFileToString(Loaded, *File);
			Check(TEXT("exit_file"), TEXT("write_overwrites_existing"), bSecond && Loaded == Json2);
			const FString Normalized = LifecycleTestNormalizedSlashes(L::ResultFilePath());
			Check(TEXT("exit_file"), TEXT("path_is_saved_logs_result_json"), Normalized.EndsWith(TEXT("/Logs/MMOShutdownResult.json")) && !FPaths::IsRelative(L::ResultFilePath()));
			const FString Blocker = Dir / TEXT("blocker.txt");
			FFileHelper::SaveStringToFile(TEXT("x"), *Blocker);
			Check(TEXT("exit_file"), TEXT("write_fails_cleanly_when_parent_is_a_file"), !L::WriteResultFile(Blocker / TEXT("child.json"), Json));
			IFileManager::Get().DeleteDirectory(*Dir, false, true);
			Check(TEXT("exit_file"), TEXT("temp_dir_cleaned"), !IFileManager::Get().DirectoryExists(*Dir));

			// Bayat durum dosyasi + run_id (denetim: onceki calismadan kalan dosya yanlis guven vermesin).
			Check(TEXT("exit_file"), TEXT("run_id_valid_charset"), L::IsValidRunId(L::RunId()) && L::RunId().Contains(TEXT("-")));
			Check(TEXT("exit_file"), TEXT("run_id_validator_rejects_bad"),
				!L::IsValidRunId(FString()) && !L::IsValidRunId(TEXT("2026 1027")) && !L::IsValidRunId(TEXT("a\"b")) && !L::IsValidRunId(TEXT("12/34"))
				&& !L::IsValidRunId(TEXT("12\\34")) && !L::IsValidRunId(FString::ChrN(41, TEXT('1'))) && L::IsValidRunId(FString::ChrN(40, TEXT('1'))));
			Check(TEXT("exit_file"), TEXT("run_id_stable_within_process"), L::RunId() == L::RunId() && &L::RunId() == &L::RunId());
			const FString RunTag = FString::Printf(TEXT("\"run_id\":\"%s\""), *L::RunId());
			Check(TEXT("exit_file"), TEXT("json_has_run_id_field"), Json2.Contains(RunTag) && Json2.StartsWith(TEXT("{") + RunTag) && L::BuildResultJson(TEXT("bad id\""), 0, 0, 0, 0, 0, FixedTime).StartsWith(TEXT("{\"run_id\":\"\",")));
			const FString StaleDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectLogDir() / TEXT("LCTStale"));
			const FString StaleFile = StaleDir / TEXT("MMOShutdownResult.json");
			IFileManager::Get().DeleteDirectory(*StaleDir, false, true);
			FFileHelper::SaveStringToFile(TEXT("{\"run_id\":\"20200101T000000Z-1\",\"exit_code\":0}"), *StaleFile);
			FFileHelper::SaveStringToFile(TEXT("x"), *(StaleFile + TEXT(".tmp")));
			const bool bStaleExisted = IFileManager::Get().FileExists(*StaleFile);
			const bool bStaleDeleted = L::DeleteStaleResultFile(StaleFile);
			Check(TEXT("exit_file"), TEXT("stale_file_deleted"), bStaleExisted && bStaleDeleted && !IFileManager::Get().FileExists(*StaleFile) && !IFileManager::Get().FileExists(*(StaleFile + TEXT(".tmp"))));
			Check(TEXT("exit_file"), TEXT("stale_delete_missing_file_is_ok"), L::DeleteStaleResultFile(StaleFile));
			IFileManager::Get().DeleteDirectory(*StaleDir, false, true);
			Check(TEXT("exit_file"), TEXT("startup_cleanup_ran_at_world_begin"), L::StaleResultCleanupDone());
			Check(TEXT("exit_file"), TEXT("startup_cleanup_is_once_per_process"), !L::ClearStaleResultOnce());
		}

		UMMOServerLifecycleSubsystem::SetSaveHookForTest(nullptr);
		UMMOServerLifecycleSubsystem::SetKickHookForTest(nullptr);
		UMMOServerLifecycleSubsystem::SetExitHookForTest(nullptr);
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

// Kapsam notu (L1-L3 + QA turu 2): duyuru metni/sirasi, komut ayristirma, PreLogin reddi, R1 kapisi, uzak-ServerExec benzetimi, kayit turu/atma/cikis kodu
// (cikis KANCAYLA yakalanir: gercek RequestExit ve gercek UNetConnection atmasi burada kanitlanmaz) ve Flushing kapilari. Iki surecli gercek
// dedicated dogrulama (TestShutdownSmoke) L4'tur.

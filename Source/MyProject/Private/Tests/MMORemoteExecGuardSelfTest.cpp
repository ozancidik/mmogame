// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/MMOExecGuard.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOStatsComponent.h"
#include "Core/MMOGameMode.h"
#include "Core/MMOServerLifecycle.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CheatManager.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Parse.h"
#include "Player/MMOPlayerController.h"

// Y-xx uzak ServerExec korumasi native testi (docs/SECURITY_AUDIT.md). CANLI dunya ister (Scripts/TestRemoteExecGuard.ps1).
// Uzak istemci = Player'i ULocalPlayer olmayan UPlayer (sunucuda UNetConnection ile ayni dal). Uzak PC'ye ConsoleCommand (zincir:
// UPlayer::Exec -> ... -> AMMOPlayerController::ProcessConsoleExec) ve dogrudan ServerExecRPC/ServerPause _Implementation cagrilir.
// Her Check benzersiz case/check cifti basar. Anonim namespace adlari ExecGuardTest onekli (unity-build tuzagi).
namespace
{
	constexpr int32 ExecGuardTestExpectedChecks = 49;

	// LogMMOExecGuard satirlarini yakalar. Okumadan once Sync().
	class FExecGuardTestLogCapture final : public FOutputDevice
	{
	public:
		FExecGuardTestLogCapture() { GLog->AddOutputDevice(this); }
		virtual ~FExecGuardTestLogCapture() override { GLog->RemoveOutputDevice(this); }
		void Sync() { GLog->Flush(); }
		int32 Count(const FString& Needle) const
		{
			int32 N = 0;
			for (const FString& L : Lines) { if (L.Contains(Needle)) ++N; }
			return N;
		}
		virtual void Serialize(const TCHAR* Text, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if (Category == LogMMOExecGuard.GetCategoryName() && (Verbosity & ELogVerbosity::VerbosityMask) == ELogVerbosity::Warning)
			{
				Lines.Add(FString(Text));
			}
		}
	private:
		TArray<FString> Lines;
	};

	UWorld* ExecGuardTestFindWorld(UWorld* Hint)
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

	bool ExecGuardTestRun(UWorld* InWorld, FString& OutError, int32& OutChecks)
	{
		OutChecks = 0;
		OutError.Reset();
		int32 Failures = 0;
		const auto Check = [&](const TCHAR* Case, const TCHAR* Label, bool bOK)
		{
			++OutChecks;
			UE_LOG(LogTemp, Display, TEXT("[MMO_EXECGUARD_CASE] %s case=%s check=%s"), bOK ? TEXT("PASS") : TEXT("FAIL"), Case, Label);
			if (!bOK) { ++Failures; if (OutError.IsEmpty()) OutError = FString(Case) + TEXT("/") + Label; }
		};

		UWorld* World = ExecGuardTestFindWorld(InWorld);
		AMMOGameMode* GM = World ? World->GetAuthGameMode<AMMOGameMode>() : nullptr;
		UMMOServerLifecycleSubsystem* Sub = World ? World->GetSubsystem<UMMOServerLifecycleSubsystem>() : nullptr;
		if (!World || !GM || !Sub)
		{
			OutError = TEXT("no_live_game_world_or_gamemode");
			return false;
		}

		// 1) Saf yardimcilar.
		{
			Check(TEXT("token"), TEXT("basic"), MMOExecGuard::FirstCommandToken(TEXT("MMOLoad")) == TEXT("MMOLoad"));
			Check(TEXT("token"), TEXT("skips_leading_space"), MMOExecGuard::FirstCommandToken(TEXT("  \t MMOPvP 1")) == TEXT("MMOPvP"));
			Check(TEXT("token"), TEXT("stops_at_space"), MMOExecGuard::FirstCommandToken(TEXT("MMOGiveItem ScrollHaste 3")) == TEXT("MMOGiveItem"));
			Check(TEXT("token"), TEXT("truncates_32"), MMOExecGuard::FirstCommandToken(*FString::ChrN(100, TEXT('A'))) == FString::ChrN(32, TEXT('A')));
			Check(TEXT("token"), TEXT("strips_control"), MMOExecGuard::FirstCommandToken(TEXT("MMO\x01Load\x7f")) == TEXT("MMOLoad"));
			Check(TEXT("token"), TEXT("empty_and_null"), MMOExecGuard::FirstCommandToken(TEXT("   ")) == TEXT("(bos)") && MMOExecGuard::FirstCommandToken(nullptr) == TEXT("(bos)"));
			Check(TEXT("token"), TEXT("sanitize_spaces_control"), MMOExecGuard::SanitizeToken(TEXT("Ali Veli\n\x02x"), 32) == TEXT("Ali_Velix"));
			Check(TEXT("token"), TEXT("sanitize_max_len"), MMOExecGuard::SanitizeToken(*FString::ChrN(50, TEXT('B')), 10).Len() == 10);
		}

		// 2) Kurulum: tek pawn, uzak (UPlayer) ve yerel (ULocalPlayer) denetleyici. Pawn yuklu degil (MMOSave/ayrilma kaydi yazilmaz).
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AMMOCharacter* Pawn = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(0, 0, 5000), FRotator::ZeroRotator, Params);
		AMMOPlayerController* RemotePC = World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(), FVector(0, 0, 5000), FRotator::ZeroRotator, Params);
		AMMOPlayerController* LocalPC = World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(), FVector(0, 0, 5000), FRotator::ZeroRotator, Params);
		UPlayer* FakeRemote = NewObject<UPlayer>(World);
		ULocalPlayer* FakeLocal = GEngine ? NewObject<ULocalPlayer>(GEngine) : nullptr;
		const bool bSetup = Pawn && RemotePC && LocalPC && FakeRemote && FakeLocal && Pawn->GetStats() && Pawn->GetInventory();
		Check(TEXT("setup"), TEXT("spawned"), bSetup);
		if (!bSetup)
		{
			if (RemotePC) RemotePC->Destroy();
			if (LocalPC) LocalPC->Destroy();
			if (Pawn) Pawn->Destroy();
			OutError = TEXT("setup_failed");
			return false;
		}
		RemotePC->Player = FakeRemote;
		FakeRemote->PlayerController = RemotePC;
		LocalPC->Player = FakeLocal;
		FakeLocal->PlayerController = LocalPC;
		RemotePC->SetPawn(Pawn);
		LocalPC->SetPawn(Pawn);
		Check(TEXT("setup"), TEXT("remote_is_remote"), MMOExecGuard::IsRemoteClientController(RemotePC));
		Check(TEXT("setup"), TEXT("local_is_not_remote"), !MMOExecGuard::IsRemoteClientController(LocalPC));

		UMMOStatsComponent* Stats = Pawn->GetStats();
		UMMOInventoryComponent* Inv = Pawn->GetInventory();
		const bool bPvPBefore = GM->bAllowPvP;
		Inv->Gold = 1234;
		const int32 Gold0 = Inv->Gold;
		const int32 Xp0 = Stats->Experience;
		const int32 Level0 = Stats->Level;
		const float Health0 = Stats->Health;
		const int32 Items0 = Inv->GetItemCount(TEXT("ScrollHaste"));
		FExecGuardTestLogCapture Log;
		const TCHAR* Rejected = TEXT("reddedildi");

		// 3) Uzak istemci: her komut reddedilir, uyari+metrik satiri yazilir.
		{
			UMMOServerLifecycleSubsystem::ResetPlanForTest();
			const auto RemoteBlocked = [&](const TCHAR* Case, const TCHAR* Cmd, const TCHAR* Token)
			{
				const FString Out = RemotePC->ConsoleCommand(Cmd, false);
				Log.Sync();
				Check(Case, TEXT("rejected"), Out.Contains(Rejected));
				Check(Case, TEXT("logged"), Log.Count(FString::Printf(TEXT("ev=remote_exec_blocked cmd=%s "), Token)) == 1);
			};
			RemoteBlocked(TEXT("cmd_load"), TEXT("MMOLoad"), TEXT("MMOLoad"));
			RemoteBlocked(TEXT("cmd_pvp"), TEXT("MMOPvP 1"), TEXT("MMOPvP"));
			RemoteBlocked(TEXT("cmd_gainxp"), TEXT("MMOGainXP 2000000000"), TEXT("MMOGainXP"));
			RemoteBlocked(TEXT("cmd_giveitem"), TEXT("MMOGiveItem ScrollHaste 3"), TEXT("MMOGiveItem"));
			RemoteBlocked(TEXT("cmd_enablecheats"), TEXT("EnableCheats"), TEXT("EnableCheats"));
			RemoteBlocked(TEXT("cmd_switchlevel"), TEXT("SwitchLevel NonexistentMapZZZ"), TEXT("SwitchLevel"));
			RemoteBlocked(TEXT("cmd_shutdown"), TEXT("MMOShutdown 600"), TEXT("MMOShutdown"));
			RemoteBlocked(TEXT("cmd_damage"), TEXT("MMODamage 99999"), TEXT("MMODamage"));
			Check(TEXT("state"), TEXT("gold_unchanged"), Inv->Gold == Gold0);
			Check(TEXT("state"), TEXT("xp_level_unchanged"), Stats->Experience == Xp0 && Stats->Level == Level0);
			Check(TEXT("state"), TEXT("health_unchanged"), Stats->Health == Health0);
			Check(TEXT("state"), TEXT("item_unchanged"), Inv->GetItemCount(TEXT("ScrollHaste")) == Items0);
			Check(TEXT("state"), TEXT("pvp_unchanged"), GM->bAllowPvP == bPvPBefore);
			Check(TEXT("state"), TEXT("no_cheat_manager"), RemotePC->CheatManager == nullptr && LocalPC->CheatManager == nullptr);
			Check(TEXT("state"), TEXT("shutdown_idle"), Sub->GetPhase() == EMMOShutdownPhase::Idle && !Sub->IsLoginClosed());
		}

		// 4) Gercek RPC yolu (ServerExecRPC_Implementation) ve ServerPause: UWorld::Exec dahil hicbir sey calismaz.
		{
			RemotePC->ServerExecRPC_Implementation(TEXT("FLUSHPERSISTENTDEBUGLINES"));
			Log.Sync();
			Check(TEXT("rpc"), TEXT("world_exec_blocked"), Log.Count(TEXT("cmd=FLUSHPERSISTENTDEBUGLINES ")) == 1);
			RemotePC->ServerExecRPC_Implementation(TEXT("MMOGainXP 2000000000"));
			Log.Sync();
			Check(TEXT("rpc"), TEXT("gainxp_blocked"), Log.Count(TEXT("cmd=MMOGainXP ")) == 2);
			Check(TEXT("rpc"), TEXT("gainxp_no_effect"), Stats->Experience == Xp0 && Stats->Level == Level0);
			RemotePC->ServerPause_Implementation();
			Log.Sync();
			Check(TEXT("rpc"), TEXT("pause_blocked"), Log.Count(TEXT("cmd=ServerPause ")) == 1);
			const bool bPaused = World->IsPaused();
			Check(TEXT("rpc"), TEXT("pause_not_paused"), !bPaused);
			if (bPaused) { RemotePC->SetPause(false); }
			// Log enjeksiyonu: kontrol karakteri ve satir sonu sizmaz, belirtec 32 ile kesilir.
			RemotePC->ServerExecRPC_Implementation(TEXT("  \x01MMO\x02Gain\nevil=1 ek"));
			RemotePC->ServerExecRPC_Implementation(*FString::ChrN(100, TEXT('A')));
			Log.Sync();
			Check(TEXT("rpc"), TEXT("log_control_stripped"), Log.Count(TEXT("cmd=MMOGain ")) == 1);
			Check(TEXT("rpc"), TEXT("log_no_forged_field"), Log.Count(TEXT("evil=1")) == 0);
			Check(TEXT("rpc"), TEXT("log_token_truncated_32"),
				Log.Count(TEXT("cmd=") + FString::ChrN(32, TEXT('A')) + TEXT(" player=")) == 1 && Log.Count(TEXT("cmd=") + FString::ChrN(33, TEXT('A'))) == 0);
		}

		// 5) Bilinen kalan risk (belgelenir, korunmus sayilmaz): UPlayer::Exec'te UWorld::Exec, PC'den ONCE sorgulanir; ConsoleCommand
		// yolundan gelen dunya komutlari (FLUSHPERSISTENTDEBUGLINES, DEMOREC ...) PC korumasina ulasmaz. Gercek RPC yolu yukarida kapali.
		{
			const FString Out = RemotePC->ConsoleCommand(TEXT("FLUSHPERSISTENTDEBUGLINES"), false);
			Check(TEXT("residual"), TEXT("world_exec_precedes_pc_via_consolecommand"), !Out.Contains(Rejected) && !Out.Contains(TEXT("Command not recognized")));
		}

		// 6) Yerel denetleyici (ULocalPlayer = host/standalone): komutlar calismaya devam eder, engel logu yazilmaz.
		{
			Log.Sync();
			const int32 BlockedBefore = Log.Count(TEXT("ev=remote_exec_blocked"));
			LocalPC->ConsoleCommand(TEXT("MMOGainXP 5"), false);
			Check(TEXT("local"), TEXT("console_gainxp_works"), Stats->Experience == Xp0 + 5 && Stats->Level == Level0);
			LocalPC->ServerExecRPC_Implementation(TEXT("MMOGainXP 3"));
			Check(TEXT("local"), TEXT("rpc_gainxp_works"), Stats->Experience == Xp0 + 8);
			LocalPC->ConsoleCommand(TEXT("MMOPvP 1"), false);
			Check(TEXT("local"), TEXT("pvp_toggled"), GM->bAllowPvP);
			LocalPC->ServerPause_Implementation();
			if (World->IsPaused()) { LocalPC->SetPause(false); }
			Log.Sync();
			Check(TEXT("local"), TEXT("never_logged_as_blocked"), Log.Count(TEXT("ev=remote_exec_blocked")) == BlockedBefore);
			Check(TEXT("local"), TEXT("pause_not_blocked"), Log.Count(TEXT("cmd=ServerPause ")) == 1);
		}

		// 7) Temizlik.
		GM->SetPvPAllowed(bPvPBefore);
		RemotePC->Player = nullptr;
		FakeRemote->PlayerController = nullptr;
		LocalPC->Player = nullptr;
		FakeLocal->PlayerController = nullptr;
		RemotePC->SetPawn(nullptr);
		LocalPC->SetPawn(nullptr);
		RemotePC->Destroy();
		LocalPC->Destroy();
		Pawn->Destroy();
		UMMOServerLifecycleSubsystem::ResetPlanForTest();
		Check(TEXT("end"), TEXT("pvp_restored"), GM->bAllowPvP == bPvPBefore);

		if (OutChecks != ExecGuardTestExpectedChecks) { ++Failures; OutError += TEXT(" check_count_mismatch"); }
		if (Failures) OutError = FString::Printf(TEXT("failures=%d first=%s"), Failures, *OutError);
		return Failures == 0;
	}

	struct FMMOExecGuardTestExec final : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestRemoteExecGuard"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = ExecGuardTestRun(World, Error, Checks);
			Ar.Logf(TEXT("[MMO_EXECGUARD_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOExecGuardTestExec GMMOExecGuardTestExec;
}
#endif

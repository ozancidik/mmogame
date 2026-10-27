// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/MMOExecGuard.h"
#include "Player/MMOPlayerController.h"
#include "Core/MMOMetrics.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerState.h"
#include "Misc/OutputDevice.h"

DEFINE_LOG_CATEGORY(LogMMOExecGuard);

// Neden: Shipping disi derlemelerde istemci APlayerController::ServerExecRPC ile sunucuda ConsoleCommand calistirabilir
// (PlayerController.cpp ServerExecRPC_Implementation). Komut UPlayer::Exec zincirinden pawn'in UFUNCTION(Exec) fonksiyonlarina
// (MMOLoad, MMOPvP, MMOGainXP, MMOGiveItem ...), CheatManager'a (EnableCheats) ve GameMode'a ulasir ve uzak pawn sunucuda yetkili
// oldugu icin IsServerAdmin kapisi atlanir. Iki katman: (1) ServerExecRPC_Implementation hic calistirmaz (UWorld::Exec dahil, o
// zincirde PC'den ONCE sorgulanir), (2) ProcessConsoleExec PC'ye ulasan her komutu reddeder (ConsoleCommand baska yoldan gelirse).
namespace MMOExecGuard
{
	bool IsRemoteClientController(const APlayerController* PC)
	{
		return PC && PC->Player && !Cast<ULocalPlayer>(PC->Player);
	}

	FString SanitizeToken(const FString& Raw, int32 MaxLen)
	{
		FString Out;
		for (const TCHAR C : Raw)
		{
			if (Out.Len() >= MaxLen) break;
			if (C < 32 || C == 127) continue;
			Out.AppendChar(C == TEXT(' ') ? TEXT('_') : C);
		}
		return Out;
	}

	FString FirstCommandToken(const TCHAR* Cmd)
	{
		FString Token;
		if (Cmd)
		{
			while (*Cmd && FChar::IsWhitespace(*Cmd)) ++Cmd;
			for (; *Cmd && !FChar::IsWhitespace(*Cmd) && Token.Len() < 32; ++Cmd)
			{
				if (*Cmd < 32 || *Cmd == 127) continue;
				Token.AppendChar(*Cmd);
			}
		}
		return Token.IsEmpty() ? FString(TEXT("(bos)")) : Token;
	}

	void LogBlocked(const APlayerController* PC, const TCHAR* Cmd)
	{
		FString Player = (PC && PC->PlayerState) ? SanitizeToken(PC->PlayerState->GetPlayerName(), 32) : FString();
		if (Player.IsEmpty()) Player = TEXT("?");
		UE_LOG(LogMMOExecGuard, Warning, TEXT("[MMOMETRIC] t=%.1f ev=remote_exec_blocked cmd=%s player=%s"),
			MMOMetrics::Seconds(), *FirstCommandToken(Cmd), *Player);
	}
}

bool AMMOPlayerController::ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor)
{
	if (MMOExecGuard::IsRemoteClientController(this))
	{
		MMOExecGuard::LogBlocked(this, Cmd);
		Ar.Log(TEXT("Komut reddedildi: uzak istemci sunucuda konsol komutu calistiramaz."));
		return true; // true = zinciri keser (Pawn / HUD / GameMode / CheatManager sorgulanmaz)
	}
	return Super::ProcessConsoleExec(Cmd, Ar, Executor);
}

void AMMOPlayerController::ServerExecRPC_Implementation(const FString& Msg)
{
	if (MMOExecGuard::IsRemoteClientController(this))
	{
		MMOExecGuard::LogBlocked(this, *Msg);
		return;
	}
	Super::ServerExecRPC_Implementation(Msg);
}

void AMMOPlayerController::ServerPause_Implementation()
{
	// Motor yalniz Shipping'de uzak duraklatmayi reddeder (ServerPause_Validate); gelistirme sunucusunda herhangi bir istemci oyunu durdurabilirdi.
	if (MMOExecGuard::IsRemoteClientController(this))
	{
		MMOExecGuard::LogBlocked(this, TEXT("ServerPause"));
		return;
	}
	Super::ServerPause_Implementation();
}

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
	bool IsConsoleExecAllowedFor(bool bHasPlayer, bool bPlayerIsLocal, ENetMode NetMode)
	{
		if (bHasPlayer) return bPlayerIsLocal;
		return NetMode == NM_Standalone;
	}

	bool IsConsoleExecAllowed(const APlayerController* PC)
	{
		if (!PC) return false;
		return IsConsoleExecAllowedFor(PC->Player != nullptr, Cast<ULocalPlayer>(PC->Player) != nullptr, PC->GetNetMode());
	}

	FString SanitizeToken(const FString& Raw, int32 MaxLen)
	{
		FString Out;
		for (const TCHAR C : Raw)
		{
			if (Out.Len() >= MaxLen) break;
			if (C == TEXT('=')) continue;
			const bool bOK = (C >= TEXT('A') && C <= TEXT('Z')) || (C >= TEXT('a') && C <= TEXT('z')) || (C >= TEXT('0') && C <= TEXT('9'))
				|| C == TEXT('_') || C == TEXT('.') || C == TEXT('-');
			Out.AppendChar(bOK ? C : TEXT('?'));
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
				Token.AppendChar(*Cmd);
			}
		}
		Token = SanitizeToken(Token, 32);
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
	if (!MMOExecGuard::IsConsoleExecAllowed(this))
	{
		MMOExecGuard::LogBlocked(this, Cmd);
		Ar.Log(TEXT("Komut reddedildi: uzak istemci sunucuda konsol komutu calistiramaz."));
		return true; // true = zinciri keser (Pawn / HUD / GameMode / CheatManager sorgulanmaz)
	}
	return Super::ProcessConsoleExec(Cmd, Ar, Executor);
}

bool AMMOPlayerController::ServerExecRPC_Validate(const FString& Msg)
{
	// false = motor bu istemcinin baglantisini keser (ServerPause_Validate Shipping'de ayni mekanizmayi kullanir). Baglanti kapanmadan
	// ayni kareden gelen RPC'ler tekrar gelebilir: PC basina yalniz ilk engel loglanir (log seli yok).
	const bool bTooLong = Msg.Len() > MMOExecGuard::MaxServerExecLength;
	if (!MMOExecGuard::IsConsoleExecAllowed(this) || (bTooLong && !IsLocalController()))
	{
		if (!bExecValidateBlockLogged)
		{
			bExecValidateBlockLogged = true;
			MMOExecGuard::LogBlocked(this, *Msg);
		}
		return false;
	}
	return Super::ServerExecRPC_Validate(Msg);
}

void AMMOPlayerController::ServerExecRPC_Implementation(const FString& Msg)
{
	if (!MMOExecGuard::IsConsoleExecAllowed(this))
	{
		MMOExecGuard::LogBlocked(this, *Msg);
		return;
	}
	if (Msg.Len() > MMOExecGuard::MaxServerExecLength)
	{
		UE_LOG(LogMMOExecGuard, Warning, TEXT("[MMOMETRIC] t=%.1f ev=exec_too_long len=%d cmd=%s"),
			MMOMetrics::Seconds(), Msg.Len(), *MMOExecGuard::FirstCommandToken(*Msg));
		return;
	}
	Super::ServerExecRPC_Implementation(Msg);
}

bool AMMOPlayerController::ServerPause_Validate()
{
	if (!MMOExecGuard::IsConsoleExecAllowed(this))
	{
		if (!bExecValidateBlockLogged)
		{
			bExecValidateBlockLogged = true;
			MMOExecGuard::LogBlocked(this, TEXT("ServerPause"));
		}
		return false;
	}
	return Super::ServerPause_Validate();
}

void AMMOPlayerController::ServerPause_Implementation()
{
	// Motor yalniz Shipping'de uzak duraklatmayi reddeder (ServerPause_Validate); gelistirme sunucusunda herhangi bir istemci oyunu durdurabilirdi.
	if (!MMOExecGuard::IsConsoleExecAllowed(this))
	{
		MMOExecGuard::LogBlocked(this, TEXT("ServerPause"));
		return;
	}
	Super::ServerPause_Implementation();
}

void AMMOPlayerController::AddCheats(bool bForce)
{
	// EnableCheats (Shipping disinda AddCheats(true)) uzak istemci icin CheatManager olusturmasin; ProcessConsoleExec'i atlayan yol.
	if (!MMOExecGuard::IsConsoleExecAllowed(this))
	{
		return;
	}
	Super::AddCheats(bForce);
}

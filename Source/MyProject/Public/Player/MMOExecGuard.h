// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"

class APlayerController;

DECLARE_LOG_CATEGORY_EXTERN(LogMMOExecGuard, Log, All);

// Y-xx (docs/SECURITY_AUDIT.md): uzak istemcinin ServerExec RPC'siyle sunucuda konsol komutu calistirmasini kesen tek karar noktasi.
// Kullanici: AMMOPlayerController::ProcessConsoleExec / ServerExecRPC_* / ServerPause_* / AddCheats
// (Private/Player/MMOPlayerControllerExecGuard.cpp). Saf yardimcilar test edilir (MMORemoteExecGuardSelfTest).
namespace MMOExecGuard
{
	// FAIL-CLOSED karar: konsol komutuna YALNIZ (Player ULocalPlayer) ya da (Player yok ve NM_Standalone) PC izin verir.
	// Uzak istemci (UNetConnection), dedicated/listen'de Player'i henuz olmayan PC ve null hepsi reddedilir.
	bool IsConsoleExecAllowed(const APlayerController* PC);

	// Saf karar (test edilebilir): IsConsoleExecAllowed'in dogruluk tablosu.
	bool IsConsoleExecAllowedFor(bool bHasPlayer, bool bPlayerIsLocal, ENetMode NetMode);

	// ServerExecRPC ust siniri (motor istemci tarafinda 128'e keser; bunun ustu istismar).
	constexpr int32 MaxServerExecLength = 256;

	// Log alani izin listesi [A-Za-z0-9_.-]: digerleri '?' olur ('=' atilir), en cok MaxLen karakter (log satiri/alan enjeksiyonuna karsi;
	// ASCII disi, Unicode bosluk/satir ayiricilari ve kontrol karakterleri dahil).
	FString SanitizeToken(const FString& Raw, int32 MaxLen);

	// Komutun ilk belirteci (bastaki bosluk atlanir, ilk bosluga kadar), SanitizeToken ile temiz, en cok 32 karakter; bossa "(bos)".
	FString FirstCommandToken(const TCHAR* Cmd);

	// "[MMOMETRIC] ev=remote_exec_blocked cmd=<ilk belirtec> player=<ad>" (Warning, LogMMOExecGuard).
	void LogBlocked(const APlayerController* PC, const TCHAR* Cmd);
}

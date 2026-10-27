// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class APlayerController;

DECLARE_LOG_CATEGORY_EXTERN(LogMMOExecGuard, Log, All);

// Y-xx (docs/SECURITY_AUDIT.md): uzak istemcinin ServerExec RPC'siyle sunucuda konsol komutu calistirmasini kesen tek karar noktasi.
// Kullanici: AMMOPlayerController::ProcessConsoleExec / ServerExecRPC_Implementation / ServerPause_Implementation
// (Private/Player/MMOPlayerControllerExecGuard.cpp). Saf yardimcilar test edilir (MMORemoteExecGuardSelfTest).
namespace MMOExecGuard
{
	// Sunucudaki denetleyicinin Player'i var ve ULocalPlayer degil (UNetConnection) = uzak istemci.
	// Host / standalone / istemcinin kendi PC'si ULocalPlayer tasir; Player'i olmayan PC uzak sayilmaz.
	bool IsRemoteClientController(const APlayerController* PC);

	// Kontrol karakterlerini atar, bosluklari '_' yapar, en cok MaxLen karakter (log satiri/alan enjeksiyonuna karsi).
	FString SanitizeToken(const FString& Raw, int32 MaxLen);

	// Komutun ilk belirteci (bastaki bosluk atlanir, ilk bosluga kadar), kontrol karakteri temiz, en cok 32 karakter; bossa "(bos)".
	FString FirstCommandToken(const TCHAR* Cmd);

	// "[MMOMETRIC] ev=remote_exec_blocked cmd=<ilk belirtec> player=<ad>" (Warning, LogMMOExecGuard).
	void LogBlocked(const APlayerController* PC, const TCHAR* Cmd);
}

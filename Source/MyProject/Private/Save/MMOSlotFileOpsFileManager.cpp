// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOSlotFileAtomic.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// Gercek dosya sistemi (docs/DESIGN_CHARACTER_SAVE_ATOMIC.md, S6.1/S6.2). Tek parcacikli: yalniz oyun is parcacigi cagirir.

bool FMMOFileManagerSlotOps::SleepForRetry()
{
	// Toplam bekleme butcesi asilmaz: oyun is parcacigi uzun bloklanmaz (bir nesne icin RetryBudgetSeconds).
	if (SleptSeconds + RetrySleepSeconds > RetryBudgetSeconds + KINDA_SMALL_NUMBER)
	{
		return false;
	}
	// OLCULEN sure birikir (Windows Sleep ~15.6 ms'ye yuvarlayabilir; sabit sayilan sure butceyi sessizce asardi).
	const double T0 = FPlatformTime::Seconds();
	FPlatformProcess::Sleep(static_cast<float>(RetrySleepSeconds));
	SleptSeconds += FPlatformTime::Seconds() - T0;
	++RetriesPerformed;
	return true;
}

bool FMMOFileManagerSlotOps::Exists(const FString& Path)
{
	return IFileManager::Get().FileExists(*Path);
}

bool FMMOFileManagerSlotOps::ReadAll(const FString& Path, TArray<uint8>& Out)
{
	Out.Reset();
	const int64 Size = IFileManager::Get().FileSize(*Path);
	if (Size < 0 || Size > FMMOSlotFileAtomic::MaxBytes)
	{
		return false; // yok / okunamaz / tavandan buyuk: I/O hatasi sayilir (Save fail-closed, Load IoError); yeniden denenmez
	}
	for (int32 Attempt = 0; Attempt < RetryAttempts; ++Attempt)
	{
		const bool bInjected = TestInjectFailure && TestInjectFailure(TEXT("read"), Path, Attempt);
		if (!bInjected && FFileHelper::LoadFileToArray(Out, *Path, FILEREAD_Silent))
		{
			return true;
		}
		Out.Reset();
		if (!IFileManager::Get().FileExists(*Path) || Attempt + 1 >= RetryAttempts || !SleepForRetry())
		{
			break;
		}
	}
	return false;
}

bool FMMOFileManagerSlotOps::WriteNew(const FString& Path, const TArray<uint8>& Bytes)
{
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	IFileHandle* Handle = TestOpenWrite ? TestOpenWrite(Path) : PF.OpenWrite(*Path, /*bAppend*/ false, /*bAllowRead*/ false);
	if (!Handle)
	{
		return false; // ornegin mevcut dosya salt-okunur
	}
	bool bOk = true;
	if (Bytes.Num() > 0)
	{
		bOk = Handle->Write(Bytes.GetData(), Bytes.Num());
	}
	// Tam flush: yazim "basarili" sayilmadan once icerik diske itilir (Move'dan once kalicilik).
	bOk = bOk && Handle->Flush(true);
	bOk = bOk && (Handle->Tell() == Bytes.Num());
	delete Handle;
	return bOk;
}

bool FMMOFileManagerSlotOps::Move(const FString& Dest, const FString& Src)
{
	// QA-12: IFileManager::Move(Replace) hedefi ONCE siler, kaynagin varligina sonra bakar (MoveFileW). Kaynak yoksa hedef bosuna kaybolurdu.
	if (!IFileManager::Get().FileExists(*Src)) { return false; }
	for (int32 Attempt = 0; Attempt < RetryAttempts; ++Attempt)
	{
		const bool bInjected = TestInjectFailure && TestInjectFailure(TEXT("move"), Src, Attempt);
		// Replace=true (motor: hedef varsa once siler), EvenIfReadOnly=false, Attributes=false, bDoNotRetryOrError=true (motorun 10x0.5 sn beklemesi yok).
		if (!bInjected && IFileManager::Get().Move(*Dest, *Src, true, false, false, true))
		{
			return true;
		}
		if (!IFileManager::Get().FileExists(*Src) || Attempt + 1 >= RetryAttempts || !SleepForRetry())
		{
			break;
		}
	}
	return false;
}

bool FMMOFileManagerSlotOps::Delete(const FString& Path, bool bEvenReadOnly)
{
	for (int32 Attempt = 0; Attempt < RetryAttempts; ++Attempt)
	{
		const bool bInjected = TestInjectFailure && TestInjectFailure(TEXT("delete"), Path, Attempt);
		// RequireExists=false: dosya yoksa true; EvenReadOnly: yalniz kodun kendi .tmp/.bak dosyalari icin; Quiet=true.
		if (!bInjected && IFileManager::Get().Delete(*Path, false, bEvenReadOnly, true))
		{
			return true;
		}
		if ((!bInjected && !IFileManager::Get().FileExists(*Path)) || Attempt + 1 >= RetryAttempts || !SleepForRetry())
		{
			break;
		}
	}
	return false;
}

void FMMOFileManagerSlotOps::ClearReadOnly(const FString& Path)
{
	if (IFileManager::Get().FileExists(*Path))
	{
		FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*Path, false);
	}
}

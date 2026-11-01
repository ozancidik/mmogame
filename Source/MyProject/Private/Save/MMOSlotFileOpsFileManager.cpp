// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOSlotFileAtomic.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// Gercek dosya sistemi (docs/DESIGN_CHARACTER_SAVE_ATOMIC.md, S6.1). Tek parcacikli: yalniz oyun is parcacigi cagirir.

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
		return false; // yok / okunamaz / tavandan buyuk: I/O hatasi sayilir (Save fail-closed, Load bozuk)
	}
	if (!FFileHelper::LoadFileToArray(Out, *Path, FILEREAD_Silent))
	{
		Out.Reset();
		return false;
	}
	return true;
}

bool FMMOFileManagerSlotOps::WriteNew(const FString& Path, const TArray<uint8>& Bytes)
{
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	IFileHandle* Handle = PF.OpenWrite(*Path, /*bAppend*/ false, /*bAllowRead*/ false);
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
	// Replace=true (motor: hedef varsa once siler), EvenIfReadOnly=false, Attributes=false, bDoNotRetryOrError=true (10x0.5 sn bekleme yok).
	return IFileManager::Get().Move(*Dest, *Src, true, false, false, true);
}

bool FMMOFileManagerSlotOps::Delete(const FString& Path)
{
	// RequireExists=false: dosya yoksa true; EvenReadOnly=false: salt-okunur dosya silinmez (false); Quiet=true.
	return IFileManager::Get().Delete(*Path, false, false, true);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOSlotFileAtomic.h"

FMMOSlotAtomicPaths FMMOSlotAtomicPaths::From(const FString& MainPath)
{
	FMMOSlotAtomicPaths P;
	P.Main = MainPath;
	P.Tmp = MainPath + TEXT(".tmp");
	P.Bak = MainPath + TEXT(".bak");
	return P;
}

FString FMMOSlotAtomicPaths::BadSlot(int32 Index) const
{
	return Index <= 0 ? Main + TEXT(".bad") : Main + FString::Printf(TEXT(".bad.%d"), Index);
}

FString FMMOSlotAtomicPaths::TmpBad() const
{
	return Tmp + TEXT(".bad");
}

bool FMMOSlotFileAtomic::IsValidSlotName(const FString& SlotName)
{
	const int32 PrefixLen = 8; // "MMOSave_"
	if (!SlotName.StartsWith(TEXT("MMOSave_"), ESearchCase::CaseSensitive)) return false;
	if (SlotName.Len() <= PrefixLen || SlotName.Len() > 128) return false;
	for (int32 i = PrefixLen; i < SlotName.Len(); ++i)
	{
		const TCHAR C = SlotName[i];
		const bool bAllowed = (C >= TEXT('A') && C <= TEXT('Z')) || (C >= TEXT('a') && C <= TEXT('z')) || (C >= TEXT('0') && C <= TEXT('9')) || C == TEXT(' ') || C == TEXT('_') || C == TEXT('-');
		if (!bAllowed) return false;
	}
	return SlotName[SlotName.Len() - 1] != TEXT(' ');
}

EMMOSlotFileState FMMOSlotFileAtomic::Classify(IMMOSlotFileOps& Ops, const FString& Path, FMMOSlotValidator Validate, TArray<uint8>* OutBytes, bool& bOutIoError)
{
	bOutIoError = false;
	if (!Ops.Exists(Path)) return EMMOSlotFileState::Missing;
	TArray<uint8> Local;
	TArray<uint8>& Buf = OutBytes ? *OutBytes : Local;
	Buf.Reset();
	if (!Ops.ReadAll(Path, Buf))
	{
		bOutIoError = true;
		return EMMOSlotFileState::Corrupt;
	}
	if (Buf.Num() <= 0 || !Validate(Buf)) return EMMOSlotFileState::Corrupt;
	return EMMOSlotFileState::Valid;
}

FMMOSlotLoadResult FMMOSlotFileAtomic::Load(IMMOSlotFileOps& Ops, const FMMOSlotAtomicPaths& Paths, FMMOSlotValidator Validate, const FMMOSlotValidator* TmpValidate)
{
	FMMOSlotLoadResult R;
	bool bIo = false;
	const EMMOSlotFileState MainState = Classify(Ops, Paths.Main, Validate, &R.Bytes, bIo);
	if (MainState == EMMOSlotFileState::Valid)
	{
		R.bOk = true;
		R.Source = EMMOSlotLoadSource::Main;
		R.Failure = EMMOSlotLoadFailure::None;
		return R;
	}
	R.Bytes.Reset();
	const EMMOSlotFileState BakState = Classify(Ops, Paths.Bak, Validate, &R.Bytes, bIo);
	if (BakState == EMMOSlotFileState::Valid)
	{
		R.bOk = true;
		R.Source = EMMOSlotLoadSource::Bak;
		R.Failure = EMMOSlotLoadFailure::None;
		return R;
	}
	R.Bytes.Reset();
	// .tmp yalniz ana VE .bak hic yokken (ilk kayit yarida kesildi) ve dogrulayici kabul ederse; yarim olabilecek dosya baska yolda kullanilmaz.
	if (MainState == EMMOSlotFileState::Missing && BakState == EMMOSlotFileState::Missing)
	{
		if (Classify(Ops, Paths.Tmp, TmpValidate ? *TmpValidate : Validate, &R.Bytes, bIo) == EMMOSlotFileState::Valid)
		{
			R.bOk = true;
			R.Source = EMMOSlotLoadSource::Tmp;
			R.Failure = EMMOSlotLoadFailure::None;
			return R;
		}
		R.Bytes.Reset();
	}
	R.bOk = false;
	R.Source = EMMOSlotLoadSource::None;
	R.Failure = (MainState == EMMOSlotFileState::Corrupt || BakState == EMMOSlotFileState::Corrupt) ? EMMOSlotLoadFailure::Corrupt : EMMOSlotLoadFailure::Missing;
	return R;
}

bool FMMOSlotFileAtomic::Exists(IMMOSlotFileOps& Ops, const FMMOSlotAtomicPaths& Paths)
{
	return Ops.Exists(Paths.Main) || Ops.Exists(Paths.Bak);
}

void FMMOSlotFileAtomic::DiscardTmp(IMMOSlotFileOps& Ops, const FMMOSlotAtomicPaths& Paths)
{
	// Kendi .tmp'miz: silinir; silinemezse .tmp.bad'e ayrilir (bir sonraki Save yeniden .tmp kullanabilsin).
	if (Ops.Exists(Paths.Tmp) && !Ops.Delete(Paths.Tmp))
	{
		Ops.Move(Paths.TmpBad(), Paths.Tmp);
	}
}

FMMOSlotSaveResult FMMOSlotFileAtomic::Save(IMMOSlotFileOps& Ops, const FMMOSlotAtomicPaths& Paths, const TArray<uint8>& Bytes, FMMOSlotValidator Validate, const FMMOSlotValidator* TmpValidate)
{
	FMMOSlotSaveResult R;
	const auto Fail = [&R](EMMOSlotSaveFailure Why) { R.bOk = false; R.Failure = Why; return R; };

	// 1-2) Boyut ve dogrulama: basarisizsa hicbir dosyaya dokunulmaz.
	if (Bytes.Num() <= 0 || Bytes.Num() > MaxBytes) return Fail(EMMOSlotSaveFailure::BadSize);
	if (!Validate(Bytes)) return Fail(EMMOSlotSaveFailure::InvalidBytes);

	// Mevcut dosyalari SINIFLA (yalniz okuma). Okunamayan dosya bozuk sayilip tasinmaz: fail-closed.
	bool bIo = false;
	const EMMOSlotFileState MainState = Classify(Ops, Paths.Main, Validate, nullptr, bIo);
	if (bIo) return Fail(EMMOSlotSaveFailure::MainUnreadable);
	EMMOSlotFileState BakState = Classify(Ops, Paths.Bak, Validate, nullptr, bIo);
	if (bIo) return Fail(EMMOSlotSaveFailure::MainUnreadable);

	// Bozuk dosyalar SILINMEZ, .bad yuvalarina ayrilir; yuvalar degisiklikten ONCE ayrilir (yetmiyorsa hicbir sey degismez).
	const int32 BadNeeded = (BakState == EMMOSlotFileState::Corrupt ? 1 : 0) + (MainState == EMMOSlotFileState::Corrupt ? 1 : 0);
	FString BadForBak;
	FString BadForMain;
	int32 Found = 0;
	for (int32 i = 0; i < MaxBadFiles && Found < BadNeeded; ++i)
	{
		const FString Slot = Paths.BadSlot(i);
		if (Ops.Exists(Slot)) continue;
		if (Found == 0 && BakState == EMMOSlotFileState::Corrupt) BadForBak = Slot;
		else BadForMain = Slot;
		++Found;
	}
	if (Found < BadNeeded) return Fail(EMMOSlotSaveFailure::BadCapReached);

	// 3a) Ana ve .bak yokken GECERLI bir .tmp, Load'un verdigi tek kopyadir (yarida kalmis ilk kayit): silinmez, .bak olur (tek yeniden adlandirma).
	if (MainState == EMMOSlotFileState::Missing && BakState == EMMOSlotFileState::Missing && Classify(Ops, Paths.Tmp, TmpValidate ? *TmpValidate : Validate, nullptr, bIo) == EMMOSlotFileState::Valid)
	{
		if (!Ops.Move(Paths.Bak, Paths.Tmp)) return Fail(EMMOSlotSaveFailure::RotateFailed);
		BakState = EMMOSlotFileState::Valid;
	}

	// 3) Eski .tmp: silinir; silinemezse false (dogrudan-yazim yedegi YOK, atomiklik geri gelmesin).
	if (Ops.Exists(Paths.Tmp) && !Ops.Delete(Paths.Tmp)) return Fail(EMMOSlotSaveFailure::TmpDeleteFailed);

	// 4) .tmp yaz, geri oku, bayt-bayt esit olmali.
	if (!Ops.WriteNew(Paths.Tmp, Bytes))
	{
		DiscardTmp(Ops, Paths);
		return Fail(EMMOSlotSaveFailure::TmpWriteFailed);
	}
	TArray<uint8> Back;
	if (!Ops.ReadAll(Paths.Tmp, Back) || Back != Bytes)
	{
		DiscardTmp(Ops, Paths);
		return Fail(EMMOSlotSaveFailure::TmpReadbackMismatch);
	}

	// 5) Eski dosyalari yoldan cek. Bozuk .bak -> .bad; bozuk ana -> .bad; gecerli ana -> .bak (eski .bak'in yerine).
	if (BakState == EMMOSlotFileState::Corrupt && !Ops.Move(BadForBak, Paths.Bak))
	{
		DiscardTmp(Ops, Paths);
		return Fail(EMMOSlotSaveFailure::RotateFailed);
	}
	if (MainState == EMMOSlotFileState::Corrupt)
	{
		if (!Ops.Move(BadForMain, Paths.Main))
		{
			DiscardTmp(Ops, Paths);
			return Fail(EMMOSlotSaveFailure::RotateFailed);
		}
	}
	else if (MainState == EMMOSlotFileState::Valid)
	{
		if (!Ops.Move(Paths.Bak, Paths.Main))
		{
			DiscardTmp(Ops, Paths);
			return Fail(EMMOSlotSaveFailure::RotateFailed);
		}
	}

	// 6) .tmp -> ana. Bu noktada ana yol bos oldugundan Move'un "once hedefi sil" penceresi yoktur; tek yeniden adlandirma.
	if (!Ops.Move(Paths.Main, Paths.Tmp))
	{
		if (Ops.Exists(Paths.Bak) && !Ops.Exists(Paths.Main))
		{
			Ops.Move(Paths.Main, Paths.Bak);
		}
		DiscardTmp(Ops, Paths);
		return Fail(EMMOSlotSaveFailure::PromoteFailed);
	}

	// 7) Basari: .bak temizlenir; silinemezse bayat kopya kalir (yalniz ana yok/gecersizken kullanilir).
	if (Ops.Exists(Paths.Bak) && !Ops.Delete(Paths.Bak)) R.bStaleBakLeft = true;
	R.bOk = true;
	R.Failure = EMMOSlotSaveFailure::None;
	return R;
}

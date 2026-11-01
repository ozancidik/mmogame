// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Karakter .sav atomik yazim ve kurtarma mantigi (docs/DESIGN_CHARACTER_SAVE_ATOMIC.md, dilim S6.0).
 * SAF: dunya, UObject, motor dosya sistemi yok. Tum disk islemleri IMMOSlotFileOps uzerinden gider; boylece
 * "her dosya isleminden sonra surec olur" matrisi sahte bir dosya sistemiyle denenebilir. Magaza baglantisi (S6.1) ayri.
 *
 * Dosya adlari (hepsi ayni klasorde): <Ana>=<Yuva>.sav, <Ana>.tmp, <Ana>.bak, <Ana>.bad, <Ana>.bad.1 ... .bad.4.
 * Save: dogrula -> .tmp yaz -> geri oku -> bozuk .bak/.ana'yi .bad'e AYIR (silme yok) -> gecerli ana'yi .bak'a al
 *       -> .tmp'yi ana adina tasi -> .bak'i temizle. Ana dosya hicbir an yarim yazilmaz.
 * Load: YAN ETKISIZ. ana gecerliyse o; degilse .bak; ana ve .bak HIC yoksa .tmp. Onarim bir sonraki basarili Save'dedir.
 */

/** Soyut dosya sistemi. Gercek uygulama (S6.1) IFileManager'a baglanir; Move gercekte "once hedefi sil, sonra tasi"dir. */
class IMMOSlotFileOps
{
public:
	virtual ~IMMOSlotFileOps() {}
	virtual bool Exists(const FString& Path) = 0;
	virtual bool ReadAll(const FString& Path, TArray<uint8>& Out) = 0;
	/** Yolu yeni icerikle olusturur/ezer. Gercekte yarida kesilirse kesik dosya kalabilir. */
	virtual bool WriteNew(const FString& Path, const TArray<uint8>& Bytes) = 0;
	/**
	 * Src'yi Dest'e tasir, Dest varsa ONCE SILER (IFileManager::Move(Replace=true) davranisi, iki ayri adim: arada Dest yoktur).
	 * Gercek uygulama tekrarsiz olmali (bDoNotRetryOrError=true): motorun 10x0.5 sn beklemesi sunucuyu dondurmasin.
	 */
	virtual bool Move(const FString& Dest, const FString& Src) = 0;
	/** Dosya yoksa true (zaten yok); silinemezse false. */
	virtual bool Delete(const FString& Path) = 0;
};

enum class EMMOSlotFileState : uint8
{
	Missing,
	Valid,    // okundu, bos degil, dogrulayici kabul etti
	Corrupt   // var ama okunamadi / bos / dogrulayici reddetti
};

enum class EMMOSlotLoadSource : uint8
{
	None,
	Main,
	Bak,
	Tmp
};

enum class EMMOSlotLoadFailure : uint8
{
	None,
	Missing,   // kullanilabilir dosya yok ve bozuk ana/.bak da yok (yeni karakter)
	Corrupt    // ana ya da .bak var ama hicbiri gecerli degil
};

enum class EMMOSlotSaveFailure : uint8
{
	None,
	BadSize,            // bos ya da MaxBytes'tan buyuk; hicbir dosyaya dokunulmadi
	InvalidBytes,       // dogrulayici yeni baytlari reddetti; hicbir dosyaya dokunulmadi
	MainUnreadable,     // ana/.bak var ama okunamadi; fail-closed, hicbir dosyaya dokunulmadi
	BadCapReached,      // bozuk dosyalari saklayacak .bad yuvasi kalmadi; hicbir dosyaya dokunulmadi
	TmpDeleteFailed,    // eski .tmp silinemedi (dogrudan-yazim yedegi YOK)
	TmpWriteFailed,
	TmpReadbackMismatch,
	RotateFailed,       // .bak/.bad'e alma basarisiz; ana dosya (varsa) yerinde
	PromoteFailed       // .tmp -> ana basarisiz; .bak varsa ana yoluna geri konmaya calisildi
};

struct FMMOSlotLoadResult
{
	bool bOk = false;
	EMMOSlotLoadSource Source = EMMOSlotLoadSource::None;
	EMMOSlotLoadFailure Failure = EMMOSlotLoadFailure::Missing;
	TArray<uint8> Bytes;
};

struct FMMOSlotSaveResult
{
	bool bOk = false;
	EMMOSlotSaveFailure Failure = EMMOSlotSaveFailure::None;
	bool bStaleBakLeft = false;  // Save basarili ama .bak silinemedi (bayat kopya; arayan uyari loglar)
};

struct FMMOSlotAtomicPaths
{
	FString Main;
	FString Tmp;
	FString Bak;

	/** Ana yol (<Yuva>.sav) verilir, digerleri ona eklenir. */
	static FMMOSlotAtomicPaths From(const FString& MainPath);
	/** Bozuk dosya yuvasi: 0 = .bad, i>=1 = .bad.<i>; en fazla MaxBadFiles yuva. */
	FString BadSlot(int32 Index) const;
	/** Silinemeyen .tmp'nin ayrildigi yol. */
	FString TmpBad() const;
};

/** Dogrulayici: baytlar tam ve gecerli bir kayit mi (gercekte LoadGameFromMemory + bValid). */
using FMMOSlotValidator = TFunctionRef<bool(const TArray<uint8>&)>;

class FMMOSlotFileAtomic
{
public:
	/** Yazim bayt tavani (tasarim 4.1 adim 1; MMOSaveFilesLib.ps1 ile ayni). [PLACEHOLDER] */
	static constexpr int32 MaxBytes = 8 * 1024 * 1024;
	/** Saklanacak en fazla bozuk dosya sayisi (.bad, .bad.1 .. .bad.4). [PLACEHOLDER 5] */
	static constexpr int32 MaxBadFiles = 5;

	/** ^MMOSave_[A-Za-z0-9 _-]+$ ve sonda bosluk yok (Windows sondaki boslugu atar -> iki ad ayni dosya olurdu). Yol gecisi savunmasi. */
	static bool IsValidSlotName(const FString& SlotName);

	/** Yan etkisiz: hicbir dosya olusturulmaz/silinmez/tasinmaz. */
	static FMMOSlotLoadResult Load(IMMOSlotFileOps& Ops, const FMMOSlotAtomicPaths& Paths, FMMOSlotValidator Validate);

	/** Ana dosya var ya da .bak var. (.tmp tek basina yetmez: yarim olabilir.) */
	static bool Exists(IMMOSlotFileOps& Ops, const FMMOSlotAtomicPaths& Paths);

	static FMMOSlotSaveResult Save(IMMOSlotFileOps& Ops, const FMMOSlotAtomicPaths& Paths, const TArray<uint8>& Bytes, FMMOSlotValidator Validate);

private:
	static EMMOSlotFileState Classify(IMMOSlotFileOps& Ops, const FString& Path, FMMOSlotValidator Validate, TArray<uint8>* OutBytes, bool& bOutIoError);
	static void DiscardTmp(IMMOSlotFileOps& Ops, const FMMOSlotAtomicPaths& Paths);
};

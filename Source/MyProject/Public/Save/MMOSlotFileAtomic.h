// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Karakter .sav atomik yazim ve kurtarma mantigi (docs/DESIGN_CHARACTER_SAVE_ATOMIC.md, dilim S6.0).
 * SAF: dunya, UObject, motor dosya sistemi yok. Tum disk islemleri IMMOSlotFileOps uzerinden gider; boylece
 * "her dosya isleminden sonra surec olur" matrisi sahte bir dosya sistemiyle denenebilir. Gercek dosya sistemi (FMMOFileManagerSlotOps, asagida)
 * ve magaza baglantisi (FMMOLocalFileCharacterStore, Private/Save) S6.1'de eklendi; bu sinif yine saf mantiktir.
 *
 * Dosya adlari (hepsi ayni klasorde): <Ana>=<Yuva>.sav, <Ana>.tmp, <Ana>.bak, <Ana>.bad, <Ana>.bad.1 ... .bad.4.
 * Save: dogrula -> .tmp yaz -> geri oku -> bozuk .bak/.ana'yi .bad'e AYIR (silme yok) -> gecerli ana'yi .bak'a al
 *       -> .tmp'yi ana adina tasi -> .bak'i temizle. Ana dosya hicbir an yarim yazilmaz.
 * Load: YAN ETKISIZ. ana gecerliyse o; ana bozuksa (icerik) .bak; ana ve .bak HIC yoksa .tmp. Ana VAR ama okunamiyorsa (I/O) .bak'a DUSULMEZ
 *       (IoError). Onarim bir sonraki basarili Save'dedir.
 * S6.2: Delete(bEvenReadOnly) yalniz kodun kendi .tmp/.bak dosyalari icin; ana->.bak tasimasindan sonra .bak'in salt-okunur biti temizlenir;
 *       Exists = ana VEYA .bak VEYA gecerli .tmp (Load ile tutarli).
 */

/** Soyut dosya sistemi. Gercek uygulama (FMMOFileManagerSlotOps) IFileManager'a baglanir; Move gercekte "once hedefi sil, sonra tasi"dir. */
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
	/**
	 * Dosya yoksa true (zaten yok); silinemezse false. bEvenReadOnly YALNIZ kodun kendi urettigi .tmp ve .bak dosyalari icin true verilir
	 * (S6.2 G-ORTA-1: salt-okunur bit onlari sonsuza dek kilitlemesin); ana dosya ve .bad kanittir, onlara false.
	 */
	virtual bool Delete(const FString& Path, bool bEvenReadOnly) = 0;
	/** Salt-okunur biti temizler (en iyi cabadir, sonuc donmez). Yol yoksa etkisiz. */
	virtual void ClearReadOnly(const FString& Path) = 0;
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
	Corrupt,   // ana ya da .bak var ama hicbiri gecerli degil
	IoError    // ana ya da .bak VAR ama I/O hatasiyla okunamadi (S6.2 G-ORTA-3): .bak'a DUSULMEZ, sonuc belirsiz
};

enum class EMMOSlotSaveFailure : uint8
{
	None,
	BadSize,            // bos ya da MaxBytes'tan buyuk; hicbir dosyaya dokunulmadi
	InvalidBytes,       // dogrulayici yeni baytlari reddetti; hicbir dosyaya dokunulmadi
	NonCanonical,       // uretilen baytlar yeniden serilestirmede ayni degil (S6.2 QA-2); hicbir dosyaya dokunulmadi
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

	/**
	 * Yan etkisiz: hicbir dosya olusturulmaz/silinmez/tasinmaz.
	 * TmpValidate (S6.1, opsiyonel): .tmp'yi (yarim yazilmis olabilecek tek dosya) SIKI denetleyen dogrulayici; verilmezse Validate.
	 * Ana/.bak eski (sonek yok, dogrudan yazilmis) dosyalari da icerdiginden gevsek Validate ile; .tmp ise her zaman bu kod tarafindan yazilir.
	 */
	static FMMOSlotLoadResult Load(IMMOSlotFileOps& Ops, const FMMOSlotAtomicPaths& Paths, FMMOSlotValidator Validate, const FMMOSlotValidator* TmpValidate = nullptr);

	/**
	 * Ana dosya var ya da .bak var ya da (TmpValidate verildiyse) GECERLI bir .tmp var. Yarim .tmp yetmez. Load ile ayni kural:
	 * Load .tmp'yi yalniz ana ve .bak hic yokken kullanir; o durumda Exists true olmali ki cagiran Load'a gitsin (ilk kayit yarida kesildi).
	 */
	static bool Exists(IMMOSlotFileOps& Ops, const FMMOSlotAtomicPaths& Paths, const FMMOSlotValidator* TmpValidate = nullptr);

	static FMMOSlotSaveResult Save(IMMOSlotFileOps& Ops, const FMMOSlotAtomicPaths& Paths, const TArray<uint8>& Bytes, FMMOSlotValidator Validate, const FMMOSlotValidator* TmpValidate = nullptr);

private:
	static EMMOSlotFileState Classify(IMMOSlotFileOps& Ops, const FString& Path, FMMOSlotValidator Validate, TArray<uint8>* OutBytes, bool& bOutIoError);
	static void DiscardTmp(IMMOSlotFileOps& Ops, const FMMOSlotAtomicPaths& Paths);
};

/**
 * Gercek dosya sistemi (S6.1): IFileManager / IPlatformFile uzerinde IMMOSlotFileOps.
 *  - Move: IFileManager::Move(Replace=true, bDoNotRetryOrError=true). Motor kaynagi (FileManagerGeneric.cpp Move): hedef varsa ONCE silinir,
 *    sonra MoveFileW; tekrar/bekleme yok (sunucu donmaz). Hedef silinemezse (salt-okunur/kilitli) MoveFileW de hedef var oldugu icin basarisiz olur.
 *  - WriteNew: IPlatformFile::OpenWrite (CREATE_ALWAYS) + Write + Flush(tam); salt-okunur mevcut dosyada acilamaz -> false.
 *  - ReadAll: MaxBytes'tan buyuk dosya okunmaz (false = I/O hatasi, Save fail-closed).
 *  - Delete: bEvenReadOnly=false iken salt-okunur dosyayi SILMEZ; dosya yoksa true.
 *  - Move: KAYNAK YOKSA hic denemez (false). IFileManager::Move(Replace) kaynak yokken de ONCE hedefi siler (motor kaynagi
 *    FileManagerGeneric.cpp Move: hedef silme, sonra MoveFileW); koruma olmasa var olmayan bir .tmp'yi tasimaya calismak ana dosyayi yok ederdi (S6.2 QA-12).
 *  - Gecici hata (antivirus/yedek tarayicisinin paylasim ihlali): ReadAll/Move/Delete en fazla RetryAttempts kez dener, denemeler arasinda
 *    RetrySleepSeconds uyur; bir nesnenin toplam bekleme butcesi RetryBudgetSeconds (oyun is parcacigi uzun bloklanmaz). Save/Load her cagrida
 *    yeni nesne kullanir. Kalici hatalar (tavandan buyuk dosya, yok) yeniden denenmez.
 * Tek parcacikli: yalniz oyun is parcacigi (docs/DESIGN_CHARACTER_SAVE_ATOMIC.md 12).
 */
class IFileHandle;
class FMMOFileManagerSlotOps : public IMMOSlotFileOps
{
public:
	static constexpr int32 RetryAttempts = 3;
	static constexpr double RetrySleepSeconds = 0.020;   // [PLACEHOLDER] denemeler arasi
	static constexpr double RetryBudgetSeconds = 0.120;  // [PLACEHOLDER] bir nesne icin toplam bekleme (OLCULEN sure; < 200 ms)

	virtual bool Exists(const FString& Path) override;
	virtual bool ReadAll(const FString& Path, TArray<uint8>& Out) override;
	virtual bool WriteNew(const FString& Path, const TArray<uint8>& Bytes) override;
	virtual bool Move(const FString& Dest, const FString& Src) override;
	virtual bool Delete(const FString& Path, bool bEvenReadOnly) override;
	virtual void ClearReadOnly(const FString& Path) override;

	/** Yeniden deneme sayaci (yalniz gozlem/test): yapilan EK deneme sayisi. */
	int32 RetriesPerformed = 0;
	/** Test enjeksiyonu: true donerse o deneme motora gitmeden BASARISIZ sayilir. Op: "read"|"move"|"delete"; Attempt 0 tabanli. */
	TFunction<bool(const TCHAR* Op, const FString& Path, int32 Attempt)> TestInjectFailure;
	/** Test enjeksiyonu: WriteNew'un dosya tutamagini disaridan saglar (Flush/Tell hatasi benzetimi). Bos = gercek IPlatformFile::OpenWrite. */
	TFunction<IFileHandle*(const FString& Path)> TestOpenWrite;

private:
	bool SleepForRetry();
	double SleptSeconds = 0.0;
};

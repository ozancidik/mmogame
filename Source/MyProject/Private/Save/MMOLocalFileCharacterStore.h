// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Save/MMOCharacterStore.h"

/**
 * Yerel dosya karakter magazasi. Dosyalar Saved/SaveGames/<Yuva>.sav (UserIndex 0); format ve yuva adlari
 * UGameplayStatics::SaveGameToSlot ile ayni (eski, dogrudan yazilmis .sav dosyalari okunur).
 *
 * S6.1 (docs/DESIGN_CHARACTER_SAVE_ATOMIC.md): Save/Load/Exists FMMOSlotFileAtomic uzerinden atomiktir:
 * <Yuva>.sav.tmp'ye yaz + geri oku + (.bak) + Move; bozuk ana/.bak silinmez .bad'e ayrilir; Load YAN ETKISIZdir.
 * Yuva adi ^MMOSave_[A-Za-z0-9 _-]+$ degilse Save/Load/Exists fail-closed (yol gecisi savunmasi).
 * S6.2: Save yalniz oyun is parcacigindan (baska parcacikta reddedilir: not_game_thread), uretilen baytlar kanonik degilse reddedilir (noncanonical),
 *       her basarisizlik [MMOMETRIC] ev=save_failed; Load ana dosya okunamiyorsa (I/O) .bak'a DUSMEZ ([MMOMETRIC] ev=load_failed reason=io);
 *       Exists = ana VEYA .bak VEYA gecerli .tmp; surec basina bir kez acilis oz-sinamasi (CheckFormatRoundTrip, ev=save_selftest_failed).
 *       COK SURECLI sunucu (ayni Saved/SaveGames'i paylasan iki surec) DESTEKLENMEZ: .tmp adi ortaktir (docs/DESIGN_CHARACTER_SAVE_ATOMIC.md 12).
 *
 * DOGRULAYICI (R1: LoadGameFromMemory kesik dosyada hata vermez, eksik alanlari varsayilan birakir):
 *  - ana/.bak (eski dosyalar da var, bicim degismedi): LoadGameFromMemory basarili VE (bValid VEYA yeniden serilestirme bayt-bayt esit).
 *  - .tmp (her zaman bu kod yazar): LoadGameFromMemory basarili VE yeniden serilestirme bayt-bayt esit ("kanonik"). Kesik .tmp asla gecerli sayilmaz.
 */
class FMMOFileManagerSlotOps;

class FMMOLocalFileCharacterStore : public IMMOCharacterStore
{
public:
	/** Bos DirectoryOverride = Saved/SaveGames (uretim). Test yalniz gecici dizin verir. */
	explicit FMMOLocalFileCharacterStore(const FString& InDirectoryOverride = FString());

	virtual bool Load(const FString& SlotName, FMMOCharacterSaveData& Out) override;
	virtual bool Save(const FString& SlotName, const FMMOCharacterSaveData& In) override;
	virtual bool Exists(const FString& SlotName) override;
	virtual void ListSlotNames(TArray<FString>& Out) override;

	/** UMMOSaveGame -> baytlar (SaveGameToMemory). Test icin acik. */
	static bool SerializeBytes(const FMMOCharacterSaveData& In, TArray<uint8>& OutBytes);
	/**
	 * LoadGameFromMemory'ye girmeden ONCE baslik denetimi. Motor, GVAS etiketi olmayan/bozuk bir dosyada coker (sinif adi FString'i
	 * milyonlarca karakter okunur -> FName assert); bu denetim bozuk dosyayi cokmeden "gecersiz" yapar. Gecerli: GVAS, surum 2-3,
	 * Optimized custom-version bicimi ve sinif adi == UMMOSaveGame yolu. Eski (dogrudan yazilmis) dosyalar ayni basligi tasir.
	 */
	static bool HasSafeHeader(const TArray<uint8>& Bytes);
	/**
	 * HasSafeHeader + ust duzey property etiketi yuruyusu: her etiketin Size'i dosyaya sigar ve akis "None" ile biter. KESIK dosya
	 * burada elenir; motorun kesik veride 'Tag.Size == Loaded' check'i (Development'ta cokme) hic calismaz. Dosyanin ic baytlarindaki
	 * bit-bozulmasi (boyutlar tutarli, icerik bozuk) burada yakalanmaz: kalan risk, S6.1 belgesinde yazili.
	 */
	static bool HasSafeStructure(const TArray<uint8>& Bytes);
	/** Baytlar -> veri (HasSafeStructure + LoadGameFromMemory + UMMOSaveGame). Yalniz ayristirir; bValid'e bakmaz. */
	static bool ParseBytes(const TArray<uint8>& Bytes, FMMOCharacterSaveData& Out);
	/** Ayristir + yeniden serilestir + bayt esitligi. Tam yazilmis (kesik olmayan) dosya kanonik; kesik olan degil. */
	static bool IsCanonical(const TArray<uint8>& Bytes);
	/** Ana/.bak dogrulayicisi (gevsek, eski dosyalar icin): ParseBytes && (bValid || IsCanonical). */
	static bool IsValidMainBytes(const TArray<uint8>& Bytes);
	/** YALNIZ TEST: Load/Save'in olusturdugu gercek dosya islemleri nesnesini yapilandirir (hata enjeksiyonu: TestInjectFailure). Bos = uretim. */
	static TFunction<void(FMMOFileManagerSlotOps&)> TestOpsConfigurator;
	/** Save'in yazmadan once uyguladigi geri-okuma denetimi: Bytes ayristirilir ve bValid In ile ayni olmali (S6.2: test icin acik). */
	static bool VerifyRoundTrip(const FMMOCharacterSaveData& In, const TArray<uint8>& Bytes);

	/**
	 * QA-3 acilis oz-sinamasi: ornek bir kaydi serilestir -> yapi yuruyusu -> ayristir -> kanonik mi. Sonuc UI'a/davranisa BAGLANMAZ;
	 * basarisizsa Error log + [MMOMETRIC] ev=save_selftest_failed (motor surumu yukseltmesinde bicim/yapi yuruyusu uyumsuzlugu uyarisi).
	 * Surec basina bir kez, magazanin ilk Load/Save/Exists/ListSlotNames cagrisinda calisir.
	 */
	static bool CheckFormatRoundTrip(FString& OutWhy);
	/** Sonucu loglar (Error + metrik). bOk ise sessiz. Test icin acik. */
	static void ReportFormatSelfTest(bool bOk, const FString& Why);
	/** Oz-sinamanin bu surecte kac kez kosuldugu (0 ya da 1). Yalniz gozlem/test. */
	static int32 GetFormatSelfTestRunCount();

private:
	static void RunFormatSelfTestOnce();
	static bool WalkStructure(const TArray<uint8>& Bytes, bool bWalkBody);
	FString SlotMainPath(const FString& SlotName) const;

	FString Directory;
};

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
 *
 * DOGRULAYICI (R1: LoadGameFromMemory kesik dosyada hata vermez, eksik alanlari varsayilan birakir):
 *  - ana/.bak (eski dosyalar da var, bicim degismedi): LoadGameFromMemory basarili VE (bValid VEYA yeniden serilestirme bayt-bayt esit).
 *  - .tmp (her zaman bu kod yazar): LoadGameFromMemory basarili VE yeniden serilestirme bayt-bayt esit ("kanonik"). Kesik .tmp asla gecerli sayilmaz.
 */
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

private:
	static bool WalkStructure(const TArray<uint8>& Bytes, bool bWalkBody);
	FString SlotMainPath(const FString& SlotName) const;

	FString Directory;
};

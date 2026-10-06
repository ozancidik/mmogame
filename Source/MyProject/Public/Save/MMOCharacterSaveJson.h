// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Save/MMOSaveGame.h"

/**
 * Bulut karakter magazalari icin surumlu JSON zarfi.
 *
 * Yerel .sav formati degismez. Bu donusturucu PlayFab/Postgres gibi uzak magazalara
 * gonderilecek blob'u uretir ve bilinmeyen ileri sema surumlerini guvenli bicimde reddeder.
 *
 * Sema surumleri:
 *  - v1: int64 alanlar JSON SAYISI (double) olarak yazilirdi (2^53 ustu kesinlik kaybi). Hala OKUNUR
 *    (yalniz 2^53 alti tam sayi kabul edilir), artik YAZILMAZ.
 *  - v2 (guncel): int64 alanlar ondalik-siz tam sayi METNI (string) olarak yazilir/okunur.
 *
 * ToJson ve FromJson SIKI calisir: her SaveGame alani blob'da bulunmali, tip/aralik/boyut dogrulanir, bilinmeyen EK
 * alanlar yok sayilir. Basarisizlikta OutData'ya DOKUNULMAZ; cagiran false donunce mevcut kaydi
 * ezmemelidir (yazma). Enum'lar AD ile yazilir; eski/elle yazilmis sayisal degerler gecerli bir
 * enumeratore denk geliyorsa tolere edilir. Enum degeri yeniden adlandirilir/silinirse eski blob'lar
 * reddedilir: yeniden adlandirma yapilacaksa once surum artirilip bu sinifa gecis (migration) eklenir.
 */
class MYPROJECT_API FMMOCharacterSaveJson
{
public:
	static constexpr int32 CurrentSchemaVersion = 2;

	static bool ToJson(const FMMOCharacterSaveData& Data, FString& OutJson);
	static bool FromJson(const FString& Json, FMMOCharacterSaveData& OutData,
		int32* OutSchemaVersion = nullptr, FString* OutError = nullptr);

	/** Iki kayit arasinda farkli SaveGame alanlarinin adlari (bos = alan alan esit). */
	static void FindDifferences(const FMMOCharacterSaveData& A, const FMMOCharacterSaveData& B,
		TArray<FString>& OutFields);

	/**
	 * Tum kayit alanlarini dolduran bir fixture ile JSON gidis-donus testi + negatif (bozuk/eksik/asiri
	 * buyuk blob reddi, OutData'nin ezilmemesi) ve tolerans (v1, eski enum, ek alan) testleri.
	 */
	static bool RunRoundTripSelfTest(FString& OutError, int32& OutUtf8Bytes, int32* OutNegativeCases = nullptr);
};

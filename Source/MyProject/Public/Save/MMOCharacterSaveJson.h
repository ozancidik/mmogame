// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Save/MMOSaveGame.h"

/**
 * Bulut karakter magazalari icin surumlu JSON zarfı.
 *
 * Yerel .sav formati degismez. Bu donusturucu PlayFab/Postgres gibi uzak magazalara
 * gonderilecek blob'u uretir ve bilinmeyen ileri sema surumlerini guvenli bicimde reddeder.
 */
class MYPROJECT_API FMMOCharacterSaveJson
{
public:
	static constexpr int32 CurrentSchemaVersion = 1;

	static bool ToJson(const FMMOCharacterSaveData& Data, FString& OutJson);
	static bool FromJson(const FString& Json, FMMOCharacterSaveData& OutData,
		int32* OutSchemaVersion = nullptr, FString* OutError = nullptr);

	/** Tum kayit alanlarini dolduran bir fixture ile JSON gidis-donus testi. */
	static bool RunRoundTripSelfTest(FString& OutError, int32& OutUtf8Bytes);
};

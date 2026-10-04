// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"

// PIE olcum gunlugu yardimcilari (docs/VERTICAL_SLICE_LV1_30.md madde 1). Sunucuda tek satir
// "[MMOMETRIC] t=.. ev=.. anahtar=deger" basilir; Scripts/PlaytestMetrics.py bu satirlari okur.
// Yalniz UE_LOG kullanilir (Shipping'de derlenmez); oyun davranisini degistirmez.
namespace MMOMetrics
{
	// Oyuncu adi (PlayerState), yoksa aktor adi. Bosluklar '_' olur ki satir anahtar=deger olarak ayrisabilsin.
	inline FString WhoName(const AActor* Actor)
	{
		FString Name;
		if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			if (const APlayerState* PS = Pawn->GetPlayerState())
			{
				Name = PS->GetPlayerName();
			}
		}
		if (Name.IsEmpty() && Actor)
		{
			Name = Actor->GetName();
		}
		if (Name.IsEmpty())
		{
			Name = TEXT("?");
		}
		return Name.Replace(TEXT(" "), TEXT("_"));
	}

	// Surec baslangicindan beri saniye (bolge degisiminde dunya saati sifirlansa da kesintisiz).
	inline double Seconds()
	{
		return FPlatformTime::Seconds() - GStartTime;
	}
}

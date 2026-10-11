// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "World/MMONpcBase.h"
#include "MMOHealerNPC.generated.h"

/**
 * Sifaci NPC (Sifaci Lyra): tiklaninca oyuncuyu tam iyilestirir (can + mana),
 * karsiliginda HealCost kadar altin alir (altin dongusu / gold sink). Govde acik mor.
 * Iyilestirme + altin dususu SUNUCUDA yapilir (istemci -> Server RPC).
 */
UCLASS()
class MYPROJECT_API AMMOHealerNPC : public AMMONpcBase
{
	GENERATED_BODY()

public:
	AMMOHealerNPC();

	// Sifacinin adi (diyalog balonunda gorunur). Sabit/baked-in; her makinede ayni.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Healer")
	FString HealerName = TEXT("Sifaci Lyra");

	// Tam iyilestirmenin TABAN altin bedeli (denge kullanicinin karari; Blueprint/config'ten ayarlanabilir).
	// Gercek bedel seviyeyle buyur: bkz. ComputeHealCost.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMO|Healer")
	int32 HealCost = 100;

	// Seviyeye gore sifa bedeli: taban + 0,4 x (Level-1)^2 (Lv1 100, Lv30 436, Lv60 1492, Lv80 2596).
	// PLACEHOLDER, PIE'de olculmedi: sabit 100 altin Lv58+ bantlarinda (EconomyModel gelir/gider ~30x) fiilen bedavaydi.
	static int32 ComputeHealCost(int32 BaseCost, int32 Level)
	{
		const int64 L = FMath::Max(0, Level - 1);
		return FMath::Max(0, BaseCost) + static_cast<int32>((L * L * 2) / 5);
	}

	// Altin yetersizken gosterilecek kibar reddetme repligi (balon).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMO|Healer")
	FString InsufficientGoldLine = TEXT("Kesende yeterli altin yok, evladim; sifa da bir bedel ister. Once altinini denkle, sonra gel.");

	// Zaten tam saglikliyken (iyilestirecek sey yokken) gosterilecek replik (balon).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMO|Healer")
	FString AlreadyHealedLine = TEXT("Yaran yok, evladim; altinina kiyma. Huzurla yoluna devam et.");
};

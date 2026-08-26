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

	// Tam iyilestirmenin altin bedeli (denge kullanicinin karari; Blueprint/config'ten ayarlanabilir).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMO|Healer")
	int32 HealCost = 100;

	// Altin yetersizken gosterilecek kibar reddetme repligi (balon).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMO|Healer")
	FString InsufficientGoldLine = TEXT("Kesende yeterli altin yok, evladim; sifa da bir bedel ister. Once altinini denkle, sonra gel.");

	// Zaten tam saglikliyken (iyilestirecek sey yokken) gosterilecek replik (balon).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMO|Healer")
	FString AlreadyHealedLine = TEXT("Yaran yok, evladim; altinina kiyma. Huzurla yoluna devam et.");
};

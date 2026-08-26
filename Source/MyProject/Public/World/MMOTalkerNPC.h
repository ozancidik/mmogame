// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "World/MMONpcBase.h"
#include "MMOTalkerNPC.generated.h"

/**
 * Konusan NPC: tiklaninca LLM'den (Anthropic Messages API) karaktere ozgu bir replik
 * uretilir ve oyuncunun HUD'unda diyalog balonu olarak gosterilir.
 * Anahtar/kredi yoksa FallbackLines'tan rastgele bir yedek replik kullanilir.
 * Govde moru.
 */
UCLASS()
class MYPROJECT_API AMMOTalkerNPC : public AMMONpcBase
{
	GENERATED_BODY()

public:
	AMMOTalkerNPC();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// NPC'nin adi (diyalog balonunda + istemci nameplate'inde gorunur). Sunucuda spawn'da
	// ayarlanip istemcilere replike olur (yoksa istemcide varsayilan gorunurdu).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "MMO|Talker")
	FString NpcName = TEXT("Kasabali");

	// NPC'nin rolu/kisiligi (LLM'e sistem promptu olarak verilir).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMO|Talker")
	FString NpcRole = TEXT("kasabanin siradan bir sakini");

	// LLM devre disiyken (anahtar/kredi yok) kullanilacak yedek replikler.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MMO|Talker")
	TArray<FString> FallbackLines;

	// FallbackLines'tan rastgele bir replik dondurur (bos ise genel bir metin).
	FString PickFallbackLine() const;
};

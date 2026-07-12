// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "World/MMONpcBase.h"
#include "MMOQuestGiverNPC.generated.h"

/** Gorev veren NPC: tiklaninca gorev penceresi acilir. Govde altin ton. */
UCLASS()
class MYPROJECT_API AMMOQuestGiverNPC : public AMMONpcBase
{
	GENERATED_BODY()

public:
	AMMOQuestGiverNPC();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Quest")
	FString NpcName = TEXT("Gorevli");
};

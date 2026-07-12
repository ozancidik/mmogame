// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "World/MMONpcBase.h"
#include "MMOVendorNPC.generated.h"

/** Satici NPC: tiklaninca dukkan acilir (iksir/ekipman al-sat). Govde mavi. */
UCLASS()
class MYPROJECT_API AMMOVendorNPC : public AMMONpcBase
{
	GENERATED_BODY()

public:
	AMMOVendorNPC();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Vendor")
	FString VendorName = TEXT("Satici");
};

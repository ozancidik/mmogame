// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "World/MMONpcBase.h"
#include "MMOWarehouseNPC.generated.h"

/**
 * Depocu NPC (KO "Inn" deposu): tiklaninca depo penceresi acilir; oyuncu cantasindaki
 * ekipmani depoya koyar / depodan cantaya alir. Tasima SUNUCUDA, depocuya yakinlik
 * (UMMOInventoryComponent::IsOwnerNearWarehouse) dogrulanarak yapilir. Govde toprak rengi.
 */
UCLASS()
class MYPROJECT_API AMMOWarehouseNPC : public AMMONpcBase
{
	GENERATED_BODY()

public:
	AMMOWarehouseNPC();

	// Depocunun adi (isim plakasi ve karsilama balonu).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Warehouse")
	FString WarehouseName = TEXT("Depocu Rana");
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "MMODotDamageType.generated.h"

/**
 * Oyuncunun mob uzerindeki periyodik hasari (DoT) isaretler. AMMOCharacter::TakeDamage bu turu gorunce savunmayi atlar
 * (UMMOStatsComponent::ApplyDamageIgnoringDefense); aksi halde Zehir tick'i (8) savunmaya karsi fiilen 1 hasar verirdi.
 * Bkz. docs/DOT_SCALING_DESIGN.md secenek A. Yalniz sunucuda kullanilir; mob -> oyuncu DoT'u degismedi.
 */
UCLASS()
class MYPROJECT_API UMMODotDamageType : public UDamageType
{
	GENERATED_BODY()
};

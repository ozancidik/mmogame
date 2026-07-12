// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MMONpcBase.generated.h"

/**
 * Etkilesime girilebilen, hareketsiz NPC'ler icin temel sinif (satici, gorevli...).
 * Manny govde + idle animasyon + fare-tiklanabilir collision kurar; alt siniflar yalnizca NpcTint verir.
 */
UCLASS(Abstract)
class MYPROJECT_API AMMONpcBase : public ACharacter
{
	GENERATED_BODY()

public:
	AMMONpcBase();

protected:
	virtual void BeginPlay() override;

	// Govde rengi (alt sinif constructor'inda ayarlanir).
	UPROPERTY(EditDefaultsOnly, Category = "MMO|NPC")
	FLinearColor NpcTint = FLinearColor(0.5f, 0.5f, 0.5f);
};

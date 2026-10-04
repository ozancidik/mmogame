// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;

/**
 * Sunucu mantiginin oyuncuya kisa durum mesaji gostermesi icin tek yol (NETWORK_AUDIT O11 madde 3).
 * Eskiden mesajlar "GEngine && IsLocallyControlled()" ile ekrana basiliyordu; sunucuda uzak
 * oyuncunun pawn'i yerel olmadigindan "Canta dolu!", "Altin yetersiz" gibi mesajlar uzak
 * istemcilere hic ulasmiyordu (listen-server'da bile).
 */
namespace MMONotify
{
	/**
	 * OwnerActor bir APawn ise: yerel kontrollu pawn'da ekrana basar; uzak istemcinin pawn'inda (sunucuda)
	 * sahibine owner-only chat RPC'siyle (sistem kanali) iletir. Pawn degilse hicbir sey yapmaz.
	 */
	MYPROJECT_API void ToOwner(const AActor* OwnerActor, const FString& Text, const FColor& Color, float Duration);
}

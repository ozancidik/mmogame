// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AMMOMob;

/** Ortak hedefleme yardimcilari. */
namespace MMOTargeting
{
	/** Verilen konuma MaxRange icindeki en yakin CANLI mob'u dondurur (yoksa nullptr). */
	MYPROJECT_API AMMOMob* FindNearestMob(const UObject* WorldContextObject, const FVector& From, float MaxRange);
	/** Oyuncu dogus noktasi cevresindeki savassiz hub alani. */
	MYPROJECT_API bool IsInsideHubSafeZone(const UObject* WorldContextObject, const FVector& Location, float Radius = 700.f);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/MMOTargeting.h"
#include "Characters/MMOMob.h"
#include "Components/MMOStatsComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

bool MMOTargeting::IsInsideHubSafeZone(const UObject* WorldContextObject, const FVector& Location, float Radius)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || Radius <= 0.f)
	{
		return false;
	}
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (FVector::Dist2D(Location, It->GetActorLocation()) < Radius)
		{
			return true;
		}
	}
	return false;
}

AMMOMob* MMOTargeting::FindNearestMob(const UObject* WorldContextObject, const FVector& From, float MaxRange)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}

	AMMOMob* Best = nullptr;
	float BestDist = MaxRange;
	for (TActorIterator<AMMOMob> It(World); It; ++It)
	{
		AMMOMob* Mob = *It;
		if (!Mob || !Mob->GetStats() || Mob->GetStats()->bIsDead)
		{
			continue;
		}
		const float D = FVector::Dist(From, Mob->GetActorLocation());
		if (D < BestDist)
		{
			BestDist = D;
			Best = Mob;
		}
	}
	return Best;
}

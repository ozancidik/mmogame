// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/MMOTargeting.h"
#include "Characters/MMOMob.h"
#include "Components/MMOStatsComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

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

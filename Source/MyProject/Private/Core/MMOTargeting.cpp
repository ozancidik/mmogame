// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/MMOTargeting.h"
#include "Characters/MMOMob.h"
#include "Components/MMOStatsComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

namespace
{
	struct FHubSafeZoneWorldCache
	{
		TWeakObjectPtr<UWorld> World;
		TArray<FVector> Centers;
	};

	// PlayerStart'lar bu projede harita yuklenirken olusan sabit aktorlerdir. Her vurus ve
	// beceri kontrolunde TActorIterator calistirmak yerine merkezleri dunya basina bir kez
	// topla. Zayif dunya isaretcisi seamless/non-seamless travel sonrasi eski kaydi guvenle
	// ayirir; gecersiz kayitlar sonraki cache miss'inde temizlenir.
	TArray<FHubSafeZoneWorldCache> GHubSafeZoneCaches;

	const TArray<FVector>& GetHubSafeZoneCenters(UWorld* World)
	{
		for (int32 Index = GHubSafeZoneCaches.Num() - 1; Index >= 0; --Index)
		{
			FHubSafeZoneWorldCache& Cache = GHubSafeZoneCaches[Index];
			if (!Cache.World.IsValid())
			{
				GHubSafeZoneCaches.RemoveAtSwap(Index);
				continue;
			}
			if (Cache.World.Get() == World)
			{
				return Cache.Centers;
			}
		}

		FHubSafeZoneWorldCache& NewCache = GHubSafeZoneCaches.AddDefaulted_GetRef();
		NewCache.World = World;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			NewCache.Centers.Add(It->GetActorLocation());
		}
		return NewCache.Centers;
	}
}

bool MMOTargeting::IsInsideHubSafeZone(const UObject* WorldContextObject, const FVector& Location, float Radius)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || Radius <= 0.f)
	{
		return false;
	}
	const float RadiusSquared = Radius * Radius;
	for (const FVector& Center : GetHubSafeZoneCenters(World))
	{
		if (FVector::DistSquared2D(Location, Center) < RadiusSquared)
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

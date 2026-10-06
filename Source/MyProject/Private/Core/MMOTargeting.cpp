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
		// Bos sonuc onbellege ALINMAZ (PlayerStart henuz yuklenmemis olabilir): bos kaldikca en erken bu
		// zamandan (FPlatformTime saniye) sonra yeniden taranir; bos tarama her cagrida aktor taramasina donmesin.
		double NextEmptyRefreshTime = 0.0;
	};

	// PlayerStart'lar bu projede harita yuklenirken olusan sabit aktorlerdir. Her vurus ve
	// beceri kontrolunde TActorIterator calistirmak yerine merkezleri dunya basina bir kez
	// topla. Zayif dunya isaretcisi seamless/non-seamless travel sonrasi eski kaydi guvenle
	// ayirir; gecersiz kayitlar sonraki cache miss'inde temizlenir.
	TArray<FHubSafeZoneWorldCache> GHubSafeZoneCaches;

	const TArray<FVector>& GetHubSafeZoneCenters(UWorld* World)
	{
		FHubSafeZoneWorldCache* Found = nullptr;
		for (int32 Index = GHubSafeZoneCaches.Num() - 1; Index >= 0; --Index)
		{
			if (!GHubSafeZoneCaches[Index].World.IsValid())
			{
				GHubSafeZoneCaches.RemoveAtSwap(Index);
			}
		}
		for (FHubSafeZoneWorldCache& Cache : GHubSafeZoneCaches)
		{
			if (Cache.World.Get() == World)
			{
				Found = &Cache;
				break;
			}
		}
		if (!Found)
		{
			Found = &GHubSafeZoneCaches.AddDefaulted_GetRef();
			Found->World = World;
		}

		// Dolu liste dunya omru boyunca kalir; bos liste saklanmaz, kisa araliklarla yeniden denenir.
		const double Now = FPlatformTime::Seconds();
		if (Found->Centers.IsEmpty() && Now >= Found->NextEmptyRefreshTime)
		{
			for (TActorIterator<APlayerStart> It(World); It; ++It)
			{
				Found->Centers.Add(It->GetActorLocation());
			}
			if (Found->Centers.IsEmpty())
			{
				Found->NextEmptyRefreshTime = Now + 1.0;
			}
		}
		return Found->Centers;
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

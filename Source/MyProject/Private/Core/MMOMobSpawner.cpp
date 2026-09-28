// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/MMOMobSpawner.h"
#include "Characters/MMOMob.h"
#include "Characters/MMOMobArchetype.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

AMMOMobSpawner::AMMOMobSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// Kok bileseni olmadan actor konumlandirilamaz (SpawnActor konumu uygulanmaz).
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AMMOMobSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (!MobClass)
	{
		MobClass = AMMOMob::StaticClass();
	}

	UE_LOG(LogTemp, Log, TEXT("[MMODBG] Spawner BeginPlay @ %s, MobClass=%s"),
		*GetActorLocation().ToCompactString(), *GetNameSafe(MobClass));

	TopUp();
	GetWorldTimerManager().SetTimer(TopUpTimer, this, &AMMOMobSpawner::TopUp, TopUpInterval, true);

	UE_LOG(LogTemp, Log, TEXT("[MMODBG] Spawner after TopUp: live=%d"), LiveMobs.Num());
}

void AMMOMobSpawner::TopUp()
{
	// Gecersiz (olmus/yok olmus) kayitlari temizle.
	LiveMobs.RemoveAll([](const TWeakObjectPtr<AMMOMob>& M)
	{
		return !M.IsValid();
	});

	const int32 Needed = MaxMobs - LiveMobs.Num();
	for (int32 i = 0; i < Needed; ++i)
	{
		SpawnOne();
	}
}

void AMMOMobSpawner::SpawnOne()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !MobClass)
	{
		return;
	}

	const FVector Base = GetActorLocation();
	const float AngleDeg = FMath::FRandRange(0.f, 360.f);
	const float Radius = FMath::FRandRange(150.f, SpawnRadius);
	const float AngleRad = FMath::DegreesToRadians(AngleDeg);
	const FVector Offset(FMath::Cos(AngleRad) * Radius, FMath::Sin(AngleRad) * Radius, 100.f);

	FVector SpawnLoc = Base + Offset;
	if (bValidateSpawnOnNavGround)
	{
		const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		const AMMOMob* MobDefaults = MobClass->GetDefaultObject<AMMOMob>();
		const UCapsuleComponent* Capsule = MobDefaults ? MobDefaults->GetCapsuleComponent() : nullptr;
		if (!Nav || !Nav->GetMainNavData() || !Capsule)
		{
			if (!bSpawnLocationWarningReported)
			{
				UE_LOG(LogTemp, Warning, TEXT("[MMODBG] Spawner %s: nav data or mob capsule unavailable; skipping spawn"), *GetName());
				bSpawnLocationWarningReported = true;
			}
			return;
		}

		constexpr int32 MaxAttempts = 8;
		bool bFoundSafeLocation = false;
		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(MMOMobSpawnGround), false, this);
		for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
		{
			if (Attempt > 0)
			{
				const float RetryAngle = FMath::FRandRange(0.f, 2.f * PI);
				const float RetryRadius = FMath::FRandRange(150.f, SpawnRadius);
				SpawnLoc = Base + FVector(FMath::Cos(RetryAngle) * RetryRadius,
					FMath::Sin(RetryAngle) * RetryRadius, 100.f);
			}

			FHitResult GroundHit;
			const FVector TraceStart(SpawnLoc.X, SpawnLoc.Y, Base.Z + 200.f);
			const FVector TraceEnd(SpawnLoc.X, SpawnLoc.Y, Base.Z - 200.f);
			if (!World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, TraceParams)
				|| GroundHit.ImpactNormal.Z < 0.7f
				|| FMath::Abs(GroundHit.ImpactPoint.Z - (Base.Z - 50.f)) > 60.f)
			{
				continue;
			}

			FNavLocation NavLocation;
			const FVector GroundPoint = GroundHit.ImpactPoint;
			if (!Nav->ProjectPointToNavigation(GroundPoint + FVector(0.f, 0.f, 20.f),
				NavLocation, FVector(50.f, 50.f, 60.f))
				|| FVector::Dist2D(NavLocation.Location, GroundPoint) > 50.f
				|| FMath::Abs(NavLocation.Location.Z - GroundPoint.Z) > 35.f)
			{
				continue;
			}

			SpawnLoc = NavLocation.Location + FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + 2.f);
			bFoundSafeLocation = true;
			break;
		}

		if (!bFoundSafeLocation)
		{
			if (!bSpawnLocationWarningReported)
			{
				UE_LOG(LogTemp, Warning, TEXT("[MMODBG] Spawner %s @ %s: no nearby ground/nav location after %d attempts; skipping spawn"),
					*GetName(), *Base.ToCompactString(), MaxAttempts);
				bSpawnLocationWarningReported = true;
			}
			return;
		}
		bSpawnLocationWarningReported = false;
	}

	// Spawn edilecek arketipi belirle:
	//  - ForcedArchetypeId varsa onu kullan (world boss / elit).
	//  - Yoksa ve seviye-bandi ayarliysa banddan sec; gecersiz/bos band spawn etmez.
	//  - Hicbiri yoksa arketipi bos birak: mob kendi BeginPlay'inde rastgele secer (mevcut davranis).
	FName ChosenArchetype = ForcedArchetypeId;
	if (ChosenArchetype.IsNone() && (MinMobLevel != 0 || MaxMobLevel != 0))
	{
		const FMMOMobArchetype* BandMob = FMMOMobDatabase::GetRandomInLevelRange(MinMobLevel, MaxMobLevel);
		if (!BandMob)
		{
			if (!bInvalidBandReported)
			{
				UE_LOG(LogTemp, Error, TEXT("[MMODBG] Spawner %s: invalid or empty normal-mob level band [%d, %d]; spawning nothing"),
					*GetName(), MinMobLevel, MaxMobLevel);
				bInvalidBandReported = true;
			}
			return;
		}
		ChosenArchetype = BandMob->ArchetypeId;
	}

	AMMOMob* Mob = nullptr;
	if (!ChosenArchetype.IsNone())
	{
		// Sabit arketip: ArchetypeId'yi BeginPlay'den once atamak icin deferred spawn.
		const FTransform SpawnTM(FRotator::ZeroRotator, SpawnLoc);
		Mob = World->SpawnActorDeferred<AMMOMob>(MobClass, SpawnTM, nullptr, nullptr,
			bValidateSpawnOnNavGround ? ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding
				: ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (Mob)
		{
			Mob->ArchetypeId = ChosenArchetype;
			UGameplayStatics::FinishSpawningActor(Mob, SpawnTM);
		}
	}
	else
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = bValidateSpawnOnNavGround
			? ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding
			: ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		Mob = World->SpawnActor<AMMOMob>(MobClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);
	}

	UE_LOG(LogTemp, Log, TEXT("[MMODBG] SpawnOne: requested=%s result=%s actualLoc=%s"),
		*SpawnLoc.ToCompactString(),
		Mob ? *Mob->GetName() : TEXT("NULL"),
		Mob ? *Mob->GetActorLocation().ToCompactString() : TEXT("-"));
	if (Mob)
	{
		LiveMobs.Add(Mob);
	}
}

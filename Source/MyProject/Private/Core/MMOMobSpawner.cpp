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

	// NETWORK_AUDIT O8: spawner yalniz sunucu zamanlayicisi ve sunucuya ait LiveMobs
	// listesini tasir; istemcinin okuyacagi replike alan ya da gorsel yoktur. GameMode runtime
	// spawner'lari zaten yalniz sunucuda uretir. Haritaya elle yerlestirilen bir spawner da
	// istemcide yuklenmesin; aksi halde bosuna actor kanali acar (ve otorite gibi BeginPlay
	// calistirma riski yaratir). Spawn edilen moblar kendi replike aktorleridir.
	bReplicates = false;
	bNetLoadOnClient = false;

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
	// Cakisma/kotu nokta yuzunden dogmayan mob ayni turda baska bir noktada birkac kez daha denenir (onceden 4 sn beklerdi;
	// engebeli veya iri kapsullu mob cepleri ilk doldurmada 1-3/6 veriyordu). Tavan: Needed x (1 + MaxSpawnRetries).
	constexpr int32 MaxSpawnRetries = 3;
	constexpr int32 EmptyTopUpsBeforeBackoff = 5;
	const int32 Retries = ConsecutiveEmptyTopUps >= EmptyTopUpsBeforeBackoff ? 0 : MaxSpawnRetries;
	const int32 LiveBefore = LiveMobs.Num();
	for (int32 i = 0; i < Needed; ++i)
	{
		for (int32 Try = 0; Try <= Retries; ++Try)
		{
			if (SpawnOne())
			{
				break;
			}
		}
	}
	ConsecutiveEmptyTopUps = (Needed > 0 && LiveMobs.Num() == LiveBefore) ? ConsecutiveEmptyTopUps + 1 : 0;
}

bool AMMOMobSpawner::FindSafeSpawnLocation(UWorld* World, const AActor* Ignore, const FVector& Base, float InSpawnRadius,
	float CapsuleHalfHeight, int32 MaxAttempts, FVector& OutLocation, FSpawnProbeStats& Stats)
{
	const UNavigationSystemV1* Nav = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!Nav || !Nav->GetMainNavData())
	{
		return false;
	}

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(MMOMobSpawnGround), false, Ignore);
	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Radius = FMath::FRandRange(150.f, InSpawnRadius);
		const FVector SpawnLoc = Base + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 100.f);

		FHitResult GroundHit;
		const FVector TraceStart(SpawnLoc.X, SpawnLoc.Y, Base.Z + 200.f);
		const FVector TraceEnd(SpawnLoc.X, SpawnLoc.Y, Base.Z - 200.f);
		if (!World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, TraceParams))
		{
			++Stats.TraceMiss;
			continue;
		}
		if (GroundHit.ImpactNormal.Z < 0.7f
			|| FMath::Abs(GroundHit.ImpactPoint.Z - (Base.Z - 50.f)) > 60.f)
		{
			++Stats.SlopeOrHeight;
			Stats.LastGroundZ = GroundHit.ImpactPoint.Z;
			Stats.LastNormalZ = GroundHit.ImpactNormal.Z;
			continue;
		}

		FNavLocation NavLocation;
		const FVector GroundPoint = GroundHit.ImpactPoint;
		if (!Nav->ProjectPointToNavigation(GroundPoint + FVector(0.f, 0.f, 20.f),
			NavLocation, FVector(50.f, 50.f, 60.f))
			|| FVector::Dist2D(NavLocation.Location, GroundPoint) > 50.f
			|| FMath::Abs(NavLocation.Location.Z - GroundPoint.Z) > 35.f)
		{
			++Stats.Nav;
			continue;
		}

		OutLocation = NavLocation.Location + FVector(0.f, 0.f, CapsuleHalfHeight + 2.f);
		return true;
	}
	return false;
}

bool AMMOMobSpawner::SpawnOne()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !MobClass)
	{
		return true;
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
			return true; // nav/kapsul yok: tekrar denemek anlamsiz
		}

		constexpr int32 MaxAttempts = 8;
		FSpawnProbeStats Stats;
		FVector FoundLoc = FVector::ZeroVector;
		const bool bFoundSafeLocation = FindSafeSpawnLocation(World, this, Base, SpawnRadius,
			Capsule->GetScaledCapsuleHalfHeight(), MaxAttempts, FoundLoc, Stats);

		if (!bFoundSafeLocation)
		{
			if (!bSpawnLocationWarningReported)
			{
				UE_LOG(LogTemp, Warning, TEXT("[MMODBG] Spawner %s @ %s: no nearby ground/nav location after %d attempts; skipping spawn (trace iskaladi=%d, egim/yukseklik=%d [son zemin Z=%.1f, normal Z=%.2f], navmesh=%d)"),
					*GetName(), *Base.ToCompactString(), MaxAttempts, Stats.TraceMiss, Stats.SlopeOrHeight, Stats.LastGroundZ, Stats.LastNormalZ, Stats.Nav);
				bSpawnLocationWarningReported = true;
			}
			return false; // rastgele 8 deneme tutmadi: baska bir denemede tutabilir
		}
		SpawnLoc = FoundLoc;
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
			return true; // gecersiz band: tekrar denemek anlamsiz
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
	return Mob != nullptr; // false: cakisma (DontSpawnIfColliding) yuzunden dogmadi; TopUp baska noktada yeniden dener
}

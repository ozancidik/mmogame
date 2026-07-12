// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/MMOMob.h"
#include "Characters/MMOMobArchetype.h"
#include "Components/MMOStatsComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "World/MMOLootPickup.h"
#include "Items/MMOItemDatabase.h"
#include "Components/MMOQuestComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

AMMOMob::AMMOMob()
{
	PrimaryActorTick.bCanEverTick = true;

	// Yerlestirilince/spawn olunca varsayilan AI controller onu ele gecirsin (hareket icin).
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Hareket yonune donsun.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;
		Move->MaxWalkSpeed = 300.f;
		Move->RotationRate = FRotator(0.f, 360.f, 0.f);
	}
	bUseControllerRotationYaw = false;

	// Gorunur govde: Manny mannequin + yurume animasyonu (oyuncu Quinn kullaniyor, ayrisir).
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
		if (MannyMesh.Succeeded())
		{
			MeshComp->SetSkeletalMesh(MannyMesh.Object);
		}

		static ConstructorHelpers::FClassFinder<UAnimInstance> UnarmedAnim(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
		if (UnarmedAnim.Succeeded())
		{
			MeshComp->SetAnimInstanceClass(UnarmedAnim.Class);
		}

		// Standart mannequin hizalamasi (BP_ThirdPersonCharacter ile ayni).
		MeshComp->SetRelativeLocation(FVector(0.f, 0.f, -89.f));
		MeshComp->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

		// Fiziksel carpisma yok ama fare imleci trace'i (Visibility) icin tiklanabilir olsun.
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
		MeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	// Moblar oyuncudan biraz daha zayif olsun (stat bileseni temel sinifta olusturuldu).
	if (Stats)
	{
		Stats->Strength = 8;
		Stats->Vitality = 5;
	}
}

void AMMOMob::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMMOMob, ArchetypeId);
	DOREPLIFETIME(AMMOMob, MobName);
	DOREPLIFETIME(AMMOMob, MobLevel);
}

void AMMOMob::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		// Arketip sec (belirtilmemisse rastgele) ve uygula.
		const FMMOMobArchetype* Arch = FMMOMobDatabase::Find(ArchetypeId);
		if (!Arch)
		{
			Arch = &FMMOMobDatabase::GetRandom();
		}
		ApplyArchetype(*Arch);
	}
	else
	{
		// Istemci: replike gelen arketiple gorseli uygula (henuz gelmediyse OnRep_Archetype halleder).
		if (const FMMOMobArchetype* Arch = FMMOMobDatabase::Find(ArchetypeId))
		{
			ApplyArchetypeVisuals(*Arch);
		}
		else
		{
			ApplyTint(FLinearColor(0.7f, 0.08f, 0.08f));
		}
	}
}

void AMMOMob::OnRep_Archetype()
{
	if (const FMMOMobArchetype* Arch = FMMOMobDatabase::Find(ArchetypeId))
	{
		ApplyArchetypeVisuals(*Arch);
	}
}

void AMMOMob::ApplyArchetype(const FMMOMobArchetype& Arch)
{
	// Kimlik / gorunum bilgisi (replike olur).
	ArchetypeId = Arch.ArchetypeId;
	MobName = Arch.Name;
	MobLevel = Arch.Level;

	// Stat / davranis.
	AggroRange = Arch.AggroRange;
	MobAttackDamage = Arch.AttackDamage;
	XPReward = Arch.XPReward;
	MinGold = Arch.MinGold;
	MaxGold = Arch.MaxGold;

	// Yetenekler.
	bIsElite = Arch.bIsElite;
	OnHitEffectId = Arch.OnHitEffectId;
	OnHitEffectChance = Arch.OnHitEffectChance;
	EnrageEffectId = Arch.EnrageEffectId;
	EnrageHealthPct = Arch.EnrageHealthPct;
	bEnraged = false;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = Arch.MoveSpeed;
	}

	if (Stats)
	{
		Stats->Level = Arch.Level;
		// Seviye/ana statlari uygula ve turetilmisleri (MaxHealth vb.) tam canla hesapla.
		Stats->InitializePrimaryStats(Arch.Strength, 10, 6, Arch.Vitality);
	}

	ApplyArchetypeVisuals(Arch);
}

void AMMOMob::ApplyArchetypeVisuals(const FMMOMobArchetype& Arch)
{
	SetActorScale3D(FVector(Arch.Scale));
	ApplyTint(Arch.Tint);
}

void AMMOMob::ApplyTint(const FLinearColor& Color)
{
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		const int32 NumMats = MeshComp->GetNumMaterials();
		for (int32 i = 0; i < NumMats; ++i)
		{
			if (UMaterialInstanceDynamic* MID = MeshComp->CreateAndSetMaterialInstanceDynamic(i))
			{
				MID->SetVectorParameterValue(TEXT("Tint"), Color);
				MID->SetVectorParameterValue(TEXT("Color"), Color);
			}
		}
	}
}

void AMMOMob::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !Stats || Stats->bIsDead)
	{
		return;
	}

	// Enrage: can esigin altina dusunce mob bir defaya mahsus kendine buff (or. Ofke) uygular.
	if (!bEnraged && EnrageHealthPct > 0.f && !EnrageEffectId.IsNone()
		&& Stats->GetHealthPercent() <= EnrageHealthPct)
	{
		bEnraged = true;
		Stats->ApplyEffect(EnrageEffectId, this);
	}

	// En yakin CANLI oyuncuyu bul (MP: birden fazla oyuncu olabilir).
	APawn* Player = nullptr;
	float Dist = TNumericLimits<float>::Max();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		APawn* Candidate = PC ? PC->GetPawn() : nullptr;
		if (!Candidate)
		{
			continue;
		}
		if (const AMMOCharacter* PChar = Cast<AMMOCharacter>(Candidate))
		{
			if (PChar->GetStats() && PChar->GetStats()->bIsDead)
			{
				continue; // olu oyuncuya saldirma
			}
		}
		const float D = FVector::Dist(GetActorLocation(), Candidate->GetActorLocation());
		if (D < Dist)
		{
			Dist = D;
			Player = Candidate;
		}
	}

	if (!Player)
	{
		return;
	}

	if (Dist > AggroRange)
	{
		return;
	}

	AAIController* AI = Cast<AAIController>(GetController());
	UWorld* World = GetWorld();

	if (Dist > AttackRange)
	{
		// Navmesh varsa yol bulma (engelleri asar); yoksa duz-cizgi harekete dus (fallback).
		const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		const bool bHasNav = AI && NavSys && NavSys->GetMainNavData() != nullptr;

		if (bHasNav)
		{
			const float Now = World->GetTimeSeconds();
			// Sik sik yeniden yol cizmeyi engelle (hareketli hedefi periyodik takip et).
			if (Now - LastRepathTime > 0.35f)
			{
				LastRepathTime = Now;
				AI->MoveToActor(Player, AttackRange * 0.8f, /*bStopOnOverlap=*/true, /*bUsePathfinding=*/true);
			}
		}
		else
		{
			const FVector Dir = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			AddMovementInput(Dir, 1.f);
		}
	}
	else
	{
		// Menzilde: yol bulmayi durdur, oyuncuya don ve periyodik vur.
		if (AI)
		{
			AI->StopMovement();
		}

		const FVector Dir = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (!Dir.IsNearlyZero())
		{
			SetActorRotation(Dir.Rotation());
		}

		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - LastMobAttackTime >= MobAttackCooldown)
		{
			LastMobAttackTime = Now;
			UGameplayStatics::ApplyDamage(Player, MobAttackDamage, GetController(), this, nullptr);

			// Vurusta sansa bagli debuff uygula (or. zehir tukuren mob, yavaslatan orumcek).
			if (!OnHitEffectId.IsNone() && OnHitEffectChance > 0.f && FMath::FRand() < OnHitEffectChance)
			{
				if (AMMOCharacter* PChar = Cast<AMMOCharacter>(Player))
				{
					if (PChar->GetStats() && !PChar->GetStats()->bIsDead)
					{
						PChar->GetStats()->ApplyEffect(OnHitEffectId, this);
					}
				}
			}
		}
	}
}

void AMMOMob::HandleDeath()
{
	Super::HandleDeath(); // Blueprint OnCharacterDied olayini tetikler.

	if (bDeathHandled)
	{
		return;
	}
	bDeathHandled = true;

	// Olduren oyuncuya XP + gorev ilerlemesi ver.
	if (AMMOCharacter* Killer = Cast<AMMOCharacter>(LastDamageInstigator.Get()))
	{
		if (Killer->GetStats())
		{
			Killer->GetStats()->GainExperience(XPReward);
		}
		if (Killer->GetQuestComponent())
		{
			Killer->GetQuestComponent()->NotifyMobKilled(ArchetypeId);
		}
	}

	// Loot dusur (altin + sansa bagli item).
	if (UWorld* World = GetWorld())
	{
		TSubclassOf<AMMOLootPickup> LootClass = LootPickupClass;
		if (!LootClass)
		{
			LootClass = AMMOLootPickup::StaticClass();
		}
		const FVector Base = GetActorLocation();

		// Altin
		{
			const FVector Loc = Base + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), -40.f);
			const FTransform SpawnTM(FRotator::ZeroRotator, Loc);
			if (AMMOLootPickup* P = World->SpawnActorDeferred<AMMOLootPickup>(LootClass, SpawnTM))
			{
				P->SetupAsGold(FMath::RandRange(MinGold, MaxGold));
				UGameplayStatics::FinishSpawningActor(P, SpawnTM);
			}
		}

		// Sansa bagli item
		if (FMath::FRand() < ItemDropChance)
		{
			const FVector Loc = Base + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), -40.f);
			const FTransform SpawnTM(FRotator::ZeroRotator, Loc);
			if (AMMOLootPickup* P = World->SpawnActorDeferred<AMMOLootPickup>(LootClass, SpawnTM))
			{
				P->SetupAsItem(DropItemId, 1);
				UGameplayStatics::FinishSpawningActor(P, SpawnTM);
			}
		}

		// Sansa bagli EKIPMAN (item veritabanindan rastgele; kucuk ihtimalle +1 dusebilir).
		if (FMath::FRand() < EquipDropChance)
		{
			const TArray<FMMOItemDef>& AllItems = FMMOItemDatabase::GetAll();
			if (AllItems.Num() > 0)
			{
				const FMMOItemDef& Picked = AllItems[FMath::RandRange(0, AllItems.Num() - 1)];
				const int32 Plus = (FMath::FRand() < 0.15f) ? 1 : 0;

				const FVector Loc = Base + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), -40.f);
				const FTransform SpawnTM(FRotator::ZeroRotator, Loc);
				if (AMMOLootPickup* P = World->SpawnActorDeferred<AMMOLootPickup>(LootClass, SpawnTM))
				{
					P->SetupAsEquipment(Picked.ItemId, Plus);
					UGameplayStatics::FinishSpawningActor(P, SpawnTM);
				}
			}
		}
	}

	// Hareketi ve carpismayi durdur, govdeyi gizle.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetVisibility(false);
	}

	// Kisa gecikmeyle yok et; spawner top-up ile yerine yenisini uretir.
	FTimerHandle DestroyTimer;
	GetWorldTimerManager().SetTimer(DestroyTimer, this, &AMMOMob::DestroySelf, 2.0f, false);
}

void AMMOMob::DestroySelf()
{
	Destroy();
}

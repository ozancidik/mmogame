// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/MMOLootPickup.h"
#include "Characters/MMOCharacter.h"
#include "Components/MMOInventoryComponent.h"
#include "Items/MMOItemDatabase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Core/MMONotify.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"

AMMOLootPickup::AMMOLootPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// Overlap alani (kok bilesen).
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(90.f);
	Collision->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Collision->SetGenerateOverlapEvents(true);
	RootComponent = Collision;

	// Gorunur mesh.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.4f));
}

void AMMOLootPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMMOLootPickup, bIsGold);
	DOREPLIFETIME(AMMOLootPickup, bIsEquipment);
}

void AMMOLootPickup::OnRep_LootVisual()
{
	ApplyVisual();
}

void AMMOLootPickup::SetReserved(const TArray<AMMOCharacter*>& Owners, float Seconds)
{
	ReservedFor.Reset();
	for (AMMOCharacter* O : Owners)
	{
		if (O)
		{
			ReservedFor.Add(O);
		}
	}
	ReservedSeconds = ReservedFor.Num() > 0 ? FMath::Max(0.f, Seconds) : 0.f;
}

void AMMOLootPickup::SetupAsGold(int32 Amount)
{
	bIsGold = true;
	GoldAmount = FMath::Max(1, Amount);
	ItemId = NAME_None;
}

void AMMOLootPickup::SetupAsItem(FName InItemId, int32 Count)
{
	bIsGold = false;
	bIsEquipment = false;
	ItemId = InItemId;
	ItemCount = FMath::Max(1, Count);
}

void AMMOLootPickup::SetupAsEquipment(FName InItemId, int32 InPlus, EMMOItemRarity InRarity)
{
	bIsGold = false;
	bIsEquipment = true;
	EquipItemId = InItemId;
	EquipPlus = FMath::Max(0, InPlus);
	EquipRarity = InRarity;
}

void AMMOLootPickup::BeginPlay()
{
	Super::BeginPlay();

	ApplyVisual();

	if (HasAuthority())
	{
		Collision->OnComponentBeginOverlap.AddDynamic(this, &AMMOLootPickup::OnOverlap);
		SetLifeSpan(LifeSpanSeconds);
		// Rezervasyon: sure dolunca ortadaki baska oyuncular icin yeniden dene (BeginOverlap tekrar tetiklenmez).
		if (ReservedSeconds > 0.f && GetWorld())
		{
			ReservedUntilTime = GetWorld()->GetTimeSeconds() + ReservedSeconds;
			GetWorldTimerManager().SetTimer(ReservationTimer, this, &AMMOLootPickup::OnReservationExpired,
				ReservedSeconds + 0.1f, false);
		}
	}
}

void AMMOLootPickup::ApplyVisual()
{
	if (!Mesh)
	{
		return;
	}

	// Altin -> sari, ekipman -> yesil, item -> mavi.
	FLinearColor Color;
	if (bIsGold)
	{
		Color = FLinearColor(1.f, 0.82f, 0.f);
	}
	else if (bIsEquipment)
	{
		Color = FLinearColor(0.2f, 0.9f, 0.25f);
	}
	else
	{
		Color = FLinearColor(0.15f, 0.55f, 1.f);
	}
	Mesh->SetRelativeScale3D(bIsGold ? FVector(0.4f) : FVector(0.5f));

	if (UMaterialInterface* Base = Mesh->GetMaterial(0))
	{
		if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			Mesh->SetMaterial(0, MID);
		}
	}
}

void AMMOLootPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Basit "juice": donme + yukari-asagi salinim.
	BobTime += DeltaSeconds;
	if (Mesh)
	{
		Mesh->AddLocalRotation(FRotator(0.f, 120.f * DeltaSeconds, 0.f));
		const float BobZ = FMath::Sin(BobTime * 3.f) * 8.f;
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, 20.f + BobZ));
	}
}

void AMMOLootPickup::OnOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	TryCollect(Cast<AMMOCharacter>(OtherActor));
}

void AMMOLootPickup::OnReservationExpired()
{
	if (!HasAuthority() || bCollected || !Collision)
	{
		return;
	}
	// Rezervasyon bitti: pickup'in uzerinde duran oyuncular (BeginOverlap daha once reddedilmisti) simdi toplayabilir.
	TArray<AActor*> Overlapping;
	Collision->GetOverlappingActors(Overlapping, AMMOCharacter::StaticClass());
	for (AActor* A : Overlapping)
	{
		TryCollect(Cast<AMMOCharacter>(A));
		if (bCollected)
		{
			break;
		}
	}
}

void AMMOLootPickup::TryCollect(AMMOCharacter* Char)
{
	if (!HasAuthority())
	{
		return;
	}

	// Sadece oyuncu toplayabilir (moblar degil).
	if (!Char || !Char->IsPlayerControlled())
	{
		return;
	}

	// Ganimet rezervasyonu: sure dolana kadar yalniz hak sahipleri (XP payi olan oyuncular) toplayabilir.
	if (ReservedFor.Num() > 0 && GetWorld() && GetWorld()->GetTimeSeconds() < ReservedUntilTime)
	{
		bool bAllowed = false;
		for (const TWeakObjectPtr<AMMOCharacter>& W : ReservedFor)
		{
			if (W.Get() == Char)
			{
				bAllowed = true;
				break;
			}
		}
		if (!bAllowed)
		{
			return;
		}
	}

	UMMOInventoryComponent* Inv = Char->GetInventory();
	if (!Inv)
	{
		return;
	}

	// Tek-sefer guard: Destroy() ayni frame'de senkron degil; grant'tan ONCE isaretle ve
	// bir daha overlap event'i uretmesin diye collision'i kapat (dupe onlemi).
	if (bCollected)
	{
		return;
	}
	// Canta doluysa AddEquipmentInstance esyayi eklemez. Pickup'i yerde birak ki
	// oyuncu yer actiktan sonra tekrar toplayabilsin.
	if (bIsEquipment && Inv->EquipmentBag.Num() >= UMMOInventoryComponent::MaxBagSize)
	{
		MMONotify::ToOwner(Char, TEXT("Canta dolu!"), FColor::Silver, 2.f);
		return;
	}
	bCollected = true;
	if (Collision)
	{
		Collision->SetGenerateOverlapEvents(false);
	}

	if (bIsGold)
	{
		Inv->AddGoldFrom(GoldAmount, TEXT("loot"));
		// Yalniz toplayanin yerel ekraninda (eskiden sunucuda HERKESIN ganimeti host ekranina basiliyordu).
		if (GEngine && Char->IsLocallyControlled())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor(255, 210, 0),
				FString::Printf(TEXT("+%d altin"), GoldAmount));
		}
	}
	else if (bIsEquipment)
	{
		Inv->AddEquipmentInstance(EquipItemId, EquipPlus, EquipRarity);
		const FLinearColor RC = FMMOItemDatabase::GetRarityColor(EquipRarity);
		MMONotify::ToOwner(Char,
			FString::Printf(TEXT("[%s] %s +%d dustu  (I ile envanteri ac)"),
				*FMMOItemDatabase::GetRarityName(EquipRarity), *EquipItemId.ToString(), EquipPlus),
			RC.ToFColor(true), 3.f);
	}
	else
	{
		Inv->AddItem(ItemId, ItemCount);
		if (GEngine && Char->IsLocallyControlled())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor(60, 140, 255),
				FString::Printf(TEXT("+%d %s"), ItemCount, *ItemId.ToString()));
		}
	}

	Destroy();
}

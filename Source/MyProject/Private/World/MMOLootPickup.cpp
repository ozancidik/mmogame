// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/MMOLootPickup.h"
#include "Characters/MMOCharacter.h"
#include "Components/MMOInventoryComponent.h"
#include "Items/MMOItemDatabase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

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
	if (!HasAuthority())
	{
		return;
	}

	// Sadece oyuncu toplayabilir (moblar degil).
	AMMOCharacter* Char = Cast<AMMOCharacter>(OtherActor);
	if (!Char || !Char->IsPlayerControlled())
	{
		return;
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
	bCollected = true;
	if (Collision)
	{
		Collision->SetGenerateOverlapEvents(false);
	}

	if (bIsGold)
	{
		Inv->AddGold(GoldAmount);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor(255, 210, 0),
				FString::Printf(TEXT("+%d altin"), GoldAmount));
		}
	}
	else if (bIsEquipment)
	{
		Inv->AddEquipmentInstance(EquipItemId, EquipPlus, EquipRarity);
		if (GEngine)
		{
			const FLinearColor RC = FMMOItemDatabase::GetRarityColor(EquipRarity);
			GEngine->AddOnScreenDebugMessage(-1, 3.f, RC.ToFColor(true),
				FString::Printf(TEXT("[%s] %s +%d dustu  (I ile envanteri ac)"),
					*FMMOItemDatabase::GetRarityName(EquipRarity), *EquipItemId.ToString(), EquipPlus));
		}
	}
	else
	{
		Inv->AddItem(ItemId, ItemCount);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor(60, 140, 255),
				FString::Printf(TEXT("+%d %s"), ItemCount, *ItemId.ToString()));
		}
	}

	Destroy();
}

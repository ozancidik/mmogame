// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/MMONpcBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AMMONpcBase::AMMONpcBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	AutoPossessAI = EAutoPossessAI::Disabled;

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

		MeshComp->SetRelativeLocation(FVector(0.f, 0.f, -89.f));
		MeshComp->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

		// Fiziksel carpisma yok ama fare imleci trace'i (Visibility) icin tiklanabilir.
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
		MeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
}

void AMMONpcBase::BeginPlay()
{
	Super::BeginPlay();

	// Govdeyi NpcTint rengine boya (materyalin "Tint"/"Color" parametreleri; olmayan zararsiz).
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		const int32 NumMats = MeshComp->GetNumMaterials();
		for (int32 i = 0; i < NumMats; ++i)
		{
			if (UMaterialInstanceDynamic* MID = MeshComp->CreateAndSetMaterialInstanceDynamic(i))
			{
				MID->SetVectorParameterValue(TEXT("Tint"), NpcTint);
				MID->SetVectorParameterValue(TEXT("Color"), NpcTint);
			}
		}
	}
}

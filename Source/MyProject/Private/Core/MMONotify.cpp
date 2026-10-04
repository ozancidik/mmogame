// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/MMONotify.h"
#include "Player/MMOPlayerController.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"

void MMONotify::ToOwner(const AActor* OwnerActor, const FString& Text, const FColor& Color, float Duration)
{
	const APawn* Pawn = Cast<APawn>(OwnerActor);
	if (!Pawn || Text.IsEmpty())
	{
		return;
	}
	if (Pawn->IsLocallyControlled())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, Duration, Color, Text);
		}
		return;
	}
	// Sunucuda uzak oyuncunun pawn'i: sahibine sistem satiri (kanal 2). Istemci makinesindeki
	// baska oyuncularin pawn'larinda kontrolcu yoktur, bu durumda hicbir sey yapilmaz.
	if (Pawn->HasAuthority())
	{
		if (AMMOPlayerController* PC = Cast<AMMOPlayerController>(Pawn->GetController()))
		{
			PC->ClientReceiveChat(FString(), Text, 2);
		}
	}
}

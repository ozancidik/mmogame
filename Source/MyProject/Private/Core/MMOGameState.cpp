// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/MMOGameState.h"
#include "Net/UnrealNetwork.h"

AMMOGameState::AMMOGameState()
{
}

void AMMOGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMMOGameState, bPvPEnabled);
}

void AMMOGameState::SetPvPEnabled(bool bEnabled)
{
	if (HasAuthority())
	{
		bPvPEnabled = bEnabled;
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/MMOTalkerNPC.h"
#include "Net/UnrealNetwork.h"

AMMOTalkerNPC::AMMOTalkerNPC()
{
	// Konusan NPC govdesi mor (satici mavi, gorevli altin sarisi ile ayrissin).
	NpcTint = FLinearColor(0.6f, 0.3f, 0.85f);
}

void AMMOTalkerNPC::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMMOTalkerNPC, NpcName);
}

FString AMMOTalkerNPC::PickFallbackLine() const
{
	if (FallbackLines.Num() == 0)
	{
		return TEXT("...");
	}
	const int32 Idx = FMath::RandRange(0, FallbackLines.Num() - 1);
	return FallbackLines[Idx];
}

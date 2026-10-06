// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOCharacterStoreSubsystem.h"
#include "Save/MMOLocalFileCharacterStore.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UMMOCharacterStoreSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Store = MakeUnique<FMMOLocalFileCharacterStore>();
}

void UMMOCharacterStoreSubsystem::Deinitialize()
{
	Store.Reset();
	Super::Deinitialize();
}

IMMOCharacterStore* UMMOCharacterStoreSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UMMOCharacterStoreSubsystem* Subsystem = GI ? GI->GetSubsystem<UMMOCharacterStoreSubsystem>() : nullptr;
	return Subsystem ? Subsystem->GetStore() : nullptr;
}

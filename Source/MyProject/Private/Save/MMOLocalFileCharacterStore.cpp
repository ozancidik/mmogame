// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOLocalFileCharacterStore.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

bool FMMOLocalFileCharacterStore::Load(const FString& SlotName, FMMOCharacterSaveData& Out)
{
	const UMMOSaveGame* SG = Cast<UMMOSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!SG)
	{
		return false;
	}
	Out = SG->Data;
	return true;
}

bool FMMOLocalFileCharacterStore::Save(const FString& SlotName, const FMMOCharacterSaveData& In)
{
	UMMOSaveGame* SG = Cast<UMMOSaveGame>(UGameplayStatics::CreateSaveGameObject(UMMOSaveGame::StaticClass()));
	if (!SG)
	{
		return false;
	}
	SG->Data = In;
	return UGameplayStatics::SaveGameToSlot(SG, SlotName, 0);
}

bool FMMOLocalFileCharacterStore::Exists(const FString& SlotName)
{
	return UGameplayStatics::DoesSaveGameExist(SlotName, 0);
}

void FMMOLocalFileCharacterStore::ListSlotNames(TArray<FString>& Out)
{
	// Yalniz karakter yuvalari (MMOSave_*): ayni klasorde baska .sav'lar olabilir.
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(FPaths::ProjectSavedDir() / TEXT("SaveGames") / TEXT("MMOSave_*.sav")), true, false);
	for (const FString& File : Files)
	{
		Out.Add(FPaths::GetBaseFilename(File));
	}
}

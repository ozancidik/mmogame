// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Save/MMOCharacterStore.h"

/**
 * Yerel dosya karakter magazasi: bugunku UGameplayStatics::SaveGameToSlot/LoadGameFromSlot +
 * UMMOSaveGame davranisinin birebir karsiligi. Dosyalar Saved/SaveGames/<Yuva>.sav (UserIndex 0);
 * format ve yuva adlari degismedi, eski .sav dosyalari okunur.
 */
class FMMOLocalFileCharacterStore : public IMMOCharacterStore
{
public:
	virtual bool Load(const FString& SlotName, FMMOCharacterSaveData& Out) override;
	virtual bool Save(const FString& SlotName, const FMMOCharacterSaveData& In) override;
	virtual bool Exists(const FString& SlotName) override;
	virtual void ListSlotNames(TArray<FString>& Out) override;
};

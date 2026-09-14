// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Skills/MMOSkillTypes.h"
#include "Items/MMOItemTypes.h"
#include "Quests/MMOQuestTypes.h"
#include "Components/MMOInventoryComponent.h"
#include "MMOSaveGame.generated.h"

/**
 * Diske kaydedilen tum karakter verisi. USaveGame yalniz SaveGame isaretli property'leri
 * serialize ettigi icin buradaki her alan (ve ic struct uyeleri) SaveGame olmali.
 */
USTRUCT()
struct FMMOCharacterSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	bool bValid = false;

	// Sinif + ilerleme
	UPROPERTY(SaveGame)
	EMMOClass CharacterClass = EMMOClass::Warrior;

	UPROPERTY(SaveGame)
	int32 Level = 1;

	UPROPERTY(SaveGame)
	int32 Experience = 0;

	UPROPERTY(SaveGame)
	int32 UnspentStatPoints = 0;

	UPROPERTY(SaveGame)
	int32 UnspentSkillPoints = 0;

	UPROPERTY(SaveGame)
	int32 Strength = 12;

	UPROPERTY(SaveGame)
	int32 Dexterity = 12;

	UPROPERTY(SaveGame)
	int32 Intelligence = 12;

	UPROPERTY(SaveGame)
	int32 Vitality = 12;

	// Cuzdan + tuketilebilir
	UPROPERTY(SaveGame)
	int32 Gold = 0;

	UPROPERTY(SaveGame)
	TArray<FMMOItemStack> Items;

	// Ekipman
	UPROPERTY(SaveGame)
	TArray<FMMOEquipmentInstance> EquipmentBag;

	UPROPERTY(SaveGame)
	TArray<FMMOEquipmentInstance> EquippedItems;

	UPROPERTY(SaveGame)
	int32 NextInstanceId = 1;

	// Skiller
	UPROPERTY(SaveGame)
	TArray<FName> LearnedSkills;

	UPROPERTY(SaveGame)
	TArray<FName> Hotbar;

	// KO mastery rutbeleri (index = EMMOSkillTree). Eski kayitlarda bos gelir.
	UPROPERTY(SaveGame)
	TArray<int32> MasteryRanks;

	// Gorevler
	UPROPERTY(SaveGame)
	TArray<FMMOQuestProgress> ActiveQuests;

	UPROPERTY(SaveGame)
	TArray<FName> CompletedQuests;

	// Konum
	UPROPERTY(SaveGame)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(SaveGame)
	FRotator Rotation = FRotator::ZeroRotator;
};

UCLASS()
class MYPROJECT_API UMMOSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	FMMOCharacterSaveData Data;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MMOQuestTypes.generated.h"

/** Gorev turu. */
UENUM(BlueprintType)
enum class EMMOQuestType : uint8
{
	KillSpecificMob UMETA(DisplayName = "Belirli Mob Oldur"),
	KillAnyMob      UMETA(DisplayName = "Herhangi Mob Oldur"),
	ReachLevel      UMETA(DisplayName = "Seviyeye Ulas"),
	HaveGold        UMETA(DisplayName = "Altin Biriktir")
};

/** Bir gorev tanimi. */
USTRUCT(BlueprintType)
struct FMMOQuestDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Quest")
	FName QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Quest")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Quest")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Quest")
	EMMOQuestType Type = EMMOQuestType::KillAnyMob;

	// KillSpecificMob icin hedef mob arketip id'si (Goblin, Orc...).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Quest")
	FName TargetMob;

	// Gereken sayi (oldurme sayisi / level / altin).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Quest")
	int32 TargetCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Quest")
	int32 RewardXP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Quest")
	int32 RewardGold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MMO|Quest")
	int32 RequiredLevel = 1;

	bool IsValidDef() const { return !QuestId.IsNone(); }
};

/** Oyuncunun bir gorevdeki ilerlemesi. */
USTRUCT(BlueprintType)
struct FMMOQuestProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "MMO|Quest")
	FName QuestId;

	// Oldurme gorevleri icin sayac (level/altin gorevleri anlik kontrol edilir).
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "MMO|Quest")
	int32 KillProgress = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "MMO|Quest")
	bool bClaimed = false;
};

/** Tum gorev tanimlarinin merkezi kayit defteri. */
class MYPROJECT_API FMMOQuestDatabase
{
public:
	static const TArray<FMMOQuestDef>& GetAll();
	static const FMMOQuestDef* Find(FName QuestId);
};

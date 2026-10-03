// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Gorev kabul/teslim NPC repliklerine erisim. Veri docs/Lore.md'den Scripts/GenQuestDialogue.py ile
 * uretilen MMOQuestDialogue.cpp icindedir (ASCII Turkce). Replik yoksa bos FString doner.
 */
class MYPROJECT_API FMMOQuestDialogue
{
public:
	static FString GetAcceptLine(FName QuestId);
	static FString GetTurnInLine(FName QuestId);
};

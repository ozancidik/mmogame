// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Skills/MMOSkillTypes.h"

/**
 * Tum skill tanimlarinin merkezi kayit defteri (KO'nun _MAGIC tablosunun karsiligi).
 * Simdilik C++'ta tanimli; ileride bir UE DataTable'a tasinabilir (FMMOSkillDef zaten FTableRowBase).
 */
class MYPROJECT_API FMMOSkillDatabase
{
public:
	// Tum skilller.
	static const TArray<FMMOSkillDef>& GetAll();

	// Id ile bul (yoksa nullptr).
	static const FMMOSkillDef* Find(FName SkillId);

	// Belirli bir sinifin skilleri.
	static TArray<FMMOSkillDef> GetForClass(EMMOClass Class);
};

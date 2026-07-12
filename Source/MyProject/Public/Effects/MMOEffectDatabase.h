// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/MMOEffectTypes.h"

/**
 * Tum durum efektlerinin (buff/debuff) statik veritabani.
 * Skill'ler ve konsol komutlari EffectId ile buradan efekt tanimini bulur.
 */
class MYPROJECT_API FMMOEffectDatabase
{
public:
	static const TArray<FMMOEffectDef>& GetAll();
	static const FMMOEffectDef* Find(FName EffectId);
};

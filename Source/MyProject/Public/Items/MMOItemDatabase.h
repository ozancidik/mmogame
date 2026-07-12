// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Items/MMOItemTypes.h"

/**
 * Tum ekipman tanimlarinin merkezi kayit defteri + upgrade (yakma) kurallari.
 * Simdilik C++'ta; ileride DataTable'a tasinabilir.
 */
class MYPROJECT_API FMMOItemDatabase
{
public:
	static const TArray<FMMOItemDef>& GetAll();
	static const FMMOItemDef* Find(FName ItemId);

	// --- Upgrade kurallari (KO tarzi) ---
	static constexpr int32 MaxPlus = 9;

	// Bir sonraki artiya gecis maliyeti (altin).
	static int32 GetUpgradeCost(int32 CurrentPlus);

	// Basari sansi (yuzde).
	static int32 GetUpgradeChancePct(int32 CurrentPlus);

	// Bu artidan denemede basarisizlik itemi YAKAR mi? (+4 ve uzeri deneme = yanma riski)
	static bool BurnsOnFail(int32 CurrentPlus);

	// Artiya gore stat hesaplari (her + %20 guclendirir).
	static float GetAttack(const FMMOItemDef& Def, int32 Plus);
	static float GetDefense(const FMMOItemDef& Def, int32 Plus);

	// --- Satici fiyatlari ---
	static constexpr int32 PotionPrice = 20;
	static int32 GetBuyPrice(const FMMOItemDef& Def);
	static int32 GetSellPrice(const FMMOItemDef& Def, int32 Plus);
};

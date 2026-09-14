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
	static constexpr int32 MaxPlus = 8;

	// Bir sonraki artiya gecis maliyeti (altin).
	static int32 GetUpgradeCost(int32 CurrentPlus);

	// Basari sansi (yuzde).
	static int32 GetUpgradeChancePct(int32 CurrentPlus);

	// Basarisiz denemede itemi YAKMA olasiligi (yuzde, 0-100).
	// +0..+2 guvenli (0), +3'ten itibaren her artiyla kademeli artar.
	static int32 GetBurnChancePct(int32 CurrentPlus);

	// Artiya gore stat hesaplari (her + %20 guclendirir).
	static float GetAttack(const FMMOItemDef& Def, int32 Plus);
	static float GetDefense(const FMMOItemDef& Def, int32 Plus);

	// --- Satici fiyatlari ---
	static constexpr int32 PotionPrice = 20;
	static int32 GetBuyPrice(const FMMOItemDef& Def);
	static int32 GetSellPrice(const FMMOItemDef& Def, int32 Plus);

	// --- Nadirlik (rarity) ---
	// Stat carpani (Common=1.0 ... Legendary=3.0).
	static float GetRarityMultiplier(EMMOItemRarity Rarity);
	// Isim/cerceve rengi.
	static FLinearColor GetRarityColor(EMMOItemRarity Rarity);
	// Kisa ad (HUD etiketi).
	static FString GetRarityName(EMMOItemRarity Rarity);
	// Agirlikli rastgele nadirlik (loot dususu icin).
	static EMMOItemRarity RollRarity();
};

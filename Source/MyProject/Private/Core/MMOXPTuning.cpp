// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/MMOXPTuning.h"

namespace
{
	struct FXPKnot
	{
		int32 Level;
		float Mult;
	};

	float Interpolate(const FXPKnot* Knots, int32 Count, int32 Level)
	{
		if (Level <= Knots[0].Level)
		{
			return Knots[0].Mult;
		}
		for (int32 i = 1; i < Count; ++i)
		{
			if (Level <= Knots[i].Level)
			{
				const float T = float(Level - Knots[i - 1].Level) / float(FMath::Max(1, Knots[i].Level - Knots[i - 1].Level));
				return FMath::Lerp(Knots[i - 1].Mult, Knots[i].Mult, T);
			}
		}
		return Knots[Count - 1].Mult;
	}

	// Lv1-5 aynen (hizli giris), Lv8'den itibaren iner, Lv34'te 1.0'a doner (Lv31+ icerik zaten 12-25 oldurme/seviye
	// hedefine gore kalibre). Model: docs/BALANCE_MODEL.md; degerler PLACEHOLDER, [MMOMETRIC] olcumuyle ayarlanir.
	// XP_TUNING_MOB: seviye:carpan
	const FXPKnot GMobKnots[] = {
		{1, 1.00f}, {5, 1.00f}, {8, 0.70f}, {10, 0.50f}, {12, 0.40f}, {16, 0.30f}, {20, 0.25f}, {25, 0.33f}, {30, 0.55f}, {34, 1.00f}
	};
	// XP_TUNING_QUEST: seviye:carpan
	const FXPKnot GQuestKnots[] = {
		{1, 1.00f}, {4, 1.00f}, {8, 0.60f}, {12, 0.40f}, {16, 0.30f}, {20, 0.26f}, {25, 0.25f}, {30, 0.26f}, {34, 1.00f}
	};
}

float MMOXPTuning::MobMultiplier(int32 MobLevel)
{
	return Interpolate(GMobKnots, UE_ARRAY_COUNT(GMobKnots), MobLevel);
}

float MMOXPTuning::QuestMultiplier(int32 GateLevel)
{
	return Interpolate(GQuestKnots, UE_ARRAY_COUNT(GQuestKnots), GateLevel);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * XP hiz ayari: mob ve gorev odul XP'sinin seviyeye gore carpani (tek ayar noktasi).
 * Tablo degerlerine dokunmadan (arketip/gorev tablolari ham deger tasir) ilerleme hizi buradan ayarlanir.
 * Carpan veri tablolari kurulurken (MakeMob / MakeQuest) uygulanir; calisma zamaninda maliyeti yok.
 * Egri noktalari Private/Core/MMOXPTuning.cpp icindeki "XP_TUNING_*" satirlarindadir ve
 * Scripts/ProgressionModel.py tarafindan OKUNUR (model kodla ayni degerleri kullanir).
 */
namespace MMOXPTuning
{
	/** Mob seviyesine gore mob XP carpani (parcali dogrusal, uclarda sabit). */
	MYPROJECT_API float MobMultiplier(int32 MobLevel);

	/** Gorev kapi (gereken) seviyesine gore gorev odul XP carpani (parcali dogrusal, uclarda sabit). */
	MYPROJECT_API float QuestMultiplier(int32 GateLevel);
}

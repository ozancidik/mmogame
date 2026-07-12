// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effects/MMOEffectDatabase.h"

namespace
{
	FMMOEffectDef MakeEffect(const TCHAR* Id, const TCHAR* Name, EMMOEffectType Type,
		float Magnitude, float Duration, float TickInterval, bool bDebuff, const TCHAR* Label)
	{
		FMMOEffectDef D;
		D.EffectId = FName(Id);
		D.Name = Name;
		D.Type = Type;
		D.Magnitude = Magnitude;
		D.Duration = Duration;
		D.TickInterval = TickInterval;
		D.bIsDebuff = bDebuff;
		D.ShortLabel = Label;
		return D;
	}

	const TArray<FMMOEffectDef>& BuildEffectTable()
	{
		static TArray<FMMOEffectDef> Table;
		if (Table.Num() > 0)
		{
			return Table;
		}

		using ET = EMMOEffectType;

		// --- Debuff'lar ---
		Table.Add(MakeEffect(TEXT("Poison"), TEXT("Zehir"), ET::PeriodicDamage, 8.f, 8.f, 1.f, true, TEXT("ZHR")));
		Table.Add(MakeEffect(TEXT("Slow"), TEXT("Yavaslatma"), ET::SpeedBuff, -35.f, 6.f, 0.f, true, TEXT("YVS")));

		// --- Buff'lar ---
		Table.Add(MakeEffect(TEXT("Regen"), TEXT("Yenilenme"), ET::PeriodicHeal, 12.f, 8.f, 1.f, false, TEXT("YEN")));
		Table.Add(MakeEffect(TEXT("Rage"), TEXT("Ofke"), ET::AttackBuff, 20.f, 15.f, 0.f, false, TEXT("OFK")));
		Table.Add(MakeEffect(TEXT("IronSkin"), TEXT("Demir Deri"), ET::DefenseBuff, 15.f, 15.f, 0.f, false, TEXT("ZRH")));
		Table.Add(MakeEffect(TEXT("Haste"), TEXT("Hiz"), ET::SpeedBuff, 45.f, 12.f, 0.f, false, TEXT("HIZ")));

		return Table;
	}
}

const TArray<FMMOEffectDef>& FMMOEffectDatabase::GetAll()
{
	return BuildEffectTable();
}

const FMMOEffectDef* FMMOEffectDatabase::Find(FName EffectId)
{
	if (EffectId.IsNone())
	{
		return nullptr;
	}
	for (const FMMOEffectDef& D : GetAll())
	{
		if (D.EffectId == EffectId)
		{
			return &D;
		}
	}
	return nullptr;
}

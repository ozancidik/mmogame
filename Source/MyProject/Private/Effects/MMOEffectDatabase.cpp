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

		// QA denetimi (2026-09-14, commit 2e401c2 sonrasi): Lv10-80 genisletmesindeki 6 yeni
		// BuffSelf skili bu tabandaki EffectId'leri BIREBIR yeniden kullaniyordu - ApplyEffect
		// siddeti tamamen EffectId'den okudugu icin (skil'in kendi Power alani BuffSelf'te hic
		// kullanilmiyor), o skiller Lv3-4'teki atalarinin kopyasi gibi davraniyordu (yuksek mana/
		// cooldown harcayip AYNI zayif etkiyi veriyorlardi). Her aile icin kademeli, daha guclu
		// bir "II"/"III" versiyonu eklendi; skill tablosundaki ilgili 6 satirin EffectId'si bu
		// yeni degerlere yonlendirildi (bkz. MMOSkillDatabase.cpp).
		Table.Add(MakeEffect(TEXT("RageII"), TEXT("Ofke II"), ET::AttackBuff, 55.f, 24.f, 0.f, false, TEXT("OF2")));
		Table.Add(MakeEffect(TEXT("IronSkinII"), TEXT("Demir Deri II"), ET::DefenseBuff, 40.f, 22.f, 0.f, false, TEXT("ZR2")));
		Table.Add(MakeEffect(TEXT("IronSkinIII"), TEXT("Demir Deri III"), ET::DefenseBuff, 60.f, 26.f, 0.f, false, TEXT("ZR3")));
		Table.Add(MakeEffect(TEXT("HasteII"), TEXT("Hiz II"), ET::SpeedBuff, 70.f, 18.f, 0.f, false, TEXT("HI2")));
		Table.Add(MakeEffect(TEXT("HasteIII"), TEXT("Hiz III"), ET::SpeedBuff, 95.f, 24.f, 0.f, false, TEXT("HI3")));
		Table.Add(MakeEffect(TEXT("RegenII"), TEXT("Yenilenme II"), ET::PeriodicHeal, 40.f, 12.f, 1.f, false, TEXT("YN2")));

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

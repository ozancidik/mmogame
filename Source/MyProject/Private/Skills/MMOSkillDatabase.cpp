// Copyright Epic Games, Inc. All Rights Reserved.

#include "Skills/MMOSkillDatabase.h"

namespace
{
	// MasteryRank: skilin kendi Tree'sinde acilmasi icin gereken rutbe (KO mastery sistemi).
	// Ayni agactaki paralel (birbirinin devami olmayan) skiller ayni rutbeyi paylasabilir.
	FMMOSkillDef MakeDef(const TCHAR* Id, const TCHAR* Name, EMMOClass Class, EMMOSkillTree Tree,
		int32 MasteryRank, int32 ReqLevel, int32 SPCost, const TCHAR* Prereq,
		EMMOSkillEffect Effect, float Mana, float CD, float Power, float Range, float Radius,
		const TCHAR* EffectId = TEXT(""))
	{
		FMMOSkillDef D;
		D.SkillId = FName(Id);
		D.Name = Name;
		D.RequiredClass = Class;
		D.Tree = Tree;
		D.RequiredMasteryRank = MasteryRank;
		D.RequiredLevel = ReqLevel;
		D.SkillPointCost = SPCost;
		D.PrereqSkillId = (Prereq && *Prereq) ? FName(Prereq) : NAME_None;
		D.Effect = Effect;
		D.ManaCost = Mana;
		D.Cooldown = CD;
		D.Power = Power;
		D.Range = Range;
		D.Radius = Radius;
		D.AppliedEffectId = (EffectId && *EffectId) ? FName(EffectId) : NAME_None;
		return D;
	}

	const TArray<FMMOSkillDef>& BuildSkillTable()
	{
		static TArray<FMMOSkillDef> Table;
		if (Table.Num() > 0)
		{
			return Table;
		}

		using E = EMMOSkillEffect;
		using C = EMMOClass;
		using T = EMMOSkillTree;

		// Mastery rutbeleri: her agacin ICINDEKI eski prereq zinciri 1,2,3... diye numaralandi.
		// Zincirde olmayan (paralel) skiller ayni rutbeyi paylasir — KO'da da bir mastery
		// grubunda ayni rutbede birden fazla skill acilabilir.

		// --- SAVASCI ---
		Table.Add(MakeDef(TEXT("War_PowerStrike"), TEXT("Guclu Vurus"), C::Warrior, T::Basic, 1, 1, 1, TEXT(""),
			E::MeleeDamage, 8.f, 2.f, 40.f, 240.f, 0.f));
		Table.Add(MakeDef(TEXT("War_ShieldBash"), TEXT("Kalkan Vurusu"), C::Warrior, T::Basic, 1, 2, 1, TEXT(""),
			E::MeleeDamage, 6.f, 3.f, 25.f, 200.f, 0.f));
		Table.Add(MakeDef(TEXT("War_Cleave"), TEXT("Savurma"), C::Warrior, T::Special0, 1, 3, 1, TEXT("War_PowerStrike"),
			E::AoESelf, 16.f, 6.f, 28.f, 0.f, 320.f));
		Table.Add(MakeDef(TEXT("War_MortalBlow"), TEXT("Olumcul Darbe"), C::Warrior, T::Special0, 2, 5, 2, TEXT("War_Cleave"),
			E::MeleeDamage, 22.f, 8.f, 75.f, 240.f, 0.f));
		// Kan Firtinasi: Olumcul Darbe zincirinin devami, etrafindaki herkesi bicer.
		Table.Add(MakeDef(TEXT("War_BloodStorm"), TEXT("Kan Firtinasi"), C::Warrior, T::Special0, 3, 7, 2, TEXT("War_MortalBlow"),
			E::AoESelf, 30.f, 14.f, 55.f, 0.f, 340.f));
		Table.Add(MakeDef(TEXT("War_BloodDrain"), TEXT("Kan Emme"), C::Warrior, T::Special1, 1, 4, 1, TEXT(""),
			E::HealSelf, 10.f, 10.f, 50.f, 0.f, 0.f));
		// Buff: saldiri gucu artisi (Ofke).
		Table.Add(MakeDef(TEXT("War_BattleShout"), TEXT("Savas Narasi"), C::Warrior, T::Special1, 1, 3, 1, TEXT(""),
			E::BuffSelf, 15.f, 18.f, 0.f, 0.f, 0.f, TEXT("Rage")));
		// Buff: savunma artisi (Demir Deri).
		Table.Add(MakeDef(TEXT("War_IronSkin"), TEXT("Demir Deri"), C::Warrior, T::Special2, 1, 4, 1, TEXT(""),
			E::BuffSelf, 15.f, 18.f, 0.f, 0.f, 0.f, TEXT("IronSkin")));

		// --- OKCU ---
		Table.Add(MakeDef(TEXT("Arc_QuickShot"), TEXT("Cabuk Atis"), C::Archer, T::Basic, 1, 1, 1, TEXT(""),
			E::RangedDamage, 6.f, 1.f, 25.f, 1400.f, 0.f));
		Table.Add(MakeDef(TEXT("Arc_PowerShot"), TEXT("Guclu Atis"), C::Archer, T::Basic, 1, 2, 1, TEXT(""),
			E::RangedDamage, 8.f, 1.5f, 40.f, 1600.f, 0.f));
		// Zehirli Ok: vurusta hedefe Poison DoT birakir.
		Table.Add(MakeDef(TEXT("Arc_PoisonArrow"), TEXT("Zehirli Ok"), C::Archer, T::Special0, 1, 3, 1, TEXT("Arc_PowerShot"),
			E::RangedDamage, 14.f, 5.f, 60.f, 1600.f, 0.f, TEXT("Poison")));
		Table.Add(MakeDef(TEXT("Arc_ArrowRain"), TEXT("Ok Yagmuru"), C::Archer, T::Special0, 2, 5, 2, TEXT("Arc_PoisonArrow"),
			E::AoETarget, 22.f, 10.f, 40.f, 1600.f, 360.f));
		// Delici Atis: tek hedefe agir hasar, vurdugu hedefi yavaslatir (Slow).
		Table.Add(MakeDef(TEXT("Arc_PiercingShot"), TEXT("Delici Atis"), C::Archer, T::Special0, 3, 7, 2, TEXT("Arc_ArrowRain"),
			E::RangedDamage, 26.f, 9.f, 95.f, 1700.f, 0.f, TEXT("Slow")));
		// Buff: hareket hizi (Hiz).
		Table.Add(MakeDef(TEXT("Arc_Sprint"), TEXT("Kosu"), C::Archer, T::Special1, 1, 3, 1, TEXT(""),
			E::BuffSelf, 12.f, 16.f, 0.f, 0.f, 0.f, TEXT("Haste")));

		// --- BUYUCU ---
		Table.Add(MakeDef(TEXT("Mag_IceBolt"), TEXT("Buz Oku"), C::Mage, T::Basic, 1, 1, 1, TEXT(""),
			E::RangedDamage, 10.f, 1.5f, 40.f, 1400.f, 0.f));
		Table.Add(MakeDef(TEXT("Mag_Fireball"), TEXT("Ates Topu"), C::Mage, T::Special0, 1, 2, 1, TEXT(""),
			E::RangedDamage, 12.f, 2.f, 58.f, 1500.f, 0.f));
		// Buz Patlamasi: alandaki hedefleri yavaslatir (Slow debuff).
		Table.Add(MakeDef(TEXT("Mag_FrostNova"), TEXT("Buz Patlamasi"), C::Mage, T::Special1, 1, 3, 1, TEXT(""),
			E::AoESelf, 20.f, 6.f, 45.f, 0.f, 360.f, TEXT("Slow")));
		Table.Add(MakeDef(TEXT("Mag_Meteor"), TEXT("Meteor"), C::Mage, T::Special0, 2, 6, 2, TEXT("Mag_Fireball"),
			E::AoETarget, 38.f, 12.f, 95.f, 1500.f, 420.f));
		// Zincir Yildirim: tek hedefe buyucunun en agir vurusu, carptigi hedefi felce ugratir (Slow).
		Table.Add(MakeDef(TEXT("Mag_ChainLightning"), TEXT("Zincir Yildirim"), C::Mage, T::Special0, 3, 8, 2, TEXT("Mag_Meteor"),
			E::RangedDamage, 42.f, 10.f, 130.f, 1600.f, 0.f, TEXT("Slow")));

		// --- PRIEST ---
		Table.Add(MakeDef(TEXT("Pri_HolyLight"), TEXT("Kutsal Isik"), C::Priest, T::Basic, 1, 1, 1, TEXT(""),
			E::HealSelf, 15.f, 3.f, 60.f, 0.f, 0.f));
		Table.Add(MakeDef(TEXT("Pri_Smite"), TEXT("Ceza"), C::Priest, T::Basic, 1, 2, 1, TEXT(""),
			E::RangedDamage, 10.f, 2.f, 35.f, 1200.f, 0.f));
		Table.Add(MakeDef(TEXT("Pri_HolyBurst"), TEXT("Kutsal Patlama"), C::Priest, T::Special0, 1, 4, 1, TEXT("Pri_Smite"),
			E::AoESelf, 25.f, 8.f, 48.f, 0.f, 320.f));
		// Buyuk Sifa: secili bir parti uyesi hedefteyse ONU iyilestirir (Priest'in asil
		// "sifaci" araci); hedef yoksa/kendine tikladiysa eskisi gibi kendini iyilestirir.
		Table.Add(MakeDef(TEXT("Pri_GreaterHeal"), TEXT("Buyuk Sifa"), C::Priest, T::Special1, 1, 5, 2, TEXT("Pri_HolyLight"),
			E::HealTarget, 40.f, 12.f, 150.f, 0.f, 0.f));
		// Ilahi Sifa: Buyuk Sifa'nin ustu; aninda buyuk iyilestirme + uzerine Yenilenme HoT.
		Table.Add(MakeDef(TEXT("Pri_DivineRestoration"), TEXT("Ilahi Sifa"), C::Priest, T::Special1, 2, 7, 2, TEXT("Pri_GreaterHeal"),
			E::HealSelf, 55.f, 20.f, 180.f, 0.f, 0.f, TEXT("Regen")));
		// Buff: zamanla iyilesme (Yenilenme HoT).
		Table.Add(MakeDef(TEXT("Pri_Renew"), TEXT("Yenilenme"), C::Priest, T::Special0, 1, 3, 1, TEXT(""),
			E::BuffSelf, 18.f, 12.f, 0.f, 0.f, 0.f, TEXT("Regen")));

		return Table;
	}
}

const TArray<FMMOSkillDef>& FMMOSkillDatabase::GetAll()
{
	return BuildSkillTable();
}

const FMMOSkillDef* FMMOSkillDatabase::Find(FName SkillId)
{
	if (SkillId.IsNone())
	{
		return nullptr;
	}
	for (const FMMOSkillDef& D : GetAll())
	{
		if (D.SkillId == SkillId)
		{
			return &D;
		}
	}
	return nullptr;
}

TArray<FMMOSkillDef> FMMOSkillDatabase::GetForClass(EMMOClass Class)
{
	TArray<FMMOSkillDef> Result;
	for (const FMMOSkillDef& D : GetAll())
	{
		if (D.RequiredClass == Class)
		{
			Result.Add(D);
		}
	}
	return Result;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/MMOMobArchetype.h"

namespace
{
	FMMOMobArchetype MakeMob(const TCHAR* Id, const TCHAR* Name, int32 Level, int32 Str, int32 Vit,
		float Dmg, int32 XP, int32 MinGold, int32 MaxGold, float Speed, float Aggro, float Scale, FLinearColor Tint)
	{
		FMMOMobArchetype A;
		A.ArchetypeId = FName(Id);
		A.Name = Name;
		A.Level = Level;
		A.Strength = Str;
		A.Vitality = Vit;
		A.AttackDamage = Dmg;
		A.XPReward = XP;
		A.MinGold = MinGold;
		A.MaxGold = MaxGold;
		A.MoveSpeed = Speed;
		A.AggroRange = Aggro;
		A.Scale = Scale;
		A.Tint = Tint;
		return A;
	}

	const TArray<FMMOMobArchetype>& BuildMobTable()
	{
		static TArray<FMMOMobArchetype> Table;
		if (Table.Num() > 0)
		{
			return Table;
		}

		//                Id            Ad            Lv Str Vit Dmg  XP  gMin gMax Speed Aggro Scale Tint
		Table.Add(MakeMob(TEXT("Goblin"),  TEXT("Goblin"),  1,  6,  4,  6.f,  30,  3,  12, 340.f, 900.f, 0.85f, FLinearColor(0.25f, 0.6f, 0.2f)));
		Table.Add(MakeMob(TEXT("WolfPup"), TEXT("Kurt"),    2,  9,  4,  9.f,  45,  2,  10, 430.f, 1150.f, 0.8f, FLinearColor(0.5f, 0.5f, 0.55f)));
		Table.Add(MakeMob(TEXT("Orc"),     TEXT("Ork"),     3, 12,  8, 12.f,  75, 10,  30, 300.f, 950.f, 1.05f, FLinearColor(0.7f, 0.1f, 0.1f)));
		Table.Add(MakeMob(TEXT("Bandit"),  TEXT("Haydut"),  4, 14,  7, 14.f,  95, 15,  45, 350.f, 1000.f, 1.0f, FLinearColor(0.45f, 0.3f, 0.15f)));
		// Haydut zehirli hancer kullanir: vurusta %35 sansla zehir.
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.35f;

		Table.Add(MakeMob(TEXT("Troll"),   TEXT("Trol"),    5, 16, 16, 20.f, 170, 25,  70, 220.f, 800.f, 1.35f, FLinearColor(0.15f, 0.4f, 0.15f)));
		// Trol can azalinca ofkelenir (%40 altinda saldiri gucu artar).
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.4f;

		// --- ELIT / BOSS'lar (rastgele spawn olmaz; sadece ArchetypeId ile cagrilinca) ---
		// Orumcek Ana: vurusta yuksek sansla yavaslatir.
		Table.Add(MakeMob(TEXT("SpiderQueen"), TEXT("Orumcek Ana"), 5, 15, 20, 16.f, 260, 40, 110, 300.f, 1100.f, 1.5f, FLinearColor(0.35f, 0.1f, 0.5f)));
		Table.Last().bIsElite = true;
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.6f;

		// Ork Reisi (boss): guclu, zehir tukurur ve dusuk canda ofkelenir.
		Table.Add(MakeMob(TEXT("OrcWarlord"), TEXT("Ork Reisi"), 6, 22, 28, 24.f, 420, 80, 200, 300.f, 1200.f, 1.7f, FLinearColor(0.6f, 0.05f, 0.05f)));
		Table.Last().bIsElite = true;
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.4f;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.5f;

		// Kadim Golem (tank elit): cok yavas ama devasa canli; can azalinca ofkelenir.
		Table.Add(MakeMob(TEXT("StoneGolem"), TEXT("Kadim Golem"), 7, 24, 40, 26.f, 410, 90, 210, 180.f, 850.f, 2.1f, FLinearColor(0.42f, 0.44f, 0.5f)));
		Table.Last().bIsElite = true;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.45f;

		// Golge Suikastci (assassin elit): cok hizli, ince canli; vurusta yuksek sansla zehirler.
		Table.Add(MakeMob(TEXT("ShadowAssassin"), TEXT("Golge Suikastci"), 7, 28, 16, 25.f, 400, 70, 180, 480.f, 1300.f, 1.35f, FLinearColor(0.12f, 0.1f, 0.18f)));
		Table.Last().bIsElite = true;
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.5f;

		return Table;
	}
}

const TArray<FMMOMobArchetype>& FMMOMobDatabase::GetAll()
{
	return BuildMobTable();
}

const FMMOMobArchetype* FMMOMobDatabase::Find(FName ArchetypeId)
{
	if (ArchetypeId.IsNone())
	{
		return nullptr;
	}
	for (const FMMOMobArchetype& A : GetAll())
	{
		if (A.ArchetypeId == ArchetypeId)
		{
			return &A;
		}
	}
	return nullptr;
}

const FMMOMobArchetype& FMMOMobDatabase::GetRandom()
{
	// Elit/boss'lar rastgele spawn olmaz; sadece normal moblar arasindan sec.
	const TArray<FMMOMobArchetype>& All = GetAll();

	TArray<int32> NormalIndices;
	for (int32 i = 0; i < All.Num(); ++i)
	{
		if (!All[i].bIsElite)
		{
			NormalIndices.Add(i);
		}
	}

	if (NormalIndices.Num() == 0)
	{
		return All[FMath::RandRange(0, All.Num() - 1)];
	}
	return All[NormalIndices[FMath::RandRange(0, NormalIndices.Num() - 1)]];
}

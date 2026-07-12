// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/MMOMobArchetype.h"

namespace
{
	FMMOMobArchetype Make(const TCHAR* Id, const TCHAR* Name, int32 Level, int32 Str, int32 Vit,
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

	const TArray<FMMOMobArchetype>& BuildTable()
	{
		static TArray<FMMOMobArchetype> Table;
		if (Table.Num() > 0)
		{
			return Table;
		}

		//                Id            Ad            Lv Str Vit Dmg  XP  gMin gMax Speed Aggro Scale Tint
		Table.Add(Make(TEXT("Goblin"),  TEXT("Goblin"),  1,  6,  4,  6.f,  30,  3,  12, 340.f, 900.f, 0.85f, FLinearColor(0.25f, 0.6f, 0.2f)));
		Table.Add(Make(TEXT("WolfPup"), TEXT("Kurt"),    2,  9,  4,  9.f,  45,  2,  10, 430.f, 1150.f, 0.8f, FLinearColor(0.5f, 0.5f, 0.55f)));
		Table.Add(Make(TEXT("Orc"),     TEXT("Ork"),     3, 12,  8, 12.f,  75, 10,  30, 300.f, 950.f, 1.05f, FLinearColor(0.7f, 0.1f, 0.1f)));
		Table.Add(Make(TEXT("Bandit"),  TEXT("Haydut"),  4, 14,  7, 14.f,  95, 15,  45, 350.f, 1000.f, 1.0f, FLinearColor(0.45f, 0.3f, 0.15f)));
		Table.Add(Make(TEXT("Troll"),   TEXT("Trol"),    5, 16, 16, 20.f, 170, 25,  70, 220.f, 800.f, 1.35f, FLinearColor(0.15f, 0.4f, 0.15f)));

		return Table;
	}
}

const TArray<FMMOMobArchetype>& FMMOMobDatabase::GetAll()
{
	return BuildTable();
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
	const TArray<FMMOMobArchetype>& All = GetAll();
	return All[FMath::RandRange(0, All.Num() - 1)];
}

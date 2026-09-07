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
		Table.Last().bUseWarrokMesh = true; // Mixamo Warrok ork mesh'i + animasyonlari
		Table.Add(MakeMob(TEXT("Bandit"),  TEXT("Haydut"),  4, 14,  7, 14.f,  95, 15,  45, 350.f, 1000.f, 1.0f, FLinearColor(0.45f, 0.3f, 0.15f)));
		// Haydut zehirli hancer kullanir: vurusta %35 sansla zehir.
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.35f;

		Table.Add(MakeMob(TEXT("Troll"),   TEXT("Trol"),    5, 16, 16, 20.f, 170, 25,  70, 220.f, 800.f, 1.35f, FLinearColor(0.15f, 0.4f, 0.15f)));
		// Trol can azalinca ofkelenir (%40 altinda saldiri gucu artar).
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.4f;

		// --- Ikinci bolge NORMAL moblari (Lv6-7; seviye-bandi spawner ile cikar, rastgele ana havuza band sayesinde sizmaz) ---
		// Ork Savascisi: Trol'den guclu, disiplinli dovusur; vurusta yaralayip yavaslatir.
		Table.Add(MakeMob(TEXT("OrcWarrior"), TEXT("Ork Savascisi"), 6, 18, 18, 22.f, 210, 35, 95, 300.f, 950.f, 1.2f, FLinearColor(0.55f, 0.15f, 0.05f)));
		Table.Last().bUseWarrokMesh = true; // Mixamo Warrok ork mesh'i + animasyonlari
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.3f;

		// Korku Kurdu: cok hizli, genis aggro; isirip avini yavaslatir.
		Table.Add(MakeMob(TEXT("DireWolf"), TEXT("Korku Kurdu"), 6, 19, 15, 23.f, 220, 30, 90, 470.f, 1250.f, 1.1f, FLinearColor(0.3f, 0.32f, 0.38f)));
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.35f;

		// Kara Buyucu: yuksek hasar, zehirli buyu; can azalinca ofkelenir.
		Table.Add(MakeMob(TEXT("Warlock"), TEXT("Kara Buyucu"), 7, 20, 18, 26.f, 270, 45, 120, 300.f, 1150.f, 1.05f, FLinearColor(0.4f, 0.15f, 0.55f)));
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.4f;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.35f;

		// --- ELIT / BOSS'lar (rastgele spawn olmaz; sadece ArchetypeId ile cagrilinca) ---
		// Orumcek Ana: vurusta yuksek sansla yavaslatir.
		Table.Add(MakeMob(TEXT("SpiderQueen"), TEXT("Orumcek Ana"), 5, 15, 20, 16.f, 260, 40, 110, 300.f, 1100.f, 1.5f, FLinearColor(0.35f, 0.1f, 0.5f)));
		Table.Last().bIsElite = true;
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.6f;
		// Garantili ekipman, +1..+2.
		Table.Last().EliteEquipDropChance = 1.0f;
		Table.Last().EliteEquipDropCount = 1;
		Table.Last().EliteMinItemPlus = 1;
		Table.Last().EliteMaxItemPlus = 2;

		// Ork Reisi (boss): guclu, zehir tukurur ve dusuk canda ofkelenir.
		Table.Add(MakeMob(TEXT("OrcWarlord"), TEXT("Ork Reisi"), 6, 22, 28, 24.f, 420, 80, 200, 300.f, 1200.f, 1.7f, FLinearColor(0.6f, 0.05f, 0.05f)));
		Table.Last().bUseWarrokMesh = true; // Mixamo Warrok ork mesh'i + animasyonlari
		Table.Last().bIsElite = true;
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.4f;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.5f;
		// Boss: en comert loot. 2 garantili ekipman, +2..+3.
		Table.Last().EliteEquipDropChance = 1.0f;
		Table.Last().EliteEquipDropCount = 2;
		Table.Last().EliteMinItemPlus = 2;
		Table.Last().EliteMaxItemPlus = 3;

		// Kadim Golem (tank elit): cok yavas ama devasa canli; can azalinca ofkelenir.
		Table.Add(MakeMob(TEXT("StoneGolem"), TEXT("Kadim Golem"), 7, 24, 40, 26.f, 410, 90, 210, 180.f, 850.f, 2.1f, FLinearColor(0.42f, 0.44f, 0.5f)));
		Table.Last().bIsElite = true;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.45f;
		// Garantili ekipman, +1..+3.
		Table.Last().EliteEquipDropChance = 1.0f;
		Table.Last().EliteEquipDropCount = 1;
		Table.Last().EliteMinItemPlus = 1;
		Table.Last().EliteMaxItemPlus = 3;

		// Golge Suikastci (assassin elit): cok hizli, ince canli; vurusta yuksek sansla zehirler.
		Table.Add(MakeMob(TEXT("ShadowAssassin"), TEXT("Golge Suikastci"), 7, 28, 16, 25.f, 400, 70, 180, 480.f, 1300.f, 1.35f, FLinearColor(0.12f, 0.1f, 0.18f)));
		Table.Last().bIsElite = true;
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.5f;
		// Garantili ekipman, +1..+3.
		Table.Last().EliteEquipDropChance = 1.0f;
		Table.Last().EliteEquipDropCount = 1;
		Table.Last().EliteMinItemPlus = 1;
		Table.Last().EliteMaxItemPlus = 3;

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

const FMMOMobArchetype& FMMOMobDatabase::GetRandomInLevelRange(int32 MinLv, int32 MaxLv)
{
	// Non-elit + seviye bandina uyanlari topla (0 = o yonde sinir yok).
	const TArray<FMMOMobArchetype>& All = GetAll();

	TArray<int32> BandIndices;
	for (int32 i = 0; i < All.Num(); ++i)
	{
		if (All[i].bIsElite)
		{
			continue;
		}
		if (MinLv > 0 && All[i].Level < MinLv)
		{
			continue;
		}
		if (MaxLv > 0 && All[i].Level > MaxLv)
		{
			continue;
		}
		BandIndices.Add(i);
	}

	// Banda uyan yoksa guvenli fallback: mevcut normal-havuz secimi.
	if (BandIndices.Num() == 0)
	{
		return GetRandom();
	}
	return All[BandIndices[FMath::RandRange(0, BandIndices.Num() - 1)]];
}

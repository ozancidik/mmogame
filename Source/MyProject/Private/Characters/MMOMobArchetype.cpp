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

	// Mixamo "Warrok" ork gorsel setini (mesh + 4 anim) bir arketipe uygular.
	// Orc/OrcWarrior/OrcWarlord ayni seti kullandigi icin tek yerde tanimli.
	// Offset/yaw/scale mevcut davranisi BIREBIR korumak icin -89 / -90 / 1 (Manny hizasiyla ayni).
	void SetWarrokVisual(FMMOMobArchetype& A)
	{
		A.MeshPath = TEXT("/Game/Mixamo/Warrok/Warrok.Warrok");
		A.AnimIdlePath = TEXT("/Game/Mixamo/Warrok/Anims/Warrok_Idle.Warrok_Idle");
		A.AnimRunPath = TEXT("/Game/Mixamo/Warrok/Anims/Warrok_Run.Warrok_Run");
		A.AnimAttackPath = TEXT("/Game/Mixamo/Warrok/Anims/Warrok_Attack.Warrok_Attack");
		A.AnimDeathPath = TEXT("/Game/Mixamo/Warrok/Anims/Warrok_Death.Warrok_Death");
		A.MeshOffsetZ = -89.f;
		A.MeshYaw = -90.f;
		A.MeshScaleMultiplier = 1.f;
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
		SetWarrokVisual(Table.Last()); // Mixamo Warrok ork mesh'i + animasyonlari (veri satiri)
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
		SetWarrokVisual(Table.Last()); // Mixamo Warrok ork mesh'i + animasyonlari (veri satiri)
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

		// --- Ucuncu bolge NORMAL moblari (Lv8-12; MinMobLevel=8/MaxMobLevel=12 bandli spawner ile cikar) ---
		// Olcekleme Lv5-7 egrisini surdurur: seviye basina kabaca +2 Str, +2 Vit, +3 hasar,
		// +%20-25 XP ve altin. Hassas ekonomi balansi degil, tutarli bir kademe.
		// Buz Hayaleti: hizli, dondurucu; vurusta yavaslatir.
		Table.Add(MakeMob(TEXT("FrostWraith"), TEXT("Buz Hayaleti"), 8, 22, 18, 29.f, 330, 55, 145, 420.f, 1200.f, 1.1f, FLinearColor(0.35f, 0.6f, 0.8f)));
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.35f;

		// Lanetli Sovalye: agir zirhli, yavas ama dayanikli; can azalinca ofkelenir.
		Table.Add(MakeMob(TEXT("CursedKnight"), TEXT("Lanetli Sovalye"), 9, 24, 26, 31.f, 400, 65, 170, 260.f, 1000.f, 1.25f, FLinearColor(0.3f, 0.32f, 0.4f)));
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.4f;

		// Zehir Orumcegi: cok hizli, genis aggro; vurusta yuksek sansla zehirler.
		Table.Add(MakeMob(TEXT("VenomSpider"), TEXT("Zehir Orumcegi"), 10, 26, 22, 36.f, 480, 80, 200, 440.f, 1250.f, 1.15f, FLinearColor(0.3f, 0.45f, 0.2f)));
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.45f;

		// Ork Berserk: bandin en guclu normali; hem yavaslatir hem ofkelenir.
		Table.Add(MakeMob(TEXT("OrcBerserker"), TEXT("Ork Berserk"), 12, 31, 30, 44.f, 660, 110, 270, 340.f, 1200.f, 1.35f, FLinearColor(0.65f, 0.2f, 0.08f)));
		SetWarrokVisual(Table.Last()); // Mixamo Warrok ork mesh'i + animasyonlari (veri satiri)
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.25f;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.5f;

		// --- Dorduncu bolge NORMAL moblari (Lv16-22; MinMobLevel=16/MaxMobLevel=22 bandli spawner ile cikar) ---
		// Tema: Alev Vadisi (volkanik) — ucuncu bolgenin buzul temasinin karsiti.
		// Olcekleme Lv8-12 egrisini AYNEN surdurur: seviye basina kabaca +2.5 Str, +2.5 Vit,
		// +3.5 hasar ve XP/altinda seviye basina ~x1.19 (Lv12 OrcBerserker 660 XP -> Lv16 1300,
		// Lv18 1850, Lv20 2600, Lv22 3700). Hassas ekonomi balansi degil, tutarli bir kademe.
		// Kor Tazisi: cok hizli akinci, genis aggro; kor isirigi yakar (Poison).
		Table.Add(MakeMob(TEXT("EmberHound"), TEXT("Kor Tazisi"), 16, 41, 38, 58.f, 1300, 200, 480, 460.f, 1250.f, 1.1f, FLinearColor(0.85f, 0.35f, 0.1f)));
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.4f;

		// Magma Golemi: cok yavas ama devasa canli tank; can azalinca ofkelenir.
		Table.Add(MakeMob(TEXT("MagmaGolem"), TEXT("Magma Golemi"), 18, 46, 52, 64.f, 1850, 280, 660, 230.f, 950.f, 1.45f, FLinearColor(0.5f, 0.2f, 0.18f)));
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.45f;

		// Kul Orku: dengeli vurucu; kul bulutu yavaslatir, dusuk canda ofkelenir.
		Table.Add(MakeMob(TEXT("AshOrc"), TEXT("Kul Orku"), 20, 52, 48, 72.f, 2600, 380, 880, 360.f, 1200.f, 1.25f, FLinearColor(0.45f, 0.35f, 0.3f)));
		SetWarrokVisual(Table.Last()); // Mixamo Warrok ork mesh'i + animasyonlari (veri satiri)
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.3f;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.4f;

		// Cehennem Hayaleti: bandin en guclu normali; hizli, yakici ve ofkelenir.
		Table.Add(MakeMob(TEXT("InfernalWraith"), TEXT("Cehennem Hayaleti"), 22, 58, 54, 82.f, 3700, 520, 1200, 430.f, 1300.f, 1.3f, FLinearColor(0.8f, 0.25f, 0.35f)));
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.45f;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.35f;

		// --- ELIT / BOSS'lar (rastgele spawn olmaz; sadece ArchetypeId ile cagrilinca) ---
		// Orumcek Ana: vurusta yuksek sansla yavaslatir.
		Table.Add(MakeMob(TEXT("SpiderQueen"), TEXT("Orumcek Ana"), 5, 15, 20, 16.f, 260, 40, 110, 300.f, 1100.f, 1.5f, FLinearColor(0.35f, 0.1f, 0.5f)));
		Table.Last().bIsElite = true;
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.6f;
		// Garantili ekipman, +1..+2.
		// Ekonomi denge raporu (2026-09-14): bu elitler MMOMobSpawner uzerinden 30-50sn'de
		// bir respawn ediyor (kalici bir dunya-boss/elit degil, tekrar farm edilebilir).
		// %100 garantili dusus, tek bir OrcWarlord olduruslunde bile ~1000+ altin likit
		// degerinde 2 esya demek - tekrarlanabilir oldugunda bu, projenin tek ciddi
		// gold-sink'i olan +4-ustu upgrade kumarindan bile daha buyuk bir altin/esya
		// kaynagi haline geliyordu. Sansa cevirmek (hala YUKSEK, ama garanti degil)
		// bu farm dongusunu yumusatir; bir sonraki playtest turunde ayarlanabilir.
		Table.Last().EliteEquipDropChance = 0.5f;
		Table.Last().EliteEquipDropCount = 1;
		Table.Last().EliteMinItemPlus = 1;
		Table.Last().EliteMaxItemPlus = 2;

		// Ork Reisi (boss): guclu, zehir tukurur ve dusuk canda ofkelenir.
		Table.Add(MakeMob(TEXT("OrcWarlord"), TEXT("Ork Reisi"), 6, 22, 28, 24.f, 420, 80, 200, 300.f, 1200.f, 1.7f, FLinearColor(0.6f, 0.05f, 0.05f)));
		SetWarrokVisual(Table.Last()); // Mixamo Warrok ork mesh'i + animasyonlari (veri satiri)
		Table.Last().bIsElite = true;
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.4f;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.5f;
		// Boss: en comert loot. 2 garantili ekipman, +2..+3. Sans orani gerekcesi icin yukarida SpiderQueen yorumuna bak.
		Table.Last().EliteEquipDropChance = 0.5f;
		Table.Last().EliteEquipDropCount = 2;
		Table.Last().EliteMinItemPlus = 2;
		Table.Last().EliteMaxItemPlus = 3;

		// Kadim Golem (tank elit): cok yavas ama devasa canli; can azalinca ofkelenir.
		Table.Add(MakeMob(TEXT("StoneGolem"), TEXT("Kadim Golem"), 7, 24, 40, 26.f, 410, 90, 210, 180.f, 850.f, 2.1f, FLinearColor(0.42f, 0.44f, 0.5f)));
		Table.Last().bIsElite = true;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.45f;
		// Garantili ekipman, +1..+3. Sans orani gerekcesi icin yukarida SpiderQueen yorumuna bak.
		Table.Last().EliteEquipDropChance = 0.5f;
		Table.Last().EliteEquipDropCount = 1;
		Table.Last().EliteMinItemPlus = 1;
		Table.Last().EliteMaxItemPlus = 3;

		// Golge Suikastci (assassin elit): cok hizli, ince canli; vurusta yuksek sansla zehirler.
		Table.Add(MakeMob(TEXT("ShadowAssassin"), TEXT("Golge Suikastci"), 7, 28, 16, 25.f, 400, 70, 180, 480.f, 1300.f, 1.35f, FLinearColor(0.12f, 0.1f, 0.18f)));
		Table.Last().bIsElite = true;
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.5f;
		// Garantili ekipman, +1..+3. Sans orani gerekcesi icin yukarida SpiderQueen yorumuna bak.
		Table.Last().EliteEquipDropChance = 0.5f;
		Table.Last().EliteEquipDropCount = 1;
		Table.Last().EliteMinItemPlus = 1;
		Table.Last().EliteMaxItemPlus = 3;

		// Buz Lordu (ucuncu bolge eliti, Lv13): bandin (Lv8-12) ustunde duran kademe-sonu elit.
		// Cok dayanikli, agir vurur; vurusta yuksek sansla yavaslatir ve dusuk canda ofkelenir.
		Table.Add(MakeMob(TEXT("FrostLich"), TEXT("Buz Lordu"), 13, 34, 44, 46.f, 900, 200, 420, 280.f, 1300.f, 1.8f, FLinearColor(0.45f, 0.75f, 0.95f)));
		Table.Last().bIsElite = true;
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.55f;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.4f;
		// 2 ekipman, +2..+4. Dusus sansi bilerek 0.5 (garantili DEGIL): ekonomi denge raporu
		// (2026-09-14) %100 garantili elit loot'u tekrar-farm edilebilir bir altin/esya
		// muslugu olarak isaretlemisti — yeni elit de ayni oranla baslar. Bkz. SpiderQueen yorumu.
		Table.Last().EliteEquipDropChance = 0.5f;
		Table.Last().EliteEquipDropCount = 2;
		Table.Last().EliteMinItemPlus = 2;
		Table.Last().EliteMaxItemPlus = 4;

		// Magma Tirani (dorduncu bolge eliti, Lv24): bandin (Lv16-22) ustunde duran kademe-sonu elit.
		// Buz Lordu'nun (Lv13) volkanik karsiligi; devasa canli, cok agir vurur, yakar ve ofkelenir.
		Table.Add(MakeMob(TEXT("MagmaTyrant"), TEXT("Magma Tirani"), 24, 68, 82, 95.f, 6800, 1100, 2300, 270.f, 1350.f, 2.0f, FLinearColor(0.9f, 0.3f, 0.08f)));
		Table.Last().bIsElite = true;
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.5f;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.4f;
		// 2 ekipman, +3..+5. Dusus sansi yine bilerek 0.5 (garantili DEGIL): ekonomi denge raporu
		// (2026-09-14) %100 garantili elit loot'u tekrar-farm edilebilir bir altin/esya muslugu
		// olarak isaretlemisti — her yeni elit ayni oranla baslar. Bkz. SpiderQueen yorumu.
		// Elit loot havuzu "RequiredLevel <= MobLevel && >= MaxReq-2" oldugu icin bu elit
		// Adamantin tier'ini (Lv16) dusurur; Mithril (Lv10) havuza girmez (MaxReq=16).
		Table.Last().EliteEquipDropChance = 0.5f;
		Table.Last().EliteEquipDropCount = 2;
		Table.Last().EliteMinItemPlus = 3;
		Table.Last().EliteMaxItemPlus = 5;

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

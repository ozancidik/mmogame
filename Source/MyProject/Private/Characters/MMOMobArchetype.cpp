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
		// GUVENLIK duzeltmesi (2026-09-18, QA entegrasyon taramasi HIGH #1): bu ve asagidaki
		// Ucuncu/Dorduncu bolge mob'larinin AggroRange'i (950-1300) spawner'lar haritanin
		// gercek zeminine sigacak sekilde PlayerStart'in ARKASINA tasindiktan (2ca4d7a/0353f01)
		// SONRA cok tehlikeli hale geldi: en yakin spawn noktasi dogus/canlanma noktasina
		// sadece ~900-1450u, aggro menzili ise buraya kadar ulasabiliyordu (bazi ciftlerde
		// menzil dogus noktasini fazlasiyla asiyordu). Taze bir Lv1 karakter (158 HP) 2-3
		// vurusta olebiliyor, `RespawnAtStart` da onu AYNI tehlikeli noktaya geri koyuyordu -
		// kalici bir olum dongusu. TUM 11 Ikinci/Ucuncu/Dorduncu-bolge mobunun AggroRange'i
		// 600'e cekildi (bagimsiz olarak python ile dogrulandi: en kotu durumda bile dogus
		// noktasi/NPC'lere >=300u guvenlik payi kaliyor). Ayni sebeple spawner SpawnRadius'lari
		// da (`MMOGameMode.cpp`) 500->400'e indirildi.
		// Ork Savascisi: Trol'den guclu, disiplinli dovusur; vurusta yaralayip yavaslatir.
		Table.Add(MakeMob(TEXT("OrcWarrior"), TEXT("Ork Savascisi"), 6, 18, 18, 22.f, 210, 35, 95, 300.f, 600.f, 1.2f, FLinearColor(0.55f, 0.15f, 0.05f)));
		SetWarrokVisual(Table.Last()); // Mixamo Warrok ork mesh'i + animasyonlari (veri satiri)
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.3f;

		// Korku Kurdu: cok hizli, genis aggro; isirip avini yavaslatir.
		Table.Add(MakeMob(TEXT("DireWolf"), TEXT("Korku Kurdu"), 6, 19, 15, 23.f, 220, 30, 90, 470.f, 600.f, 1.1f, FLinearColor(0.3f, 0.32f, 0.38f)));
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.35f;

		// Kara Buyucu: yuksek hasar, zehirli buyu; can azalinca ofkelenir.
		Table.Add(MakeMob(TEXT("Warlock"), TEXT("Kara Buyucu"), 7, 20, 18, 26.f, 270, 45, 120, 300.f, 600.f, 1.05f, FLinearColor(0.4f, 0.15f, 0.55f)));
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.4f;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.35f;

		// --- Ucuncu bolge NORMAL moblari (Lv8-12; MinMobLevel=8/MaxMobLevel=12 bandli spawner ile cikar) ---
		// Olcekleme Lv5-7 egrisini surdurur: seviye basina kabaca +2 Str, +2 Vit, +3 hasar,
		// +%20-25 XP ve altin. Hassas ekonomi balansi degil, tutarli bir kademe.
		// Buz Hayaleti: hizli, dondurucu; vurusta yavaslatir.
		Table.Add(MakeMob(TEXT("FrostWraith"), TEXT("Buz Hayaleti"), 8, 22, 18, 29.f, 330, 55, 145, 420.f, 600.f, 1.1f, FLinearColor(0.35f, 0.6f, 0.8f)));
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.35f;

		// Lanetli Sovalye: agir zirhli, yavas ama dayanikli; can azalinca ofkelenir.
		Table.Add(MakeMob(TEXT("CursedKnight"), TEXT("Lanetli Sovalye"), 9, 24, 26, 31.f, 400, 65, 170, 260.f, 600.f, 1.25f, FLinearColor(0.3f, 0.32f, 0.4f)));
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.4f;

		// Zehir Orumcegi: cok hizli, genis aggro; vurusta yuksek sansla zehirler.
		Table.Add(MakeMob(TEXT("VenomSpider"), TEXT("Zehir Orumcegi"), 10, 26, 22, 36.f, 480, 80, 200, 440.f, 600.f, 1.15f, FLinearColor(0.3f, 0.45f, 0.2f)));
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.45f;

		// Ork Berserk: bandin en guclu normali; hem yavaslatir hem ofkelenir.
		Table.Add(MakeMob(TEXT("OrcBerserker"), TEXT("Ork Berserk"), 12, 31, 30, 44.f, 660, 110, 270, 340.f, 600.f, 1.35f, FLinearColor(0.65f, 0.2f, 0.08f)));
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
		Table.Add(MakeMob(TEXT("EmberHound"), TEXT("Kor Tazisi"), 16, 41, 38, 58.f, 1300, 200, 480, 460.f, 600.f, 1.1f, FLinearColor(0.85f, 0.35f, 0.1f)));
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.4f;

		// Magma Golemi: cok yavas ama devasa canli tank; can azalinca ofkelenir.
		Table.Add(MakeMob(TEXT("MagmaGolem"), TEXT("Magma Golemi"), 18, 46, 52, 64.f, 1850, 280, 660, 230.f, 600.f, 1.45f, FLinearColor(0.5f, 0.2f, 0.18f)));
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.45f;

		// Kul Orku: dengeli vurucu; kul bulutu yavaslatir, dusuk canda ofkelenir.
		Table.Add(MakeMob(TEXT("AshOrc"), TEXT("Kul Orku"), 20, 52, 48, 72.f, 2600, 380, 880, 360.f, 600.f, 1.25f, FLinearColor(0.45f, 0.35f, 0.3f)));
		SetWarrokVisual(Table.Last()); // Mixamo Warrok ork mesh'i + animasyonlari (veri satiri)
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.3f;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.4f;

		// Cehennem Hayaleti: bandin en guclu normali; hizli, yakici ve ofkelenir.
		Table.Add(MakeMob(TEXT("InfernalWraith"), TEXT("Cehennem Hayaleti"), 22, 58, 54, 82.f, 3700, 520, 1200, 430.f, 600.f, 1.3f, FLinearColor(0.8f, 0.25f, 0.35f)));
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.45f;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.35f;

		// --- Besinci bolge NORMAL moblari (Lv25-29; Summer_scene'deki Obsidyen spawner'i) ---
		// Obsidyen Avcisi: hizli ve zehirli; onceki bandin en guclu normalinden kademeli artis.
		Table.Add(MakeMob(TEXT("ObsidianStalker"), TEXT("Obsidyen Avcisi"), 25, 63, 60, 88.f, 4400, 600, 1400, 440.f, 600.f, 1.2f, FLinearColor(0.22f, 0.16f, 0.28f)));
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.35f;

		// Obsidyen Muhafizi: yavas/dayanikli tank; vurusta yavaslatir.
		Table.Add(MakeMob(TEXT("ObsidianSentinel"), TEXT("Obsidyen Muhafizi"), 27, 68, 76, 94.f, 5100, 800, 1700, 250.f, 600.f, 1.5f, FLinearColor(0.28f, 0.3f, 0.38f)));
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.35f;

		// Obsidyen Akincisi: Warrok ork gorseli, dengeli vurucu; dusuk canda ofkelenir.
		Table.Add(MakeMob(TEXT("ObsidianRaider"), TEXT("Obsidyen Akincisi"), 29, 73, 70, 100.f, 5900, 950, 1900, 365.f, 600.f, 1.3f, FLinearColor(0.32f, 0.18f, 0.22f)));
		SetWarrokVisual(Table.Last());
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.4f;

		// --- Altinci bolge NORMAL moblari (Lv31-39; yalnizca kendi seviye bandinda spawn olur) ---
		// Dar Summer_scene alani icin 600u aggro; varsayilan 450u wander / 1800u leash ile hub'dan uzak.
		Table.Add(MakeMob(TEXT("DuskProwler"), TEXT("Alacakaranlik Avcisi"), 31, 78, 74, 107.f, 6800, 1100, 2200, 435.f, 600.f, 1.2f, FLinearColor(0.22f, 0.3f, 0.42f)));
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.35f;

		Table.Add(MakeMob(TEXT("DuskGuardian"), TEXT("Alacakaranlik Muhafizi"), 33, 83, 86, 114.f, 8000, 1300, 2500, 255.f, 600.f, 1.5f, FLinearColor(0.28f, 0.36f, 0.48f)));
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.35f;

		Table.Add(MakeMob(TEXT("DuskReaver"), TEXT("Alacakaranlik Yagmacisi"), 36, 91, 84, 125.f, 9800, 1550, 3000, 370.f, 600.f, 1.3f, FLinearColor(0.34f, 0.2f, 0.42f)));
		SetWarrokVisual(Table.Last());
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.4f;

		Table.Add(MakeMob(TEXT("DuskWraith"), TEXT("Alacakaranlik Hayaleti"), 39, 98, 90, 136.f, 12000, 1850, 3500, 410.f, 600.f, 1.35f, FLinearColor(0.42f, 0.26f, 0.55f)));
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.4f;

		// --- Yedinci bolge NORMAL moblari (Lv42-48; Winter temali icerik verisi) ---
		// Lv31-39 egrisinden olculu artis; spawner/harita yerlesimi ayri entegrasyon adimi.
		Table.Add(MakeMob(TEXT("WinterStalker"), TEXT("Kis Izcisi"), 42, 104, 98, 144.f, 14000, 2100, 3900, 430.f, 600.f, 1.2f, FLinearColor(0.55f, 0.72f, 0.82f)));
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.35f;

		Table.Add(MakeMob(TEXT("WinterGuardian"), TEXT("Kis Muhafizi"), 44, 109, 110, 151.f, 16000, 2350, 4300, 255.f, 600.f, 1.5f, FLinearColor(0.42f, 0.58f, 0.7f)));
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.35f;

		Table.Add(MakeMob(TEXT("WinterReaver"), TEXT("Kis Yagmacisi"), 46, 115, 108, 160.f, 19000, 2650, 4800, 365.f, 600.f, 1.3f, FLinearColor(0.36f, 0.5f, 0.64f)));
		SetWarrokVisual(Table.Last());
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.4f;

		Table.Add(MakeMob(TEXT("WinterWraith"), TEXT("Kis Hayaleti"), 48, 120, 116, 169.f, 22000, 3000, 5300, 405.f, 600.f, 1.35f, FLinearColor(0.65f, 0.8f, 0.95f)));
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.4f;

		// --- Sekizinci kademe NORMAL moblari (Lv51-55; Frostbound, Winter_scene) ---
		// PROGRESSION_PLAN Lv51-57 dilimi. XP'ler plan hipotezi (seviye basina ~12-25 kill):
		// 51->52 esigi ~543K / 30K ~= 19 kill. Denge PIE'de dogrulanmadi.
		Table.Add(MakeMob(TEXT("FrostboundScout"), TEXT("Buzbagli Gozcu"), 51, 128, 122, 182.f, 30000, 3300, 5800, 440.f, 600.f, 1.25f, FLinearColor(0.7f, 0.85f, 1.f)));
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.35f;

		Table.Add(MakeMob(TEXT("FrostboundSentinel"), TEXT("Buzbagli Nobetci"), 55, 138, 140, 198.f, 36000, 3800, 6600, 260.f, 600.f, 1.55f, FLinearColor(0.45f, 0.62f, 0.85f)));
		SetWarrokVisual(Table.Last());
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.4f;

		// --- Dokuzuncu kademe NORMAL moblari (Lv58-62; Rift, Summer_scene dis cepleri) ---
		// XP plan hipotezi: 65->66 esigi ~1.16M / 52K ~= 22 kill. PIE'de dogrulanmadi.
		Table.Add(MakeMob(TEXT("RiftSeeker"), TEXT("Yarik Arayicisi"), 58, 160, 154, 222.f, 45000, 4300, 7500, 440.f, 600.f, 1.25f, FLinearColor(0.6f, 0.3f, 0.85f)));
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.35f;

		Table.Add(MakeMob(TEXT("RiftKeeper"), TEXT("Yarik Bekcisi"), 62, 172, 178, 238.f, 52000, 4800, 8300, 265.f, 600.f, 1.55f, FLinearColor(0.45f, 0.22f, 0.7f)));
		SetWarrokVisual(Table.Last());
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.4f;

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

		// Obsidyen Reisi (Lv30): besinci bolge sonu; Warrok gorseli ve mevcut Slow/Rage efektleri.
		Table.Add(MakeMob(TEXT("ObsidianWarlord"), TEXT("Obsidyen Reisi"), 30, 84, 100, 114.f, 11000, 1600, 3200, 290.f, 1200.f, 1.9f, FLinearColor(0.25f, 0.12f, 0.3f)));
		SetWarrokVisual(Table.Last());
		Table.Last().bIsElite = true;
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.5f;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.4f;
		// Iki bagimsiz %50 zar; Lv25 Obsidyen seti elitin ust-tier loot havuzudur.
		Table.Last().EliteEquipDropChance = 0.5f;
		Table.Last().EliteEquipDropCount = 2;
		Table.Last().EliteMinItemPlus = 3;
		Table.Last().EliteMaxItemPlus = 5;

		// Alacakaranlik Hukumdari (Lv40): yeni bolgenin son eliti, 33. seviye setini dusurur.
		Table.Add(MakeMob(TEXT("DuskOverlord"), TEXT("Alacakaranlik Hukumdari"), 40, 112, 128, 150.f, 19000, 2600, 4800, 285.f, 600.f, 1.95f, FLinearColor(0.26f, 0.14f, 0.42f)));
		SetWarrokVisual(Table.Last());
		Table.Last().bIsElite = true;
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.5f;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.4f;
		Table.Last().EliteEquipDropChance = 0.5f;
		Table.Last().EliteEquipDropCount = 2;
		Table.Last().EliteMinItemPlus = 3;
		Table.Last().EliteMaxItemPlus = 5;

		// Kis Hukumdari (Lv50): Winter kademesinin son eliti; Lv42 seti elit loot havuzunda.
		Table.Add(MakeMob(TEXT("WinterSovereign"), TEXT("Kis Hukumdari"), 50, 136, 150, 184.f, 30000, 3900, 6900, 280.f, 600.f, 2.0f, FLinearColor(0.48f, 0.68f, 0.88f)));
		Table.Last().bIsElite = true;
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.5f;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.4f;
		Table.Last().EliteEquipDropChance = 0.5f;
		Table.Last().EliteEquipDropCount = 2;
		Table.Last().EliteMinItemPlus = 3;
		Table.Last().EliteMaxItemPlus = 5;

		// Buzbagli Muhafiz (Lv57): Frostbound kademesinin kapi eliti; Lv51 seti elit loot havuzunda.
		Table.Add(MakeMob(TEXT("FrostboundWarden"), TEXT("Buzbagli Muhafiz"), 57, 152, 168, 210.f, 52000, 5000, 9000, 285.f, 600.f, 2.05f, FLinearColor(0.38f, 0.55f, 0.9f)));
		Table.Last().bIsElite = true;
		Table.Last().OnHitEffectId = FName(TEXT("Slow"));
		Table.Last().OnHitEffectChance = 0.5f;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.4f;
		Table.Last().EliteEquipDropChance = 0.5f;
		Table.Last().EliteEquipDropCount = 2;
		Table.Last().EliteMinItemPlus = 3;
		Table.Last().EliteMaxItemPlus = 5;

		// Yarik Habercisi (Lv65): Rift kademesinin eliti; Lv58 seti elit loot havuzunda.
		Table.Add(MakeMob(TEXT("RiftHerald"), TEXT("Yarik Habercisi"), 65, 186, 200, 255.f, 75000, 6500, 11500, 290.f, 600.f, 2.1f, FLinearColor(0.55f, 0.18f, 0.8f)));
		Table.Last().bIsElite = true;
		Table.Last().OnHitEffectId = FName(TEXT("Poison"));
		Table.Last().OnHitEffectChance = 0.5f;
		Table.Last().EnrageEffectId = FName(TEXT("Rage"));
		Table.Last().EnrageHealthPct = 0.4f;
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

const FMMOMobArchetype* FMMOMobDatabase::GetRandomInLevelRange(int32 MinLv, int32 MaxLv)
{
	if (MinLv < 0 || MaxLv < 0 || (MinLv > 0 && MaxLv > 0 && MinLv > MaxLv))
	{
		return nullptr;
	}

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

	if (BandIndices.Num() == 0)
	{
		return nullptr;
	}
	return &All[BandIndices[FMath::RandRange(0, BandIndices.Num() - 1)]];
}

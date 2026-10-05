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

		// Lv10-80 genisletmesi (KO tarzi uydurma icerik, bkz. commit mesaji): Special0 fiziksel
		// zincirin devami, Special1 kan/ofke hatti, Special2 savunma hatti, Special3 = gec oyun
		// "ustalik sonu" agaci (iki capstone, Lv70 ve Lv80).
		Table.Add(MakeDef(TEXT("War_ShatterBlow"), TEXT("Parcalayici Darbe"), C::Warrior, T::Special0, 4, 10, 2, TEXT("War_BloodStorm"),
			E::MeleeDamage, 45.f, 10.f, 150.f, 240.f, 0.f, TEXT("Slow")));
		// Lv11-30 bosluk dolgusu (2026-10-04): Lv10 (150) ile Lv24 arasinda yeni tek hedef hasar yoktu.
		// Guc/mana Lv10 ve Lv24 satirlari arasinda dogrusal ara deger; PLACEHOLDER, olcumle ayarlanir.
		Table.Add(MakeDef(TEXT("War_CrushingBlow"), TEXT("Ezici Darbe"), C::Warrior, T::Special0, 4, 13, 2, TEXT("War_BloodStorm"),
			E::MeleeDamage, 58.f, 9.f, 190.f, 240.f, 0.f));
		Table.Add(MakeDef(TEXT("War_Execute"), TEXT("Infaz"), C::Warrior, T::Special0, 5, 20, 2, TEXT("War_ShatterBlow"),
			E::MeleeDamage, 85.f, 12.f, 250.f, 240.f, 0.f));
		Table.Add(MakeDef(TEXT("War_TempestSlash"), TEXT("Savas Firtinasi"), C::Warrior, T::Special0, 5, 24, 2, TEXT("War_ShatterBlow"),
			E::AoESelf, 95.f, 12.f, 270.f, 0.f, 440.f));
		// QA BULGU #6 duzeltmesi (2026-09-15): Special0 fiziksel hasar zinciri Lv24'te
		// (War_TempestSlash) tikaniyordu - Lv24'ten Lv70'e (Titan Ofkesi capstone) kadar 46
		// seviye boyunca Warrior'a YENI hicbir hasar skili gelmiyordu (Special1/Special2'deki
		// Lv45/58 eklemeleri buff, Power=0). Archer/Mage ayni araliktaki Special2 hattinda
		// (Lv45/58) yeni hasar skilleri kazaniyordu, bu yuzden Warrior "bos" kaliyordu. Fiziksel
		// zincir Lv45 ve Lv58'de iki halka daha uzatildi, Power/Mana/CD Archer'in
		// Arc_SpikeTrap/PoisonCloud ve Mage'in Mag_ArcaneBurst/ManaStorm ile (ayni seviyedeki
		// hasar skilleri) AYNI olcekte tutuldu (Power ~510 @Lv45, ~660 @Lv58; Mana 195/250;
		// CD Archer'inkiyle birebir 18/20). Savas Firtinasi'ndan (rutbe 5) devam eden zincir.
		Table.Add(MakeDef(TEXT("War_WarCyclone"), TEXT("Savas Kasirgasi"), C::Warrior, T::Special0, 6, 45, 2, TEXT("War_TempestSlash"),
			E::MeleeDamage, 195.f, 18.f, 510.f, 240.f, 0.f));
		Table.Add(MakeDef(TEXT("War_WarDeluge"), TEXT("Savas Tufani"), C::Warrior, T::Special0, 7, 58, 3, TEXT("War_WarCyclone"),
			E::AoESelf, 250.f, 20.f, 660.f, 0.f, 560.f));
		Table.Add(MakeDef(TEXT("War_LifeReap"), TEXT("Yasam Cali"), C::Warrior, T::Special1, 2, 16, 2, TEXT("War_BloodDrain"),
			E::HealSelf, 70.f, 16.f, 310.f, 0.f, 0.f));
		// Lv24-33 bosluk dolgusu (2026-10-04): Special0 rutbe 6 (1 puan harcatir); PLACEHOLDER degerler.
		Table.Add(MakeDef(TEXT("War_Rampage"), TEXT("Kudurma"), C::Warrior, T::Special0, 6, 28, 2, TEXT("War_TempestSlash"),
			E::MeleeDamage, 110.f, 13.f, 340.f, 240.f, 0.f));
		// Lv31/39 ara halkalari (docs/CONTENT_PLAN_LV11_45.md): Lv28-45 boslugunda alan kontrolu
		// ve tek hedef secenegi. Rutbe 6 zaten yatirildigi icin seviyede otomatik acilir.
		Table.Add(MakeDef(TEXT("War_Earthshaker"), TEXT("Yer Sarsintisi"), C::Warrior, T::Special0, 6, 31, 2, TEXT("War_TempestSlash"),
			E::AoESelf, 130.f, 14.f, 330.f, 0.f, 460.f, TEXT("Slow")));
		Table.Add(MakeDef(TEXT("War_BloodLord"), TEXT("Kan Efendisi"), C::Warrior, T::Special1, 3, 33, 2, TEXT("War_LifeReap"),
			E::BuffSelf, 145.f, 24.f, 0.f, 0.f, 0.f, TEXT("RageII")));
		Table.Add(MakeDef(TEXT("War_Skullsplitter"), TEXT("Kafa Yarici"), C::Warrior, T::Special0, 6, 39, 2, TEXT("War_Rampage"),
			E::MeleeDamage, 165.f, 16.f, 445.f, 240.f, 0.f));
		Table.Add(MakeDef(TEXT("War_SteelArmor"), TEXT("Celik Zirh"), C::Warrior, T::Special2, 2, 45, 2, TEXT("War_IronSkin"),
			E::BuffSelf, 120.f, 22.f, 0.f, 0.f, 0.f, TEXT("IronSkinII")));
		Table.Add(MakeDef(TEXT("War_Unbreakable"), TEXT("Yikilmaz"), C::Warrior, T::Special2, 3, 58, 3, TEXT("War_SteelArmor"),
			E::BuffSelf, 150.f, 26.f, 0.f, 0.f, 0.f, TEXT("IronSkinIII")));
		// Capstone 1/2: Titan Ofkesi.
		Table.Add(MakeDef(TEXT("War_TitanWrath"), TEXT("Titan Ofkesi"), C::Warrior, T::Special3, 1, 70, 3, TEXT(""),
			E::MeleeDamage, 320.f, 20.f, 1150.f, 260.f, 0.f));
		// Capstone 2/2: Kiyamet Darbesi (Savascinin nihai skilli).
		// QA BULGU #5 duzeltmesi (2026-09-14): eskiden Power=1200 idi, Titan Ofkesi'nden
		// (1150) sadece +%4.3 gucluydu ve uzayan CD (20->26s) yuzunden saniye-basina hasar
		// fiilen DUSUYORDU (57.5->46). Diger 3 sinifin Lv70->80 sicramasi +%50-86 arasindaydi;
		// bu da Mage'in +%50'siyle tutarli olacak sekilde 1725'e cikarildi (~1150*1.5).
		Table.Add(MakeDef(TEXT("War_DoomsdayBlow"), TEXT("Kiyamet Darbesi"), C::Warrior, T::Special3, 2, 80, 3, TEXT("War_TitanWrath"),
			E::AoESelf, 400.f, 26.f, 1725.f, 0.f, 650.f));

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

		// Lv10-80 genisletmesi (KO tarzi uydurma icerik, bkz. commit mesaji): Special0 menzilli
		// hasar zincirinin devami, Special1 hiz/kacis hatti, Special2 yeni tuzak/zehir hatti,
		// Special3 = gec oyun "ustalik sonu" agaci (iki capstone, Lv70 ve Lv80).
		Table.Add(MakeDef(TEXT("Arc_EagleEye"), TEXT("Kartal Gozu"), C::Archer, T::Special0, 4, 10, 2, TEXT("Arc_PiercingShot"),
			E::RangedDamage, 45.f, 10.f, 150.f, 1800.f, 0.f));
		// Lv11-30 bosluk dolgusu (2026-10-04); PLACEHOLDER degerler.
		Table.Add(MakeDef(TEXT("Arc_Volley"), TEXT("Ok Sagnagi"), C::Archer, T::Special0, 4, 13, 2, TEXT("Arc_PiercingShot"),
			E::AoETarget, 62.f, 11.f, 140.f, 1700.f, 380.f, TEXT("Poison")));
		Table.Add(MakeDef(TEXT("Arc_Snipe"), TEXT("Keskin Nisan"), C::Archer, T::Special0, 5, 20, 2, TEXT("Arc_EagleEye"),
			E::RangedDamage, 90.f, 14.f, 290.f, 2000.f, 0.f));
		Table.Add(MakeDef(TEXT("Arc_DragonArrow"), TEXT("Ejder Oku"), C::Archer, T::Special0, 5, 24, 2, TEXT("Arc_EagleEye"),
			E::RangedDamage, 105.f, 13.f, 360.f, 1900.f, 0.f, TEXT("Slow")));
		Table.Add(MakeDef(TEXT("Arc_ShadowStep"), TEXT("Golge Adimi"), C::Archer, T::Special1, 2, 16, 2, TEXT("Arc_Sprint"),
			E::BuffSelf, 70.f, 18.f, 0.f, 0.f, 0.f, TEXT("HasteII")));
		// Lv24-33 bosluk dolgusu (2026-10-04): Special0 rutbe 6; PLACEHOLDER degerler.
		Table.Add(MakeDef(TEXT("Arc_DeathMark"), TEXT("Olum Isareti"), C::Archer, T::Special0, 6, 28, 2, TEXT("Arc_DragonArrow"),
			E::RangedDamage, 125.f, 15.f, 430.f, 2000.f, 0.f, TEXT("Poison")));
		Table.Add(MakeDef(TEXT("Arc_ThornVolley"), TEXT("Dikenli Sagnak"), C::Archer, T::Special0, 6, 31, 2, TEXT("Arc_Volley"),
			E::AoETarget, 125.f, 14.f, 330.f, 1700.f, 420.f, TEXT("Poison")));
		Table.Add(MakeDef(TEXT("Arc_WindsBreath"), TEXT("Ruzgarin Nefesi"), C::Archer, T::Special1, 3, 33, 2, TEXT("Arc_ShadowStep"),
			E::BuffSelf, 145.f, 24.f, 0.f, 0.f, 0.f, TEXT("HasteIII")));
		Table.Add(MakeDef(TEXT("Arc_Impaler"), TEXT("Mizrakli Ok"), C::Archer, T::Special0, 6, 39, 2, TEXT("Arc_DeathMark"),
			E::RangedDamage, 160.f, 16.f, 470.f, 2000.f, 0.f));
		// Civili Tuzak: alandaki hedeflere zehir birakan yeni okcu hatti.
		Table.Add(MakeDef(TEXT("Arc_SpikeTrap"), TEXT("Civili Tuzak"), C::Archer, T::Special2, 1, 45, 2, TEXT(""),
			E::AoETarget, 195.f, 18.f, 510.f, 1900.f, 480.f, TEXT("Poison")));
		Table.Add(MakeDef(TEXT("Arc_PoisonCloud"), TEXT("Zehir Bulutu"), C::Archer, T::Special2, 2, 58, 3, TEXT("Arc_SpikeTrap"),
			E::AoETarget, 250.f, 20.f, 660.f, 2000.f, 560.f, TEXT("Poison")));
		// Capstone 1/2: Ok Firtinasi.
		Table.Add(MakeDef(TEXT("Arc_ArrowTempest"), TEXT("Ok Firtinasi"), C::Archer, T::Special3, 1, 70, 3, TEXT(""),
			E::AoETarget, 320.f, 22.f, 860.f, 2100.f, 620.f));
		// Capstone 2/2: Avcinin Sonu (Okcunun nihai skilli - tek hedefe en agir atis).
		Table.Add(MakeDef(TEXT("Arc_HuntersReckoning"), TEXT("Avcinin Sonu"), C::Archer, T::Special3, 2, 80, 3, TEXT("Arc_ArrowTempest"),
			E::RangedDamage, 400.f, 22.f, 1600.f, 2200.f, 0.f));

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

		// Lv10-80 genisletmesi (KO tarzi uydurma icerik, bkz. commit mesaji): Special0 ates/yildirim
		// zincirinin devami, Special1 buz hattinin devami, Special2 yeni arkane hatti, Special3 =
		// gec oyun "ustalik sonu" agaci (iki capstone, Lv70 ve Lv80).
		// QA denetimi (2026-09-14, BULGU #4): bu skil bir onceki zincir halkasi olan
		// Mag_ChainLightning'den (Power 130, tek hedef) daha DUSUK Power (115) ve Mag_Meteor'dan
		// (Radius 420) daha kucuk Radius (400) tasiyordu - ters egri. AoE oldugu icin tek-hedef
		// ChainLightning'den daha yuksek olmasi gerekmiyordu ama zincirdeki bir onceki AoE (Meteor)
		// ve tek-hedef (ChainLightning) skillerin IKISINDEN de dusuk olmasi mantiksizdi; ikisinin de
		// uzerine cikaracak sekilde duzeltildi.
		Table.Add(MakeDef(TEXT("Mag_FlameStorm"), TEXT("Alev Firtinasi"), C::Mage, T::Special0, 4, 10, 2, TEXT("Mag_ChainLightning"),
			E::AoETarget, 45.f, 11.f, 155.f, 1800.f, 430.f));
		// Lv11-30 bosluk dolgusu (2026-10-04); PLACEHOLDER degerler.
		Table.Add(MakeDef(TEXT("Mag_IceLance"), TEXT("Buz Mizragi"), C::Mage, T::Special1, 2, 13, 2, TEXT("Mag_FrostNova"),
			E::RangedDamage, 58.f, 8.f, 195.f, 1800.f, 0.f, TEXT("Slow")));
		Table.Add(MakeDef(TEXT("Mag_ArcaneBlast"), TEXT("Gizemli Patlama"), C::Mage, T::Special0, 5, 20, 2, TEXT("Mag_FlameStorm"),
			E::AoETarget, 95.f, 13.f, 270.f, 1800.f, 450.f));
		Table.Add(MakeDef(TEXT("Mag_Thunderclap"), TEXT("Gok Gurlemesi"), C::Mage, T::Special0, 5, 24, 2, TEXT("Mag_FlameStorm"),
			E::RangedDamage, 105.f, 13.f, 380.f, 1900.f, 0.f, TEXT("Slow")));
		Table.Add(MakeDef(TEXT("Mag_IceChain"), TEXT("Buz Zinciri"), C::Mage, T::Special1, 2, 16, 2, TEXT("Mag_FrostNova"),
			E::RangedDamage, 70.f, 12.f, 245.f, 1800.f, 0.f, TEXT("Slow")));
		// Lv24-33 bosluk dolgusu (2026-10-04): Special0 rutbe 6; PLACEHOLDER degerler.
		Table.Add(MakeDef(TEXT("Mag_Disintegrate"), TEXT("Parcalama"), C::Mage, T::Special0, 6, 28, 2, TEXT("Mag_Thunderclap"),
			E::RangedDamage, 120.f, 12.f, 420.f, 1900.f, 0.f));
		Table.Add(MakeDef(TEXT("Mag_Inferno"), TEXT("Alev Patlamasi"), C::Mage, T::Special0, 6, 31, 2, TEXT("Mag_ArcaneBlast"),
			E::AoETarget, 130.f, 14.f, 360.f, 1900.f, 470.f));
		Table.Add(MakeDef(TEXT("Mag_FreezingTempest"), TEXT("Donduran Firtinasi"), C::Mage, T::Special1, 3, 33, 2, TEXT("Mag_IceChain"),
			E::AoESelf, 145.f, 16.f, 380.f, 0.f, 480.f, TEXT("Slow")));
		Table.Add(MakeDef(TEXT("Mag_Starfall"), TEXT("Yildiz Dusu"), C::Mage, T::Special0, 6, 39, 2, TEXT("Mag_Disintegrate"),
			E::RangedDamage, 170.f, 17.f, 540.f, 2000.f, 0.f));
		// Arcane Patlama: buyucunun yeni arkane hatti, elementsiz saf mana hasari.
		Table.Add(MakeDef(TEXT("Mag_ArcaneBurst"), TEXT("Arcane Patlama"), C::Mage, T::Special2, 1, 45, 2, TEXT(""),
			E::AoETarget, 195.f, 16.f, 520.f, 1900.f, 500.f));
		Table.Add(MakeDef(TEXT("Mag_ManaStorm"), TEXT("Mana Firtinasi"), C::Mage, T::Special2, 2, 58, 3, TEXT("Mag_ArcaneBurst"),
			E::AoETarget, 250.f, 18.f, 680.f, 2000.f, 560.f));
		// Capstone 1/2: Meteor Yagmuru.
		Table.Add(MakeDef(TEXT("Mag_MeteorShower"), TEXT("Meteor Yagmuru"), C::Mage, T::Special3, 1, 70, 3, TEXT(""),
			E::AoETarget, 320.f, 20.f, 900.f, 2000.f, 640.f));
		// Capstone 2/2: Mutlak Yikim (Buyucunun nihai skilli - oyundaki en yuksek alan hasari).
		Table.Add(MakeDef(TEXT("Mag_AbsoluteAnnihilation"), TEXT("Mutlak Yikim"), C::Mage, T::Special3, 2, 80, 3, TEXT("Mag_MeteorShower"),
			E::AoETarget, 420.f, 26.f, 1350.f, 2100.f, 700.f));

		// --- PRIEST ---
		Table.Add(MakeDef(TEXT("Pri_HolyLight"), TEXT("Kutsal Isik"), C::Priest, T::Basic, 1, 1, 1, TEXT(""),
			E::HealSelf, 15.f, 3.f, 60.f, 0.f, 0.f));
		// QA "ilk 10 dk": Rahip'in Lv1'de hic hasar skili yoktu (baslangic skili sifa) —
		// Ceza Lv1'e cekildi, GrantStarterSkill Kutsal Isik ile birlikte ikisini de verir.
		// (Rutbe 1 <= Lv1: mastery kurali korunuyor.)
		Table.Add(MakeDef(TEXT("Pri_Smite"), TEXT("Ceza"), C::Priest, T::Basic, 1, 1, 1, TEXT(""),
			E::RangedDamage, 10.f, 2.f, 35.f, 1200.f, 0.f));
		Table.Add(MakeDef(TEXT("Pri_HolyBurst"), TEXT("Kutsal Patlama"), C::Priest, T::Special0, 1, 4, 1, TEXT("Pri_Smite"),
			E::AoESelf, 25.f, 8.f, 48.f, 0.f, 320.f));
		// Buyuk Sifa: secili bir parti uyesi hedefteyse ONU iyilestirir (Priest'in asil
		// "sifaci" araci); hedef yoksa/kendine tikladiysa eskisi gibi kendini iyilestirir.
		Table.Add(MakeDef(TEXT("Pri_GreaterHeal"), TEXT("Buyuk Sifa"), C::Priest, T::Special1, 1, 5, 2, TEXT("Pri_HolyLight"),
			E::HealTarget, 40.f, 12.f, 150.f, 0.f, 0.f));
		// Ilahi Sifa: Buyuk Sifa'nin ustu; aninda buyuk iyilestirme + uzerine Yenilenme HoT.
		// QA gozlemi (2026-09-14): zincirdeki diger 3 sifa skili (GreaterHeal/HolyRain/LifeBond)
		// hepsi HealTarget iken bu HealSelf birakilmisti - secili muttefigi degil, hep kendini
		// iyilestiriyordu. HealTarget'a alindi (hedef yoksa/kendine tiklandiysa zaten kendini
		// iyilestirmeye devam eder, bkz. Pri_GreaterHeal yorumu) - Priest'in "sifaci" kimligiyle tutarli.
		Table.Add(MakeDef(TEXT("Pri_DivineRestoration"), TEXT("Ilahi Sifa"), C::Priest, T::Special1, 2, 7, 2, TEXT("Pri_GreaterHeal"),
			E::HealTarget, 55.f, 20.f, 180.f, 0.f, 0.f, TEXT("Regen")));
		// Buff: zamanla iyilesme (Yenilenme HoT).
		Table.Add(MakeDef(TEXT("Pri_Renew"), TEXT("Yenilenme"), C::Priest, T::Special0, 1, 3, 1, TEXT(""),
			E::BuffSelf, 18.f, 12.f, 0.f, 0.f, 0.f, TEXT("Regen")));

		// Lv10-80 genisletmesi (KO tarzi uydurma icerik, bkz. commit mesaji): Special0 kutsal hasar
		// hattinin devami, Special1 sifa zincirinin devami, Special2 yeni koruma hatti, Special3 =
		// gec oyun "ustalik sonu" agaci (iki capstone, Lv70 ve Lv80 - biri hasar biri sifa, priestin
		// hem savasci hem sifaci kimligini yansitir).
		Table.Add(MakeDef(TEXT("Pri_HolyFlame"), TEXT("Kutsal Alev"), C::Priest, T::Special0, 2, 10, 2, TEXT("Pri_HolyBurst"),
			E::RangedDamage, 45.f, 10.f, 150.f, 1500.f, 0.f));
		// Lv11-30 bosluk dolgusu (2026-10-04); PLACEHOLDER degerler. Mending: HolyRain (380) ile LifeBond (750) arasi sifa.
		Table.Add(MakeDef(TEXT("Pri_Judgement"), TEXT("Ilahi Hukum"), C::Priest, T::Special0, 2, 13, 2, TEXT("Pri_HolyFlame"),
			E::RangedDamage, 58.f, 9.f, 195.f, 1500.f, 0.f));
		Table.Add(MakeDef(TEXT("Pri_Mending"), TEXT("Kutsal Onarim"), C::Priest, T::Special1, 3, 20, 2, TEXT("Pri_HolyRain"),
			E::HealTarget, 120.f, 14.f, 520.f, 0.f, 0.f));
		Table.Add(MakeDef(TEXT("Pri_WrathOfAngels"), TEXT("Meleklerin Ofkesi"), C::Priest, T::Special0, 3, 24, 2, TEXT("Pri_HolyFlame"),
			E::AoETarget, 105.f, 13.f, 290.f, 1500.f, 420.f));
		// QA BULGU #6 duzeltmesi (2026-09-15): Warrior ile ayni sorun Priest'te de vardi -
		// Special0 kutsal hasar zinciri Lv24'te (Pri_WrathOfAngels) tikaniyordu, Lv70 capstone'a
		// kadar yeni hasar skili gelmiyordu (Special2'deki Lv45/58 eklemeleri buff). Zincir
		// Lv45 ve Lv58'de iki halka daha uzatildi, Power/Mana/CD Archer'in
		// Arc_SpikeTrap/PoisonCloud ve Mage'in Mag_ArcaneBurst/ManaStorm ile (ayni seviyedeki
		// hasar skilleri) AYNI olcekte tutuldu (Power ~510 @Lv45, ~660 @Lv58; Mana 195/250;
		// CD Archer'inkiyle birebir 18/20). Meleklerin Ofkesi'nden (rutbe 3) devam eden zincir.
		Table.Add(MakeDef(TEXT("Pri_AngelicBlade"), TEXT("Meleklerin Kilici"), C::Priest, T::Special0, 4, 45, 2, TEXT("Pri_WrathOfAngels"),
			E::RangedDamage, 195.f, 18.f, 510.f, 1500.f, 0.f));
		Table.Add(MakeDef(TEXT("Pri_AngelicStorm"), TEXT("Meleklerin Firtinasi"), C::Priest, T::Special0, 5, 58, 3, TEXT("Pri_AngelicBlade"),
			E::AoETarget, 250.f, 20.f, 660.f, 1600.f, 560.f));
		// Kutsal Yagmur: Buyuk Sifa'nin devami, secili muttefigi iyilestirir (yoksa kendini).
		Table.Add(MakeDef(TEXT("Pri_HolyRain"), TEXT("Kutsal Yagmur"), C::Priest, T::Special1, 3, 16, 2, TEXT("Pri_DivineRestoration"),
			E::HealTarget, 110.f, 16.f, 380.f, 0.f, 0.f));
		// Lv24-33 bosluk dolgusu (2026-10-04): Special0 rutbe 4; PLACEHOLDER degerler.
		Table.Add(MakeDef(TEXT("Pri_Retribution"), TEXT("Karsilik"), C::Priest, T::Special0, 4, 28, 2, TEXT("Pri_WrathOfAngels"),
			E::RangedDamage, 105.f, 11.f, 330.f, 1500.f, 0.f));
		Table.Add(MakeDef(TEXT("Pri_RadiantNova"), TEXT("Isiltili Nova"), C::Priest, T::Special0, 4, 31, 2, TEXT("Pri_WrathOfAngels"),
			E::AoETarget, 135.f, 14.f, 340.f, 1500.f, 440.f));
		Table.Add(MakeDef(TEXT("Pri_LifeBond"), TEXT("Yasam Bagi"), C::Priest, T::Special1, 4, 33, 3, TEXT("Pri_HolyRain"),
			E::HealTarget, 210.f, 20.f, 750.f, 0.f, 0.f, TEXT("Regen")));
		Table.Add(MakeDef(TEXT("Pri_SalvationLight"), TEXT("Kurtulus Isigi"), C::Priest, T::Special1, 4, 39, 3, TEXT("Pri_LifeBond"),
			E::HealTarget, 260.f, 30.f, 1100.f, 0.f, 0.f));
		// Kutsal Kalkan: Priestin yeni koruma hatti (Special2, ilk defa kullaniliyor).
		Table.Add(MakeDef(TEXT("Pri_HolyShield"), TEXT("Kutsal Kalkan"), C::Priest, T::Special2, 1, 45, 2, TEXT(""),
			E::BuffSelf, 120.f, 22.f, 0.f, 0.f, 0.f, TEXT("IronSkinII")));
		Table.Add(MakeDef(TEXT("Pri_ResiliencePrayer"), TEXT("Direnc Duasi"), C::Priest, T::Special2, 2, 58, 3, TEXT("Pri_HolyShield"),
			E::BuffSelf, 150.f, 24.f, 0.f, 0.f, 0.f, TEXT("RegenII")));
		// Capstone 1/2: Tanrisal Yargi (saldiri).
		Table.Add(MakeDef(TEXT("Pri_DivineJudgment"), TEXT("Tanrisal Yargi"), C::Priest, T::Special3, 1, 70, 3, TEXT(""),
			E::AoETarget, 320.f, 22.f, 950.f, 1600.f, 600.f));
		// Capstone 2/2: Cennetin Eli (Priestin nihai skilli - oyundaki en buyuk tekil sifa).
		Table.Add(MakeDef(TEXT("Pri_HandOfHeaven"), TEXT("Cennetin Eli"), C::Priest, T::Special3, 2, 80, 3, TEXT("Pri_DivineJudgment"),
			E::HealTarget, 420.f, 24.f, 1650.f, 0.f, 0.f, TEXT("Regen")));

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

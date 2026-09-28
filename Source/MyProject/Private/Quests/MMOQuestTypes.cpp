// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quests/MMOQuestTypes.h"

namespace
{
	FMMOQuestDef MakeQuest(const TCHAR* Id, const TCHAR* Name, const TCHAR* Desc, EMMOQuestType Type,
		const TCHAR* TargetMob, int32 Count, int32 RewardXP, int32 RewardGold, int32 ReqLevel,
		const TCHAR* Prereq = TEXT(""))
	{
		FMMOQuestDef Q;
		Q.QuestId = FName(Id);
		Q.Name = Name;
		Q.Description = Desc;
		Q.Type = Type;
		Q.TargetMob = (TargetMob && *TargetMob) ? FName(TargetMob) : NAME_None;
		Q.TargetCount = Count;
		Q.RewardXP = RewardXP;
		Q.RewardGold = RewardGold;
		Q.RequiredLevel = ReqLevel;
		Q.PrereqQuestId = (Prereq && *Prereq) ? FName(Prereq) : NAME_None;
		return Q;
	}

	const TArray<FMMOQuestDef>& BuildQuestTable()
	{
		static TArray<FMMOQuestDef> Table;
		if (Table.Num() > 0)
		{
			return Table;
		}

		using T = EMMOQuestType;

		// --- Ana gorev zinciri (her biri bir oncekini gerektirir) ---
		Table.Add(MakeQuest(TEXT("Q_Goblins"), TEXT("Goblin Avi"), TEXT("Koydeki goblinleri temizle: 5 Goblin oldur."),
			T::KillSpecificMob, TEXT("Goblin"), 5, 200, 120, 1));
		Table.Last().RewardConsumableId = FName(TEXT("HealthPotion"));
		Table.Last().RewardConsumableCount = 3;

		Table.Add(MakeQuest(TEXT("Q_Wolves"), TEXT("Kurt Surusu"), TEXT("Yollari kesen kurtlari avla: 5 Kurt oldur."),
			T::KillSpecificMob, TEXT("WolfPup"), 5, 300, 160, 2, TEXT("Q_Goblins")));
		Table.Last().RewardConsumableId = FName(TEXT("ManaPotion"));
		Table.Last().RewardConsumableCount = 3;

		Table.Add(MakeQuest(TEXT("Q_Orcs"), TEXT("Ork Tehdidi"), TEXT("Guclu orklari avla: 3 Ork oldur."),
			T::KillSpecificMob, TEXT("Orc"), 3, 500, 300, 3, TEXT("Q_Wolves")));

		Table.Add(MakeQuest(TEXT("Q_Bandits"), TEXT("Haydutlari Temizle"), TEXT("Ticaret yolunu ac: 4 Haydut oldur."),
			T::KillSpecificMob, TEXT("Bandit"), 4, 650, 400, 4, TEXT("Q_Orcs")));

		Table.Add(MakeQuest(TEXT("Q_Boss"), TEXT("Ork Reisini Yen"), TEXT("Bolgenin dehsetini bitir: Ork Reisi'ni oldur."),
			T::KillSpecificMob, TEXT("OrcWarlord"), 1, 1500, 1000, 5, TEXT("Q_Bandits")));
		Table.Last().RewardEquipmentId = FName(TEXT("GreatSword"));

		// --- Ucuncu bolge zinciri (Lv8-15): Q_Boss'tan SONRA baslar ---
		// Q_Boss (1500XP/1000g) oyuncuyu ~Lv15-16'da birakip icerigi bitiriyordu; bu zincir
		// Lv8-13 mob'lariyla o boslugun ilk kademesini doldurur. Oduller Q_Boss'un uzerine
		// kademeli cikar (1800 -> 4500 XP). NOT: Lv16-80 hala bos, bilerek boyle.
		Table.Add(MakeQuest(TEXT("Q_FrostWraiths"), TEXT("Buz Hayaletleri"), TEXT("Ucuncu bolgenin buzul sisini dagit: 5 Buz Hayaleti oldur."),
			T::KillSpecificMob, TEXT("FrostWraith"), 5, 1800, 1200, 8, TEXT("Q_Boss")));
		Table.Last().RewardConsumableId = FName(TEXT("GreaterHealthPotion"));
		Table.Last().RewardConsumableCount = 3;

		Table.Add(MakeQuest(TEXT("Q_CursedKnights"), TEXT("Lanetli Sovalyeler"), TEXT("Harabelerin lanetli muhafizlarini devir: 5 Lanetli Sovalye oldur."),
			T::KillSpecificMob, TEXT("CursedKnight"), 5, 2200, 1450, 9, TEXT("Q_FrostWraiths")));
		Table.Last().RewardEquipmentId = FName(TEXT("MithrilBoots"));

		Table.Add(MakeQuest(TEXT("Q_VenomSpiders"), TEXT("Zehir Orumcekleri"), TEXT("Yuvayi zehirden temizle: 6 Zehir Orumcegi oldur."),
			T::KillSpecificMob, TEXT("VenomSpider"), 6, 2700, 1750, 10, TEXT("Q_CursedKnights")));
		Table.Last().RewardEquipmentId = FName(TEXT("MithrilHelm"));

		Table.Add(MakeQuest(TEXT("Q_OrcBerserkers"), TEXT("Ork Berserkleri"), TEXT("Kuzey kampindaki cilgin orklari kir: 5 Ork Berserk oldur."),
			T::KillSpecificMob, TEXT("OrcBerserker"), 5, 3400, 2200, 12, TEXT("Q_VenomSpiders")));
		Table.Last().RewardEquipmentId = FName(TEXT("MithrilPlate"));

		// Zincirin sonu: ucuncu bolge eliti (Lv13).
		Table.Add(MakeQuest(TEXT("Q_FrostLich"), TEXT("Buz Lordu"), TEXT("Buzul tahtindaki lordu devir: Buz Lordu'nu oldur."),
			T::KillSpecificMob, TEXT("FrostLich"), 1, 4500, 3000, 13, TEXT("Q_OrcBerserkers")));
		Table.Last().RewardEquipmentId = FName(TEXT("MithrilBlade"));

		// --- Dorduncu bolge zinciri (Lv16-24, Alev Vadisi): Q_FrostLich'ten SONRA baslar ---
		// Q_FrostLich (4500XP/3000g) zincirin ucuncu kademesini bitiriyordu; bu zincir Lv16-22
		// mob'lari + Lv24 eliti ile bir sonraki kademeyi acar. Oduller Q_FrostLich'in uzerine
		// kademeli cikar (5400 -> 12000 XP, 3500 -> 8000 altin) ve Adamantin tier'ini (Lv16)
		// dagitir. NOT: Lv31-80 hala bos, bilerek boyle — kademeli ilerliyoruz.
		Table.Add(MakeQuest(TEXT("Q_EmberHounds"), TEXT("Kor Tazilari"), TEXT("Alev Vadisi'nin gecidini ac: 5 Kor Tazisi oldur."),
			T::KillSpecificMob, TEXT("EmberHound"), 5, 5400, 3500, 16, TEXT("Q_FrostLich")));
		Table.Last().RewardConsumableId = FName(TEXT("GreaterHealthPotion"));
		Table.Last().RewardConsumableCount = 4;

		Table.Add(MakeQuest(TEXT("Q_MagmaGolems"), TEXT("Magma Golemleri"), TEXT("Lav golunun bekcilerini kir: 5 Magma Golemi oldur."),
			T::KillSpecificMob, TEXT("MagmaGolem"), 5, 6500, 4200, 18, TEXT("Q_EmberHounds")));
		Table.Last().RewardEquipmentId = FName(TEXT("AdamantineBoots"));

		Table.Add(MakeQuest(TEXT("Q_AshOrcs"), TEXT("Kul Orklari"), TEXT("Kul ovasindaki ork akincilarini dagit: 6 Kul Orku oldur."),
			T::KillSpecificMob, TEXT("AshOrc"), 6, 7800, 5000, 20, TEXT("Q_MagmaGolems")));
		Table.Last().RewardEquipmentId = FName(TEXT("AdamantineHelm"));

		Table.Add(MakeQuest(TEXT("Q_InfernalWraiths"), TEXT("Cehennem Hayaletleri"), TEXT("Vadinin dibindeki hayaletleri kov: 5 Cehennem Hayaleti oldur."),
			T::KillSpecificMob, TEXT("InfernalWraith"), 5, 9400, 6000, 22, TEXT("Q_AshOrcs")));
		Table.Last().RewardEquipmentId = FName(TEXT("AdamantinePlate"));

		// Zincirin sonu: dorduncu bolge eliti (Lv24).
		Table.Add(MakeQuest(TEXT("Q_MagmaTyrant"), TEXT("Magma Tirani"), TEXT("Alev Vadisi'nin hukumdarini devir: Magma Tirani'ni oldur."),
			T::KillSpecificMob, TEXT("MagmaTyrant"), 1, 12000, 8000, 24, TEXT("Q_InfernalWraiths")));
		Table.Last().RewardEquipmentId = FName(TEXT("AdamantineBlade"));

		// --- Besinci bolge zinciri (Lv25-30): Q_MagmaTyrant'tan sonra ---
		Table.Add(MakeQuest(TEXT("Q_ObsidianStalkers"), TEXT("Obsidyen Avcilari"), TEXT("Obsidyen avcilarini durdur: 5 Obsidyen Avcisi oldur."),
			T::KillSpecificMob, TEXT("ObsidianStalker"), 5, 9000, 9000, 25, TEXT("Q_MagmaTyrant")));
		Table.Last().RewardEquipmentId = FName(TEXT("ObsidianBoots"));

		Table.Add(MakeQuest(TEXT("Q_ObsidianSentinels"), TEXT("Obsidyen Muhafizlari"), TEXT("Gecidi koruyan muhafizlari devir: 5 Obsidyen Muhafizi oldur."),
			T::KillSpecificMob, TEXT("ObsidianSentinel"), 5, 11000, 11000, 27, TEXT("Q_ObsidianStalkers")));
		Table.Last().RewardEquipmentId = FName(TEXT("ObsidianHelm"));

		Table.Add(MakeQuest(TEXT("Q_ObsidianRaiders"), TEXT("Obsidyen Akincilari"), TEXT("Obsidyen akincilarini dagit: 5 Obsidyen Akincisi oldur."),
			T::KillSpecificMob, TEXT("ObsidianRaider"), 5, 13000, 13000, 29, TEXT("Q_ObsidianSentinels")));
		Table.Last().RewardEquipmentId = FName(TEXT("ObsidianPlate"));

		Table.Add(MakeQuest(TEXT("Q_ObsidianWarlord"), TEXT("Obsidyen Reisi"), TEXT("Obsidyen ordusunun reisini devir: Obsidyen Reisi'ni oldur."),
			T::KillSpecificMob, TEXT("ObsidianWarlord"), 1, 17000, 18000, 30, TEXT("Q_ObsidianRaiders")));
		Table.Last().RewardEquipmentId = FName(TEXT("ObsidianBlade"));

		// --- Bagimsiz (yan) gorevler ---
		Table.Add(MakeQuest(TEXT("Q_Cleanup"), TEXT("Temizlik"), TEXT("Bolgedeki tehlikeyi azalt: 10 canavar oldur."),
			T::KillAnyMob, TEXT(""), 10, 350, 200, 1));

		Table.Add(MakeQuest(TEXT("Q_Level5"), TEXT("Guclen"), TEXT("Kendini gelistir: 5. seviyeye ulas."),
			T::ReachLevel, TEXT(""), 5, 400, 250, 1));
		Table.Last().RewardConsumableId = FName(TEXT("GreaterManaPotion"));
		Table.Last().RewardConsumableCount = 2;

		Table.Add(MakeQuest(TEXT("Q_Gold500"), TEXT("Tuccar Ruhu"), TEXT("500 altin biriktir."),
			T::HaveGold, TEXT(""), 500, 300, 0, 1));

		// Elit yan gorev: Orumcek Ana (bagimsiz — kurt surusunu bitirenlere acilir).
		Table.Add(MakeQuest(TEXT("Q_SpiderQueen"), TEXT("Orumcek Ana"), TEXT("Magaranin dehsetini yok et: Orumcek Ana'yi oldur."),
			T::KillSpecificMob, TEXT("SpiderQueen"), 1, 900, 600, 5, TEXT("Q_Wolves")));
		Table.Last().RewardEquipmentId = FName(TEXT("PlateArmor"));

		// Kadim Golem (elit zinciri: Orumcek Ana'yi bitirenlere acilir).
		Table.Add(MakeQuest(TEXT("Q_StoneGolem"), TEXT("Kadim Golem"), TEXT("Harabelerin bekcisini devir: Kadim Golem'i oldur."),
			T::KillSpecificMob, TEXT("StoneGolem"), 1, 1000, 700, 6, TEXT("Q_SpiderQueen")));
		Table.Last().RewardEquipmentId = FName(TEXT("DragonBlade"));

		// Golge Suikastci (elit zinciri: Kadim Golem'den sonra acilir).
		Table.Add(MakeQuest(TEXT("Q_ShadowAssassin"), TEXT("Golge Suikastci"), TEXT("Golgelerdeki katili avla: Golge Suikastci'yi oldur."),
			T::KillSpecificMob, TEXT("ShadowAssassin"), 1, 1100, 750, 7, TEXT("Q_StoneGolem")));
		Table.Last().RewardEquipmentId = FName(TEXT("DragonHelm"));

		// Trol avi (bagimsiz yan gorev).
		Table.Add(MakeQuest(TEXT("Q_Trolls"), TEXT("Trol Avi"), TEXT("Ormandaki trolleri sec: 3 Trol oldur."),
			T::KillSpecificMob, TEXT("Troll"), 3, 550, 380, 5));

		// Ikinci bolge yan gorevi: Kara Buyuculeri sec (Lv7 normal; band spawner'dan cikar).
		// Altin odulu ekonomi denge raporunda (2026-09-14) bulunan bir tutarsizlik yuzunden
		// 500->650 cekildi: erken zincir gorevleri (Goblins/Wolves/Orcs) mob-grinding'e gore
		// 3.2x-5.3x altin verirken bu ve asagidaki 2 yan gorev sadece 1.5x-2.0x veriyordu -
		// zorluk arttikca odul-carpaninin DUSMESI ters bir tesvikti. Tam 3x'e degil, orta bir
		// yukseltmeye gidildi (genel ekonomi zaten "altin cabuk anlamsizlasiyor" bulgusuna
		// sahip - fazladan altin enjekte etmek yerine sadece tersine tutarsizligi kapatildi).
		Table.Add(MakeQuest(TEXT("Q_Warlocks"), TEXT("Kara Buyuculer"), TEXT("Ikinci bolgedeki kara buyuculeri devir: 4 Kara Buyucu oldur."),
			T::KillSpecificMob, TEXT("Warlock"), 4, 800, 650, 6));
		Table.Last().RewardEquipmentId = FName(TEXT("BattleAxe"));

		// Ikinci bolge yan gorevi: Ork Savascilarini sec (Lv6 normal; band spawner'dan cikar).
		// Q_Warlocks ile ayni bagimsiz-yan-gorev deseni; SteelHelmet o zamana kadar hicbir
		// gorevde odul olarak kullanilmamisti (sadece elit loot/dukkanda vardi). Altin
		// odulu 480->620 (bkz. Q_Warlocks yorumu).
		Table.Add(MakeQuest(TEXT("Q_OrcWarriors"), TEXT("Ork Savascilari"), TEXT("Ikinci bolgedeki ork savascilarini devir: 4 Ork Savascisi oldur."),
			T::KillSpecificMob, TEXT("OrcWarrior"), 4, 750, 620, 6));
		Table.Last().RewardEquipmentId = FName(TEXT("SteelHelmet"));

		// Ikinci bolge yan gorevi: Korku Kurtlarini sec (Lv6 normal; band spawner'dan cikar).
		// SteelBoots de ayni sekilde hicbir gorevde odul olarak kullanilmamisti. Altin odulu
		// 480->600 (bkz. Q_Warlocks yorumu).
		Table.Add(MakeQuest(TEXT("Q_DireWolves"), TEXT("Korku Kurtlari"), TEXT("Ikinci bolgedeki korku kurtlarini devir: 4 Korku Kurdu oldur."),
			T::KillSpecificMob, TEXT("DireWolf"), 4, 750, 600, 6));
		Table.Last().RewardEquipmentId = FName(TEXT("SteelBoots"));

		// Buyuk servet (bagimsiz altin biriktirme).
		Table.Add(MakeQuest(TEXT("Q_Gold1000"), TEXT("Buyuk Servet"), TEXT("1000 altin biriktir."),
			T::HaveGold, TEXT(""), 1000, 500, 0, 3));

		// --- Lv16-40 kilometre tasi gorevleri (SPAWNER/MOB GEREKTIRMEZ) ---
		// Bu blok bilerek sadece ReachLevel/HaveGold tipinde: ikisi de oyuncunun mevcut
		// durumunu (seviye/altin) anlik kontrol eder, hicbir yeni mob arketipi veya
		// spawner yerlesimi istemez. Son iki tier'de eklenen elit spawner'lar
		// PlayerStart'tan 4200-5600 birim uzaga konuldu ve o mesafede gercek zemin/navmesh
		// olup olmadigi henuz PIE'de dogrulanmadi; icerigi bu riski buyutmeden derinlestirmek
		// icin Lv16-40 araligi once mob gerektirmeyen gorevlerle dolduruluyor.
		// Oduller mevcut tavanin (Q_MagmaTyrant: 12000XP/8000g, Lv24) etrafinda kademeli
		// ilerler: Lv20 milestone'u tavanin altinda, Lv30/Lv40 uzerinde kalir.
		// Altin gorevleri mevcut desene uyup altin DEGIL sadece XP oduller (bkz. Q_Gold500/1000).

		// Seviye kilometre taslari zinciri: Q_Level5 -> Q_Level20 -> Q_Level30 -> Q_Level40.
		Table.Add(MakeQuest(TEXT("Q_Level20"), TEXT("Kidemli Savasci"), TEXT("Deneyimini kanitla: 20. seviyeye ulas."),
			T::ReachLevel, TEXT(""), 20, 9000, 5500, 16, TEXT("Q_Level5")));
		Table.Last().RewardConsumableId = FName(TEXT("GreaterHealthPotion"));
		Table.Last().RewardConsumableCount = 5;

		Table.Add(MakeQuest(TEXT("Q_Level30"), TEXT("Usta Savasci"), TEXT("Ustaligini taclandir: 30. seviyeye ulas."),
			T::ReachLevel, TEXT(""), 30, 16000, 10000, 24, TEXT("Q_Level20")));
		Table.Last().RewardConsumableId = FName(TEXT("GreaterManaPotion"));
		Table.Last().RewardConsumableCount = 5;

		Table.Add(MakeQuest(TEXT("Q_Level40"), TEXT("Efsanevi Sovalye"), TEXT("Adini efsanelere yaz: 40. seviyeye ulas."),
			T::ReachLevel, TEXT(""), 40, 24000, 15000, 30, TEXT("Q_Level30")));
		Table.Last().RewardConsumableId = FName(TEXT("GreaterHealthPotion"));
		Table.Last().RewardConsumableCount = 8;

		// Altin biriktirme zinciri devami: Q_Gold1000 -> Q_Gold5000 -> Q_Gold15000.
		Table.Add(MakeQuest(TEXT("Q_Gold5000"), TEXT("Vadinin Tuccari"), TEXT("5000 altin biriktir."),
			T::HaveGold, TEXT(""), 5000, 6000, 0, 16, TEXT("Q_Gold1000")));

		Table.Add(MakeQuest(TEXT("Q_Gold15000"), TEXT("Efsanevi Servet"), TEXT("15000 altin biriktir."),
			T::HaveGold, TEXT(""), 15000, 14000, 0, 28, TEXT("Q_Gold5000")));

		// 2026-09-15: kademeli icerik genisletmesinin devami - spawner/mob GEREKTIRMEYEN
		// (harita zaten dolu, bkz. MMOGameMode.cpp'deki kritik spawner duzeltmesi) ilerleme
		// gorevleri. Seviye zinciri artik gercek MaxLevel'a (80) kadar uzaniyor - Q_Level80
		// oyuncunun karakterini tamamen doldurdugunda alacagi bir "bitis cizgisi" gorevi.
		Table.Add(MakeQuest(TEXT("Q_Level50"), TEXT("Savas Ustasi"), TEXT("Gucunu pekistir: 50. seviyeye ulas."),
			T::ReachLevel, TEXT(""), 50, 35000, 20000, 40, TEXT("Q_Level40")));

		Table.Add(MakeQuest(TEXT("Q_Level65"), TEXT("Kahraman"), TEXT("Kahramanlik cagina gir: 65. seviyeye ulas."),
			T::ReachLevel, TEXT(""), 65, 55000, 30000, 50, TEXT("Q_Level50")));

		Table.Add(MakeQuest(TEXT("Q_Level80"), TEXT("Zirve"), TEXT("Yolculugunun zirvesine ulas: 80. seviyeye ulas."),
			T::ReachLevel, TEXT(""), 80, 90000, 50000, 65, TEXT("Q_Level65")));

		// Altin biriktirme zinciri devami: Q_Gold15000 -> Q_Gold30000 -> Q_Gold50000.
		Table.Add(MakeQuest(TEXT("Q_Gold30000"), TEXT("Bolge Tuccari"), TEXT("30000 altin biriktir."),
			T::HaveGold, TEXT(""), 30000, 28000, 0, 28, TEXT("Q_Gold15000")));

		Table.Add(MakeQuest(TEXT("Q_Gold50000"), TEXT("Altin Cagin Efendisi"), TEXT("50000 altin biriktir."),
			T::HaveGold, TEXT(""), 50000, 45000, 0, 40, TEXT("Q_Gold30000")));

		return Table;
	}
}

const TArray<FMMOQuestDef>& FMMOQuestDatabase::GetAll()
{
	return BuildQuestTable();
}

const FMMOQuestDef* FMMOQuestDatabase::Find(FName QuestId)
{
	if (QuestId.IsNone())
	{
		return nullptr;
	}
	for (const FMMOQuestDef& Q : GetAll())
	{
		if (Q.QuestId == QuestId)
		{
			return &Q;
		}
	}
	return nullptr;
}

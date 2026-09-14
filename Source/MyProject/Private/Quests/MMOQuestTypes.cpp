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

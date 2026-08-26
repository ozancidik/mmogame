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

		Table.Add(MakeQuest(TEXT("Q_Wolves"), TEXT("Kurt Surusu"), TEXT("Yollari kesen kurtlari avla: 5 Kurt oldur."),
			T::KillSpecificMob, TEXT("WolfPup"), 5, 300, 160, 2, TEXT("Q_Goblins")));

		Table.Add(MakeQuest(TEXT("Q_Orcs"), TEXT("Ork Tehdidi"), TEXT("Guclu orklari avla: 3 Ork oldur."),
			T::KillSpecificMob, TEXT("Orc"), 3, 500, 300, 3, TEXT("Q_Wolves")));

		Table.Add(MakeQuest(TEXT("Q_Bandits"), TEXT("Haydutlari Temizle"), TEXT("Ticaret yolunu ac: 4 Haydut oldur."),
			T::KillSpecificMob, TEXT("Bandit"), 4, 650, 400, 4, TEXT("Q_Orcs")));

		Table.Add(MakeQuest(TEXT("Q_Boss"), TEXT("Ork Reisini Yen"), TEXT("Bolgenin dehsetini bitir: Ork Reisi'ni oldur."),
			T::KillSpecificMob, TEXT("OrcWarlord"), 1, 1500, 1000, 5, TEXT("Q_Bandits")));

		// --- Bagimsiz (yan) gorevler ---
		Table.Add(MakeQuest(TEXT("Q_Cleanup"), TEXT("Temizlik"), TEXT("Bolgedeki tehlikeyi azalt: 10 canavar oldur."),
			T::KillAnyMob, TEXT(""), 10, 350, 200, 1));

		Table.Add(MakeQuest(TEXT("Q_Level5"), TEXT("Guclen"), TEXT("Kendini gelistir: 5. seviyeye ulas."),
			T::ReachLevel, TEXT(""), 5, 400, 250, 1));

		Table.Add(MakeQuest(TEXT("Q_Gold500"), TEXT("Tuccar Ruhu"), TEXT("500 altin biriktir."),
			T::HaveGold, TEXT(""), 500, 300, 0, 1));

		// Elit yan gorev: Orumcek Ana (bagimsiz — kurt surusunu bitirenlere acilir).
		Table.Add(MakeQuest(TEXT("Q_SpiderQueen"), TEXT("Orumcek Ana"), TEXT("Magaranin dehsetini yok et: Orumcek Ana'yi oldur."),
			T::KillSpecificMob, TEXT("SpiderQueen"), 1, 900, 600, 5, TEXT("Q_Wolves")));

		// Kadim Golem (elit zinciri: Orumcek Ana'yi bitirenlere acilir).
		Table.Add(MakeQuest(TEXT("Q_StoneGolem"), TEXT("Kadim Golem"), TEXT("Harabelerin bekcisini devir: Kadim Golem'i oldur."),
			T::KillSpecificMob, TEXT("StoneGolem"), 1, 1000, 700, 6, TEXT("Q_SpiderQueen")));

		// Golge Suikastci (elit zinciri: Kadim Golem'den sonra acilir).
		Table.Add(MakeQuest(TEXT("Q_ShadowAssassin"), TEXT("Golge Suikastci"), TEXT("Golgelerdeki katili avla: Golge Suikastci'yi oldur."),
			T::KillSpecificMob, TEXT("ShadowAssassin"), 1, 1100, 750, 7, TEXT("Q_StoneGolem")));

		// Trol avi (bagimsiz yan gorev).
		Table.Add(MakeQuest(TEXT("Q_Trolls"), TEXT("Trol Avi"), TEXT("Ormandaki trolleri sec: 3 Trol oldur."),
			T::KillSpecificMob, TEXT("Troll"), 3, 550, 380, 5));

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

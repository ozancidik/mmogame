// Copyright Epic Games, Inc. All Rights Reserved.

#include "Achievements/MMOAchievementTypes.h"

namespace
{
	using K = EMMOAchievementKind;

	FMMOAchievementDef MakeAch(const TCHAR* Id, const TCHAR* Name, const TCHAR* Desc, K Kind, const TCHAR* TargetId,
		int32 Target, int32 RewardGold, const TCHAR* Title)
	{
		FMMOAchievementDef D;
		D.Id = FName(Id);
		D.Name = Name;
		D.Description = Desc;
		D.Kind = Kind;
		D.TargetId = (TargetId && *TargetId) ? FName(TargetId) : NAME_None;
		D.Target = Target;
		D.RewardGold = RewardGold;
		D.Title = Title;
		return D;
	}

	// Oduller bilerek kucuk (tum tablo toplami ~65.000 altin; Lv58+ bandinda tek seviyenin geliri milyonlarda).
	// XP odulu YOK: ilerleme egrisini (MMOXPTuning, ProgressionModel) bozmasin. Asil odul unvan.
	const TArray<FMMOAchievementDef>& BuildTable()
	{
		static TArray<FMMOAchievementDef> Table;
		if (Table.Num() > 0)
		{
			return Table;
		}

		// --- Toplam oldurme ---
		Table.Add(MakeAch(TEXT("Ach_Kill10"),    TEXT("Ilk Kan"),            TEXT("10 canavar oldur."),       K::KillTotal, TEXT(""), 10,    50,    TEXT("Acemi")));
		Table.Add(MakeAch(TEXT("Ach_Kill100"),   TEXT("Avci"),               TEXT("100 canavar oldur."),      K::KillTotal, TEXT(""), 100,   200,   TEXT("Avci")));
		Table.Add(MakeAch(TEXT("Ach_Kill500"),   TEXT("Kasap"),              TEXT("500 canavar oldur."),      K::KillTotal, TEXT(""), 500,   800,   TEXT("Kasap")));
		Table.Add(MakeAch(TEXT("Ach_Kill2000"),  TEXT("Savas Makinesi"),     TEXT("2000 canavar oldur."),     K::KillTotal, TEXT(""), 2000,  3000,  TEXT("Savas Makinesi")));
		Table.Add(MakeAch(TEXT("Ach_Kill10000"), TEXT("Olum Melegi"),        TEXT("10000 canavar oldur."),    K::KillTotal, TEXT(""), 10000, 12000, TEXT("Olum Melegi")));

		// --- Seviye ---
		Table.Add(MakeAch(TEXT("Ach_Lv10"), TEXT("Yolun Basi"),       TEXT("Seviye 10'a ulas."), K::ReachLevel, TEXT(""), 10, 100,   TEXT("Yolcu")));
		Table.Add(MakeAch(TEXT("Ach_Lv20"), TEXT("Deneyimli"),        TEXT("Seviye 20'ye ulas."), K::ReachLevel, TEXT(""), 20, 300,   TEXT("Deneyimli")));
		Table.Add(MakeAch(TEXT("Ach_Lv30"), TEXT("Kahraman Adayi"),   TEXT("Seviye 30'a ulas."), K::ReachLevel, TEXT(""), 30, 800,   TEXT("Kahraman Adayi")));
		Table.Add(MakeAch(TEXT("Ach_Lv45"), TEXT("Kis Gezgini"),      TEXT("Seviye 45'e ulas."), K::ReachLevel, TEXT(""), 45, 1500,  TEXT("Kis Gezgini")));
		Table.Add(MakeAch(TEXT("Ach_Lv60"), TEXT("Yarik Yuruyeni"),   TEXT("Seviye 60'a ulas."), K::ReachLevel, TEXT(""), 60, 3000,  TEXT("Yarik Yuruyeni")));
		Table.Add(MakeAch(TEXT("Ach_Lv70"), TEXT("Kul Hatti Savasci"),TEXT("Seviye 70'e ulas."), K::ReachLevel, TEXT(""), 70, 5000,  TEXT("Kul Savascisi")));
		Table.Add(MakeAch(TEXT("Ach_Lv80"), TEXT("Efsane"),           TEXT("Seviye 80'e ulas."), K::ReachLevel, TEXT(""), 80, 10000, TEXT("Efsane")));

		// --- Boss / elit avi (arketip kimlikleri MMOMobArchetype.cpp ile ayni olmali; ValidateData denetler) ---
		Table.Add(MakeAch(TEXT("Ach_SpiderQueen"),     TEXT("Orumcek Katili"),      TEXT("Orumcek Kralicesini oldur."),     K::KillMob, TEXT("SpiderQueen"),     1, 300,  TEXT("Orumcek Katili")));
		Table.Add(MakeAch(TEXT("Ach_OrcWarlord"),      TEXT("Ork Reisi Avcisi"),    TEXT("Ork Savas Lordunu oldur."),       K::KillMob, TEXT("OrcWarlord"),      1, 300,  TEXT("Ork Avcisi")));
		Table.Add(MakeAch(TEXT("Ach_FrostLich"),       TEXT("Buz Lichi Yikicisi"),  TEXT("Buz Lichi'ni oldur."),            K::KillMob, TEXT("FrostLich"),       1, 800,  TEXT("Buz Yikicisi")));
		Table.Add(MakeAch(TEXT("Ach_MagmaTyrant"),     TEXT("Magma Tirani Avcisi"), TEXT("Magma Tirani'ni oldur."),         K::KillMob, TEXT("MagmaTyrant"),     1, 1200, TEXT("Magma Avcisi")));
		Table.Add(MakeAch(TEXT("Ach_WinterSovereign"), TEXT("Kis Egemeni Katili"),  TEXT("Kis Egemeni'ni oldur."),          K::KillMob, TEXT("WinterSovereign"), 1, 2000, TEXT("Kis Katili")));
		Table.Add(MakeAch(TEXT("Ach_FrostboundWarden"),TEXT("Buzbagli Muhafiz Katili"), TEXT("Buzbagli Muhafiz'i oldur."),   K::KillMob, TEXT("FrostboundWarden"),1, 2500, TEXT("Sinir Muhafizi")));
		Table.Add(MakeAch(TEXT("Ach_RiftHerald"),      TEXT("Yarik Habercisi Katili"),TEXT("Yarik Habercisi'ni oldur."),    K::KillMob, TEXT("RiftHerald"),      1, 3500, TEXT("Yarik Katili")));
		Table.Add(MakeAch(TEXT("Ach_AshTyrant"),       TEXT("Kul Tirani Katili"),   TEXT("Kul Tirani'ni oldur."),           K::KillMob, TEXT("AshTyrant"),       1, 4500, TEXT("Kul Katili")));
		Table.Add(MakeAch(TEXT("Ach_AbyssRegent"),     TEXT("Ucurum Naibi Katili"), TEXT("Ucurum Naibi'ni oldur."),         K::KillMob, TEXT("AbyssRegent"),     1, 8000, TEXT("Naip Katili")));

		// --- Gorev ---
		Table.Add(MakeAch(TEXT("Ach_Quests10"), TEXT("Is Bitirici"),  TEXT("10 gorev tamamla."),  K::QuestsDone, TEXT(""), 10, 200,  TEXT("Is Bitirici")));
		Table.Add(MakeAch(TEXT("Ach_Quests40"), TEXT("Gorev Ustasi"), TEXT("40 gorev tamamla."),  K::QuestsDone, TEXT(""), 40, 1000, TEXT("Gorev Ustasi")));
		Table.Add(MakeAch(TEXT("Ach_Quests90"), TEXT("Akcakule Dostu"),TEXT("90 gorev tamamla."), K::QuestsDone, TEXT(""), 90, 4000, TEXT("Akcakule Dostu")));

		// --- Zenginlik ---
		Table.Add(MakeAch(TEXT("Ach_Gold10k"),  TEXT("Cuzdan Dolu"),  TEXT("Ayni anda 10.000 altin tasi."),    K::GoldHeld, TEXT(""), 10000,   0, TEXT("")));
		Table.Add(MakeAch(TEXT("Ach_Gold1m"),   TEXT("Zengin"),       TEXT("Ayni anda 1.000.000 altin tasi."), K::GoldHeld, TEXT(""), 1000000, 0, TEXT("Zengin")));

		// --- Olum ---
		Table.Add(MakeAch(TEXT("Ach_Death10"),  TEXT("Inatci"),       TEXT("10 kez ol ve yine de yola devam et."), K::Deaths, TEXT(""), 10,  0, TEXT("Inatci")));
		Table.Add(MakeAch(TEXT("Ach_Death100"), TEXT("Olumsuz"),      TEXT("100 kez ol."),                         K::Deaths, TEXT(""), 100, 0, TEXT("Olumsuz")));

		return Table;
	}
}

const TArray<FMMOAchievementDef>& FMMOAchievementDatabase::GetAll()
{
	return BuildTable();
}

const FMMOAchievementDef* FMMOAchievementDatabase::Find(FName Id)
{
	if (Id.IsNone())
	{
		return nullptr;
	}
	for (const FMMOAchievementDef& D : GetAll())
	{
		if (D.Id == Id)
		{
			return &D;
		}
	}
	return nullptr;
}

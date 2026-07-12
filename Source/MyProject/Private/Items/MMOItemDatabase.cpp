// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/MMOItemDatabase.h"

namespace
{
	FMMOItemDef MakeItem(const TCHAR* Id, const TCHAR* Name, EMMOEquipSlot Slot,
		float Attack, float Defense, int32 ReqLevel)
	{
		FMMOItemDef D;
		D.ItemId = FName(Id);
		D.Name = Name;
		D.Slot = Slot;
		D.BaseAttack = Attack;
		D.BaseDefense = Defense;
		D.RequiredLevel = ReqLevel;
		return D;
	}

	const TArray<FMMOItemDef>& BuildTable()
	{
		static TArray<FMMOItemDef> Table;
		if (Table.Num() > 0)
		{
			return Table;
		}

		using S = EMMOEquipSlot;

		// Silahlar
		Table.Add(MakeItem(TEXT("ShortSword"), TEXT("Kisa Kilic"), S::Weapon, 8.f, 0.f, 1));
		Table.Add(MakeItem(TEXT("LongSword"), TEXT("Uzun Kilic"), S::Weapon, 14.f, 0.f, 3));
		Table.Add(MakeItem(TEXT("BattleAxe"), TEXT("Savas Baltasi"), S::Weapon, 22.f, 0.f, 5));

		// Zirhlar
		Table.Add(MakeItem(TEXT("LeatherArmor"), TEXT("Deri Zirh"), S::Armor, 0.f, 5.f, 1));
		Table.Add(MakeItem(TEXT("ChainMail"), TEXT("Zincir Zirh"), S::Armor, 0.f, 10.f, 3));

		// Kasklar
		Table.Add(MakeItem(TEXT("LeatherCap"), TEXT("Deri Baslik"), S::Helmet, 0.f, 3.f, 1));
		Table.Add(MakeItem(TEXT("IronHelmet"), TEXT("Demir Kask"), S::Helmet, 0.f, 6.f, 3));

		// Botlar
		Table.Add(MakeItem(TEXT("LeatherBoots"), TEXT("Deri Bot"), S::Boots, 0.f, 2.f, 1));
		Table.Add(MakeItem(TEXT("IronBoots"), TEXT("Demir Bot"), S::Boots, 0.f, 5.f, 3));

		return Table;
	}
}

const TArray<FMMOItemDef>& FMMOItemDatabase::GetAll()
{
	return BuildTable();
}

const FMMOItemDef* FMMOItemDatabase::Find(FName ItemId)
{
	if (ItemId.IsNone())
	{
		return nullptr;
	}
	for (const FMMOItemDef& D : GetAll())
	{
		if (D.ItemId == ItemId)
		{
			return &D;
		}
	}
	return nullptr;
}

int32 FMMOItemDatabase::GetUpgradeCost(int32 CurrentPlus)
{
	// +1 = 50, +2 = 100, ... dogrusal artis.
	return 50 * (CurrentPlus + 1);
}

int32 FMMOItemDatabase::GetUpgradeChancePct(int32 CurrentPlus)
{
	// KO hissi: dusuk artilar kolay, yuksek artilar kumar.
	static const int32 Chances[MaxPlus] = { 90, 80, 70, 55, 45, 35, 25, 20, 15 };
	if (CurrentPlus < 0 || CurrentPlus >= MaxPlus)
	{
		return 0;
	}
	return Chances[CurrentPlus];
}

bool FMMOItemDatabase::BurnsOnFail(int32 CurrentPlus)
{
	// +4'e ve uzerine denemede basarisizlik itemi yakar (klasik KO gerilimi).
	return CurrentPlus >= 3;
}

float FMMOItemDatabase::GetAttack(const FMMOItemDef& Def, int32 Plus)
{
	return FMath::RoundToFloat(Def.BaseAttack * (1.f + 0.2f * Plus));
}

float FMMOItemDatabase::GetDefense(const FMMOItemDef& Def, int32 Plus)
{
	return FMath::RoundToFloat(Def.BaseDefense * (1.f + 0.2f * Plus));
}

int32 FMMOItemDatabase::GetBuyPrice(const FMMOItemDef& Def)
{
	return FMath::RoundToInt((Def.BaseAttack + Def.BaseDefense) * 12.f) + Def.RequiredLevel * 25 + 50;
}

int32 FMMOItemDatabase::GetSellPrice(const FMMOItemDef& Def, int32 Plus)
{
	// Alis fiyatinin %40'i, arti seviyesiyle artar.
	return FMath::RoundToInt(GetBuyPrice(Def) * 0.4f * (1.f + 0.3f * Plus));
}

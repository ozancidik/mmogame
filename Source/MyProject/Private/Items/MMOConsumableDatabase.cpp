// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/MMOConsumableTypes.h"

namespace
{
	FMMOConsumableDef MakeConsumable(const TCHAR* Id, const TCHAR* Name, int32 Price,
		float Heal, float ManaRestore, const TCHAR* EffectId)
	{
		FMMOConsumableDef D;
		D.ItemId = FName(Id);
		D.Name = Name;
		D.Price = Price;
		D.Heal = Heal;
		D.ManaRestore = ManaRestore;
		D.EffectId = (EffectId && *EffectId) ? FName(EffectId) : NAME_None;
		return D;
	}

	const TArray<FMMOConsumableDef>& BuildConsumableTable()
	{
		static TArray<FMMOConsumableDef> Table;
		if (Table.Num() > 0)
		{
			return Table;
		}

		//                       Id               Ad                    Fiyat Heal Mana Effect
		Table.Add(MakeConsumable(TEXT("ManaPotion"),    TEXT("Mana Iksiri"),        20, 0.f,  60.f, TEXT("")));
		Table.Add(MakeConsumable(TEXT("GreaterManaPotion"), TEXT("Buyuk Mana Iksiri"), 40, 0.f, 120.f, TEXT("")));
		Table.Add(MakeConsumable(TEXT("ScrollRage"),    TEXT("Ofke Parsomeni"),     60, 0.f,   0.f, TEXT("Rage")));
		Table.Add(MakeConsumable(TEXT("ScrollHaste"),   TEXT("Hiz Parsomeni"),      50, 0.f,   0.f, TEXT("Haste")));
		Table.Add(MakeConsumable(TEXT("ScrollIronSkin"),TEXT("Koruma Parsomeni"),   60, 0.f,   0.f, TEXT("IronSkin")));
		Table.Add(MakeConsumable(TEXT("ScrollRegen"),   TEXT("Yenilenme Parsomeni"),45, 0.f,   0.f, TEXT("Regen")));

		return Table;
	}
}

const TArray<FMMOConsumableDef>& FMMOConsumableDatabase::GetAll()
{
	return BuildConsumableTable();
}

const FMMOConsumableDef* FMMOConsumableDatabase::Find(FName ItemId)
{
	if (ItemId.IsNone())
	{
		return nullptr;
	}
	for (const FMMOConsumableDef& D : GetAll())
	{
		if (D.ItemId == ItemId)
		{
			return &D;
		}
	}
	return nullptr;
}

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

	const TArray<FMMOItemDef>& BuildItemTable()
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
		Table.Add(MakeItem(TEXT("GreatSword"), TEXT("Buyuk Kilic"), S::Weapon, 30.f, 0.f, 6));
		Table.Add(MakeItem(TEXT("DragonBlade"), TEXT("Ejder Kilici"), S::Weapon, 38.f, 0.f, 7));
		// Mithril tier (Lv10): ucuncu bolge (Lv8-15) kademesinin ust-tier'i. Ejder setinin
		// uzerine mevcut egriyle devam eder. Elit loot havuzu "RequiredLevel <= MobLevel &&
		// >= MaxReq-2" ile secildigi icin mevcut Lv5-7 elitleri bu tier'i DUSURMEZ (MaxReq=7).
		Table.Add(MakeItem(TEXT("MithrilBlade"), TEXT("Mithril Kilic"), S::Weapon, 48.f, 0.f, 10));
		// Adamantin tier (Lv16): dorduncu bolge (Lv16-22, Alev Vadisi) kademesinin ust-tier'i.
		// Mithril'in uzerine ayni egriyle devam eder (Ejder->Mithril ~+%30; Mithril->Adamantin de oyle).
		// Elit loot havuzu "RequiredLevel <= MobLevel && >= MaxReq-2" oldugu icin mevcut Lv5-13
		// elitleri bu tier'i DUSURMEZ (onlarin MaxReq'i hala <=10); sadece Magma Tirani (Lv24) dusurur.
		Table.Add(MakeItem(TEXT("AdamantineBlade"), TEXT("Adamantin Kilic"), S::Weapon, 62.f, 0.f, 16));
		// Obsidyen tier (Lv25): Adamantin +1 civarinda; mevcut +2..+8 yatirimini gecersiz kilmaz.
		Table.Add(MakeItem(TEXT("ObsidianBlade"), TEXT("Obsidyen Kilic"), S::Weapon, 76.f, 0.f, 25));
		// Lv33 tier: Lv29 Obsidian normalleri (loot tavani 29+3=32) ve Lv30 eliti bunu dusuremez.
		Table.Add(MakeItem(TEXT("DuskBlade"), TEXT("Alacakaranlik Kilici"), S::Weapon, 92.f, 0.f, 33));
		// Lv42 Winter kademesi: onceki setin uzerine olculu artis.
		Table.Add(MakeItem(TEXT("WinterBlade"), TEXT("Kis Kilici"), S::Weapon, 108.f, 0.f, 42));
		// Lv51 Frostbound kademesi (PROGRESSION_PLAN Lv51-57 dilimi, gecici deger).
		Table.Add(MakeItem(TEXT("FrostboundBlade"), TEXT("Buzbagli Kilic"), S::Weapon, 124.f, 0.f, 51));
		// Lv58 Rift kademesi (PROGRESSION_PLAN, gecici deger).
		Table.Add(MakeItem(TEXT("RiftBlade"), TEXT("Yarik Kilici"), S::Weapon, 142.f, 0.f, 58));
		// Lv66 Kul (Ashen) ve Lv72 Ucurum (Abyss) kademeleri (PROGRESSION_PLAN, gecici deger).
		Table.Add(MakeItem(TEXT("AshenBlade"), TEXT("Kul Kilici"), S::Weapon, 164.f, 0.f, 66));
		Table.Add(MakeItem(TEXT("AbyssBlade"), TEXT("Ucurum Kilici"), S::Weapon, 188.f, 0.f, 72));
		// Lv78 Taht kademesi (CONTENT_PLAN_LV46_80.md 2.6; Lv73-80 ekipman hedefi yoktu). Onceki kademe farklari +26/+17/+7/+10; gecici deger.
		// Naib (Lv80 elit) elit havuzu MaxReq=78 oldugu icin artik yalniz bu seti dusurur (Abyss seti gorev odulu/satici/normal mobda kalir).
		Table.Add(MakeItem(TEXT("ThroneBlade"), TEXT("Taht Kilici"), S::Weapon, 214.f, 0.f, 78));

		// Zirhlar
		Table.Add(MakeItem(TEXT("LeatherArmor"), TEXT("Deri Zirh"), S::Armor, 0.f, 5.f, 1));
		Table.Add(MakeItem(TEXT("ChainMail"), TEXT("Zincir Zirh"), S::Armor, 0.f, 10.f, 3));
		Table.Add(MakeItem(TEXT("PlateArmor"), TEXT("Plaka Zirh"), S::Armor, 0.f, 15.f, 5));
		Table.Add(MakeItem(TEXT("DragonPlate"), TEXT("Ejder Zirh"), S::Armor, 0.f, 21.f, 7));
		Table.Add(MakeItem(TEXT("MithrilPlate"), TEXT("Mithril Zirh"), S::Armor, 0.f, 28.f, 10));
		Table.Add(MakeItem(TEXT("AdamantinePlate"), TEXT("Adamantin Zirh"), S::Armor, 0.f, 38.f, 16));
		Table.Add(MakeItem(TEXT("ObsidianPlate"), TEXT("Obsidyen Zirh"), S::Armor, 0.f, 46.f, 25));
		Table.Add(MakeItem(TEXT("DuskPlate"), TEXT("Alacakaranlik Zirh"), S::Armor, 0.f, 55.f, 33));
		Table.Add(MakeItem(TEXT("WinterPlate"), TEXT("Kis Zirhi"), S::Armor, 0.f, 65.f, 42));
		Table.Add(MakeItem(TEXT("FrostboundPlate"), TEXT("Buzbagli Zirh"), S::Armor, 0.f, 75.f, 51));
		Table.Add(MakeItem(TEXT("RiftPlate"), TEXT("Yarik Zirhi"), S::Armor, 0.f, 86.f, 58));
		Table.Add(MakeItem(TEXT("AshenPlate"), TEXT("Kul Zirhi"), S::Armor, 0.f, 99.f, 66));
		Table.Add(MakeItem(TEXT("AbyssPlate"), TEXT("Ucurum Zirhi"), S::Armor, 0.f, 114.f, 72));
		Table.Add(MakeItem(TEXT("ThronePlate"), TEXT("Taht Zirhi"), S::Armor, 0.f, 131.f, 78));

		// Kasklar
		Table.Add(MakeItem(TEXT("LeatherCap"), TEXT("Deri Baslik"), S::Helmet, 0.f, 3.f, 1));
		Table.Add(MakeItem(TEXT("IronHelmet"), TEXT("Demir Kask"), S::Helmet, 0.f, 6.f, 3));
		Table.Add(MakeItem(TEXT("SteelHelmet"), TEXT("Celik Kask"), S::Helmet, 0.f, 7.f, 5));
		Table.Add(MakeItem(TEXT("DragonHelm"), TEXT("Ejder Kask"), S::Helmet, 0.f, 9.f, 7));
		Table.Add(MakeItem(TEXT("MithrilHelm"), TEXT("Mithril Kask"), S::Helmet, 0.f, 12.f, 10));
		Table.Add(MakeItem(TEXT("AdamantineHelm"), TEXT("Adamantin Kask"), S::Helmet, 0.f, 16.f, 16));
		Table.Add(MakeItem(TEXT("ObsidianHelm"), TEXT("Obsidyen Kask"), S::Helmet, 0.f, 19.f, 25));
		Table.Add(MakeItem(TEXT("DuskHelm"), TEXT("Alacakaranlik Kask"), S::Helmet, 0.f, 23.f, 33));
		Table.Add(MakeItem(TEXT("WinterHelm"), TEXT("Kis Kaski"), S::Helmet, 0.f, 27.f, 42));
		Table.Add(MakeItem(TEXT("FrostboundHelm"), TEXT("Buzbagli Kask"), S::Helmet, 0.f, 31.f, 51));
		Table.Add(MakeItem(TEXT("RiftHelm"), TEXT("Yarik Kaski"), S::Helmet, 0.f, 36.f, 58));
		Table.Add(MakeItem(TEXT("AshenHelm"), TEXT("Kul Kaski"), S::Helmet, 0.f, 42.f, 66));
		Table.Add(MakeItem(TEXT("AbyssHelm"), TEXT("Ucurum Kaski"), S::Helmet, 0.f, 48.f, 72));
		Table.Add(MakeItem(TEXT("ThroneHelm"), TEXT("Taht Kaski"), S::Helmet, 0.f, 55.f, 78));

		// Botlar
		Table.Add(MakeItem(TEXT("LeatherBoots"), TEXT("Deri Bot"), S::Boots, 0.f, 2.f, 1));
		Table.Add(MakeItem(TEXT("IronBoots"), TEXT("Demir Bot"), S::Boots, 0.f, 5.f, 3));
		Table.Add(MakeItem(TEXT("SteelBoots"), TEXT("Celik Bot"), S::Boots, 0.f, 8.f, 5));
		Table.Add(MakeItem(TEXT("DragonBoots"), TEXT("Ejder Bot"), S::Boots, 0.f, 12.f, 7));
		Table.Add(MakeItem(TEXT("MithrilBoots"), TEXT("Mithril Bot"), S::Boots, 0.f, 16.f, 10));
		Table.Add(MakeItem(TEXT("AdamantineBoots"), TEXT("Adamantin Bot"), S::Boots, 0.f, 22.f, 16));
		Table.Add(MakeItem(TEXT("ObsidianBoots"), TEXT("Obsidyen Bot"), S::Boots, 0.f, 26.f, 25));
		Table.Add(MakeItem(TEXT("DuskBoots"), TEXT("Alacakaranlik Bot"), S::Boots, 0.f, 31.f, 33));
		Table.Add(MakeItem(TEXT("WinterBoots"), TEXT("Kis Botu"), S::Boots, 0.f, 36.f, 42));
		Table.Add(MakeItem(TEXT("FrostboundBoots"), TEXT("Buzbagli Bot"), S::Boots, 0.f, 41.f, 51));
		Table.Add(MakeItem(TEXT("RiftBoots"), TEXT("Yarik Botu"), S::Boots, 0.f, 47.f, 58));
		Table.Add(MakeItem(TEXT("AshenBoots"), TEXT("Kul Botu"), S::Boots, 0.f, 55.f, 66));
		Table.Add(MakeItem(TEXT("AbyssBoots"), TEXT("Ucurum Botu"), S::Boots, 0.f, 64.f, 72));
		Table.Add(MakeItem(TEXT("ThroneBoots"), TEXT("Taht Botu"), S::Boots, 0.f, 74.f, 78));

		return Table;
	}
}

const TArray<FMMOItemDef>& FMMOItemDatabase::GetAll()
{
	return BuildItemTable();
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

int32 FMMOItemDatabase::GetUpgradeCost(int32 CurrentPlus, int32 RequiredLevel)
{
	// Taban: +1 = 50, +2 = 100, ... dogrusal artis. Esya seviyesiyle (100 + Lv^2) / 100 carpaniyla buyur (tamsayi, tasma yok:
	// Lv80 +9 = 50 x 10 x 65 = 32.500).
	const int64 Lv = FMath::Clamp(RequiredLevel, 0, 100);
	return static_cast<int32>(50ll * (CurrentPlus + 1) * (100ll + Lv * Lv) / 100ll);
}

int32 FMMOItemDatabase::GetUpgradeChancePct(int32 CurrentPlus)
{
	// KO hissi: dusuk artilar kolay, yuksek artilar kumar.
	static const int32 Chances[MaxPlus] = { 90, 80, 70, 55, 45, 35, 25, 20 };
	if (CurrentPlus < 0 || CurrentPlus >= MaxPlus)
	{
		return 0;
	}
	return Chances[CurrentPlus];
}

int32 FMMOItemDatabase::GetBurnChancePct(int32 CurrentPlus)
{
	// 2026-09-14 guncellemesi: sabit esik yerine kademeli yanma olasiligi.
	// +0..+2 denemesi tamamen guvenli (basarisizlikta item hep kalir).
	// +3'ten itibaren basarisizlikta yanma sansi her seviyede artar, son deneme
	// (+7 -> +8, MaxPlus=8 ile ulasilabilecek en yuksek deneme) yanma garantisiyle biter.
	static const int32 BurnChances[MaxPlus] = { 0, 0, 0, 15, 30, 50, 70, 100 };
	if (CurrentPlus < 0 || CurrentPlus >= MaxPlus)
	{
		return 0;
	}
	return BurnChances[CurrentPlus];
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

float FMMOItemDatabase::GetRarityMultiplier(EMMOItemRarity Rarity)
{
	switch (Rarity)
	{
	case EMMOItemRarity::Uncommon:  return 1.25f;
	case EMMOItemRarity::Rare:      return 1.6f;
	case EMMOItemRarity::Epic:      return 2.1f;
	case EMMOItemRarity::Legendary: return 3.0f;
	default:                        return 1.0f; // Common
	}
}

FLinearColor FMMOItemDatabase::GetRarityColor(EMMOItemRarity Rarity)
{
	switch (Rarity)
	{
	case EMMOItemRarity::Uncommon:  return FLinearColor(0.35f, 0.85f, 0.35f); // yesil
	case EMMOItemRarity::Rare:      return FLinearColor(0.3f, 0.6f, 1.f);     // mavi
	case EMMOItemRarity::Epic:      return FLinearColor(0.75f, 0.4f, 1.f);    // mor
	case EMMOItemRarity::Legendary: return FLinearColor(1.f, 0.6f, 0.15f);    // turuncu
	default:                        return FLinearColor(0.85f, 0.85f, 0.85f); // Common (gri-beyaz)
	}
}

FString FMMOItemDatabase::GetRarityName(EMMOItemRarity Rarity)
{
	switch (Rarity)
	{
	case EMMOItemRarity::Uncommon:  return TEXT("Nadir");
	case EMMOItemRarity::Rare:      return TEXT("Ender");
	case EMMOItemRarity::Epic:      return TEXT("Destansi");
	case EMMOItemRarity::Legendary: return TEXT("Efsanevi");
	default:                        return TEXT("Siradan");
	}
}

EMMOItemRarity FMMOItemDatabase::RollRarity()
{
	// Agirlikli: Common %60, Uncommon %25, Rare %10, Epic %4, Legendary %1.
	const float R = FMath::FRand();
	if (R < 0.60f) return EMMOItemRarity::Common;
	if (R < 0.85f) return EMMOItemRarity::Uncommon;
	if (R < 0.95f) return EMMOItemRarity::Rare;
	if (R < 0.99f) return EMMOItemRarity::Epic;
	return EMMOItemRarity::Legendary;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Items/MMOItemTypes.h"

class UMMOInventoryComponent;

/**
 * Ekipman birlestirme (nadirlik fuzyonu, docs/DESIGN_FUSION.md): cantadaki 3 ayni esya (ayni ItemId,
 * ayni nadirlik, +0) satici yaninda altin karsiligi bir ust nadirlikte TEK yeni esyaya donusur.
 * Sonuc kesin (sans yok), durum tutulmaz. Ucret ve nadirlik her zaman sunucuda hesaplanir.
 */
enum class EMMOFusionResult : uint8
{
	Ok,
	NotAuthority,
	NotNearVendor,
	BadIds,
	DuplicateId,
	NotInBag,
	ItemMismatch,
	RarityMismatch,
	PlusNotZero,
	RarityCapped,
	UnknownItem,
	NotEnoughGold,
	InternalError,
	// QA/guvenlik turu (D1): sona eklendi, mevcut degerler kaymaz.
	NotPlayer,   // sahip oyuncu kontrollu AMMOCharacter degil (mob/NPC/AI)
	Locked,      // kayit yuklenmedi ya da bolge gecisi kaydi onaylandi (degisiklik kalici olmaz)
	TradeLocked  // girdi esya aktif takas teklifinde
};

/** Check/FindFromSlot sonucu: sunucunun hesapladigi esya, nadirlik gecisi ve ucret. */
struct FMMOFusionPreview
{
	EMMOFusionResult Result = EMMOFusionResult::BadIds;
	FName ItemId;
	EMMOItemRarity From = EMMOItemRarity::Common;
	EMMOItemRarity To = EMMOItemRarity::Common;
	int32 Fee = 0;
	int32 InstanceIds[3] = {0, 0, 0};
};

struct MYPROJECT_API FMMOFusion
{
	static constexpr int32 InputCount = 3;
	// Birlestirmeyle ulasilabilen en yuksek nadirlik; Efsanevi yalniz ganimet (karar A1). PLACEHOLDER.
	static constexpr EMMOItemRarity MaxOutputRarity = EMMOItemRarity::Epic;

	// Pure: no world, no inventory.
	static bool NextRarity(EMMOItemRarity In, EMMOItemRarity& Out);
	static int32 FeeFor(const FMMOItemDef& Def, EMMOItemRarity Input);

	// Server read-only check (no mutation). Used by preview and by Execute.
	// Order: authority, player gate (NotPlayer, Locked), vendor range, then the ids/items.
	static FMMOFusionPreview Check(const UMMOInventoryComponent& Inv, int32 A, int32 B, int32 C);

	// The item in the 1-based bag slot is always the first input; the other two are the matching
	// bag items (same ItemId and rarity, Plus 0) with the lowest InstanceIds. Same gates as Check first.
	static FMMOFusionPreview FindFromSlot(const UMMOInventoryComponent& Inv, int32 BagSlot1Based);

	// Server only. Re-runs Check, then mutates silently, then broadcasts once.
	static EMMOFusionResult Execute(UMMOInventoryComponent& Inv, int32 A, int32 B, int32 C, int32* OutNewInstanceId = nullptr);

	static const TCHAR* ResultText(EMMOFusionResult R); // Turkish ASCII player message

private:
	// Gates before any item check (shared by Check and FindFromSlot). Member so the inventory friend applies.
	static EMMOFusionResult GateResult(const UMMOInventoryComponent& Inv);
};

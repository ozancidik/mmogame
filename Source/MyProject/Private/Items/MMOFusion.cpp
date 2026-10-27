// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/MMOFusion.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOStatsComponent.h"
#include "Components/MMOTradeComponent.h"
#include "Characters/MMOCharacter.h"
#include "Items/MMOItemDatabase.h"
#include "Core/MMOMetrics.h"
#include "Core/MMONotify.h"
#include "Core/MMOServerLifecycle.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogMMOFusion, Log, All);

namespace
{
	// Ucret carpani, GIRDI nadirligine gore (Common, Uncommon, Rare): Fee = Round(GetBuyPrice x carpan).
	// Epic/Legendary girdisi birlestirilemez (MaxOutputRarity). PLACEHOLDER: D0 ekonomi modeli kesinlestirir.
	constexpr float FusionFeeMult[] = { 0.5f, 1.0f, 2.0f };

	// Aktif takas teklifindeki esya birlestirilemez (F6). Yalniz UMMOTradeComponent'in public alanlari okunur.
	bool FusionIsInTradeOffer(const UMMOInventoryComponent& Inv, int32 InstanceId)
	{
		const AMMOCharacter* Char = Cast<AMMOCharacter>(Inv.GetOwner());
		const UMMOTradeComponent* Trade = Char ? Char->GetTradeComponent() : nullptr;
		if (!Trade || !Trade->IsTrading())
		{
			return false;
		}
		return Trade->MyOffer.Items.ContainsByPredicate([InstanceId](const FMMOEquipmentInstance& I) { return I.InstanceId == InstanceId; });
	}
}

// Esyadan once gelen kapilar (Check ve FindFromSlot ortak): yetki, oyuncu, kayit kilidi, satici yakinligi.
EMMOFusionResult FMMOFusion::GateResult(const UMMOInventoryComponent& Inv)
{
	const AActor* Owner = Inv.GetOwner();
	if (!IsValid(Owner) || Owner->IsActorBeingDestroyed() || !Owner->HasAuthority() || Inv.IsBeingDestroyed())
	{
		return EMMOFusionResult::NotAuthority;
	}
	// Mob/NPC/AI'nin envanteri birlestirilmez (F3).
	const AMMOCharacter* Char = Cast<AMMOCharacter>(Owner);
	if (!Char || !Char->IsPlayerControlled())
	{
		return EMMOFusionResult::NotPlayer;
	}
	// Posta deseni (UMMOMailComponent::IsOwnerLocked): yuklenmemis karakterin degisikligi kaydedilemez,
	// gecis onayindan sonra da EndPlay kaydi atlanir; ikisinde de mutasyon kalici olmaz (F1).
	if (!Char->IsInitialLoadComplete() || Char->IsTravelSaveCommitted())
	{
		return EMMOFusionResult::Locked;
	}
	// Planli kapanis Flushing'e girdi: kayit turundan sonra yapilan birlestirme kalici olmaz (F1). Oyuncuya "yukleniyor" degil "bakim" denir.
	if (MMOServerLifecycle::IsFlushingOrLater())
	{
		return EMMOFusionResult::Maintenance;
	}
	// Olu karakteri de reddeder (IsOwnerNearVendor). Esya kontrollerinden once (D-4).
	if (!Inv.IsOwnerNearVendor())
	{
		return EMMOFusionResult::NotNearVendor;
	}
	return EMMOFusionResult::Ok;
}

bool FMMOFusion::NextRarity(EMMOItemRarity In, EMMOItemRarity& Out)
{
	Out = In;
	if ((uint8)In >= (uint8)EMMOItemRarity::MAX)
	{
		return false;
	}
	const EMMOItemRarity Next = (EMMOItemRarity)((uint8)In + 1);
	if ((uint8)Next > (uint8)MaxOutputRarity)
	{
		return false; // Tavan: cikti en cok Destansi.
	}
	Out = Next;
	return true;
}

int32 FMMOFusion::FeeFor(const FMMOItemDef& Def, EMMOItemRarity Input)
{
	const int32 Index = (int32)Input;
	if (Index < 0 || Index >= (int32)UE_ARRAY_COUNT(FusionFeeMult))
	{
		return 0;
	}
	// bVendorSold=false esyalarda da ayni alis formulu kullanilir (satilmasa da hesaplanabilir).
	return FMath::RoundToInt(static_cast<double>(FMMOItemDatabase::GetBuyPrice(Def)) * FusionFeeMult[Index]);
}

FMMOFusionPreview FMMOFusion::Check(const UMMOInventoryComponent& Inv, int32 A, int32 B, int32 C)
{
	FMMOFusionPreview P;
	P.InstanceIds[0] = A;
	P.InstanceIds[1] = B;
	P.InstanceIds[2] = C;

	P.Result = GateResult(Inv);
	if (P.Result != EMMOFusionResult::Ok)
	{
		return P;
	}
	P.Result = EMMOFusionResult::BadIds;
	if (A <= 0 || B <= 0 || C <= 0)
	{
		P.Result = EMMOFusionResult::BadIds;
		return P;
	}
	if (A == B || B == C || A == C)
	{
		P.Result = EMMOFusionResult::DuplicateId;
		return P;
	}

	// Yalniz canta: kusanilmis ve depodaki esya burada bulunmaz.
	const FMMOEquipmentInstance* Inputs[InputCount] = {};
	for (int32 i = 0; i < InputCount; ++i)
	{
		const int32 Index = Inv.FindBagIndexByInstanceId(P.InstanceIds[i]);
		if (Index == INDEX_NONE)
		{
			P.Result = EMMOFusionResult::NotInBag;
			return P;
		}
		Inputs[i] = &Inv.EquipmentBag[Index];
	}
	for (int32 i = 0; i < InputCount; ++i)
	{
		if (FusionIsInTradeOffer(Inv, P.InstanceIds[i]))
		{
			P.Result = EMMOFusionResult::TradeLocked;
			return P;
		}
	}

	P.ItemId = Inputs[0]->ItemId;
	P.From = Inputs[0]->Rarity;
	P.To = P.From;
	for (int32 i = 1; i < InputCount; ++i)
	{
		if (Inputs[i]->ItemId != P.ItemId)
		{
			P.Result = EMMOFusionResult::ItemMismatch;
			return P;
		}
	}
	for (int32 i = 1; i < InputCount; ++i)
	{
		if (Inputs[i]->Rarity != P.From)
		{
			P.Result = EMMOFusionResult::RarityMismatch;
			return P;
		}
	}
	// Yukseltilmis esya kabul edilmez (+N'ye harcanan altin kaybolmasin, +N aklanmasin; karar A2). PLACEHOLDER.
	for (int32 i = 0; i < InputCount; ++i)
	{
		if (Inputs[i]->Plus != 0)
		{
			P.Result = EMMOFusionResult::PlusNotZero;
			return P;
		}
	}

	const FMMOItemDef* Def = FMMOItemDatabase::Find(P.ItemId);
	if (!Def)
	{
		P.Result = EMMOFusionResult::UnknownItem;
		return P;
	}
	if (!NextRarity(P.From, P.To))
	{
		P.Result = EMMOFusionResult::RarityCapped;
		return P;
	}
	P.Fee = FeeFor(*Def, P.From);
	if (P.Fee <= 0)
	{
		P.Result = EMMOFusionResult::InternalError;
		return P;
	}
	if (Inv.Gold < P.Fee)
	{
		P.Result = EMMOFusionResult::NotEnoughGold;
		return P;
	}
	P.Result = EMMOFusionResult::Ok;
	return P;
}

FMMOFusionPreview FMMOFusion::FindFromSlot(const UMMOInventoryComponent& Inv, int32 BagSlot1Based)
{
	FMMOFusionPreview P;
	// Uzaktaki/kilitli oyuncu once kapi mesajini gorur, slot hatasini degil (D-4).
	P.Result = GateResult(Inv);
	if (P.Result != EMMOFusionResult::Ok)
	{
		return P;
	}
	P.Result = EMMOFusionResult::BadIds;
	// Cikarmadan once sinir: BagSlot1Based - 1 INT_MIN'de tasmaz (F4/D-2).
	if (BagSlot1Based <= 0 || BagSlot1Based > Inv.EquipmentBag.Num())
	{
		return P;
	}
	const FMMOEquipmentInstance& Ref = Inv.EquipmentBag[BagSlot1Based - 1];
	P.ItemId = Ref.ItemId;
	P.From = Ref.Rarity;
	P.To = Ref.Rarity;
	P.InstanceIds[0] = Ref.InstanceId;
	if (Ref.InstanceId <= 0)
	{
		return P; // BadIds
	}
	if (Ref.Plus != 0)
	{
		P.Result = EMMOFusionResult::PlusNotZero;
		return P;
	}

	// Secilen slotun esyasi her zaman ilk girdi (oyuncunun tikladigi esya gider, D-1); kalan ikisi esdegerlerin
	// en kucuk kimlikleri (deterministik secim).
	TArray<int32> Ids;
	for (const FMMOEquipmentInstance& Inst : Inv.EquipmentBag)
	{
		if (Inst.InstanceId > 0 && Inst.InstanceId != Ref.InstanceId && Inst.ItemId == Ref.ItemId
			&& Inst.Rarity == Ref.Rarity && Inst.Plus == 0)
		{
			Ids.Add(Inst.InstanceId);
		}
	}
	if (Ids.Num() < InputCount - 1)
	{
		return P; // BadIds: esdeger 3 esya yok
	}
	Ids.Sort();
	return Check(Inv, Ref.InstanceId, Ids[0], Ids[1]);
}

EMMOFusionResult FMMOFusion::Execute(UMMOInventoryComponent& Inv, int32 A, int32 B, int32 C, int32* OutNewInstanceId)
{
	if (OutNewInstanceId)
	{
		*OutNewInstanceId = 0;
	}
	// (1) Tum dogrulama mutasyondan once; istemciden gelen hicbir sayi (ucret/nadirlik) kullanilmaz.
	const FMMOFusionPreview P = Check(Inv, A, B, C);
	AActor* Owner = Inv.GetOwner();
	if (P.Result != EMMOFusionResult::Ok)
	{
		if (P.Result != EMMOFusionResult::NotAuthority && P.Result != EMMOFusionResult::NotPlayer)
		{
			MMONotify::ToOwner(Owner, ResultText(P.Result), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		}
		return P.Result;
	}
	const FMMOItemDef* Def = FMMOItemDatabase::Find(P.ItemId);

	// (2) Girdilerin kopyasi + indeksleri (artan indeks sirasinda), geri alma icin.
	int32 Indices[InputCount];
	FMMOEquipmentInstance Copies[InputCount];
	for (int32 i = 0; i < InputCount; ++i)
	{
		Indices[i] = Inv.FindBagIndexByInstanceId(P.InstanceIds[i]);
		Copies[i] = Inv.EquipmentBag[Indices[i]];
	}
	for (int32 i = 0; i < InputCount; ++i)
	{
		for (int32 j = i + 1; j < InputCount; ++j)
		{
			if (Indices[j] < Indices[i])
			{
				Swap(Indices[i], Indices[j]);
				Swap(Copies[i], Copies[j]);
			}
		}
	}
	const int32 OldNextInstanceId = Inv.NextInstanceId;
	const auto RestoreInputs = [&Inv, &Indices, &Copies]()
	{
		for (int32 i = 0; i < InputCount; ++i)
		{
			Inv.EquipmentBag.Insert(Copies[i], Indices[i]);
		}
	};

	// (3) Buyukten kucuge cikar (indeksler kaymasin).
	for (int32 i = InputCount - 1; i >= 0; --i)
	{
		Inv.EquipmentBag.RemoveAt(Indices[i]);
	}

	// (4) Cikti: yeni InstanceId, Plus 0, sessiz.
	if (!Inv.AddEquipmentInstanceInternal(P.ItemId, 0, P.To, false))
	{
		Inv.NextInstanceId = OldNextInstanceId;
		RestoreInputs();
		UE_LOG(LogMMOFusion, Warning, TEXT("event=item_fuse_failed actor=\"%s\" item=\"%s\" stage=output"),
			*MMOMetrics::WhoName(Owner), *P.ItemId.ToString());
		MMONotify::ToOwner(Owner, ResultText(EMMOFusionResult::InternalError), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return EMMOFusionResult::InternalError;
	}
	const FMMOEquipmentInstance Output = Inv.EquipmentBag.Last();

	// (5) Ucret, sessiz. ApplyGoldInternal kendi [MMOMETRIC] ev=gold src=birlestirme satirini basar.
	const int32 Applied = Inv.ApplyGoldInternal(-P.Fee, TEXT("birlestirme"), false);
	if (Applied != -P.Fee)
	{
		// Olmamali (Gold >= Fee dogrulandi): kismi kesintiyi ve ciktiyi geri al.
		// Iade de ApplyGoldInternal'dan gecer: [MMOMETRIC] ev=gold satirlari dengelenir (F2).
		Inv.ApplyGoldInternal(-Applied, TEXT("birlestirme-iade"), false);
		Inv.EquipmentBag.RemoveAt(Inv.EquipmentBag.Num() - 1);
		Inv.NextInstanceId = OldNextInstanceId;
		RestoreInputs();
		UE_LOG(LogMMOFusion, Warning, TEXT("event=item_fuse_failed actor=\"%s\" item=\"%s\" stage=fee applied=%d fee=%d"),
			*MMOMetrics::WhoName(Owner), *P.ItemId.ToString(), Applied, P.Fee);
		MMONotify::ToOwner(Owner, ResultText(EMMOFusionResult::InternalError), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return EMMOFusionResult::InternalError;
	}
	if (OutNewInstanceId)
	{
		*OutNewInstanceId = Output.InstanceId;
	}

	// (6) Denetim satiri + olcum.
	const int32 Balance = Inv.Gold;
	const AMMOCharacter* OwnerChar = Cast<AMMOCharacter>(Owner);
	const UMMOStatsComponent* Stats = OwnerChar ? OwnerChar->GetStats() : nullptr;
	UE_LOG(LogMMOFusion, Display,
		TEXT("event=item_fuse actor=\"%s\" item=\"%s\" from=%d to=%d inputs=%d,%d,%d output=%d fee=%d balance=%d"),
		*MMOMetrics::WhoName(Owner), *P.ItemId.ToString(), (int32)P.From, (int32)P.To,
		P.InstanceIds[0], P.InstanceIds[1], P.InstanceIds[2], Output.InstanceId, P.Fee, Balance);
	UE_LOG(LogTemp, Display, TEXT("[MMOMETRIC] t=%.1f ev=fusion who=%s item=%s from=%d to=%d fee=%d lv=%d"),
		MMOMetrics::Seconds(), *MMOMetrics::WhoName(Owner), *P.ItemId.ToString(), (int32)P.From, (int32)P.To,
		P.Fee, Stats ? Stats->Level : 0);

	// (7) Delegeler mutasyondan SONRA, birer kez: gozlemci cantayi ve altini zaten son halinde gorur.
	const TWeakObjectPtr<UMMOInventoryComponent> WeakInv(&Inv);
	const TWeakObjectPtr<AActor> WeakOwner(Owner);
	Inv.OnInventoryChanged.Broadcast();
	if (UMMOInventoryComponent* Live = WeakInv.Get(); Live && !Live->IsBeingDestroyed())
	{
		Live->OnGoldChanged.Broadcast(Live->Gold);
	}

	// (8) Sahibe sonuc satiri.
	if (AActor* LiveOwner = WeakOwner.Get(); LiveOwner && !LiveOwner->IsActorBeingDestroyed())
	{
		MMONotify::ToOwner(LiveOwner, FString::Printf(TEXT("Birlestirildi: %s %s (-%d altin)"),
			Def ? *Def->Name : *P.ItemId.ToString(), *FMMOItemDatabase::GetRarityName(P.To), P.Fee),
			FColor(120, 220, 120), 2.f);
	}
	return EMMOFusionResult::Ok;
}

const TCHAR* FMMOFusion::ResultText(EMMOFusionResult R)
{
	switch (R)
	{
	case EMMOFusionResult::Ok:             return TEXT("Birlestirme tamamlandi");
	case EMMOFusionResult::NotAuthority:   return TEXT("Birlestirme yalniz sunucuda yapilir");
	case EMMOFusionResult::NotNearVendor:  return TEXT("Satici yakininda degilsin");
	case EMMOFusionResult::BadIds:         return TEXT("Birlestirme icin ayni 3 esya gerekir");
	case EMMOFusionResult::DuplicateId:    return TEXT("Ayni esya iki kez secilemez");
	case EMMOFusionResult::NotInBag:       return TEXT("Esya cantada degil");
	case EMMOFusionResult::ItemMismatch:   return TEXT("Uc esya ayni olmali");
	case EMMOFusionResult::RarityMismatch: return TEXT("Uc esyanin nadirligi ayni olmali");
	case EMMOFusionResult::PlusNotZero:    return TEXT("Yukseltilmis (+) esya birlestirilemez");
	case EMMOFusionResult::RarityCapped:   return TEXT("Bu nadirlik daha fazla birlestirilemez");
	case EMMOFusionResult::UnknownItem:    return TEXT("Bilinmeyen esya");
	case EMMOFusionResult::NotEnoughGold:  return TEXT("Altin yetersiz");
	case EMMOFusionResult::InternalError:  return TEXT("Birlestirme basarisiz");
	case EMMOFusionResult::NotPlayer:      return TEXT("Birlestirme yalniz oyuncu karakteri icindir");
	case EMMOFusionResult::Locked:         return TEXT("Karakter yukleniyor ya da bolge degistiriyor; birlestirme su an yapilamaz");
	case EMMOFusionResult::TradeLocked:    return TEXT("Takasa konan esya birlestirilemez");
	case EMMOFusionResult::Maintenance:    return TEXT("Sunucu bakima giriyor");
	}
	return TEXT("Birlestirme basarisiz");
}

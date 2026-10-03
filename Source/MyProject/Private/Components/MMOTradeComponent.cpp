// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMOTradeComponent.h"
#include "Characters/MMOCharacter.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOStatsComponent.h"
#include "Items/MMOItemDatabase.h"
#include "Player/MMOPlayerController.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

namespace
{
	FString TradeNameOf(const AMMOCharacter* C)
	{
		if (C)
		{
			if (const APlayerState* PS = C->GetPlayerState())
			{
				return PS->GetPlayerName();
			}
		}
		return TEXT("?");
	}

	bool TradeIsDead(const AMMOCharacter* C)
	{
		const UMMOStatsComponent* S = C ? C->GetStats() : nullptr;
		return !S || S->bIsDead;
	}

	// Bir tarafin teklifi cantayla/cuzdanla hala tutarli mi (takas aninda yeniden dogrulama).
	// Teklif edilen her ekipman hala cantada, ayni Plus/Rarity ile durmali; altin yeterli olmali.
	bool TradeSideStillValid(const FMMOTradeOffer& Offer, const UMMOInventoryComponent* Inv)
	{
		if (!Inv || Offer.Gold < 0 || Offer.Gold > Inv->Gold
			|| Offer.Items.Num() > UMMOTradeComponent::MaxOfferItems)
		{
			return false;
		}
		TArray<int32, TInlineAllocator<UMMOTradeComponent::MaxOfferItems>> Seen;
		for (const FMMOEquipmentInstance& Offered : Offer.Items)
		{
			if (!Offered.IsValidInstance() || Seen.Contains(Offered.InstanceId) || !FMMOItemDatabase::Find(Offered.ItemId))
			{
				return false;
			}
			Seen.Add(Offered.InstanceId);
			const FMMOEquipmentInstance* InBag = Inv->EquipmentBag.FindByPredicate([&Offered](const FMMOEquipmentInstance& I)
			{
				return I.InstanceId == Offered.InstanceId;
			});
			if (!InBag || InBag->ItemId != Offered.ItemId || InBag->Plus != Offered.Plus || InBag->Rarity != Offered.Rarity)
			{
				return false;
			}
		}
		return true;
	}
}

UMMOTradeComponent::UMMOTradeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMMOTradeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Yalniz sahibine: karsi tarafin teklifi de sahibin bileseninde ayna olarak durur.
	DOREPLIFETIME_CONDITION(UMMOTradeComponent, bTradeActive, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMMOTradeComponent, PartnerName, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMMOTradeComponent, MyOffer, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMMOTradeComponent, TheirOffer, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMMOTradeComponent, OfferRevision, COND_OwnerOnly);
}

void UMMOTradeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Karakter yok olurken (bolge degisimi, baglanti kopmasi) acik takas iptal olur.
	if (HasAuthorityInternal() && bTradeActive)
	{
		EndTradeServer(TEXT("Ticaret iptal edildi: karsi taraf ayrildi."));
	}
	Super::EndPlay(EndPlayReason);
}

bool UMMOTradeComponent::HasAuthorityInternal() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->HasAuthority() : false;
}

AMMOCharacter* UMMOTradeComponent::GetOwnerCharacter() const
{
	return Cast<AMMOCharacter>(GetOwner());
}

void UMMOTradeComponent::OnRep_Trade()
{
	OnTradeChanged.Broadcast();
}

void UMMOTradeComponent::NotifyOwner(const FString& Text) const
{
	if (Text.IsEmpty())
	{
		return;
	}
	if (const AMMOCharacter* C = GetOwnerCharacter())
	{
		if (AMMOPlayerController* PC = Cast<AMMOPlayerController>(C->GetController()))
		{
			PC->ClientReceiveChat(FString(), Text, 2);
		}
	}
}

UMMOTradeComponent* UMMOTradeComponent::GetValidPartnerComp() const
{
	if (!bTradeActive)
	{
		return nullptr;
	}
	AMMOCharacter* P = Partner.Get();
	UMMOTradeComponent* PC = P ? P->GetTradeComponent() : nullptr;
	if (!PC || !PC->bTradeActive || PC->Partner.Get() != GetOwner())
	{
		return nullptr;
	}
	return PC;
}

AMMOCharacter* UMMOTradeComponent::FindNearestTradeTarget() const
{
	AMMOCharacter* Self = GetOwnerCharacter();
	UWorld* World = GetWorld();
	if (!Self || !World)
	{
		return nullptr;
	}

	AMMOCharacter* Best = nullptr;
	float BestDist = InviteRange;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		AMMOCharacter* C = PC ? Cast<AMMOCharacter>(PC->GetPawn()) : nullptr;
		if (!C || C == Self || TradeIsDead(C))
		{
			continue;
		}
		const float D = FVector::Dist(Self->GetActorLocation(), C->GetActorLocation());
		if (D < BestDist)
		{
			BestDist = D;
			Best = C;
		}
	}
	return Best;
}

// ----------------------------------------------------------------------
// Istemci istekleri
// ----------------------------------------------------------------------

void UMMOTradeComponent::RequestInviteNearest()
{
	if (HasAuthorityInternal()) { ServerInviteNearest_Implementation(); } else { ServerInviteNearest(); }
}

void UMMOTradeComponent::RequestAccept()
{
	if (HasAuthorityInternal()) { ServerAccept_Implementation(); } else { ServerAccept(); }
}

void UMMOTradeComponent::RequestCancel()
{
	if (HasAuthorityInternal()) { ServerCancel_Implementation(); } else { ServerCancel(); }
}

void UMMOTradeComponent::RequestAddItem(int32 InstanceId)
{
	if (HasAuthorityInternal()) { ServerAddItem_Implementation(InstanceId); } else { ServerAddItem(InstanceId); }
}

void UMMOTradeComponent::RequestRemoveItem(int32 InstanceId)
{
	if (HasAuthorityInternal()) { ServerRemoveItem_Implementation(InstanceId); } else { ServerRemoveItem(InstanceId); }
}

void UMMOTradeComponent::RequestSetGold(int32 NewGold)
{
	if (HasAuthorityInternal()) { ServerSetGold_Implementation(NewGold); } else { ServerSetGold(NewGold); }
}

void UMMOTradeComponent::RequestConfirm()
{
	if (HasAuthorityInternal()) { ServerConfirm_Implementation(OfferRevision); } else { ServerConfirm(OfferRevision); }
}

// ----------------------------------------------------------------------
// Sunucu tarafi
// ----------------------------------------------------------------------

void UMMOTradeComponent::ServerInviteNearest_Implementation()
{
	AMMOCharacter* Self = GetOwnerCharacter();
	const UWorld* World = GetWorld();
	if (!Self || !World)
	{
		return;
	}

	// Davet spam korumasi (hedefin ekranini doldurmasin).
	const float Now = World->GetTimeSeconds();
	if (Now - LastInviteTime < InviteCooldown)
	{
		return;
	}
	LastInviteTime = Now;

	if (bTradeActive)
	{
		NotifyOwner(TEXT("Zaten bir ticaretin icindesin."));
		return;
	}
	if (TradeIsDead(Self))
	{
		return;
	}

	AMMOCharacter* Target = FindNearestTradeTarget();
	UMMOTradeComponent* TC = Target ? Target->GetTradeComponent() : nullptr;
	if (!TC)
	{
		NotifyOwner(TEXT("Yakinda ticaret edilecek oyuncu yok."));
		return;
	}
	if (TC->bTradeActive)
	{
		NotifyOwner(TEXT("Oyuncu su an baska bir ticarette."));
		return;
	}

	// Davet kapma korumasi: hedefin gecerli baska bir bekleyen daveti varsa ezme.
	if (TC->PendingInviteFrom.IsValid() && TC->PendingInviteFrom.Get() != Self
		&& TC->PendingInviteTime >= 0.f && Now - TC->PendingInviteTime < InviteExpirySeconds)
	{
		NotifyOwner(TEXT("Oyuncunun bekleyen baska bir daveti var; biraz sonra tekrar dene."));
		return;
	}
	TC->PendingInviteFrom = Self;
	TC->PendingInviteTime = Now;
	TC->NotifyOwner(FString::Printf(TEXT("%s seninle ticaret etmek istiyor. Kabul: /trade kabul"), *TradeNameOf(Self)));
	NotifyOwner(FString::Printf(TEXT("%s adli oyuncuya ticaret daveti gonderildi."), *TradeNameOf(Target)));
}

void UMMOTradeComponent::ServerAccept_Implementation()
{
	AMMOCharacter* Self = GetOwnerCharacter();
	AMMOCharacter* Inviter = PendingInviteFrom.Get();
	const float InviteTime = PendingInviteTime;
	PendingInviteFrom.Reset();
	PendingInviteTime = -1.f;

	const UWorld* World = GetWorld();
	if (!Self || !World)
	{
		return;
	}
	if (!Inviter || Inviter == Self)
	{
		NotifyOwner(TEXT("Bekleyen ticaret daveti yok."));
		return;
	}
	if (InviteTime < 0.f || World->GetTimeSeconds() - InviteTime > InviteExpirySeconds)
	{
		NotifyOwner(TEXT("Ticaret davetinin suresi doldu."));
		return;
	}

	UMMOTradeComponent* IC = Inviter->GetTradeComponent();
	if (!IC || bTradeActive || IC->bTradeActive)
	{
		NotifyOwner(TEXT("Ticaret baslatilamadi: biri zaten ticarette."));
		return;
	}
	if (TradeIsDead(Self) || TradeIsDead(Inviter))
	{
		return;
	}
	if (FVector::Dist(Self->GetActorLocation(), Inviter->GetActorLocation()) > CancelDistance)
	{
		NotifyOwner(TEXT("Davet eden cok uzakta."));
		return;
	}

	StartTradeServer(Inviter, Self);
}

void UMMOTradeComponent::ServerCancel_Implementation()
{
	if (bTradeActive)
	{
		EndTradeServer(TEXT("Ticaret iptal edildi."));
	}
	else
	{
		// Bekleyen daveti de dusur.
		PendingInviteFrom.Reset();
		PendingInviteTime = -1.f;
	}
}

void UMMOTradeComponent::ServerAddItem_Implementation(int32 InstanceId)
{
	UMMOTradeComponent* PC = GetValidPartnerComp();
	AMMOCharacter* Self = GetOwnerCharacter();
	const UMMOInventoryComponent* Inv = Self ? Self->GetInventory() : nullptr;
	const UWorld* World = GetWorld();
	if (!PC || !Inv || !World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	if (Now - LastOfferChangeTime < OfferChangeCooldown)
	{
		return;
	}
	if (MyOffer.Items.Num() >= MaxOfferItems)
	{
		NotifyOwner(FString::Printf(TEXT("En fazla %d ekipman teklif edebilirsin."), MaxOfferItems));
		return;
	}
	if (MyOffer.Items.ContainsByPredicate([InstanceId](const FMMOEquipmentInstance& I) { return I.InstanceId == InstanceId; }))
	{
		return;
	}
	const FMMOEquipmentInstance* InBag = Inv->EquipmentBag.FindByPredicate([InstanceId](const FMMOEquipmentInstance& I)
	{
		return I.InstanceId == InstanceId;
	});
	if (!InBag || !InBag->IsValidInstance())
	{
		return;
	}

	LastOfferChangeTime = Now;
	MyOffer.Items.Add(*InBag);
	OnMyOfferChanged();
}

void UMMOTradeComponent::ServerRemoveItem_Implementation(int32 InstanceId)
{
	if (!GetValidPartnerComp() || !GetWorld())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastOfferChangeTime < OfferChangeCooldown)
	{
		return;
	}
	const int32 Index = MyOffer.Items.IndexOfByPredicate([InstanceId](const FMMOEquipmentInstance& I)
	{
		return I.InstanceId == InstanceId;
	});
	if (Index == INDEX_NONE)
	{
		return;
	}
	LastOfferChangeTime = Now;
	MyOffer.Items.RemoveAt(Index);
	OnMyOfferChanged();
}

void UMMOTradeComponent::ServerSetGold_Implementation(int32 NewGold)
{
	AMMOCharacter* Self = GetOwnerCharacter();
	const UMMOInventoryComponent* Inv = Self ? Self->GetInventory() : nullptr;
	if (!GetValidPartnerComp() || !Inv || !GetWorld())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastOfferChangeTime < OfferChangeCooldown)
	{
		return;
	}
	// Teklif sahibin sahip oldugu altini asamaz (takas aninda yine kontrol edilir).
	const int32 Clamped = FMath::Clamp(NewGold, 0, Inv->Gold);
	if (Clamped == MyOffer.Gold)
	{
		return;
	}
	LastOfferChangeTime = Now;
	MyOffer.Gold = Clamped;
	OnMyOfferChanged();
}

void UMMOTradeComponent::ServerConfirm_Implementation(int32 SeenRevision)
{
	UMMOTradeComponent* PC = GetValidPartnerComp();
	if (!PC || MyOffer.bConfirmed)
	{
		return;
	}

	// Onay, oyuncunun GORDUGU teklife baglidir: arada teklif degistiyse reddet.
	if (SeenRevision != OfferRevision)
	{
		NotifyOwner(TEXT("Teklif degisti; yeni teklifi gorup tekrar onayla."));
		return;
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	// Son degisiklikten hemen sonra onay kilidi + onay hiz siniri (spam / kapma yarisi).
	if (Now - LastTradeChangeTime < ConfirmLockSeconds)
	{
		NotifyOwner(TEXT("Teklif az once degisti; bir saniye bekleyip tekrar onayla."));
		return;
	}
	if (Now - LastConfirmTime < ConfirmCooldown)
	{
		return;
	}
	LastConfirmTime = Now;

	const bool bNothing = MyOffer.Items.Num() == 0 && MyOffer.Gold == 0
		&& PC->MyOffer.Items.Num() == 0 && PC->MyOffer.Gold == 0;
	if (bNothing)
	{
		NotifyOwner(TEXT("Teklifte hicbir sey yok."));
		return;
	}

	MyOffer.bConfirmed = true;
	PC->TheirOffer.bConfirmed = true;
	OnTradeChanged.Broadcast();
	PC->OnTradeChanged.Broadcast();
	PC->NotifyOwner(FString::Printf(TEXT("%s teklifi onayladi."), *TradeNameOf(GetOwnerCharacter())));

	TryExecuteTrade();
}

// ----------------------------------------------------------------------
// Takas durumu yonetimi (sunucu)
// ----------------------------------------------------------------------

void UMMOTradeComponent::StartTradeServer(AMMOCharacter* A, AMMOCharacter* B)
{
	UMMOTradeComponent* CA = A ? A->GetTradeComponent() : nullptr;
	UMMOTradeComponent* CB = B ? B->GetTradeComponent() : nullptr;
	if (!CA || !CB || CA == CB)
	{
		return;
	}

	auto Begin = [](UMMOTradeComponent* Self, AMMOCharacter* Other)
	{
		Self->Partner = Other;
		Self->bTradeActive = true;
		Self->PartnerName = TradeNameOf(Other);
		Self->MyOffer = FMMOTradeOffer();
		Self->TheirOffer = FMMOTradeOffer();
		Self->OfferRevision = 0;
		Self->LastTradeChangeTime = Self->GetWorld() ? Self->GetWorld()->GetTimeSeconds() : 0.f;
		Self->PendingInviteFrom.Reset();
		Self->PendingInviteTime = -1.f;
		if (UWorld* World = Self->GetWorld())
		{
			World->GetTimerManager().SetTimer(Self->ValidateTimerHandle, Self, &UMMOTradeComponent::ValidateTick, 0.25f, true);
		}
		Self->OnTradeChanged.Broadcast();
		Self->NotifyOwner(FString::Printf(TEXT("%s ile ticaret basladi."), *Self->PartnerName));
	};
	Begin(CA, B);
	Begin(CB, A);
}

void UMMOTradeComponent::ResetLocalState()
{
	bTradeActive = false;
	PartnerName.Reset();
	MyOffer = FMMOTradeOffer();
	TheirOffer = FMMOTradeOffer();
	Partner.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ValidateTimerHandle);
	}
	OnTradeChanged.Broadcast();
}

void UMMOTradeComponent::EndTradeServer(const FString& Reason)
{
	if (!HasAuthorityInternal())
	{
		return;
	}

	// Karsi tarafi, o da bizimle eslesmisse temizle (bayat esleme baska bir ticareti bozmasin).
	AMMOCharacter* P = Partner.Get();
	UMMOTradeComponent* PC = P ? P->GetTradeComponent() : nullptr;

	NotifyOwner(Reason);
	ResetLocalState();

	if (PC && PC != this && PC->bTradeActive && PC->Partner.Get() == GetOwner())
	{
		PC->NotifyOwner(Reason);
		PC->ResetLocalState();
	}
}

void UMMOTradeComponent::OnMyOfferChanged()
{
	// Teklif degisti: iki tarafin onayi sifirlanir (surpriz degisikligi onlemek icin).
	MyOffer.bConfirmed = false;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	++OfferRevision;
	LastTradeChangeTime = Now;
	if (UMMOTradeComponent* PC = GetValidPartnerComp())
	{
		PC->MyOffer.bConfirmed = false;
		PC->TheirOffer = MyOffer;
		PC->OfferRevision = OfferRevision; // iki tarafta ayni revizyon
		PC->LastTradeChangeTime = Now;
		PC->OnTradeChanged.Broadcast();
	}
	TheirOffer.bConfirmed = false;
	OnTradeChanged.Broadcast();
}

void UMMOTradeComponent::ValidateTick()
{
	if (!bTradeActive)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ValidateTimerHandle);
		}
		return;
	}

	AMMOCharacter* Self = GetOwnerCharacter();
	UMMOTradeComponent* PC = GetValidPartnerComp();
	AMMOCharacter* Other = PC ? PC->GetOwnerCharacter() : nullptr;
	if (!Self || !PC || !Other)
	{
		EndTradeServer(TEXT("Ticaret iptal edildi: karsi taraf ayrildi."));
		return;
	}
	if (TradeIsDead(Self) || TradeIsDead(Other))
	{
		EndTradeServer(TEXT("Ticaret iptal edildi: bir taraf oldu."));
		return;
	}
	if (FVector::Dist(Self->GetActorLocation(), Other->GetActorLocation()) > CancelDistance)
	{
		EndTradeServer(TEXT("Ticaret iptal edildi: oyuncular birbirinden uzaklasti."));
		return;
	}
	// Bosta kalan ticaret (AFK partner) pencereyi/kilitleri sonsuza dek acik tutmasin.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (Now - LastTradeChangeTime > IdleTimeoutSeconds)
	{
		EndTradeServer(TEXT("Ticaret iptal edildi: uzun suredir islem yapilmadi."));
	}
}

void UMMOTradeComponent::TryExecuteTrade()
{
	UMMOTradeComponent* PC = GetValidPartnerComp();
	AMMOCharacter* A = GetOwnerCharacter();
	AMMOCharacter* B = PC ? PC->GetOwnerCharacter() : nullptr;
	if (!PC || !A || !B || !MyOffer.bConfirmed || !PC->MyOffer.bConfirmed)
	{
		return;
	}

	UMMOInventoryComponent* IA = A->GetInventory();
	UMMOInventoryComponent* IB = B->GetInventory();
	if (!IA || !IB)
	{
		EndTradeServer(TEXT("Ticaret iptal edildi."));
		return;
	}

	// Her sey takas aninda sunucuda YENIDEN dogrulanir (teklif sonrasi satis/kusanma/yukseltme,
	// altin harcama, uzaklasma, olum vb. olmus olabilir).
	if (TradeIsDead(A) || TradeIsDead(B)
		|| FVector::Dist(A->GetActorLocation(), B->GetActorLocation()) > CancelDistance)
	{
		EndTradeServer(TEXT("Ticaret iptal edildi: oyuncular uygun degil."));
		return;
	}

	const FMMOTradeOffer OfferA = MyOffer;
	const FMMOTradeOffer OfferB = PC->MyOffer;
	if (!TradeSideStillValid(OfferA, IA) || !TradeSideStillValid(OfferB, IB))
	{
		EndTradeServer(TEXT("Ticaret iptal edildi: teklif edilen esya veya altin degisti."));
		return;
	}

	// Alicinin cantasinda yer var mi (gonderdikleri cikip gelenler girdikten sonra).
	const int32 BagA = IA->EquipmentBag.Num() - OfferA.Items.Num() + OfferB.Items.Num();
	const int32 BagB = IB->EquipmentBag.Num() - OfferB.Items.Num() + OfferA.Items.Num();
	if (BagA > UMMOInventoryComponent::MaxBagSize || BagB > UMMOInventoryComponent::MaxBagSize)
	{
		EndTradeServer(TEXT("Ticaret iptal edildi: bir tarafin cantasinda yer yok."));
		return;
	}

	// Altin tasmasi (AddGold sikistirir; sessizce altin yutmasin diye once reddet).
	const int64 GoldA = (int64)IA->Gold - OfferA.Gold + OfferB.Gold;
	const int64 GoldB = (int64)IB->Gold - OfferB.Gold + OfferA.Gold;
	if (GoldA > MAX_int32 || GoldB > MAX_int32)
	{
		EndTradeServer(TEXT("Ticaret iptal edildi: altin siniri asilir."));
		return;
	}

	// Atomik takas: once ikisinden de cikar, sonra karsiliklu ekle (ayni sunucu karesinde).
	const TArray<FMMOEquipmentInstance> BagBackupA = IA->EquipmentBag;
	const TArray<FMMOEquipmentInstance> BagBackupB = IB->EquipmentBag;
	TArray<FMMOEquipmentInstance> FromA, FromB;
	bool bTookAll = true;
	for (const FMMOEquipmentInstance& I : OfferA.Items)
	{
		FMMOEquipmentInstance Taken;
		if (IA->TakeEquipmentByInstanceId(I.InstanceId, Taken)) { FromA.Add(Taken); } else { bTookAll = false; }
	}
	for (const FMMOEquipmentInstance& I : OfferB.Items)
	{
		FMMOEquipmentInstance Taken;
		if (IB->TakeEquipmentByInstanceId(I.InstanceId, Taken)) { FromB.Add(Taken); } else { bTookAll = false; }
	}
	if (!bTookAll)
	{
		// Dogrulamadan sonra olmamali; yine de geri al, kimse bir sey kaybetmesin.
		for (const FMMOEquipmentInstance& I : FromA) { IA->ReceiveTradedEquipment(I); }
		for (const FMMOEquipmentInstance& I : FromB) { IB->ReceiveTradedEquipment(I); }
		EndTradeServer(TEXT("Ticaret iptal edildi: esya dogrulanamadi."));
		return;
	}

	// Alma basarisiz olursa (beklenmez; on dogrulama kapasiteyi ve veritabanini kontrol etti)
	// iki cantayi da takas oncesi haline dondur: kimse esya kaybetmesin.
	bool bReceivedAll = true;
	for (const FMMOEquipmentInstance& I : FromA) { bReceivedAll &= IB->ReceiveTradedEquipment(I); }
	for (const FMMOEquipmentInstance& I : FromB) { bReceivedAll &= IA->ReceiveTradedEquipment(I); }
	if (!bReceivedAll)
	{
		IA->EquipmentBag = BagBackupA;
		IB->EquipmentBag = BagBackupB;
		EndTradeServer(TEXT("Ticaret iptal edildi: esyalar teslim edilemedi."));
		return;
	}
	IA->AddGold(-OfferA.Gold);
	IB->AddGold(-OfferB.Gold);
	IA->AddGold(OfferB.Gold);
	IB->AddGold(OfferA.Gold);

	EndTradeServer(TEXT("Ticaret tamamlandi."));

	// Cokme/baglanti kopmasi halinde tek tarafli kayit (esya cogalmasi) kalmasin diye
	// iki karakteri de hemen kaydet.
	A->SaveCharacter();
	B->SaveCharacter();
}

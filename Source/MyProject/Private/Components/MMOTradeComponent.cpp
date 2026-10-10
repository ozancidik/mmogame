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

DEFINE_LOG_CATEGORY_STATIC(LogMMOTradeAudit, Log, All);

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

	FString DescribeTradeItems(const TArray<FMMOEquipmentInstance>& Items)
	{
		FString Result;
		for (const FMMOEquipmentInstance& Item : Items)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT(",");
			}
			Result += FString::Printf(TEXT("%s#%d+%d:r%d"), *Item.ItemId.ToString(), Item.InstanceId,
				Item.Plus, static_cast<int32>(Item.Rarity));
		}
		return Result.IsEmpty() ? TEXT("-") : Result;
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
	DOREPLIFETIME_CONDITION(UMMOTradeComponent, TradeSessionId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMMOTradeComponent, PendingInviteId, COND_OwnerOnly);
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

bool UMMOTradeComponent::IsSeenOfferCurrent(uint64 SeenSessionId, int32 SeenRevision) const
{
	if (!HasAuthorityInternal() || SeenSessionId == 0 || SeenSessionId != TradeSessionId || SeenRevision != OfferRevision)
	{
		return false;
	}
	const UMMOTradeComponent* PC = GetValidPartnerComp();
	return PC && PC->TradeSessionId == TradeSessionId && PC->OfferRevision == OfferRevision;
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
	RequestAcceptSeen(PendingInviteId);
}

void UMMOTradeComponent::RequestAcceptSeen(uint64 SeenInviteId)
{
	if (HasAuthorityInternal()) { ServerAccept_Implementation(SeenInviteId); } else { ServerAccept(SeenInviteId); }
}

void UMMOTradeComponent::RequestDeclineInviteSeen(uint64 SeenInviteId)
{
	if (HasAuthorityInternal()) { ServerDeclineInvite_Implementation(SeenInviteId); } else { ServerDeclineInvite(SeenInviteId); }
}

void UMMOTradeComponent::RequestCancel()
{
	if (bTradeActive) { RequestCancelSeen(TradeSessionId); }
	else { RequestDeclineInviteSeen(PendingInviteId); }
}

void UMMOTradeComponent::RequestCancelSeen(uint64 SeenSessionId)
{
	if (HasAuthorityInternal()) { ServerCancel_Implementation(SeenSessionId); }
	else { ServerCancel(SeenSessionId); }
}

void UMMOTradeComponent::RequestAddItem(int32 InstanceId)
{
	RequestAddItemSeen(TradeSessionId, OfferRevision, InstanceId);
}

void UMMOTradeComponent::RequestRemoveItem(int32 InstanceId)
{
	RequestRemoveItemSeen(TradeSessionId, OfferRevision, InstanceId);
}

void UMMOTradeComponent::RequestSetGold(int32 NewGold)
{
	RequestSetGoldSeen(TradeSessionId, OfferRevision, NewGold);
}

void UMMOTradeComponent::RequestAddItemSeen(uint64 SeenSessionId, int32 SeenRevision, int32 InstanceId)
{
	if (HasAuthorityInternal()) { ServerAddItem_Implementation(SeenSessionId, SeenRevision, InstanceId); }
	else { ServerAddItem(SeenSessionId, SeenRevision, InstanceId); }
}

void UMMOTradeComponent::RequestRemoveItemSeen(uint64 SeenSessionId, int32 SeenRevision, int32 InstanceId)
{
	if (HasAuthorityInternal()) { ServerRemoveItem_Implementation(SeenSessionId, SeenRevision, InstanceId); }
	else { ServerRemoveItem(SeenSessionId, SeenRevision, InstanceId); }
}

void UMMOTradeComponent::RequestSetGoldSeen(uint64 SeenSessionId, int32 SeenRevision, int32 NewGold)
{
	if (HasAuthorityInternal()) { ServerSetGold_Implementation(SeenSessionId, SeenRevision, NewGold); }
	else { ServerSetGold(SeenSessionId, SeenRevision, NewGold); }
}

void UMMOTradeComponent::RequestConfirm()
{
	RequestConfirmSeen(TradeSessionId, OfferRevision); // Console/C++ intent: current state.
}

void UMMOTradeComponent::RequestConfirmSeen(uint64 SeenSessionId, int32 SeenRevision)
{
	if (HasAuthorityInternal()) { ServerConfirm_Implementation(SeenSessionId, SeenRevision); }
	else { ServerConfirm(SeenSessionId, SeenRevision); }
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

	// Ayni daveti tekrar yollamak hedefteki zaman damgasini yenilemesin. Aksi halde
	// davetci her cooldown sonunda hedefi bildirimle doldurup daveti suresiz tutabiliyordu.
	if (TC->PendingInviteFrom.Get() == Self && TC->PendingInviteTime >= 0.f
		&& Now - TC->PendingInviteTime < InviteExpirySeconds)
	{
		NotifyOwner(TEXT("Bu oyuncuya ticaret daveti zaten gonderildi; yanitini bekle."));
		return;
	}

	// Davet kapma korumasi: hedefin gecerli baska bir bekleyen daveti varsa ezme.
	if (TC->PendingInviteFrom.IsValid() && TC->PendingInviteFrom.Get() != Self
		&& TC->PendingInviteTime >= 0.f && Now - TC->PendingInviteTime < InviteExpirySeconds)
	{
		NotifyOwner(TEXT("Oyuncunun bekleyen baska bir daveti var; biraz sonra tekrar dene."));
		return;
	}
	if (!TC->SetPendingInviteServer(Self)) return;
	TC->NotifyOwner(FString::Printf(TEXT("%s seninle ticaret etmek istiyor. Kabul: /trade kabul"), *TradeNameOf(Self)));
	NotifyOwner(FString::Printf(TEXT("%s adli oyuncuya ticaret daveti gonderildi."), *TradeNameOf(Target)));
}

bool UMMOTradeComponent::SetPendingInviteServer(AMMOCharacter* Inviter)
{
	const AMMOCharacter* Self = GetOwnerCharacter();
	const UWorld* World = GetWorld();
	if (!HasAuthorityInternal() || bTradeActive || !Self || !World || !Inviter || Inviter == Self
		|| !Inviter->HasAuthority() || Inviter->GetWorld() != World || PendingInviteId == MAX_uint64) return false;
	++PendingInviteId; // Never wrap to a previously accepted token in this recipient pawn's lifetime.
	PendingInviteFrom = Inviter;
	PendingInviteTime = World->GetTimeSeconds();
	return true;
}

void UMMOTradeComponent::ServerAccept_Implementation(uint64 SeenInviteId)
{
	if (!HasAuthorityInternal() || bTradeActive || SeenInviteId == 0 || SeenInviteId != PendingInviteId) return;
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

void UMMOTradeComponent::ServerCancel_Implementation(uint64 SeenSessionId)
{
	if (!HasAuthorityInternal() || !bTradeActive || SeenSessionId == 0 || SeenSessionId != TradeSessionId) return;
	// Revision does not gate cancellation: an offer change must not trap the player in a trade.
	const UMMOTradeComponent* PC = GetValidPartnerComp();
	if (PC && PC->TradeSessionId != TradeSessionId) return;
	EndTradeServer(TEXT("Ticaret iptal edildi."));
}

void UMMOTradeComponent::ServerDeclineInvite_Implementation(uint64 SeenInviteId)
{
	if (!HasAuthorityInternal() || bTradeActive || SeenInviteId == 0 || SeenInviteId != PendingInviteId) return;
	PendingInviteFrom.Reset();
	PendingInviteTime = -1.f;
}

void UMMOTradeComponent::ServerAddItem_Implementation(uint64 SeenSessionId, int32 SeenRevision, int32 InstanceId)
{
	if (!IsSeenOfferCurrent(SeenSessionId, SeenRevision)) return;
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

void UMMOTradeComponent::ServerRemoveItem_Implementation(uint64 SeenSessionId, int32 SeenRevision, int32 InstanceId)
{
	if (!IsSeenOfferCurrent(SeenSessionId, SeenRevision)) return;
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

void UMMOTradeComponent::ServerSetGold_Implementation(uint64 SeenSessionId, int32 SeenRevision, int32 NewGold)
{
	if (!IsSeenOfferCurrent(SeenSessionId, SeenRevision)) return;
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

void UMMOTradeComponent::ServerConfirm_Implementation(uint64 SeenSessionId, int32 SeenRevision)
{
	if (!HasAuthorityInternal()) return;
	UMMOTradeComponent* PC = GetValidPartnerComp();
	if (!PC || MyOffer.bConfirmed)
	{
		return;
	}

	// Onay, oyuncunun GORDUGU teklife baglidir: arada teklif degistiyse reddet.
	if (!IsSeenOfferCurrent(SeenSessionId, SeenRevision))
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

	// Callouts may replace this trade, revise its offers, or retire an endpoint.
	// Capture this approval's identity; reacquire weak endpoints after every callout.
	const TWeakObjectPtr<UMMOTradeComponent> Issuer(this), Recipient(PC);
	const FString Confirmation = FString::Printf(TEXT("%s teklifi onayladi."), *TradeNameOf(GetOwnerCharacter()));
	const auto CurrentApproval = [Issuer, Recipient, SeenSessionId, SeenRevision](bool RequireConfirmed)
	{
		const UMMOTradeComponent* A = Issuer.Get();
		const UMMOTradeComponent* B = Recipient.Get();
		const AMMOCharacter* CA = A ? A->GetOwnerCharacter() : nullptr;
		const AMMOCharacter* CB = B ? B->GetOwnerCharacter() : nullptr;
		return IsValid(CA) && IsValid(CB) && !CA->IsActorBeingDestroyed() && !CB->IsActorBeingDestroyed()
			&& CA->HasActorBegunPlay() && CB->HasActorBegunPlay() && !CA->HasStartedEndPlay() && !CB->HasStartedEndPlay()
			&& A->HasAuthorityInternal() && B->HasAuthorityInternal() && A->GetWorld() && A->GetWorld() == B->GetWorld()
			&& A->bTradeActive && B->bTradeActive && A->Partner.Get() == CB && B->Partner.Get() == CA
			&& A->TradeSessionId == SeenSessionId && B->TradeSessionId == SeenSessionId
			&& A->OfferRevision == SeenRevision && B->OfferRevision == SeenRevision
			&& (!RequireConfirmed || (A->MyOffer.bConfirmed && B->TheirOffer.bConfirmed));
	};
	if (!CurrentApproval(false)) return;
	MyOffer.bConfirmed = true;
	PC->TheirOffer.bConfirmed = true;
	// Publish the accepted approval before a delegate can cancel or replace it.
	Recipient.Get()->NotifyOwner(Confirmation);
	if (!CurrentApproval(true)) return;
	Issuer.Get()->OnTradeChanged.Broadcast();
	if (!CurrentApproval(true)) return;
	Recipient.Get()->OnTradeChanged.Broadcast();
	if (!CurrentApproval(true)) return;
	Issuer.Get()->TryExecuteTrade();
}

// ----------------------------------------------------------------------
// Takas durumu yonetimi (sunucu)
// ----------------------------------------------------------------------

void UMMOTradeComponent::StartTradeServer(AMMOCharacter* A, AMMOCharacter* B)
{
	// Destruction/EndPlay callbacks can still retain an actor pointer. Admission
	// must reject that endpoint before installing another session or timer.
	if (!IsValid(A) || !IsValid(B) || A->IsActorBeingDestroyed() || B->IsActorBeingDestroyed()
		|| !A->HasActorBegunPlay() || !B->HasActorBegunPlay() || A->HasStartedEndPlay() || B->HasStartedEndPlay()) return;
	UMMOTradeComponent* CA = A->GetTradeComponent();
	UMMOTradeComponent* CB = B->GetTradeComponent();
	if (!IsValid(CA) || !IsValid(CB) || CA == CB || !CA->HasAuthorityInternal() || !CB->HasAuthorityInternal()
		|| !CA->GetWorld() || CA->bTradeActive || CB->bTradeActive || CA->GetWorld() != CB->GetWorld())
	{
		return;
	}

	// Every participant advances beyond its own last generation, including when partners change.
	const uint64 PreviousSession = FMath::Max(CA->TradeSessionId, CB->TradeSessionId);
	if (PreviousSession == MAX_uint64) return; // Fail closed rather than reuse a generation after wrap.
	const uint64 NewSession = PreviousSession + 1;
	auto Begin = [NewSession](UMMOTradeComponent* Self, AMMOCharacter* Other)
	{
		Self->TradeSessionId = NewSession;
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
	};
	Begin(CA, B);
	Begin(CB, A);
	// Capture original payloads; publish both messages before observers may cancel
	// or restart. Every publication revalidates the original reciprocal generation.
	const TWeakObjectPtr<UMMOTradeComponent> Endpoints[] = {CA, CB};
	const FString Messages[] = {
		FString::Printf(TEXT("%s ile ticaret basladi."), *CA->PartnerName),
		FString::Printf(TEXT("%s ile ticaret basladi."), *CB->PartnerName)};
	const auto CurrentPair = [&Endpoints, NewSession]()
	{
		const UMMOTradeComponent* X = Endpoints[0].Get();
		const UMMOTradeComponent* Y = Endpoints[1].Get();
		const AMMOCharacter* OX = X ? X->GetOwnerCharacter() : nullptr;
		const AMMOCharacter* OY = Y ? Y->GetOwnerCharacter() : nullptr;
		return X && Y && IsValid(OX) && IsValid(OY)
			&& !OX->IsActorBeingDestroyed() && !OY->IsActorBeingDestroyed()
			&& OX->HasActorBegunPlay() && OY->HasActorBegunPlay() && !OX->HasStartedEndPlay() && !OY->HasStartedEndPlay()
			&& OX->HasAuthority() && OY->HasAuthority()
			&& X->bTradeActive && Y->bTradeActive && X->TradeSessionId == NewSession && Y->TradeSessionId == NewSession
			&& X->Partner.Get() == OY && Y->Partner.Get() == OX;
	};
	for (int32 I = 0; I < 2; ++I)
	{
		if (CurrentPair()) Endpoints[I].Get()->NotifyOwner(Messages[I]);
	}
	for (const auto& Endpoint : Endpoints)
	{
		if (CurrentPair()) Endpoint.Get()->OnTradeChanged.Broadcast();
	}
}

void UMMOTradeComponent::ResetLocalState()
{
	bTradeActive = false;
	PartnerName.Reset();
	MyOffer = FMMOTradeOffer();
	TheirOffer = FMMOTradeOffer();
	Partner.Reset();
	// Keep TradeSessionId: the next session must never reuse this pawn's generation.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ValidateTimerHandle);
	}
}

void UMMOTradeComponent::EndTradeServer(const FString& Reason, bool bSaveCompletedInventories)
{
	if (!HasAuthorityInternal() || !bTradeActive)
	{
		return;
	}

	// Karsi tarafi, o da bizimle eslesmisse temizle (bayat esleme baska bir ticareti bozmasin).
	AMMOCharacter* P = Partner.Get();
	UMMOTradeComponent* PC = P ? P->GetTradeComponent() : nullptr;
	const uint64 EndingSession = TradeSessionId;
	const FString EndingReason = Reason;
	const bool bResetPeer = PC && PC != this && PC->bTradeActive
		&& PC->Partner.Get() == GetOwner() && PC->TradeSessionId == EndingSession;
	const TWeakObjectPtr<UMMOTradeComponent> Endpoints[] = {this, bResetPeer ? PC : nullptr};
	if (!EndingReason.Equals(TEXT("Ticaret tamamlandi."), ESearchCase::CaseSensitive))
	{
		UE_LOG(LogMMOTradeAudit, Display,
			TEXT("event=trade_cancel a=\"%s\" b=\"%s\" revision=%d reason=\"%s\""),
			*TradeNameOf(GetOwnerCharacter()), *TradeNameOf(P), OfferRevision, *EndingReason);
	}

	// Commit both old states and timers before any synchronous observer can restart.
	ResetLocalState();
	if (bResetPeer) PC->ResetLocalState();
	// Successful settlement is already committed on both inventories. Close the
	// session before saving too: a store/notification must not reenter an old trade.
	if (bSaveCompletedInventories)
	{
		for (const auto& Endpoint : Endpoints)
		{
			UMMOTradeComponent* T = Endpoint.Get();
			AMMOCharacter* C = T ? T->GetOwnerCharacter() : nullptr;
			if (IsValid(C) && !C->IsActorBeingDestroyed() && C->HasActorBegunPlay()
				&& !C->HasStartedEndPlay() && C->HasAuthority()) C->SaveCharacter();
		}
	}

	// An observer can replace a session or destroy an endpoint. Never publish an old
	// teardown to that replacement; cleanup itself still runs during actor EndPlay.
	const auto EndedEndpoint = [EndingSession](const TWeakObjectPtr<UMMOTradeComponent>& Endpoint)
	{
		UMMOTradeComponent* T = Endpoint.Get();
		const AMMOCharacter* Owner = T ? T->GetOwnerCharacter() : nullptr;
		return T && IsValid(Owner) && !Owner->IsActorBeingDestroyed() && Owner->HasActorBegunPlay()
			&& !Owner->HasStartedEndPlay() && !T->bTradeActive && T->TradeSessionId == EndingSession ? T : nullptr;
	};
	for (const auto& Endpoint : Endpoints)
	{
		if (UMMOTradeComponent* T = EndedEndpoint(Endpoint)) T->NotifyOwner(EndingReason);
	}
	for (const auto& Endpoint : Endpoints)
	{
		if (UMMOTradeComponent* T = EndedEndpoint(Endpoint)) T->OnTradeChanged.Broadcast();
	}
}

void UMMOTradeComponent::OnMyOfferChanged()
{
	// Teklif degisti: iki tarafin onayi sifirlanir (surpriz degisikligi onlemek icin).
	MyOffer.bConfirmed = false;
	TheirOffer.bConfirmed = false;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	++OfferRevision;
	LastTradeChangeTime = Now;
	UMMOTradeComponent* PC = GetValidPartnerComp();
	if (PC)
	{
		PC->MyOffer.bConfirmed = false;
		PC->TheirOffer = MyOffer;
		PC->OfferRevision = OfferRevision; // iki tarafta ayni revizyon
		PC->LastTradeChangeTime = Now;
	}
	// All mirrors and clocks are committed before callouts. A listener may
	// replace the session or edit either offer again; the old frame publishes
	// only while its captured pair, session and revision are still current.
	const TWeakObjectPtr<UMMOTradeComponent> Issuer(this), Recipient(PC);
	const uint64 Session = TradeSessionId;
	const int32 Revision = OfferRevision;
	const auto CurrentOffer = [Issuer, Recipient, Session, Revision]()
	{
		const UMMOTradeComponent* A = Issuer.Get();
		const UMMOTradeComponent* B = Recipient.Get();
		const AMMOCharacter* CA = A ? A->GetOwnerCharacter() : nullptr;
		const AMMOCharacter* CB = B ? B->GetOwnerCharacter() : nullptr;
		return IsValid(CA) && IsValid(CB) && !CA->IsActorBeingDestroyed() && !CB->IsActorBeingDestroyed()
			&& CA->HasActorBegunPlay() && CB->HasActorBegunPlay() && !CA->HasStartedEndPlay() && !CB->HasStartedEndPlay()
			&& A->HasAuthorityInternal() && B->HasAuthorityInternal() && A->GetWorld() && A->GetWorld() == B->GetWorld()
			&& A->bTradeActive && B->bTradeActive && A->Partner.Get() == CB && B->Partner.Get() == CA
			&& A->TradeSessionId == Session && B->TradeSessionId == Session
			&& A->OfferRevision == Revision && B->OfferRevision == Revision;
	};
	if (!CurrentOffer()) return;
	Recipient.Get()->OnTradeChanged.Broadcast();
	if (!CurrentOffer()) return;
	Issuer.Get()->OnTradeChanged.Broadcast();
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

	// Altin tavani asimi (AddGold sikistirir; sessizce altin yutmasin diye once reddet).
	const int64 GoldA = (int64)IA->Gold - OfferA.Gold + OfferB.Gold;
	const int64 GoldB = (int64)IB->Gold - OfferB.Gold + OfferA.Gold;
	if (GoldA > UMMOInventoryComponent::MaxGold || GoldB > UMMOInventoryComponent::MaxGold)
	{
		EndTradeServer(TEXT("Ticaret iptal edildi: altin siniri asilir."));
		return;
	}

	// Plan both bags on copies. No inventory delegate may observe a half trade,
	// and a rejected allocation leaves bag contents and both ID counters intact.
	const auto PlanBag = [](const UMMOInventoryComponent* Inv, const FMMOTradeOffer& Outgoing,
		const FMMOTradeOffer& Incoming, int32 Next, TArray<FMMOEquipmentInstance>& Bag, int32& NewNext)
	{
		Bag = Inv->EquipmentBag;
		NewNext = Next;
		for (const auto& Item : Outgoing.Items)
		{
			if (Bag.RemoveAll([&Item](const FMMOEquipmentInstance& I) { return I.InstanceId == Item.InstanceId; }) != 1) return false;
		}
		for (const auto& Item : Incoming.Items)
		{
			if (NewNext <= 0 || NewNext == MAX_int32) return false;
			const auto Used = [NewNext](const FMMOEquipmentInstance& I) { return I.InstanceId == NewNext; };
			if (Bag.ContainsByPredicate(Used) || Inv->EquipmentBag.ContainsByPredicate(Used)
				|| Inv->EquippedItems.ContainsByPredicate(Used) || Inv->Warehouse.ContainsByPredicate(Used)) return false;
			FMMOEquipmentInstance Received = Item;
			Received.InstanceId = NewNext++;
			Bag.Add(Received);
		}
		return Bag.Num() <= UMMOInventoryComponent::MaxBagSize;
	};
	TArray<FMMOEquipmentInstance> NewBagA, NewBagB;
	int32 NextA = IA->NextInstanceId, NextB = IB->NextInstanceId;
	if (!PlanBag(IA, OfferA, OfferB, NextA, NewBagA, NextA)
		|| !PlanBag(IB, OfferB, OfferA, NextB, NewBagB, NextB))
	{
		EndTradeServer(TEXT("Ticaret iptal edildi: esyalar teslim edilemedi."));
		return;
	}
	const bool bBagChanged = !OfferA.Items.IsEmpty() || !OfferB.Items.IsEmpty();
	const bool bGoldChanged = OfferA.Gold != 0 || OfferB.Gold != 0;
	const TWeakObjectPtr<UMMOInventoryComponent> Inventories[] = {IA, IB};
	IA->EquipmentBag = MoveTemp(NewBagA);
	IB->EquipmentBag = MoveTemp(NewBagB);
	IA->NextInstanceId = NextA;
	IB->NextInstanceId = NextB;
	// Preserve the four mutation metrics, suppress only synchronous observers.
	IA->AddGoldInternal(-OfferA.Gold, nullptr, false);
	IB->AddGoldInternal(-OfferB.Gold, nullptr, false);
	IA->AddGoldInternal(OfferB.Gold, nullptr, false);
	IB->AddGoldInternal(OfferA.Gold, nullptr, false);
	const TArray<FMMOEquipmentInstance>& FromA = OfferA.Items;
	const TArray<FMMOEquipmentInstance>& FromB = OfferB.Items;
	// NETWORK_AUDIT O12: ekonomi mutasyonu tamamlaninca iki taraf, altin ve ekipman kimliklerini
	// tek yapilandirilmis sunucu satirinda kaydet. Log mutasyondan sonra, state sifirlanmadan once
	// yazilir; basarisiz/geri alinan denemeler trade_cancel olarak ayri izlenir.
	UE_LOG(LogMMOTradeAudit, Display,
		TEXT("event=trade_complete a=\"%s\" b=\"%s\" gold_a_to_b=%d gold_b_to_a=%d items_a_to_b=\"%s\" items_b_to_a=\"%s\" balance_a=%d balance_b=%d"),
		*TradeNameOf(A), *TradeNameOf(B), OfferA.Gold, OfferB.Gold,
		*DescribeTradeItems(FromA), *DescribeTradeItems(FromB), IA->Gold, IB->Gold);

	EndTradeServer(TEXT("Ticaret tamamlandi."), true);

	// These are inventory refreshes, not old trade publications. A listener may
	// start a new session or mutate balances; publish current values and reacquire
	// each endpoint before every callout. Never touch retired owners.
	const auto LiveInventory = [](const TWeakObjectPtr<UMMOInventoryComponent>& Weak)
	{
		UMMOInventoryComponent* Inv = Weak.Get();
		AMMOCharacter* C = Inv ? Cast<AMMOCharacter>(Inv->GetOwner()) : nullptr;
		return IsValid(C) && !C->IsActorBeingDestroyed() && C->HasActorBegunPlay()
			&& !C->HasStartedEndPlay() && C->HasAuthority() && C->GetInventory() == Inv ? Inv : nullptr;
	};
	for (const auto& Weak : Inventories)
	{
		if (bBagChanged) if (auto* Inv = LiveInventory(Weak)) Inv->OnInventoryChanged.Broadcast();
		if (bGoldChanged) if (auto* Inv = LiveInventory(Weak)) Inv->OnGoldChanged.Broadcast(Inv->Gold);
	}
}

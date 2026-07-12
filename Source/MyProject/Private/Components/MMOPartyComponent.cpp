// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMOPartyComponent.h"
#include "Characters/MMOCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Net/UnrealNetwork.h"

UMMOPartyComponent::UMMOPartyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMMOPartyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMMOPartyComponent, Members);
}

bool UMMOPartyComponent::HasAuthorityInternal() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->HasAuthority() : false;
}

bool UMMOPartyComponent::IsLocallyControlledOwner() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return OwnerPawn ? OwnerPawn->IsLocallyControlled() : false;
}

AMMOCharacter* UMMOPartyComponent::GetOwnerCharacter() const
{
	return Cast<AMMOCharacter>(GetOwner());
}

void UMMOPartyComponent::OnRep_Members()
{
	OnPartyChanged.Broadcast();
}

AMMOCharacter* UMMOPartyComponent::FindNearestOtherPlayer(float MaxRange) const
{
	AMMOCharacter* Self = GetOwnerCharacter();
	UWorld* World = GetWorld();
	if (!Self || !World)
	{
		return nullptr;
	}

	AMMOCharacter* Best = nullptr;
	float BestDist = MaxRange;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		AMMOCharacter* C = PC ? Cast<AMMOCharacter>(PC->GetPawn()) : nullptr;
		if (!C || C == Self)
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

void UMMOPartyComponent::GetPartyInRange(const FVector& Origin, float Range, TArray<AMMOCharacter*>& Out) const
{
	const float R2 = Range * Range;

	if (AMMOCharacter* Self = GetOwnerCharacter())
	{
		if (FVector::DistSquared(Self->GetActorLocation(), Origin) <= R2)
		{
			Out.AddUnique(Self);
		}
	}
	for (AMMOCharacter* M : Members)
	{
		if (M && FVector::DistSquared(M->GetActorLocation(), Origin) <= R2)
		{
			Out.AddUnique(M);
		}
	}
}

// ----------------------------------------------------------------------
// Istemci istekleri
// ----------------------------------------------------------------------

void UMMOPartyComponent::RequestInviteNearest()
{
	if (HasAuthorityInternal())
	{
		ServerInviteNearest_Implementation();
	}
	else
	{
		ServerInviteNearest();
	}
}

void UMMOPartyComponent::RequestAccept()
{
	if (HasAuthorityInternal())
	{
		ServerAccept_Implementation();
	}
	else
	{
		ServerAccept();
	}
}

void UMMOPartyComponent::RequestLeave()
{
	if (HasAuthorityInternal())
	{
		ServerLeave_Implementation();
	}
	else
	{
		ServerLeave();
	}
}

// ----------------------------------------------------------------------
// Sunucu tarafi
// ----------------------------------------------------------------------

void UMMOPartyComponent::ServerInviteNearest_Implementation()
{
	AMMOCharacter* Inviter = GetOwnerCharacter();
	if (!Inviter)
	{
		return;
	}

	AMMOCharacter* Target = FindNearestOtherPlayer(1600.f);
	if (!Target)
	{
		if (GEngine && Inviter->IsLocallyControlled())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Yakinda davet edilecek oyuncu yok"));
		}
		return;
	}

	UMMOPartyComponent* TP = Target->GetPartyComponent();
	if (!TP)
	{
		return;
	}

	TP->PendingInviteFrom = Inviter;
	TP->ClientNotifyInvited(Inviter);

	if (GEngine && Inviter->IsLocallyControlled())
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor(120, 220, 120), TEXT("Parti daveti gonderildi"));
	}
}

void UMMOPartyComponent::ServerAccept_Implementation()
{
	AMMOCharacter* Invitee = GetOwnerCharacter();
	AMMOCharacter* Inviter = PendingInviteFrom.Get();
	PendingInviteFrom = nullptr;

	if (!Invitee || !Inviter || Invitee == Inviter)
	{
		return;
	}
	FormOrJoin(Inviter, Invitee);
}

void UMMOPartyComponent::ServerLeave_Implementation()
{
	LeavePartyServer(GetOwnerCharacter());
}

void UMMOPartyComponent::ClientNotifyInvited_Implementation(AMMOCharacter* From)
{
	// Sadece bilgilendirme; kabul kararini sunucudaki PendingInviteFrom belirler.
	PendingInviteFrom = From;
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor(120, 200, 255),
			TEXT("Parti daveti aldin! Kabul icin konsola: MMOPartyAccept"));
	}
}

// ----------------------------------------------------------------------
// Parti kumesi yonetimi (sunucu)
// ----------------------------------------------------------------------

void UMMOPartyComponent::ApplyPartySet(const TArray<AMMOCharacter*>& FullSet)
{
	for (AMMOCharacter* M : FullSet)
	{
		if (!M)
		{
			continue;
		}
		UMMOPartyComponent* PC = M->GetPartyComponent();
		if (!PC)
		{
			continue;
		}

		PC->Members.Reset();
		for (AMMOCharacter* Other : FullSet)
		{
			if (Other && Other != M)
			{
				PC->Members.Add(Other);
			}
		}
		// Sunucudaki (host) dinleyicileri hemen tetikle; istemcilerde OnRep yapar.
		PC->OnPartyChanged.Broadcast();
	}
}

void UMMOPartyComponent::FormOrJoin(AMMOCharacter* Inviter, AMMOCharacter* Invitee)
{
	if (!Inviter || !Invitee || Inviter == Invitee)
	{
		return;
	}

	TArray<AMMOCharacter*> Set;
	auto AddUniqueChar = [&Set](AMMOCharacter* C)
	{
		if (C && !Set.Contains(C))
		{
			Set.Add(C);
		}
	};

	AddUniqueChar(Inviter);
	if (UMMOPartyComponent* IP = Inviter->GetPartyComponent())
	{
		for (AMMOCharacter* M : IP->Members) { AddUniqueChar(M); }
	}
	AddUniqueChar(Invitee);
	if (UMMOPartyComponent* VP = Invitee->GetPartyComponent())
	{
		for (AMMOCharacter* M : VP->Members) { AddUniqueChar(M); }
	}

	const int32 MaxParty = 8;
	if (Set.Num() > MaxParty)
	{
		Set.SetNum(MaxParty);
	}

	ApplyPartySet(Set);
}

void UMMOPartyComponent::LeavePartyServer(AMMOCharacter* Leaver)
{
	if (!Leaver)
	{
		return;
	}
	UMMOPartyComponent* LP = Leaver->GetPartyComponent();
	if (!LP || LP->Members.Num() == 0)
	{
		return;
	}

	// Ayrilanin partideki digerleri.
	TArray<AMMOCharacter*> Others;
	for (AMMOCharacter* M : LP->Members)
	{
		if (M)
		{
			Others.Add(M);
		}
	}

	// Ayrilani partisiz yap.
	LP->Members.Reset();
	LP->OnPartyChanged.Broadcast();

	// Kalanlari yeniden ayarla (tek kisi kalirsa o da partisiz olur).
	if (Others.Num() >= 2)
	{
		ApplyPartySet(Others);
	}
	else if (Others.Num() == 1)
	{
		if (UMMOPartyComponent* OP = Others[0]->GetPartyComponent())
		{
			OP->Members.Reset();
			OP->OnPartyChanged.Broadcast();
		}
	}
}

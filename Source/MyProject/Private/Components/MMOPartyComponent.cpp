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
	DOREPLIFETIME(UMMOPartyComponent, Leader);
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

void UMMOPartyComponent::OnRep_Leader()
{
	OnPartyChanged.Broadcast();
}

bool UMMOPartyComponent::IsPartyLeader() const
{
	return Members.Num() > 0 && Leader != nullptr && Leader == GetOwnerCharacter();
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

void UMMOPartyComponent::RequestKick(AMMOCharacter* Target)
{
	if (HasAuthorityInternal())
	{
		ServerKick_Implementation(Target);
	}
	else
	{
		ServerKick(Target);
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

	// Sunucu-otoriter davet spam korumasi: art arda cagrilirsa hedefin ekranini
	// bildirimle doldurup oyunu oynanmaz hale getirebiliyordu (throttle yoktu).
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	if (Now - LastInviteTime < InviteCooldown)
	{
		return;
	}
	LastInviteTime = Now;

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
	TP->PendingInviteTime = Now;
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
	const float InviteTime = PendingInviteTime;
	PendingInviteFrom = nullptr;
	PendingInviteTime = -1.f;

	if (!Invitee || !Inviter || Invitee == Inviter)
	{
		return;
	}

	// Sunucu-otoriter dogrulama: davet SURESIZ bekleyip her zaman kabul edilebiliyordu
	// (davet eden cok uzaklassa/uzun sure gecse bile parti kuruluyordu). Sure + mesafe
	// kontrolu ekle; biri saglanmazsa daveti sessizce dusur.
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	if (InviteTime < 0.f || Now - InviteTime > InviteExpirySeconds)
	{
		if (GEngine && IsLocallyControlledOwner())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Davetin suresi doldu"));
		}
		return;
	}
	if (FVector::Dist(Invitee->GetActorLocation(), Inviter->GetActorLocation()) > InviteAcceptMaxDistance)
	{
		if (GEngine && IsLocallyControlledOwner())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Davet eden cok uzakta"));
		}
		return;
	}

	FormOrJoin(Inviter, Invitee);
}

void UMMOPartyComponent::ServerLeave_Implementation()
{
	LeavePartyServer(GetOwnerCharacter());
}

void UMMOPartyComponent::ServerKick_Implementation(AMMOCharacter* Target)
{
	AMMOCharacter* Self = GetOwnerCharacter();
	if (!Self || !Target || Target == Self)
	{
		return;
	}

	// Sunucu-otoriter dogrulama: sadece lider, kendi partisindeki bir uyeyi cikarabilir.
	if (!IsPartyLeader() || !Members.Contains(Target))
	{
		if (GEngine && Self->IsLocallyControlled())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Sadece parti lideri uye cikarabilir"));
		}
		return;
	}

	UMMOPartyComponent* TP = Target->GetPartyComponent();

	// Cikarma, normal ayrilmayla ayni yolu kullanir.
	LeavePartyServer(Target);

	if (TP)
	{
		TP->ClientNotifyKicked();
	}
	if (GEngine && Self->IsLocallyControlled())
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor(220, 180, 120), TEXT("Uye partiden cikarildi"));
	}
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

void UMMOPartyComponent::ClientNotifyKicked_Implementation()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor(255, 170, 120), TEXT("Parti lideri seni partiden cikardi"));
	}
}

// ----------------------------------------------------------------------
// Parti kumesi yonetimi (sunucu)
// ----------------------------------------------------------------------

void UMMOPartyComponent::ApplyPartySet(const TArray<AMMOCharacter*>& FullSet, AMMOCharacter* NewLeader)
{
	// Lider kumenin disinda kaldiysa (ya da verilmediyse) ilk uye lider olur —
	// boylece parti asla lidersiz kalmaz.
	if (!NewLeader || !FullSet.Contains(NewLeader))
	{
		NewLeader = FullSet.Num() > 0 ? FullSet[0] : nullptr;
	}

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

		PC->Leader = NewLeader;
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

	// Iki (kalabalik) parti birlesip MaxParty'yi asarsa: SetNum ile sessizce kirpmak
	// disaridaki uyeleri ApplyPartySet'in disinda birakirdi - onlarin KENDI Members
	// dizisi hic dokunulmadan eski (artik yanlis/asimetrik) haliyle kalirdi (hayalet
	// parti: A hala B'yi partili saniyor ama B'nin listesinde A yok). Kirpma yerine
	// birlesmeyi tumden reddet; hicbir tarafin mevcut partisi bozulmasin.
	const int32 MaxParty = 8;
	if (Set.Num() > MaxParty)
	{
		if (GEngine && Inviter->IsLocallyControlled())
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Parti dolu (maksimum 8)"));
		}
		return;
	}

	// Davet eden zaten bir partideyse o partinin lideri korunur; yoksa partiyi kuran
	// (davet eden) lider olur.
	AMMOCharacter* NewLeader = Inviter;
	if (const UMMOPartyComponent* IP = Inviter->GetPartyComponent())
	{
		AMMOCharacter* ExistingLeader = IP->Leader.Get();
		if (IP->Members.Num() > 0 && ExistingLeader && Set.Contains(ExistingLeader))
		{
			NewLeader = ExistingLeader;
		}
	}

	ApplyPartySet(Set, NewLeader);
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

	// Ayrilan lider ise liderlik kalanlardan birine gecer (ApplyPartySet, kumede
	// olmayan lideri ilk uyeyle degistirir); parti dagilmaz.
	AMMOCharacter* OldLeader = LP->Leader.Get();

	// Ayrilani partisiz yap.
	LP->Members.Reset();
	LP->Leader = nullptr;
	LP->OnPartyChanged.Broadcast();

	// Kalanlari yeniden ayarla (tek kisi kalirsa o da partisiz olur).
	if (Others.Num() >= 2)
	{
		ApplyPartySet(Others, OldLeader);
	}
	else if (Others.Num() == 1)
	{
		if (UMMOPartyComponent* OP = Others[0]->GetPartyComponent())
		{
			OP->Members.Reset();
			OP->Leader = nullptr;
			OP->OnPartyChanged.Broadcast();
		}
	}
}

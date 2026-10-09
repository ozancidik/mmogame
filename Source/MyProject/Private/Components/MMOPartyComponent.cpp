// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMOPartyComponent.h"
#include "Characters/MMOCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Net/UnrealNetwork.h"
#include "Core/MMONotify.h"
#include "GameFramework/PlayerState.h"
#if !UE_BUILD_SHIPPING
#include "Components/MMOInventoryComponent.h"
#include "Player/MMOPlayerController.h"
#include "Save/MMOCharacterSaveJson.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#endif

namespace
{
	FString PartyNameOf(const AMMOCharacter* C)
	{
		if (C)
		{
			if (const APlayerState* PS = C->GetPlayerState())
			{
				return PS->GetPlayerName();
			}
		}
		return TEXT("Oyuncu");
	}
}

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
	DOREPLIFETIME_CONDITION(UMMOPartyComponent, PendingInviteId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMMOPartyComponent, PartySessionId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMMOPartyComponent, PartyMembershipId, COND_OwnerOnly);
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
	RequestAcceptSeen(PendingInviteId);
}

void UMMOPartyComponent::RequestAcceptSeen(uint64 SeenInviteId)
{
	if (HasAuthorityInternal())
	{
		ServerAccept_Implementation(SeenInviteId);
	}
	else
	{
		ServerAccept(SeenInviteId);
	}
}

void UMMOPartyComponent::RequestDecline()
{
	RequestDeclineSeen(PendingInviteId);
}

void UMMOPartyComponent::RequestDeclineSeen(uint64 SeenInviteId)
{
	if (HasAuthorityInternal())
	{
		ServerDecline_Implementation(SeenInviteId);
	}
	else
	{
		ServerDecline(SeenInviteId);
	}
}

void UMMOPartyComponent::RequestLeave()
{
	RequestLeaveSeen(PartyMembershipId);
}

void UMMOPartyComponent::RequestLeaveSeen(const FGuid& SeenMembershipId)
{
	if (HasAuthorityInternal())
	{
		ServerLeave_Implementation(SeenMembershipId);
	}
	else
	{
		ServerLeave(SeenMembershipId);
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
		MMONotify::ToOwner(Inviter, TEXT("Yakinda davet edilecek oyuncu yok"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	UMMOPartyComponent* TP = Target->GetPartyComponent();
	if (!TP)
	{
		return;
	}
	if (Members.Contains(Target) || !CanCombineParties(Inviter, Target))
	{
		MMONotify::ToOwner(Inviter, TEXT("Mevcut partileri birlestirmek icin parti liderlerini davet edin."), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	// Hedefteki gecerli daveti ezme. Daveti kosulsuz degistirmek, kotu niyetli bir
	// oyuncunun gercek daveti kurban kabul etmeden hemen once kapmasina izin veriyordu.
	// Davetciye geri bildirim verilir (eskiden sessizdi: davet gitti mi, reddedildi mi ayirt edilemiyordu).
	if (TP->PendingInviteFrom.Get() == Inviter && TP->PendingInviteTime >= 0.f
		&& Now - TP->PendingInviteTime < InviteExpirySeconds)
	{
		// ayni daveti yenileyip sureyi uzatma
		MMONotify::ToOwner(Inviter, TEXT("Bu oyuncuya parti daveti zaten gonderildi; yanitini bekle."), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}
	if (TP->PendingInviteFrom.IsValid() && TP->PendingInviteFrom.Get() != Inviter
		&& TP->PendingInviteTime >= 0.f && Now - TP->PendingInviteTime < InviteExpirySeconds)
	{
		// baska oyuncunun bekleyen davetini kapma
		MMONotify::ToOwner(Inviter, TEXT("Oyuncunun bekleyen baska bir daveti var; biraz sonra tekrar dene."), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	if (!TP->SetPendingInviteServer(Inviter)) return;
	TP->ClientNotifyInvited(Inviter);

	MMONotify::ToOwner(Inviter, TEXT("Parti daveti gonderildi"), FColor(120, 220, 120), 2.f, MMONotify::EMMONotifyKind::Hint);
}

bool UMMOPartyComponent::SetPendingInviteServer(AMMOCharacter* Inviter)
{
	const AMMOCharacter* Self = GetOwnerCharacter();
	const UWorld* World = GetWorld();
	if (!HasAuthorityInternal() || !Self || !World || !Inviter || Inviter == Self
		|| !Inviter->HasAuthority() || Inviter->GetWorld() != World || PendingInviteId == MAX_uint64) return false;
	UMMOPartyComponent* Source = Inviter->GetPartyComponent();
	if (!Source) return false;
	if (!Source->PartyInviteContextId.IsValid()) Source->PartyInviteContextId = FGuid::NewGuid();
	PendingInviterContextId = Source->PartyInviteContextId;
	++PendingInviteId; // Do not reuse a consumed invitation during this pawn's lifetime.
	PendingInviteFrom = Inviter;
	PendingInviteTime = World->GetTimeSeconds();
	return true;
}

void UMMOPartyComponent::ServerDecline_Implementation(uint64 SeenInviteId)
{
	if (!HasAuthorityInternal() || SeenInviteId == 0 || SeenInviteId != PendingInviteId) return;
	AMMOCharacter* Self = GetOwnerCharacter();
	AMMOCharacter* Inviter = PendingInviteFrom.Get();
	const float InviteTime = PendingInviteTime;
	PendingInviteFrom = nullptr;
	PendingInviteTime = -1.f;
	PendingInviterContextId.Invalidate();
	if (!Self)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	if (!Inviter || Inviter == Self || InviteTime < 0.f || Now - InviteTime > InviteExpirySeconds)
	{
		MMONotify::ToOwner(Self, TEXT("Bekleyen parti daveti yok."), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	MMONotify::ToOwner(Self, TEXT("Parti daveti reddedildi."), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
	MMONotify::ToOwner(Inviter, FString::Printf(TEXT("%s parti davetini reddetti."), *PartyNameOf(Self)), FColor(255, 170, 120), 3.f);
}

void UMMOPartyComponent::ServerAccept_Implementation(uint64 SeenInviteId)
{
	if (!HasAuthorityInternal() || SeenInviteId == 0 || SeenInviteId != PendingInviteId) return;
	AMMOCharacter* Invitee = GetOwnerCharacter();
	AMMOCharacter* Inviter = PendingInviteFrom.Get();
	const float InviteTime = PendingInviteTime;
	const FGuid SourceContext = PendingInviterContextId;
	PendingInviteFrom = nullptr;
	PendingInviteTime = -1.f;
	PendingInviterContextId.Invalidate();

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
		MMONotify::ToOwner(Invitee, TEXT("Davetin suresi doldu"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}
	if (FVector::Dist(Invitee->GetActorLocation(), Inviter->GetActorLocation()) > InviteAcceptMaxDistance)
	{
		MMONotify::ToOwner(Invitee, TEXT("Davet eden cok uzakta"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	const UMMOPartyComponent* Source = Inviter->GetPartyComponent();
	if (!Source || !SourceContext.IsValid() || SourceContext != Source->PartyInviteContextId)
	{
		MMONotify::ToOwner(Invitee, TEXT("Davet edenin parti durumu degisti; yeni davet isteyin."), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	FormOrJoin(Inviter, Invitee);
}

void UMMOPartyComponent::ServerLeave_Implementation(const FGuid& SeenMembershipId)
{
	if (!HasAuthorityInternal() || !IsInParty() || !SeenMembershipId.IsValid()
		|| SeenMembershipId != PartyMembershipId) return;
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
		MMONotify::ToOwner(Self, TEXT("Sadece parti lideri uye cikarabilir"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	UMMOPartyComponent* TP = Target->GetPartyComponent();

	// Cikarma, normal ayrilmayla ayni yolu kullanir.
	LeavePartyServer(Target);

	if (TP)
	{
		TP->ClientNotifyKicked();
	}
	MMONotify::ToOwner(Self, TEXT("Uye partiden cikarildi"), FColor(220, 180, 120), 2.f);
}

void UMMOPartyComponent::ClientNotifyInvited_Implementation(AMMOCharacter* From)
{
	// Sadece bilgilendirme; kabul kararini sunucudaki PendingInviteFrom belirler.
	PendingInviteFrom = From;
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor(120, 200, 255),
			TEXT("Parti daveti aldin! Kabul: /parti kabul (MMOPartyAccept), ret: /parti ret (MMOPartyDecline)"));
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

void UMMOPartyComponent::ApplyPartySet(const TArray<AMMOCharacter*>& FullSet, AMMOCharacter* NewLeader, const FGuid& SessionId)
{
	if (!SessionId.IsValid()) return;
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

		// Continuing members retain their context; rejoining/transferred members get a new identity.
		if (PC->PartySessionId != SessionId || !PC->PartyMembershipId.IsValid())
		{
			PC->PartyMembershipId = FGuid::NewGuid();
			PC->PartyInviteContextId = FGuid::NewGuid();
		}
		PC->PartySessionId = SessionId;
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

bool UMMOPartyComponent::CanCombineParties(AMMOCharacter* Inviter, AMMOCharacter* Invitee)
{
	const UMMOPartyComponent* IP = Inviter ? Inviter->GetPartyComponent() : nullptr;
	const UMMOPartyComponent* VP = Invitee ? Invitee->GetPartyComponent() : nullptr;
	if (!IP || !VP)
	{
		return false;
	}
	// Ayni partiye tekrar kabul, baska bir grubun sahipligini degistirmez.
	if (IP->Members.Contains(Invitee))
	{
		return true;
	}
	// Partisiz oyuncu kendi adina karar verir. Uye -> partisiz oyuncu daveti korunur.
	// Davet edilenin zaten partisi varsa, iki taraf da tum grubu temsil ediyor.
	return !VP->IsInParty() || (VP->IsPartyLeader() && (!IP->IsInParty() || IP->IsPartyLeader()));
}

void UMMOPartyComponent::FormOrJoin(AMMOCharacter* Inviter, AMMOCharacter* Invitee)
{
	if (!HasAuthorityInternal() || !Inviter || !Invitee || Inviter == Invitee)
	{
		return;
	}
	// Davet ile kabul arasinda liderlik/uyelik degisebilir; yetkiyi mutasyon oncesi yenile.
	if (!CanCombineParties(Inviter, Invitee))
	{
		MMONotify::ToOwner(Invitee, TEXT("Parti birlestirme icin iki partinin lider onayi gerekir."), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}
	if (Inviter->GetPartyComponent()->Members.Contains(Invitee))
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
		MMONotify::ToOwner(Inviter, TEXT("Parti dolu (maksimum 8)"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
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

	const UMMOPartyComponent* IP = Inviter->GetPartyComponent();
	const UMMOPartyComponent* VP = Invitee->GetPartyComponent();
	const FGuid SessionId = IP->IsInParty() ? IP->PartySessionId
		: (VP->IsInParty() ? VP->PartySessionId : FGuid::NewGuid());
	ApplyPartySet(Set, NewLeader, SessionId);
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
	const FGuid OldSessionId = LP->PartySessionId;

	// Ayrilani partisiz yap.
	LP->Members.Reset();
	LP->Leader = nullptr;
	LP->PartySessionId.Invalidate();
	LP->PartyMembershipId.Invalidate();
	LP->PartyInviteContextId = FGuid::NewGuid();
	LP->OnPartyChanged.Broadcast();

	// Kalanlari yeniden ayarla (tek kisi kalirsa o da partisiz olur).
	if (Others.Num() >= 2)
	{
		ApplyPartySet(Others, OldLeader, OldSessionId);
	}
	else if (Others.Num() == 1)
	{
		if (UMMOPartyComponent* OP = Others[0]->GetPartyComponent())
		{
			OP->Members.Reset();
			OP->Leader = nullptr;
			OP->PartySessionId.Invalidate();
			OP->PartyMembershipId.Invalidate();
			OP->PartyInviteContextId = FGuid::NewGuid();
			OP->OnPartyChanged.Broadcast();
		}
	}
}

#if !UE_BUILD_SHIPPING
bool UMMOPartyComponent::RunMergeConsentSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0;
	OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() != NM_Standalone
		|| !FParse::Param(FCommandLine::Get(), TEXT("unattended")) || !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
	{
		OutError = TEXT("disposable standalone headless world required");
		return false;
	}
	TArray<AMMOCharacter*> Actors;
	TArray<AMMOPlayerController*> LifecycleControllers;
	const bool bSuccess = [&]()
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (int32 I = 0; I < 9; ++I)
		{
			AMMOCharacter* Character = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(0, 0, 5000), FRotator::ZeroRotator, Params);
			if (!Character)
			{
				OutError = TEXT("fixture spawn failed");
				return false;
			}
			Actors.Add(Character);
		}
		auto Accept = [&](int32 From, int32 To)
		{
			UMMOPartyComponent* Target = Actors[To]->GetPartyComponent();
			if (!Target->SetPendingInviteServer(Actors[From])) { OutError = TEXT("consent fixture invite issuance"); return false; }
			Target->RequestAccept();
			return true;
		};
		auto Matches = [&](const TArray<AMMOCharacter*>& Set, AMMOCharacter* ExpectedLeader)
		{
			for (AMMOCharacter* Character : Set)
			{
				const UMMOPartyComponent* Party = Character->GetPartyComponent();
				if (Party->Leader != ExpectedLeader || Party->Members.Num() != Set.Num() - 1
					|| !Party->PartySessionId.IsValid() || !Party->PartyMembershipId.IsValid()
					|| Party->PartySessionId != Set[0]->GetPartyComponent()->PartySessionId)
				{
					return false;
				}
				for (AMMOCharacter* Other : Set)
				{
					if (Other != Character && !Party->Members.Contains(Other)) { return false; }
				}
			}
			return true;
		};
		auto Check = [&](bool Condition, const TCHAR* Label)
		{
			if (!Condition) { OutError = Label; return false; }
			++OutChecks;
			return true;
		};
		const TArray<AMMOCharacter*> A { Actors[0], Actors[1] };
		const TArray<AMMOCharacter*> B { Actors[2], Actors[3] };
		if (!Accept(0, 1) || !Accept(2, 3)) return false;
		if (!Check(Matches(A, Actors[0]) && Matches(B, Actors[2]), TEXT("solo formation"))) { return false; }
		if (!Accept(0, 3)) return false;
		if (!Check(Matches(A, Actors[0]) && Matches(B, Actors[2]), TEXT("invitee member moved group"))) { return false; }
		if (!Accept(1, 2)) return false;
		if (!Check(Matches(A, Actors[0]) && Matches(B, Actors[2]), TEXT("inviter member merged group"))) { return false; }
		// Davet yetkiliyken gonderilir; lider kabul gelmeden ayrilir/liderlik degisir.
		UMMOPartyComponent* Target = Actors[2]->GetPartyComponent();
		if (!Target->SetPendingInviteServer(Actors[0])) { OutError = TEXT("inviter leadership invite issuance"); return false; }
		ApplyPartySet(A, Actors[1], Actors[1]->GetPartyComponent()->PartySessionId);
		Target->RequestAccept();
		if (!Check(Matches(A, Actors[1]) && Matches(B, Actors[2]) && !Target->PendingInviteFrom.IsValid(), TEXT("stale inviter leadership"))) { return false; }
		ApplyPartySet(A, Actors[0], Actors[0]->GetPartyComponent()->PartySessionId);
		if (!Target->SetPendingInviteServer(Actors[0])) { OutError = TEXT("invitee leadership invite issuance"); return false; }
		ApplyPartySet(B, Actors[3], Actors[3]->GetPartyComponent()->PartySessionId);
		Target->RequestAccept();
		if (!Check(Matches(A, Actors[0]) && Matches(B, Actors[3]), TEXT("stale invitee leadership"))) { return false; }
		ApplyPartySet(B, Actors[2], Actors[2]->GetPartyComponent()->PartySessionId);
		if (!Accept(1, 0)) return false;
		if (!Check(Matches(A, Actors[0]), TEXT("same group changed leader"))) { return false; }
		if (!Accept(0, 2)) return false;
		TArray<AMMOCharacter*> Merged { Actors[0], Actors[1], Actors[2], Actors[3] };
		if (!Check(Matches(Merged, Actors[0]), TEXT("leader consent merge"))) { return false; }
		if (!Accept(1, 4)) return false; // Siradan uyenin partisiz oyuncu davet etmesi halen desteklenir.
		Merged.Add(Actors[4]);
		if (!Check(Matches(Merged, Actors[0]), TEXT("member solo invitation"))) { return false; }
		for (int32 I = 5; I < 8; ++I) { if (!Accept(0, I)) return false; Merged.Add(Actors[I]); }
		if (!Check(Matches(Merged, Actors[0]), TEXT("eight members"))) { return false; }
		if (!Accept(0, 8)) return false;
		if (!Check(Matches(Merged, Actors[0]) && !Actors[8]->GetPartyComponent()->IsInParty() && Actors[8]->GetPartyComponent()->Leader == nullptr, TEXT("capacity overflow mutated groups"))) { return false; }
		struct FPartySnapshot
		{
			TArray<TObjectPtr<AMMOCharacter>> Members;
			TObjectPtr<AMMOCharacter> Leader;
			TWeakObjectPtr<AMMOCharacter> Pending;
			uint64 Id;
			FGuid Session, Membership, SourceContext, PendingSourceContext;
			float Time, LastInvite;
			FMMOCharacterSaveData Inventory;
		};
		auto ActionUnchanged = [&](auto Action, const UMMOPartyComponent* ConsumedRecipient = nullptr)
		{
			TArray<FPartySnapshot> Before;
			for (AMMOCharacter* Character : Actors)
			{
				const UMMOPartyComponent* P = Character->GetPartyComponent();
				FPartySnapshot& S = Before.AddDefaulted_GetRef();
				S.Members = P->Members; S.Leader = P->Leader; S.Pending = P->PendingInviteFrom;
				S.Session = P->PartySessionId; S.Membership = P->PartyMembershipId;
				S.SourceContext = P->PartyInviteContextId; S.PendingSourceContext = P->PendingInviterContextId;
				S.Id = P->PendingInviteId; S.Time = P->PendingInviteTime; S.LastInvite = P->LastInviteTime;
				Character->GetInventory()->FillSaveData(S.Inventory);
			}
			Action();
			for (int32 I = 0; I < Actors.Num(); ++I)
			{
				const UMMOPartyComponent* P = Actors[I]->GetPartyComponent(); const FPartySnapshot& S = Before[I];
				const bool CheckPending = P != ConsumedRecipient;
				FMMOCharacterSaveData After; Actors[I]->GetInventory()->FillSaveData(After);
				TArray<FString> Differences; FMMOCharacterSaveJson::FindDifferences(S.Inventory, After, Differences);
				if (P->Members != S.Members || P->Leader != S.Leader || (CheckPending && P->PendingInviteFrom != S.Pending)
					|| P->PartySessionId != S.Session || P->PartyMembershipId != S.Membership
					|| P->PartyInviteContextId != S.SourceContext || (CheckPending && P->PendingInviterContextId != S.PendingSourceContext)
					|| P->PendingInviteId != S.Id || (CheckPending && P->PendingInviteTime != S.Time) || P->LastInviteTime != S.LastInvite
					|| !Differences.IsEmpty()) return false;
			}
			return true;
		};
		auto RejectedUnchanged = [&](uint64 Seen, bool Decline)
		{
			return ActionUnchanged([&]() { if (Decline) Target->RequestDeclineSeen(Seen); else Target->RequestAcceptSeen(Seen); });
		};
		for (int32 Side = 0; Side < 2; ++Side)
		{
			for (AMMOCharacter* Character : Actors) Character->GetPartyComponent()->RequestLeave();
			AMMOCharacter* Recipient = Actors[Side == 0 ? 0 : 8]; Target = Recipient->GetPartyComponent();
			const uint64 BeforeId = Target->PendingInviteId;
			if (!Check(Target->SetPendingInviteServer(Actors[1]) && Target->PendingInviteId > BeforeId, TEXT("party invite generation"))) return false;
			const uint64 Old = Target->PendingInviteId;
			Target->RequestDeclineSeen(Old);
			if (!Check(!Target->PendingInviteFrom.IsValid() && Target->PendingInviteTime == -1.f
				&& Target->PendingInviteId == Old && !Target->IsInParty(), TEXT("fresh party decline retains generation"))) return false;
			AMMOCharacter* FreshInviter = Actors[Side == 0 ? 1 : 2]; // Same sender reissue and different sender replacement.
			if (!Check(Target->SetPendingInviteServer(FreshInviter) && Target->PendingInviteId > Old, TEXT("replacement party invite"))) return false;
			const uint64 Fresh = Target->PendingInviteId;
			for (uint64 Seen : { Old, uint64(0), Fresh + 1 })
			{
				if (!Check(RejectedUnchanged(Seen, false), TEXT("stale/zero/future party accept mutated state"))) return false;
				if (!Check(RejectedUnchanged(Seen, true), TEXT("old decline cleared replacement party invite"))) return false;
			}
			if (Side == 0) Target->RequestAcceptSeen(Fresh); else Target->RequestAccept();
			const TArray<AMMOCharacter*> Pair { FreshInviter, Recipient };
			if (!Check(Matches(Pair, FreshInviter) && !Actors[Side == 0 ? 2 : 1]->GetPartyComponent()->IsInParty()
				&& !Target->PendingInviteFrom.IsValid() && Target->PendingInviteId == Fresh, TEXT("fresh party accept correct sender"))) return false;
			if (!Check(RejectedUnchanged(Fresh, false), TEXT("duplicate party accept mutated state"))) return false;
			Target->RequestLeave();
			if (!Check(!Target->IsInParty() && Target->Leader == nullptr && Target->PendingInviteId == Fresh, TEXT("party leave retains invite generation"))) return false;
			if (!Target->SetPendingInviteServer(Actors[1])) return false;
			const uint64 Declined = Target->PendingInviteId; Target->RequestDecline();
			if (!Check(!Target->PendingInviteFrom.IsValid() && Target->PendingInviteId == Declined, TEXT("current party decline wrapper"))) return false;
		}
		Target->PendingInviteId = MAX_uint64 - 1;
		if (!Check(Target->SetPendingInviteServer(Actors[1]) && Target->PendingInviteId == MAX_uint64, TEXT("party invite max generation"))) return false;
		const float MaxInviteTime = Target->PendingInviteTime;
		if (!Check(!Target->SetPendingInviteServer(Actors[2]) && Target->PendingInviteFrom.Get() == Actors[1]
			&& Target->PendingInviteId == MAX_uint64 && Target->PendingInviteTime == MaxInviteTime, TEXT("party invite overflow fail closed"))) return false;
		// Native delayed leave intent: group identity and per-pawn membership lifetime differ.
		auto ClearGroups = [&]() { for (AMMOCharacter* C : Actors) C->GetPartyComponent()->RequestLeave(); };
		auto Solo = [&](int32 I) {
			const UMMOPartyComponent* P = Actors[I]->GetPartyComponent();
			return !P->IsInParty() && !P->Leader && !P->PartySessionId.IsValid() && !P->PartyMembershipId.IsValid();
		};
		auto LeaveRejected = [&](int32 I, const FGuid& Seen) {
			return ActionUnchanged([&]() { Actors[I]->GetPartyComponent()->ServerLeave_Implementation(Seen); });
		};
		ClearGroups();
		if (!Accept(0, 1)) return false;
		UMMOPartyComponent* P0 = Actors[0]->GetPartyComponent();
		UMMOPartyComponent* P1 = Actors[1]->GetPartyComponent();
		UMMOPartyComponent* P2 = Actors[2]->GetPartyComponent();
		UMMOPartyComponent* P3 = Actors[3]->GetPartyComponent();
		const FGuid OldSession = P0->PartySessionId, Old0 = P0->PartyMembershipId;
		if (!Check(Matches(A, Actors[0]) && P0->PartyMembershipId != P1->PartyMembershipId, TEXT("unique party membership identities"))) return false;
		if (!Check(LeaveRejected(0, FGuid()), TEXT("zero leave mutated party"))) return false;
		if (!Check(LeaveRejected(0, P1->PartyMembershipId), TEXT("other member leave identity accepted"))) return false;
		if (!Check(LeaveRejected(0, FGuid::NewGuid()), TEXT("unknown leave identity accepted"))) return false;
		P0->RequestLeaveSeen(Old0);
		if (!Check(Solo(0) && Solo(1), TEXT("fresh leave clears pair identities"))) return false;
		if (!Accept(0, 1)) return false;
		if (!Check(Matches(A, Actors[0]) && P0->PartySessionId != OldSession && P0->PartyMembershipId != Old0, TEXT("reformed pair reused identities"))) return false;
		if (!Check(LeaveRejected(0, Old0), TEXT("old leave removed reformed same pair"))) return false;
		P0->RequestLeave();
		if (!Accept(0, 2)) return false;
		if (!Check(LeaveRejected(0, Old0), TEXT("old leave removed replacement party"))) return false;
		const FGuid Continuing = P0->PartySessionId, Current0 = P0->PartyMembershipId, Current2 = P2->PartyMembershipId;
		if (!Accept(0, 1)) return false;
		const FGuid First1 = P1->PartyMembershipId;
		if (!Check(Matches({ Actors[0], Actors[1], Actors[2] }, Actors[0]) && P0->PartyMembershipId == Current0
			&& P2->PartyMembershipId == Current2 && P0->PartySessionId == Continuing, TEXT("adding member changed continuing membership"))) return false;
		P1->RequestLeaveSeen(First1);
		if (!Check(Solo(1) && Matches({ Actors[0], Actors[2] }, Actors[0]) && P0->PartySessionId == Continuing
			&& P0->PartyMembershipId == Current0 && P2->PartyMembershipId == Current2, TEXT("member departure changed survivors"))) return false;
		if (!Accept(0, 1)) return false;
		if (!Check(P1->PartySessionId == Continuing && P1->PartyMembershipId.IsValid() && P1->PartyMembershipId != First1,
			TEXT("same party rejoin reused personal identity"))) return false;
		if (!Check(LeaveRejected(1, First1), TEXT("delayed leave removed same party rejoin"))) return false;
		const FGuid Rejoined1 = P1->PartyMembershipId;
		P0->RequestLeaveSeen(Current0);
		if (!Check(Solo(0) && Matches({ Actors[1], Actors[2] }, Actors[2]) && P1->PartySessionId == Continuing
			&& P1->PartyMembershipId == Rejoined1 && P2->PartyMembershipId == Current2, TEXT("leader departure lost continuing membership"))) return false;
		P1->RequestLeaveSeen(Rejoined1);
		if (!Check(Solo(1) && Solo(2), TEXT("current leave after leadership change"))) return false;
		if (!Accept(0, 1) || !Accept(2, 3)) return false;
		const FGuid SessionA = P0->PartySessionId, A0 = P0->PartyMembershipId, A1 = P1->PartyMembershipId;
		const FGuid B2 = P2->PartyMembershipId, B3 = P3->PartyMembershipId;
		if (!Accept(0, 2)) return false;
		if (!Check(Matches({ Actors[0], Actors[1], Actors[2], Actors[3] }, Actors[0]) && P0->PartySessionId == SessionA
			&& P0->PartyMembershipId == A0 && P1->PartyMembershipId == A1 && P2->PartyMembershipId != B2
			&& P3->PartyMembershipId != B3, TEXT("merge failed to renew transferred members"))) return false;
		if (!Check(LeaveRejected(2, B2), TEXT("old invitee group leave removed merged membership"))) return false;
		P0->RequestLeave();
		const FGuid Survivor1 = P1->PartyMembershipId, Survivor2 = P2->PartyMembershipId, Survivor3 = P3->PartyMembershipId;
		if (!Check(Solo(0) && Matches({ Actors[1], Actors[2], Actors[3] }, Actors[1]) && P1->PartySessionId == SessionA,
			TEXT("merged group leader departure"))) return false;
		if (!Accept(0, 1)) return false; // Solo inviter enters invitee's continuing group.
		if (!Check(Matches({ Actors[0], Actors[1], Actors[2], Actors[3] }, Actors[0]) && P0->PartySessionId == SessionA
			&& P0->PartyMembershipId != A0 && P1->PartyMembershipId == Survivor1 && P2->PartyMembershipId == Survivor2
			&& P3->PartyMembershipId == Survivor3, TEXT("solo inviter replaced continuing group identities"))) return false;
		// A recipient nonce cannot authorize a different lifetime of the sender's party/solo context.
		for (int32 Case = 0; Case < 8; ++Case)
		{
			ClearGroups();
			if ((Case < 3 || Case == 6 || Case == 7) && !Accept(0, 1)) return false;
			if (Case == 1 && !Accept(0, 2)) return false;
			if (Case == 6 && !Accept(2, 3)) return false;
			UMMOPartyComponent* Recipient = Actors[4]->GetPartyComponent();
			if (!Check(Recipient->SetPendingInviteServer(Actors[0]), TEXT("source context invite setup"))) return false;
			const uint64 Seen = Recipient->PendingInviteId;
			if (Case < 3) P0->RequestLeave();
			if (Case == 0 && !Accept(0, 2)) return false; // A -> B.
			if (Case == 1 && !Accept(0, 1)) return false; // Rejoin the surviving session.
			// Case 2 remains solo after the original group dissolved.
			if (Case >= 3 && Case < 6 && !Accept(0, 1)) return false;
			if (Case == 6 && !Accept(2, 0)) return false; // Source transferred by a leader-consented merge.
			if (Case == 7) P1->RequestLeave(); // Grouped sender becomes the solo last survivor.
			if (Case == 5) P0->RequestLeave(); // Roundtrip by explicit departure.
			if (Case == 4) P1->RequestLeave(); // Solo -> group -> solo via last survivor collapse.
			if (!Check(ActionUnchanged([&]() { Recipient->RequestAcceptSeen(0); }), TEXT("zero source response changed pending state"))) return false;
			const bool Unchanged = ActionUnchanged([&]() { Recipient->RequestAcceptSeen(Seen); }, Recipient);
			if (!Check(Unchanged && Solo(4) && !Recipient->PendingInviteFrom.IsValid()
				&& Recipient->PendingInviteTime == -1.f && Recipient->PendingInviteId == Seen && !Recipient->PendingInviterContextId.IsValid(),
				*FString::Printf(TEXT("old party invite followed source context case=%d"), Case))) return false;
			if (!Check(ActionUnchanged([&]() { Recipient->RequestAcceptSeen(Seen); }), TEXT("consumed source response repeated"))) return false;
			if (!Accept(0, 4)) return false;
			TArray<AMMOCharacter*> Expected { Actors[0], Actors[4] };
			if (Case == 0 || Case == 1) Expected.Add(Actors[2]);
			if (Case == 1 || Case == 3 || Case == 6) Expected.Add(Actors[1]);
			if (Case == 6) { Expected.Add(Actors[2]); Expected.Add(Actors[3]); }
			if (!Check(Matches(Expected, Actors[Case == 6 ? 2 : 0]) && Recipient->PendingInviteId > Seen,
				TEXT("fresh source context invite recovery"))) return false;
		}
		ClearGroups();
		if (!Accept(0, 1)) return false;
		UMMOPartyComponent* ContextRecipient = Actors[4]->GetPartyComponent();
		if (!Check(ContextRecipient->SetPendingInviteServer(Actors[1]), TEXT("continuous member invite setup"))) return false;
		if (!Accept(0, 2)) return false;
		P0->RequestLeave(); // Roster/leadership changes preserve a continuing member's invitation.
		ContextRecipient->RequestAccept();
		if (!Check(Matches({ Actors[1], Actors[2], Actors[4] }, Actors[1]), TEXT("continuous member source invite invalidated"))) return false;
		ClearGroups();
		if (!ContextRecipient->SetPendingInviteServer(Actors[0])) return false;
		ContextRecipient->RequestDecline();
		if (!Check(!ContextRecipient->PendingInviteFrom.IsValid() && ContextRecipient->PendingInviteTime == -1.f && !ContextRecipient->PendingInviterContextId.IsValid(),
			TEXT("source decline consumption"))) return false;
		for (const FGuid& InvalidSource : { FGuid(), FGuid::NewGuid() })
		{
			if (!ContextRecipient->SetPendingInviteServer(Actors[0])) return false;
			ContextRecipient->PendingInviterContextId = InvalidSource;
			const uint64 Seen = ContextRecipient->PendingInviteId;
			const bool Unchanged = ActionUnchanged([&]() { ContextRecipient->RequestAcceptSeen(Seen); }, ContextRecipient);
			if (!Check(Unchanged && Solo(4) && !ContextRecipient->PendingInviteFrom.IsValid()
				&& !ContextRecipient->PendingInviterContextId.IsValid() && ContextRecipient->PendingInviteTime == -1.f,
				TEXT("invalid source context accepted"))) return false;
		}
		if (!AMMOPlayerController::RunPartyChatContextSelfTest(World, OutError, OutChecks)) return false;
		ClearGroups();
		// Real native UnPossess/Destroy/RouteEndPlay. Same frame: no AutoLoad/save/remote logout proof.
		auto PossessFixture = [&](int32 I) {
			AMMOPlayerController* PC = World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(), Params);
			if (!PC) return static_cast<AMMOPlayerController*>(nullptr);
			LifecycleControllers.Add(PC); PC->Possess(Actors[I]); PC->SetActorTickEnabled(false);
			Actors[I]->SetActorTickEnabled(false); return PC;
		};
		if (!Accept(0, 1)) return false;
		AMMOPlayerController* DetachedPC = PossessFixture(0);
		if (!Check(DetachedPC && Actors[0]->IsPlayerControlled() && Matches(A, Actors[0]), TEXT("lifecycle possessed pair setup"))) return false;
		DetachedPC->UnPossess();
		if (!Check(!Actors[0]->IsPlayerControlled() && !Actors[0]->GetPlayerState() && !DetachedPC->GetPawn()
			&& Matches(A, Actors[0]), TEXT("unpossess lifecycle precondition"))) return false;
		if (!Actors[0]->Destroy()) { OutError = TEXT("detached native Destroy failed"); return false; }
		if (!Check(Solo(1), TEXT("destroy after unpossess retained dead party member"))) return false;
		if (!Accept(2, 3) || !Accept(2, 4)) return false;
		AMMOPlayerController* AttachedPC = PossessFixture(2);
		const FGuid DestroySession = P3->PartySessionId, Surviving3 = P3->PartyMembershipId;
		const FGuid Surviving4 = Actors[4]->GetPartyComponent()->PartyMembershipId;
		if (!Check(AttachedPC && Actors[2]->IsPlayerControlled() && Matches({ Actors[2], Actors[3], Actors[4] }, Actors[2]),
			TEXT("destroy attached leader setup"))) return false;
		if (!Actors[2]->Destroy()) { OutError = TEXT("attached native Destroy failed"); return false; }
		if (!Check(!AttachedPC->GetPawn() && Matches({ Actors[3], Actors[4] }, Actors[3]) && P3->PartySessionId == DestroySession
			&& P3->PartyMembershipId == Surviving3 && Actors[4]->GetPartyComponent()->PartyMembershipId == Surviving4,
			TEXT("destroy leader lost survivor roster or membership"))) return false;
		if (!Accept(5, 6) || !Accept(5, 7)) return false;
		UMMOPartyComponent* P5 = Actors[5]->GetPartyComponent(); UMMOPartyComponent* P7 = Actors[7]->GetPartyComponent();
		const FGuid MemberSession = P5->PartySessionId, Surviving5 = P5->PartyMembershipId, Surviving7 = P7->PartyMembershipId;
		AMMOPlayerController* MemberPC = PossessFixture(6);
		if (!Check(MemberPC && Actors[6]->IsPlayerControlled(), TEXT("possessed ordinary member setup"))) return false;
		MemberPC->UnPossess();
		if (!Check(!Actors[6]->IsPlayerControlled() && Matches({ Actors[5], Actors[6], Actors[7] }, Actors[5]),
			TEXT("detached member lifecycle setup"))) return false;
		if (!Actors[6]->Destroy()) { OutError = TEXT("member native Destroy failed"); return false; }
		if (!Check(Matches({ Actors[5], Actors[7] }, Actors[5]) && P5->PartySessionId == MemberSession
			&& P5->PartyMembershipId == Surviving5 && P7->PartyMembershipId == Surviving7,
			TEXT("destroy member changed survivor identity"))) return false;
		Actors[7]->RouteEndPlay(EEndPlayReason::LevelTransition);
		if (!Check(Solo(5) && Solo(7), TEXT("native transition EndPlay left party references"))) return false;
		FMMOCharacterSaveData BeforeFinalDestroy; Actors[5]->GetInventory()->FillSaveData(BeforeFinalDestroy);
		if (!Actors[7]->Destroy()) { OutError = TEXT("post EndPlay native Destroy failed"); return false; }
		FMMOCharacterSaveData AfterFinalDestroy; Actors[5]->GetInventory()->FillSaveData(AfterFinalDestroy);
		TArray<FString> FinalDifferences; FMMOCharacterSaveJson::FindDifferences(BeforeFinalDestroy, AfterFinalDestroy, FinalDifferences);
		return Check(Solo(5) && FinalDifferences.IsEmpty(), TEXT("post-EndPlay Destroy changed solo inventory"));
	}();
	// Controllersiz fixture'lar EndPlay'de oyuncu kaydi yazmaz; grup referanslarini da temizle.
	for (AMMOPlayerController* PC : LifecycleControllers) { if (IsValid(PC)) { PC->UnPossess(); PC->Destroy(); } }
	for (AMMOCharacter* Character : Actors) { if (IsValid(Character)) Character->GetPartyComponent()->RequestLeave(); }
	for (AMMOCharacter* Character : Actors) { if (IsValid(Character)) Character->Destroy(); }
	return bSuccess;
}

namespace
{
	struct FMMOPartyConsentTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestPartyConsent"))) { return false; }
			FString Error;
			int32 Checks = 0;
			if (UMMOPartyComponent::RunMergeConsentSelfTest(InWorld, Error, Checks))
			{
				Ar.Logf(TEXT("[MMO_PARTY_CONSENT_TEST] PASS checks=%d"), Checks);
			}
			else
			{
				Ar.Logf(TEXT("[MMO_PARTY_CONSENT_TEST] FAIL checks=%d reason=%s"), Checks, *Error);
			}
			return true;
		}
	};
	FMMOPartyConsentTestExec GMMOPartyConsentTestExec;
}
#endif

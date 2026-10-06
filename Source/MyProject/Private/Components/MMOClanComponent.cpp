// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMOClanComponent.h"
#include "Characters/MMOCharacter.h"
#include "Components/MMOStatsComponent.h"
#include "Player/MMOPlayerController.h"
#include "Save/MMOSaveGame.h"
#include "Save/MMOCharacterStoreSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogMMOClanAudit, Log, All);

namespace
{
	// Katilma sirasi sayaci (sunucu sureci boyunca artar; kucuk = daha eski uye).
	int64 GClanJoinCounter = 0;

	FString ClanPlayerNameOf(const AMMOCharacter* C)
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

	bool ClanIsDead(const AMMOCharacter* C)
	{
		const UMMOStatsComponent* S = C ? C->GetStats() : nullptr;
		return !S || S->bIsDead;
	}

	UMMOClanComponent* ClanCompOf(const AMMOCharacter* C)
	{
		return C ? C->GetClanComponent() : nullptr;
	}

	// NETWORK_AUDIT O3: kayit yuvasi -> klan adi ozeti kisa sure onbellege alinir. Eskiden her
	// /klan kur denemesi tum MMOSave_*.sav dosyalarini oyun is parcacliginda tam deserialize
	// ediyordu (Kur/Ayril dongusuyle takilma uretilebilirdi). Onbellek yalniz TTL dolunca diskten
	// yeniden kurulur; bu surecte yeni kurulan klan CmdCreate'te onbellege de eklenir, yani
	// ayni sunucuda kurulan klanlar her zaman gorunur. Tek taviz: bir klandan ayrilan uyenin
	// eski adi en fazla TTL kadar "kullanimda" gorunebilir.
	TMap<FString, FString> GSavedClanBySlot;
	double GSavedClanCacheTime = -1.0e9;
	constexpr double SavedClanCacheTtlSeconds = 30.0;

	// Disk kayitlarinda (baska bir karakter yuvasinda) bu klan adi kullaniliyor mu?
	bool SavedClanNameInUse(const UObject* WorldContext, const FString& Name, const FString& ExcludeSlot)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - GSavedClanCacheTime > SavedClanCacheTtlSeconds)
		{
			// Kayit magazasi (IMMOCharacterStore) yoksa cakisma dogrulanamaz: ad "kullanimda" sayilir.
			IMMOCharacterStore* Store = UMMOCharacterStoreSubsystem::Get(WorldContext);
			if (!Store)
			{
				return true;
			}
			GSavedClanBySlot.Reset();
			TArray<FString> Slots;
			Store->ListSlotNames(Slots);
			for (const FString& Slot : Slots)
			{
				FMMOCharacterSaveData SavedData;
				if (Store->Load(Slot, SavedData) && !SavedData.ClanName.IsEmpty())
				{
					GSavedClanBySlot.Add(Slot, SavedData.ClanName);
				}
			}
			GSavedClanCacheTime = Now;
		}
		for (const TPair<FString, FString>& Pair : GSavedClanBySlot)
		{
			if (!Pair.Key.Equals(ExcludeSlot, ESearchCase::IgnoreCase) && Pair.Value.Equals(Name, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}
}

UMMOClanComponent::UMMOClanComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMMOClanComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMMOClanComponent, ClanName);
	DOREPLIFETIME_CONDITION(UMMOClanComponent, bIsLeader, COND_OwnerOnly);
}

void UMMOClanComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Lider baglantisi koparsa / bolge degisirse liderlik cevrimici en eski uyeye gecer.
	// Kendi kaydi degismez (kayit zaten yazildi; geri donunce ApplySaveData liderlik cakismasini cozer).
	const UWorld* World = GetWorld();
	if (HasAuthorityInternal() && bIsLeader && IsInClan() && World && !World->bIsTearingDown)
	{
		EnsureLeader(ClanName, true);
	}
	Super::EndPlay(EndPlayReason);
}

bool UMMOClanComponent::HasAuthorityInternal() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->HasAuthority() : false;
}

AMMOCharacter* UMMOClanComponent::GetOwnerCharacter() const
{
	return Cast<AMMOCharacter>(GetOwner());
}

void UMMOClanComponent::OnRep_Clan()
{
	OnClanChanged.Broadcast();
}

void UMMOClanComponent::MarkChanged()
{
	OnClanChanged.Broadcast();
}

bool UMMOClanComponent::IsValidClanName(const FString& Name)
{
	if (Name.Len() < MinNameLength || Name.Len() > MaxNameLength)
	{
		return false;
	}
	for (const TCHAR Ch : Name)
	{
		if (!FChar::IsAlnum(Ch))
		{
			return false;
		}
	}
	return true;
}

void UMMOClanComponent::NotifyOwner(const FString& Text) const
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

void UMMOClanComponent::GatherOnlineMembers(const FString& InClanName, TArray<AMMOCharacter*>& Out, bool bExcludeSelf) const
{
	Out.Reset();
	const UWorld* World = GetWorld();
	if (!World || InClanName.IsEmpty())
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		AMMOCharacter* C = PC ? Cast<AMMOCharacter>(PC->GetPawn()) : nullptr;
		UMMOClanComponent* CC = ClanCompOf(C);
		if (!CC || (bExcludeSelf && CC == this) || !CC->ClanName.Equals(InClanName, ESearchCase::IgnoreCase))
		{
			continue;
		}
		Out.Add(C);
	}
}

void UMMOClanComponent::NotifyClan(const FString& InClanName, const FString& Text, bool bExcludeSelf) const
{
	TArray<AMMOCharacter*> Members;
	GatherOnlineMembers(InClanName, Members, bExcludeSelf);
	for (const AMMOCharacter* M : Members)
	{
		if (const UMMOClanComponent* CC = ClanCompOf(M))
		{
			CC->NotifyOwner(Text);
		}
	}
}

void UMMOClanComponent::EnsureLeader(const FString& InClanName, bool bExcludeSelf)
{
	TArray<AMMOCharacter*> Members;
	GatherOnlineMembers(InClanName, Members, bExcludeSelf);

	UMMOClanComponent* Best = nullptr;
	for (AMMOCharacter* M : Members)
	{
		UMMOClanComponent* CC = ClanCompOf(M);
		if (!CC)
		{
			continue;
		}
		if (CC->bIsLeader)
		{
			return; // zaten bir lider var
		}
		if (!Best || CC->JoinSeq < Best->JoinSeq)
		{
			Best = CC;
		}
	}
	if (!Best)
	{
		return; // cevrimici uye yok; klan kayitlarda kalir
	}
	Best->bIsLeader = true;
	Best->bActingLeader = true;
	Best->MarkChanged();
	UE_LOG(LogMMOClanAudit, Display, TEXT("event=clan_leader_transfer clan=\"%s\" leader=\"%s\" acting=1"),
		*Best->ClanName, *ClanPlayerNameOf(Best->GetOwnerCharacter()));
	Best->NotifyClan(Best->ClanName, FString::Printf(TEXT("%s artik klan lideri."), *ClanPlayerNameOf(Best->GetOwnerCharacter())), false);
}

// ----------------------------------------------------------------------
// Komutlar
// ----------------------------------------------------------------------

void UMMOClanComponent::CmdCreate(const FString& Name)
{
	AMMOCharacter* Self = GetOwnerCharacter();
	if (!HasAuthorityInternal() || !Self)
	{
		return;
	}
	if (ClanIsDead(Self))
	{
		NotifyOwner(TEXT("Olu iken klan komutu kullanilamaz."));
		return;
	}
	if (IsInClan())
	{
		NotifyOwner(TEXT("Zaten bir klandasin. Once /klan ayril."));
		return;
	}

	const FString N = Name.TrimStartAndEnd();
	if (!IsValidClanName(N))
	{
		NotifyOwner(FString::Printf(TEXT("Klan adi %d-%d karakter olmali; yalniz harf ve rakam. Kullanim: /klan kur ad"), MinNameLength, MaxNameLength));
		return;
	}

	// Sunucuda su an cevrimici baska bir klan bu adi kullaniyorsa reddet (cevrimdisi cakisma kontrol edilmez).
	TArray<AMMOCharacter*> Existing;
	GatherOnlineMembers(N, Existing, true);
	if (Existing.Num() > 0)
	{
		NotifyOwner(TEXT("Bu klan adi kullanimda."));
		return;
	}

	// Kayitlarda (cevrimdisi uyeler) bu adi tasiyan baska bir karakter varsa da reddet:
	// aksi halde ad kapan, geri donen eski uyeleri/lideri devralirdi (QA ORTA).
	if (SavedClanNameInUse(this, N, Self->GetSaveSlotName()))
	{
		NotifyOwner(TEXT("Bu klan adi kullanimda (cevrimdisi uyeleri var)."));
		return;
	}

	ClanName = N;
	bIsLeader = true;
	bActingLeader = false;
	JoinSeq = ++GClanJoinCounter;
	GSavedClanBySlot.Add(Self->GetSaveSlotName(), N); // onbellek: yeni klan hemen gorunsun
	MarkChanged();
	UE_LOG(LogMMOClanAudit, Display, TEXT("event=clan_create clan=\"%s\" actor=\"%s\" slot=\"%s\""),
		*ClanName, *ClanPlayerNameOf(Self), *Self->GetSaveSlotName());
	NotifyOwner(FString::Printf(TEXT("Klan kuruldu: [%s]. Davet: /klan davet"), *ClanName));
	Self->SaveCharacter();
}

void UMMOClanComponent::CmdInvite()
{
	AMMOCharacter* Self = GetOwnerCharacter();
	const UWorld* World = GetWorld();
	if (!HasAuthorityInternal() || !Self || !World)
	{
		return;
	}
	if (ClanIsDead(Self))
	{
		NotifyOwner(TEXT("Olu iken klan komutu kullanilamaz."));
		return;
	}
	if (!IsInClan())
	{
		NotifyOwner(TEXT("Bir klanda degilsin."));
		return;
	}
	if (!bIsLeader)
	{
		NotifyOwner(TEXT("Yalniz klan lideri davet edebilir."));
		return;
	}

	// Davet spam korumasi (hedefin ekranini doldurmasin).
	const float Now = World->GetTimeSeconds();
	if (Now - LastInviteTime < InviteCooldown)
	{
		NotifyOwner(TEXT("Cok hizli davet gonderiyorsun; biraz bekle."));
		return;
	}
	LastInviteTime = Now;

	AMMOCharacter* Target = nullptr;
	float BestDist = InviteRange;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		AMMOCharacter* C = PC ? Cast<AMMOCharacter>(PC->GetPawn()) : nullptr;
		if (!C || C == Self || ClanIsDead(C))
		{
			continue;
		}
		// Zaten klandaki yakin oyuncular adayligi engellemesin (QA): yalniz klansizlar aday.
		const UMMOClanComponent* CandClan = ClanCompOf(C);
		if (!CandClan || CandClan->IsInClan())
		{
			continue;
		}
		const float D = FVector::Dist(Self->GetActorLocation(), C->GetActorLocation());
		if (D < BestDist)
		{
			BestDist = D;
			Target = C;
		}
	}
	UMMOClanComponent* TC = ClanCompOf(Target);
	if (!TC)
	{
		NotifyOwner(TEXT("Yakinda klansiz davet edilecek oyuncu yok."));
		return;
	}

	// Ayni davet eden ayni hedefe yenilerse sureyi uzatma (hedefi baska liderlere kilitlemesin).
	if (TC->PendingInviteFrom.Get() == Self && TC->PendingInviteTime >= 0.f
		&& Now - TC->PendingInviteTime < InviteExpirySeconds)
	{
		NotifyOwner(TEXT("Bu oyuncuya davet zaten gonderildi; yanitini bekle."));
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
	TC->PendingInviteClan = ClanName;
	TC->PendingInviteTime = Now;
	TC->NotifyOwner(FString::Printf(TEXT("%s seni [%s] klanina davet etti. Kabul: /klan kabul"), *ClanPlayerNameOf(Self), *ClanName));
	NotifyOwner(FString::Printf(TEXT("%s adli oyuncuya klan daveti gonderildi."), *ClanPlayerNameOf(Target)));
}

void UMMOClanComponent::CmdAccept()
{
	AMMOCharacter* Self = GetOwnerCharacter();
	const UWorld* World = GetWorld();
	if (!HasAuthorityInternal() || !Self || !World)
	{
		return;
	}

	// Davet her durumda tuketilir (eski davet sonradan kabul edilemesin).
	AMMOCharacter* Inviter = PendingInviteFrom.Get();
	const FString InviteClan = PendingInviteClan;
	const float InviteTime = PendingInviteTime;
	PendingInviteFrom.Reset();
	PendingInviteClan.Empty();
	PendingInviteTime = -1.f;

	if (ClanIsDead(Self))
	{
		NotifyOwner(TEXT("Olu iken klan komutu kullanilamaz."));
		return;
	}
	if (!Inviter || Inviter == Self || InviteClan.IsEmpty())
	{
		NotifyOwner(TEXT("Bekleyen klan daveti yok."));
		return;
	}
	if (InviteTime < 0.f || World->GetTimeSeconds() - InviteTime > InviteExpirySeconds)
	{
		NotifyOwner(TEXT("Klan davetinin suresi doldu."));
		return;
	}
	if (IsInClan())
	{
		NotifyOwner(TEXT("Zaten bir klandasin."));
		return;
	}
	// Davet eden hala o klanda olmali (ayrildiysa / atildiysa davet gecersiz).
	const UMMOClanComponent* IC = ClanCompOf(Inviter);
	if (!IC || !IC->ClanName.Equals(InviteClan, ESearchCase::IgnoreCase))
	{
		NotifyOwner(TEXT("Davet gecersiz: davet eden artik o klanda degil."));
		return;
	}

	ClanName = IC->ClanName;
	bIsLeader = false;
	bActingLeader = false;
	JoinSeq = ++GClanJoinCounter;
	MarkChanged();
	UE_LOG(LogMMOClanAudit, Display, TEXT("event=clan_join clan=\"%s\" actor=\"%s\" inviter=\"%s\""),
		*ClanName, *ClanPlayerNameOf(Self), *ClanPlayerNameOf(Inviter));
	NotifyClan(ClanName, FString::Printf(TEXT("%s klana katildi."), *ClanPlayerNameOf(Self)), false);
	Self->SaveCharacter();
}

void UMMOClanComponent::CmdLeave()
{
	AMMOCharacter* Self = GetOwnerCharacter();
	if (!HasAuthorityInternal() || !Self)
	{
		return;
	}
	if (ClanIsDead(Self))
	{
		NotifyOwner(TEXT("Olu iken klan komutu kullanilamaz."));
		return;
	}
	if (!IsInClan())
	{
		NotifyOwner(TEXT("Bir klanda degilsin."));
		return;
	}

	const FString OldName = ClanName;
	const bool bWasLeader = bIsLeader;
	ClanName.Empty();
	bIsLeader = false;
	bActingLeader = false;
	JoinSeq = 0;
	MarkChanged();
	UE_LOG(LogMMOClanAudit, Display, TEXT("event=clan_leave clan=\"%s\" actor=\"%s\" was_leader=%d"),
		*OldName, *ClanPlayerNameOf(Self), bWasLeader ? 1 : 0);
	NotifyOwner(FString::Printf(TEXT("[%s] klanindan ayrildin."), *OldName));
	NotifyClan(OldName, FString::Printf(TEXT("%s klandan ayrildi."), *ClanPlayerNameOf(Self)), false);
	if (bWasLeader)
	{
		EnsureLeader(OldName, true);
	}
	Self->SaveCharacter();
}

void UMMOClanComponent::CmdKick(const FString& TargetName)
{
	AMMOCharacter* Self = GetOwnerCharacter();
	if (!HasAuthorityInternal() || !Self)
	{
		return;
	}
	if (ClanIsDead(Self))
	{
		NotifyOwner(TEXT("Olu iken klan komutu kullanilamaz."));
		return;
	}
	if (!IsInClan())
	{
		NotifyOwner(TEXT("Bir klanda degilsin."));
		return;
	}
	if (!bIsLeader)
	{
		NotifyOwner(TEXT("Yalniz klan lideri uye atabilir."));
		return;
	}

	const FString Wanted = TargetName.TrimStartAndEnd();
	if (Wanted.IsEmpty())
	{
		NotifyOwner(TEXT("Kullanim: /klan at <isim>"));
		return;
	}

	TArray<AMMOCharacter*> Members;
	GatherOnlineMembers(ClanName, Members, true);
	AMMOCharacter* Victim = nullptr;
	for (AMMOCharacter* M : Members)
	{
		if (ClanPlayerNameOf(M).Equals(Wanted, ESearchCase::IgnoreCase))
		{
			Victim = M;
			break;
		}
	}
	UMMOClanComponent* VC = ClanCompOf(Victim);
	if (!VC)
	{
		NotifyOwner(FString::Printf(TEXT("%s adli cevrimici klan uyesi bulunamadi."), *Wanted));
		return;
	}

	const FString OldName = ClanName;
	const FString VictimName = ClanPlayerNameOf(Victim);
	VC->ClanName.Empty();
	VC->bIsLeader = false;
	VC->bActingLeader = false;
	VC->JoinSeq = 0;
	VC->MarkChanged();
	UE_LOG(LogMMOClanAudit, Display, TEXT("event=clan_kick clan=\"%s\" actor=\"%s\" target=\"%s\""),
		*OldName, *ClanPlayerNameOf(Self), *VictimName);
	VC->NotifyOwner(FString::Printf(TEXT("[%s] klanindan atildin."), *OldName));
	NotifyClan(OldName, FString::Printf(TEXT("%s klandan atildi."), *VictimName), false);
	Victim->SaveCharacter();
}

void UMMOClanComponent::CmdList()
{
	if (!HasAuthorityInternal())
	{
		return;
	}
	if (!IsInClan())
	{
		NotifyOwner(TEXT("Bir klanda degilsin."));
		return;
	}

	TArray<AMMOCharacter*> Members;
	GatherOnlineMembers(ClanName, Members, false);
	// TArray<T*>::Sort, ifadeye eleman referanslari verir (isaretci degil).
	Members.Sort([](const AMMOCharacter& A, const AMMOCharacter& B)
	{
		const UMMOClanComponent* CA = ClanCompOf(&A);
		const UMMOClanComponent* CB = ClanCompOf(&B);
		return (CA ? CA->JoinSeq : 0) < (CB ? CB->JoinSeq : 0);
	});

	FString List;
	for (int32 i = 0; i < Members.Num(); ++i)
	{
		const AMMOCharacter* M = Members[i];
		const UMMOClanComponent* CC = ClanCompOf(M);
		const int32 Lv = (M && M->GetStats()) ? M->GetStats()->Level : 0;
		List += FString::Printf(TEXT("%s%s (Lv%d)%s"), i > 0 ? TEXT(", ") : TEXT(""), *ClanPlayerNameOf(M), Lv,
			(CC && CC->bIsLeader) ? TEXT(" [lider]") : TEXT(""));
	}
	NotifyOwner(FString::Printf(TEXT("[%s] cevrimici uyeler (%d): %s"), *ClanName, Members.Num(), *List));
}

void UMMOClanComponent::CmdChat(const FString& Text)
{
	AMMOCharacter* Self = GetOwnerCharacter();
	if (!HasAuthorityInternal() || !Self)
	{
		return;
	}
	if (!IsInClan())
	{
		NotifyOwner(TEXT("Bir klanda degilsin; mesaj gonderilmedi."));
		return;
	}

	const FString Sender = ClanPlayerNameOf(Self);
	TArray<AMMOCharacter*> Members;
	GatherOnlineMembers(ClanName, Members, false);
	for (const AMMOCharacter* M : Members)
	{
		if (AMMOPlayerController* PC = M ? Cast<AMMOPlayerController>(M->GetController()) : nullptr)
		{
			PC->ClientReceiveChat(Sender, Text, 5);
		}
	}
}

// ----------------------------------------------------------------------
// Kalicilik
// ----------------------------------------------------------------------

void UMMOClanComponent::FillSaveData(FMMOCharacterSaveData& Out) const
{
	Out.ClanName = ClanName;
	Out.bClanLeader = IsInClan() && bIsLeader && !bActingLeader;
}

void UMMOClanComponent::ApplySaveData(const FMMOCharacterSaveData& In)
{
	if (!HasAuthorityInternal())
	{
		return;
	}

	// Eski kayitlarda bos gelir; bozuk/gecersiz ad klansiz sayilir.
	const FString N = In.ClanName.TrimStartAndEnd();
	if (!IsValidClanName(N))
	{
		ClanName.Empty();
		bIsLeader = false;
		JoinSeq = 0;
		MarkChanged();
		return;
	}

	ClanName = N;
	JoinSeq = ++GClanJoinCounter;

	// Liderlik tekil olmali. Kayitli lider geri geldiyse liderligi geri alir (vekil lider uyeye doner;
	// bolge gecisinde yukleme sirasi onemsiz kalir). Lider degilse yalniz cevrimici lider yoksa
	// (lider ayrilmis / kimse cevrimici degil) lider olur.
	TArray<AMMOCharacter*> Others;
	GatherOnlineMembers(N, Others, true);
	bool bOtherRealLeader = false;
	bool bOtherAnyLeader = false;
	UMMOClanComponent* OtherActing = nullptr;
	for (const AMMOCharacter* M : Others)
	{
		UMMOClanComponent* CC = ClanCompOf(M);
		if (CC && CC->bIsLeader)
		{
			bOtherAnyLeader = true;
			if (CC->bActingLeader)
			{
				OtherActing = CC;
			}
			else
			{
				bOtherRealLeader = true;
			}
		}
	}

	if (In.bClanLeader && !bOtherRealLeader)
	{
		// Asil lider geri dondu: cevrimici vekil lider (varsa) liderligi birakir ve haberdar edilir.
		if (OtherActing)
		{
			OtherActing->bIsLeader = false;
			OtherActing->bActingLeader = false;
			OtherActing->MarkChanged();
			OtherActing->NotifyOwner(TEXT("Asil klan lideri geri dondu; liderlik ona gecti."));
		}
		bIsLeader = true;
		bActingLeader = false;
	}
	else if (In.bClanLeader)
	{
		// Baska bir asil lider zaten cevrimici (ad cakismasi / bozuk kayit): uye olarak katil,
		// onun liderligini ezme.
		bIsLeader = false;
		bActingLeader = false;
	}
	else
	{
		// Lider degil: yalniz cevrimici hic lider yoksa vekil olarak terfi eder (kayda yazilmaz).
		bIsLeader = !bOtherAnyLeader;
		bActingLeader = bIsLeader;
	}
	MarkChanged();
}

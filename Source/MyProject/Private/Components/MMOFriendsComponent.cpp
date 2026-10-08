// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMOFriendsComponent.h"
#include "Characters/MMOCharacter.h"
#include "Components/MMOStatsComponent.h"
#include "Player/MMOPlayerController.h"
#include "Save/MMOSaveGame.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "HAL/PlatformTime.h"
#if !UE_BUILD_SHIPPING
#include "UI/MMOHUD.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#endif

namespace
{
	TMap<FString, TPair<bool, double>>& MMOFriendPresenceRecords()
	{
		static TMap<FString, TPair<bool, double>> Records;
		return Records;
	}

	FString FriendPlayerNameOf(const AMMOCharacter* C)
	{
		if (C)
		{
			if (const APlayerState* PS = C->GetPlayerState())
			{
				return PS->GetPlayerName();
			}
		}
		return FString();
	}

	UMMOFriendsComponent* FriendsCompOf(const AMMOCharacter* C)
	{
		return C ? C->GetFriendsComponent() : nullptr;
	}

	// Ada gore cevrimici oyuncu karakterini bulur (buyuk/kucuk harf duyarsiz).
	AMMOCharacter* FindOnlineByName(const UWorld* World, const FString& Name)
	{
		if (!World || Name.IsEmpty())
		{
			return nullptr;
		}
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			AMMOCharacter* C = PC ? Cast<AMMOCharacter>(PC->GetPawn()) : nullptr;
			if (C && FriendPlayerNameOf(C).Equals(Name, ESearchCase::IgnoreCase))
			{
				return C;
			}
		}
		return nullptr;
	}
}

UMMOFriendsComponent::UMMOFriendsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMMOFriendsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UMMOFriendsComponent, Friends, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMMOFriendsComponent, PendingInviteId, COND_OwnerOnly);
}

bool UMMOFriendsComponent::HasAuthorityInternal() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->HasAuthority() : false;
}

AMMOCharacter* UMMOFriendsComponent::GetOwnerCharacter() const
{
	return Cast<AMMOCharacter>(GetOwner());
}

void UMMOFriendsComponent::NotifyOwner(const FString& Text) const
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

bool UMMOFriendsComponent::HasFriend(const FString& Name) const
{
	for (const FString& F : Friends)
	{
		if (F.Equals(Name, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

// ----------------------------------------------------------------------
// Komutlar
// ----------------------------------------------------------------------

void UMMOFriendsComponent::CmdAdd(const FString& Name)
{
	AMMOCharacter* Me = GetOwnerCharacter();
	if (!HasAuthorityInternal() || !Me)
	{
		return;
	}
	const FString N = Name.TrimStartAndEnd();
	if (N.IsEmpty())
	{
		NotifyOwner(TEXT("Kullanim: /arkadas ekle isim"));
		return;
	}
	if (N.Len() > MaxNameLength)
	{
		NotifyOwner(TEXT("Oyuncu adi cok uzun."));
		return;
	}
	if (Friends.Num() >= MaxFriends)
	{
		NotifyOwner(FString::Printf(TEXT("Arkadas listesi dolu (en fazla %d)."), MaxFriends));
		return;
	}
	// Yalniz cevrimici oyuncu eklenebilir (yazim hatasiyla cop isim birikmesin).
	const AMMOCharacter* Target = FindOnlineByName(GetWorld(), N);
	if (!Target)
	{
		NotifyOwner(FString::Printf(TEXT("%s adli cevrimici oyuncu bulunamadi."), *N));
		return;
	}
	if (Target == Me)
	{
		NotifyOwner(TEXT("Kendini arkadas olarak ekleyemezsin."));
		return;
	}
	// Kayit icin gercek (buyuk/kucuk harfli) oyuncu adi kullanilir.
	const FString Canonical = FriendPlayerNameOf(Target);
	// Oturumdan oturuma degisen misafir adlari ("Player123") baska birini gosterebilir (QA).
	if (Canonical.StartsWith(TEXT("Player"), ESearchCase::IgnoreCase)
		&& Canonical.RightChop(6).IsNumeric())
	{
		NotifyOwner(TEXT("Misafir adli oyuncu eklenemez; once MMOSetName ile kalici bir ad almali."));
		return;
	}
	if (Canonical.IsEmpty() || Canonical.Len() > MaxNameLength)
	{
		NotifyOwner(TEXT("Bu oyuncu eklenemez."));
		return;
	}
	UMMOFriendsComponent* TargetFriends = FriendsCompOf(Target);
	if (!TargetFriends)
	{
		NotifyOwner(TEXT("Bu oyuncuya arkadaslik istegi gonderilemiyor."));
		return;
	}
	const FString MyName = FriendPlayerNameOf(Me);
	if (HasFriend(Canonical) && TargetFriends->HasFriend(MyName))
	{
		NotifyOwner(FString::Printf(TEXT("%s zaten arkadas listende."), *Canonical));
		return;
	}
	if (TargetFriends->Friends.Num() >= MaxFriends && !TargetFriends->HasFriend(MyName))
	{
		NotifyOwner(FString::Printf(TEXT("%s oyuncusunun arkadas listesi dolu."), *Canonical));
		return;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	if (TargetFriends->PendingFriendInviter.IsValid() && Now < TargetFriends->PendingFriendInviteExpiry)
	{
		if (TargetFriends->PendingFriendInviter.Get() == Me)
		{
			NotifyOwner(FString::Printf(TEXT("%s icin arkadaslik istegin zaten bekliyor."), *Canonical));
		}
		else
		{
			NotifyOwner(FString::Printf(TEXT("%s su anda baska bir arkadaslik istegini yanitliyor."), *Canonical));
		}
		return;
	}

	// Fail closed instead of reusing a consumed generation after integer wrap.
	if (TargetFriends->PendingInviteId == MAX_uint64)
	{
		NotifyOwner(TEXT("Bu oyuncuya yeni arkadaslik istegi gonderilemiyor."));
		return;
	}
	++TargetFriends->PendingInviteId;
	TargetFriends->PendingFriendInviter = Me;
	TargetFriends->PendingFriendInviteExpiry = Now + FriendInviteDuration;
	NotifyOwner(FString::Printf(TEXT("%s oyuncusuna arkadaslik istegi gonderildi."), *Canonical));
	TargetFriends->NotifyOwner(FString::Printf(
		TEXT("%s sana arkadaslik istegi gonderdi. Kabul: /arkadas kabul (20 sn)"), *MyName));
}

void UMMOFriendsComponent::CmdAccept()
{
	CmdAcceptSeen(PendingInviteId);
}

void UMMOFriendsComponent::CmdAcceptSeen(uint64 SeenInviteId)
{
	AMMOCharacter* Me = GetOwnerCharacter();
	const UWorld* World = GetWorld();
	if (!HasAuthorityInternal() || !Me || !World)
	{
		return;
	}

	if (SeenInviteId == 0 || SeenInviteId != PendingInviteId) return;
	AMMOCharacter* Inviter = PendingFriendInviter.Get();
	const float Now = World->GetTimeSeconds();
	PendingFriendInviter.Reset();
	const bool bExpired = Now > PendingFriendInviteExpiry;
	PendingFriendInviteExpiry = -1.f;
	if (!Inviter || bExpired || Inviter->GetWorld() != World)
	{
		NotifyOwner(TEXT("Bekleyen arkadaslik istegi yok veya suresi doldu."));
		return;
	}

	UMMOFriendsComponent* InviterFriends = FriendsCompOf(Inviter);
	const FString MyName = FriendPlayerNameOf(Me);
	const FString InviterName = FriendPlayerNameOf(Inviter);
	if (!InviterFriends || MyName.IsEmpty() || InviterName.IsEmpty())
	{
		NotifyOwner(TEXT("Arkadaslik istegi tamamlanamadi."));
		return;
	}
	if ((!HasFriend(InviterName) && Friends.Num() >= MaxFriends)
		|| (!InviterFriends->HasFriend(MyName) && InviterFriends->Friends.Num() >= MaxFriends))
	{
		NotifyOwner(TEXT("Arkadas listelerinden biri dolu; istek tamamlanamadi."));
		InviterFriends->NotifyOwner(TEXT("Arkadaslik istegi liste siniri nedeniyle tamamlanamadi."));
		return;
	}

	if (!HasFriend(InviterName))
	{
		Friends.Add(InviterName);
	}
	if (!InviterFriends->HasFriend(MyName))
	{
		InviterFriends->Friends.Add(MyName);
	}
	NotifyOwner(FString::Printf(TEXT("%s ile arkadas oldunuz."), *InviterName));
	InviterFriends->NotifyOwner(FString::Printf(TEXT("%s arkadaslik istegini kabul etti."), *MyName));
	SaveThrottled(Me);
	InviterFriends->SaveThrottled(Inviter);
}

#if !UE_BUILD_SHIPPING
bool UMMOFriendsComponent::RunInviteGenerationBoundarySelfTest(AMMOCharacter* A, AMMOCharacter* B,
	AMMOCharacter* Recipient, FString& OutError, int32& OutChecks)
{
	UWorld* World = IsValid(Recipient) ? Recipient->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld() || World->GetNetMode() != NM_Standalone
		|| !FParse::Param(FCommandLine::Get(), TEXT("unattended")) || !FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))
		|| !IsValid(A) || !IsValid(B) || A == B || A == Recipient || B == Recipient
		|| A->GetWorld() != World || B->GetWorld() != World)
	{ OutError = TEXT("friend boundary disposable fixture required"); return false; }
	UMMOFriendsComponent* AF = A->GetFriendsComponent(); UMMOFriendsComponent* BF = B->GetFriendsComponent();
	UMMOFriendsComponent* RF = Recipient->GetFriendsComponent();
	if (!AF || !BF || !RF) { OutError = TEXT("friend boundary components missing"); return false; }
	const FString AName = FriendPlayerNameOf(A), BName = FriendPlayerNameOf(B), RName = FriendPlayerNameOf(Recipient);
	auto Check = [&](bool Value, const TCHAR* Label)
	{
		++OutChecks; if (!Value) OutError = FString::Printf(TEXT("%s (check=%d)"), Label, OutChecks); return Value;
	};
	RF->CmdRemove(BName);
	if (!Check(AF->Friends.IsEmpty() && BF->Friends.IsEmpty() && RF->Friends.IsEmpty(), TEXT("boundary mutual cleanup"))) return false;
	AF->CmdAdd(RName); const uint64 ExpiredId = RF->PendingInviteId;
	RF->PendingFriendInviteExpiry = World->GetTimeSeconds() - 1.f; // Drive expiry without ticking actors/load timers.
	RF->CmdAcceptSeen(ExpiredId);
	if (!Check(RF->Friends.IsEmpty() && AF->Friends.IsEmpty() && !RF->PendingFriendInviter.IsValid()
		&& RF->PendingFriendInviteExpiry < 0.f && RF->PendingInviteId == ExpiredId, TEXT("expired friend consent/reset generation"))) return false;
	BF->CmdAdd(RName); const uint64 Fresh = RF->PendingInviteId; const float Expiry = RF->PendingFriendInviteExpiry;
	if (!Check(Fresh > ExpiredId && RF->PendingFriendInviter.Get() == B, TEXT("expired friend replacement issuance"))) return false;
	RF->CmdAcceptSeen(ExpiredId);
	if (!Check(RF->PendingFriendInviter.Get() == B && RF->PendingFriendInviteExpiry == Expiry && RF->PendingInviteId == Fresh
		&& RF->Friends.IsEmpty() && BF->Friends.IsEmpty(), TEXT("stale direct consent consumed pending friend"))) return false;
	RF->CmdAcceptSeen(Fresh);
	if (!Check(RF->HasFriend(BName) && BF->HasFriend(RName) && !RF->PendingFriendInviter.IsValid()
		&& RF->PendingFriendInviteExpiry < 0.f && RF->PendingInviteId == Fresh, TEXT("fresh boundary friend consent"))) return false;
	BF->CmdAdd(RName);
	if (!Check(RF->PendingInviteId == Fresh && !RF->PendingFriendInviter.IsValid(), TEXT("duplicate friendship rotated invite"))) return false;
	RF->CmdRemove(BName); RF->PendingInviteId = MAX_uint64 - 1;
	AF->CmdAdd(RName);
	if (!Check(RF->PendingInviteId == MAX_uint64 && RF->PendingFriendInviter.Get() == A, TEXT("last friend generation issuance"))) return false;
	const float MaxExpiry = RF->PendingFriendInviteExpiry; BF->CmdAdd(RName);
	if (!Check(RF->PendingInviteId == MAX_uint64 && RF->PendingFriendInviter.Get() == A
		&& RF->PendingFriendInviteExpiry == MaxExpiry, TEXT("occupied friend invite replaced/refreshed"))) return false;
	RF->CmdAcceptSeen(MAX_uint64);
	if (!Check(RF->HasFriend(AName) && AF->HasFriend(RName) && RF->PendingInviteId == MAX_uint64
		&& !RF->PendingFriendInviter.IsValid(), TEXT("last friend generation acceptance"))) return false;
	RF->CmdRemove(AName); BF->CmdAdd(RName);
	return Check(RF->PendingInviteId == MAX_uint64 && !RF->PendingFriendInviter.IsValid() && RF->PendingFriendInviteExpiry < 0.f
		&& RF->Friends.IsEmpty() && AF->Friends.IsEmpty() && BF->Friends.IsEmpty(), TEXT("friend generation wrapped/reissued after exhaustion"));
}
#endif

void UMMOFriendsComponent::CmdRemove(const FString& Name)
{
	AMMOCharacter* Me = GetOwnerCharacter();
	if (!HasAuthorityInternal() || !Me)
	{
		return;
	}
	const FString N = Name.TrimStartAndEnd();
	if (N.IsEmpty())
	{
		NotifyOwner(TEXT("Kullanim: /arkadas sil isim"));
		return;
	}
	for (int32 i = 0; i < Friends.Num(); ++i)
	{
		if (Friends[i].Equals(N, ESearchCase::IgnoreCase))
		{
			const FString Removed = Friends[i];
			Friends.RemoveAt(i);
			NotifyOwner(FString::Printf(TEXT("%s arkadas listenden silindi."), *Removed));
			// Karsi taraf cevrimiciyse iliskiyi iki listeden de kaldir. Cevrimdisi eski kayit
			// tek yonlu kalsa bile CmdList/AnnouncePresence karsiliklilik aradigi icin durum ve
			// seviye takibi yapamaz; bir sonraki kabul iliskiyi yeniden kurabilir.
			if (AMMOCharacter* Other = FindOnlineByName(GetWorld(), Removed))
			{
				if (UMMOFriendsComponent* OtherFriends = FriendsCompOf(Other))
				{
					for (int32 OtherIndex = OtherFriends->Friends.Num() - 1; OtherIndex >= 0; --OtherIndex)
					{
						if (OtherFriends->Friends[OtherIndex].Equals(FriendPlayerNameOf(Me), ESearchCase::IgnoreCase))
						{
							OtherFriends->Friends.RemoveAt(OtherIndex);
						}
					}
					OtherFriends->NotifyOwner(FString::Printf(TEXT("%s arkadasligini kaldirdi."), *FriendPlayerNameOf(Me)));
					OtherFriends->SaveThrottled(Other);
				}
			}
			SaveThrottled(Me);
			return;
		}
	}
	NotifyOwner(FString::Printf(TEXT("%s arkadas listende yok."), *N.Left(MaxNameLength)));
}

void UMMOFriendsComponent::SaveThrottled(AMMOCharacter* Me)
{
	// Tam diske kayit pahali: ekle/sil spam'i saniyede onlarca yazma uretmesin. Arada kalan
	// degisiklikler 60 sn'lik otomatik kayitla ve cikista yazilir.
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	if (Me && Now - LastFriendSaveTime >= FriendSaveCooldown)
	{
		LastFriendSaveTime = Now;
		Me->SaveCharacter();
	}
}

void UMMOFriendsComponent::CmdList()
{
	if (!HasAuthorityInternal())
	{
		return;
	}
	if (Friends.IsEmpty())
	{
		NotifyOwner(TEXT("Arkadas listen bos. /arkadas ekle isim"));
		return;
	}
	// NETWORK_AUDIT O2: tum liste TEK mesajda (tek reliable RPC); satirlar '\n' ile ayrilir,
	// istemci ClientReceiveChat'te sistem kanalini satir satir boler.
	FString ListText = FString::Printf(TEXT("Arkadaslar (%d/%d):"), Friends.Num(), MaxFriends);
	for (const FString& F : Friends)
	{
		const AMMOCharacter* C = FindOnlineByName(GetWorld(), F);
		const UMMOFriendsComponent* OtherFriends = FriendsCompOf(C);
		const FString MyName = FriendPlayerNameOf(GetOwnerCharacter());
		if (C && OtherFriends && OtherFriends->HasFriend(MyName))
		{
			const int32 Lv = C->GetStats() ? C->GetStats()->Level : 0;
			ListText += FString::Printf(TEXT("\n%s - cevrimici (Lv%d)"), *F, Lv);
		}
		else if (C)
		{
			ListText += FString::Printf(TEXT("\n%s - karsilikli onay bekliyor"), *F);
		}
		else
		{
			ListText += FString::Printf(TEXT("\n%s - cevrimdisi"), *F);
		}
	}
	NotifyOwner(ListText);
}

void UMMOFriendsComponent::AnnouncePresence(bool bOnline, const FString& NameOverride) const
{
	const UWorld* World = GetWorld();
	const AMMOCharacter* Me = GetOwnerCharacter();
	if (!HasAuthorityInternal() || !World || World->bIsTearingDown || !Me)
	{
		return;
	}
	const FString MyName = NameOverride.IsEmpty() ? FriendPlayerNameOf(Me) : NameOverride;
	if (MyName.IsEmpty())
	{
		return;
	}

	// Durum tekrari bastirma: ayni ad icin ayni durum zaten duyurulduysa sus. Bolge gecisinde
	// eski pawn "cevrimdisi" duyurmaz, yeni pawn tekrar "cevrimici" demez. Guvenlik: 5 dk'dan eski
	// "cevrimici" kaydi (kopma sirasinda cevrimdisi kacmis olabilir) yenilenebilir.
	TMap<FString, TPair<bool, double>>& GAnnounced = MMOFriendPresenceRecords();
	const FString Key = MyName.ToLower();
	const double NowSec = FPlatformTime::Seconds();
	if (const TPair<bool, double>* Last = GAnnounced.Find(Key))
	{
		if (Last->Key == bOnline && (NowSec - Last->Value) < 300.0)
		{
			return;
		}
	}
	if (!bOnline && !GAnnounced.Contains(Key))
	{
		return; // hic cevrimici duyurulmamis ad icin cevrimdisi duyuru gitmez
	}
	// NETWORK_AUDIT D6: tablo sinirsiz buyumesin; 512'yi asinca 5 dk'dan eski kayitlar silinir.
	if (GAnnounced.Num() > 512)
	{
		for (auto It = GAnnounced.CreateIterator(); It; ++It)
		{
			if ((NowSec - It.Value().Value) >= 300.0)
			{
				It.RemoveCurrent();
			}
		}
	}
	GAnnounced.Add(Key, TPair<bool, double>(bOnline, NowSec));
	const FString Text = FString::Printf(TEXT("%s %s"), *MyName, bOnline ? TEXT("cevrimici oldu.") : TEXT("cevrimdisi oldu."));
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const AMMOCharacter* Other = PC ? Cast<AMMOCharacter>(PC->GetPawn()) : nullptr;
		const UMMOFriendsComponent* FC = FriendsCompOf(Other);
		if (FC && Other != Me && FC->HasFriend(MyName) && HasFriend(FriendPlayerNameOf(Other)))
		{
			FC->NotifyOwner(Text);
		}
	}
}

// ----------------------------------------------------------------------
// Kalicilik
// ----------------------------------------------------------------------

void UMMOFriendsComponent::FillSaveData(FMMOCharacterSaveData& Out) const
{
	Out.Friends = Friends;
}

void UMMOFriendsComponent::ApplySaveData(const FMMOCharacterSaveData& In)
{
	if (!HasAuthorityInternal())
	{
		return;
	}
	// Eski kayitlarda bos gelir; bozuk/tekrarlayan/cok uzun adlar ve fazla kayit elenir.
	Friends.Reset();
	for (const FString& Raw : In.Friends)
	{
		const FString N = Raw.TrimStartAndEnd();
		if (N.IsEmpty() || N.Len() > MaxNameLength || HasFriend(N))
		{
			continue;
		}
		Friends.Add(N);
		if (Friends.Num() >= MaxFriends)
		{
			break;
		}
	}
}


#if !UE_BUILD_SHIPPING
bool UMMOFriendsComponent::RunPresenceLifecycleSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || World->GetNetMode() != NM_Standalone
		|| !FParse::Param(FCommandLine::Get(), TEXT("unattended"))
		|| !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
	{
		OutError = TEXT("disposable standalone headless world required"); return false;
	}
	// Avoid production-cache pruning by the synthetic fixture. Only its GUID keys are removed.
	if (MMOFriendPresenceRecords().Num() > 480) { OutError = TEXT("presence cache too full for isolated fixture"); return false; }
	const FString Prefix = TEXT("ZPres") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(16);
	TArray<FString> Names;
	TArray<AMMOCharacter*> Pawns;
	TArray<AMMOPlayerController*> PCs;
	TArray<AMMOHUD*> HUDs;
	for (int32 I = 0; I < 10; ++I) Names.Add(Prefix + FString::FromInt(I));
	for (const FString& Name : Names)
	{
		if (MMOFriendPresenceRecords().Contains(Name.ToLower())) { OutError = TEXT("presence fixture key collision"); return false; }
	}
	for (int32 I = 0; I < Names.Num(); ++I)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AMMOCharacter* Pawn = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(),
			FVector(0., 0., 5000. + I * 200.), FRotator::ZeroRotator, Params);
		AMMOPlayerController* PC = World->SpawnActor<AMMOPlayerController>();
		Pawns.Add(Pawn); PCs.Add(PC);
		if (!Pawn || !PC) { OutError = TEXT("fixture spawn failed"); break; }
		Pawn->SetActorTickEnabled(false); PC->SetActorTickEnabled(false);
		PC->Possess(Pawn);
		if (!Pawn->GetPlayerState()) { OutError = TEXT("fixture PlayerState missing"); break; }
		Pawn->GetPlayerState()->SetPlayerName(Names[I]);
		PC->ClientSetHUD(AMMOHUD::StaticClass()); // Native client endpoint in standalone, no rendering.
		AMMOHUD* HUD = Cast<AMMOHUD>(PC->GetHUD());
		HUDs.Add(HUD);
		if (!HUD) { OutError = TEXT("fixture HUD missing"); break; }
		HUD->SetActorTickEnabled(false);
	}
	const auto Check = [&](bool bOk, const TCHAR* Why)
	{
		++OutChecks;
		if (!bOk && OutError.IsEmpty()) OutError = Why;
		return bOk;
	};
	const auto Count = [&](int32 Observer, const FString& Name, bool bOnline)
	{
		const FString Expected = Name + (bOnline ? TEXT(" cevrimici oldu.") : TEXT(" cevrimdisi oldu."));
		int32 N = 0;
		for (const FMMOChatLine& L : HUDs[Observer]->GetChatLinesForTests()) if (L.Text.Equals(Expected, ESearchCase::CaseSensitive)) ++N;
		return N;
	};
	const auto Run = [&]() -> bool
	{
		if (!OutError.IsEmpty()) return false;
		for (int32 I = 2; I < Pawns.Num(); ++I)
		{
			Pawns[0]->GetFriendsComponent()->Friends.Add(Names[I]);
			Pawns[1]->GetFriendsComponent()->Friends.Add(Names[I]); // One-way, never consented by source.
			Pawns[I]->GetFriendsComponent()->Friends.Add(Names[0]);
		}
		if (!Check(Pawns[2]->IsPlayerControlled() && HUDs[0]->GetChatLinesForTests().IsEmpty(), TEXT("presence fixture invalid"))) return false;
		Pawns[2]->GetFriendsComponent()->AnnouncePresence(true);
		if (!Check(Count(0, Names[2], true) == 1, TEXT("mutual online not delivered to native HUD"))) return false;
		Pawns[2]->GetFriendsComponent()->AnnouncePresence(true);
		if (!Check(Count(0, Names[2], true) == 1, TEXT("duplicate online delivered"))) return false;
		if (!Check(HUDs[1]->GetChatLinesForTests().IsEmpty() && HUDs[2]->GetChatLinesForTests().IsEmpty(), TEXT("presence leaked to one-way or self"))) return false;
		if (!Check(Pawns[2]->Destroy(), TEXT("possessed Destroy rejected"))) return false;
		if (!Check(Count(0, Names[2], false) == 1, TEXT("possessed Destroy lost offline notification"))) return false;
		const TPair<bool, double>* Last = MMOFriendPresenceRecords().Find(Names[2].ToLower());
		if (!Check(Last && !Last->Key, TEXT("destroy retained online dedupe state"))) return false;

		// A fresh pawn reconnecting under the same name within the 300s dedupe window.
		const FString ReconnectName = Names[2].ToLower();
		Pawns[6]->GetPlayerState()->SetPlayerName(ReconnectName);
		Pawns[6]->GetFriendsComponent()->AnnouncePresence(true);
		if (!Check(Count(0, Names[2], true) == 1 && Count(0, ReconnectName, true) == 1, TEXT("immediate reconnect online suppressed"))) return false;
		Pawns[6]->GetFriendsComponent()->AnnouncePresence(true);
		if (!Check(Count(0, Names[2], true) == 1 && Count(0, ReconnectName, true) == 1, TEXT("reconnect duplicate online delivered"))) return false;
		if (!Check(Pawns[6]->Destroy() && Count(0, Names[2], false) == 1 && Count(0, ReconnectName, false) == 1, TEXT("reconnected pawn offline lost or duplicated"))) return false;

		Pawns[3]->GetFriendsComponent()->AnnouncePresence(true);
		PCs[3]->UnPossess();
		if (!Check(!Pawns[3]->GetPlayerState() && Count(0, Names[3], false) == 1, TEXT("UnPossess lost old name offline"))) return false;
		Pawns[3]->GetFriendsComponent()->AnnouncePresence(false, Names[3]);
		if (!Check(Count(0, Names[3], false) == 1, TEXT("duplicate detached offline delivered"))) return false;
		PCs[3]->Possess(Pawns[3]);
		Pawns[3]->GetFriendsComponent()->AnnouncePresence(true);
		if (!Check(Count(0, Names[3], true) == 2, TEXT("repossess online suppressed"))) return false;
		PCs[3]->UnPossess();
		if (!Check(Pawns[3]->Destroy() && Count(0, Names[3], false) == 2, TEXT("post-UnPossess Destroy duplicated offline"))) return false;

		Pawns[4]->GetFriendsComponent()->AnnouncePresence(true);
		Pawns[4]->SetTravelSaveCommitted(true);
		if (!Check(Pawns[4]->Destroy() && Count(0, Names[4], false) == 0, TEXT("committed travel announced offline"))) return false;
		Pawns[7]->GetPlayerState()->SetPlayerName(Names[4]);
		Pawns[7]->GetFriendsComponent()->AnnouncePresence(true);
		if (!Check(Count(0, Names[4], true) == 1, TEXT("travel replacement duplicated online"))) return false;
		// Synthetic abort flag reset, not a real GameMode travel abort.
		Pawns[7]->SetTravelSaveCommitted(true);
		Pawns[7]->SetTravelSaveCommitted(false);
		if (!Check(Pawns[7]->Destroy() && Count(0, Names[4], false) == 1, TEXT("abort flag reset suppressed disconnect"))) return false;

		Pawns[5]->GetFriendsComponent()->AnnouncePresence(true);
		Pawns[5]->RouteEndPlay(EEndPlayReason::LevelTransition); // NOT actual map travel.
		if (!Check(!Pawns[5]->GetPlayerState() && Count(0, Names[5], false) == 0, TEXT("transition route announced offline"))) return false;
		Pawns[8]->GetPlayerState()->SetPlayerName(Names[5]);
		Pawns[8]->GetFriendsComponent()->AnnouncePresence(true);
		if (!Check(Count(0, Names[5], true) == 1, TEXT("transition replacement duplicated online"))) return false;

		const int32 BeforeTeardown = HUDs[0]->GetChatLinesForTests().Num();
		const bool bWasTearingDown = World->bIsTearingDown;
		World->bIsTearingDown = true;
		Pawns[8]->GetFriendsComponent()->AnnouncePresence(false);
		World->bIsTearingDown = bWasTearingDown;
		const TPair<bool, double>* TransitionLast = MMOFriendPresenceRecords().Find(Names[5].ToLower());
		if (!Check(HUDs[0]->GetChatLinesForTests().Num() == BeforeTeardown && TransitionLast && TransitionLast->Key,
			TEXT("teardown guard changed presence"))) return false;
		FActorSpawnParameters NPCParams;
		NPCParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AMMOCharacter* NPC = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(),
			FVector(0., 0., 9000.), FRotator::ZeroRotator, NPCParams);
		const bool bNPCDestroyed = NPC && NPC->Destroy();
		if (IsValid(NPC)) NPC->Destroy();
		if (!Check(bNPCDestroyed && HUDs[0]->GetChatLinesForTests().Num() == BeforeTeardown,
			TEXT("uncontrolled NPC changed presence"))) return false;
		bool bSystemColor = true;
		for (const FMMOChatLine& L : HUDs[0]->GetChatLinesForTests()) bSystemColor &= L.Color == FLinearColor(1.f, 0.65f, 0.2f);
		if (!Check(bSystemColor, TEXT("presence channel color changed"))) return false;

		if (!Check(Pawns[9]->Destroy() && Count(0, Names[9], false) == 0
			&& !MMOFriendPresenceRecords().Contains(Names[9].ToLower()), TEXT("never-online pawn announced offline"))) return false;
		if (!Check(HUDs[1]->GetChatLinesForTests().IsEmpty() && HUDs[9]->GetChatLinesForTests().IsEmpty(), TEXT("lifecycle presence leaked to one-way or unrelated player"))) return false;
		return true;
	};
	const bool bPassed = Run();
	// No LoadCharacter/world tick: fixtures never write a save. Skip cleanup announcements.
	for (AMMOCharacter* Pawn : Pawns) if (IsValid(Pawn)) Pawn->SetTravelSaveCommitted(true);
	for (AMMOPlayerController* PC : PCs) if (IsValid(PC)) { PC->UnPossess(); PC->Destroy(); }
	for (AMMOCharacter* Pawn : Pawns) if (IsValid(Pawn)) Pawn->Destroy();
	for (const FString& Name : Names) MMOFriendPresenceRecords().Remove(Name.ToLower());
	return bPassed;
}
namespace
{
	struct FMMOFriendsPresenceTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestFriendsPresence"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = UMMOFriendsComponent::RunPresenceLifecycleSelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_FRIENDS_PRESENCE_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOFriendsPresenceTestExec GMMOFriendsPresenceTestExec;
}
#endif

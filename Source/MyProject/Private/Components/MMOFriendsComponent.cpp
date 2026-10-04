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

namespace
{
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
	if (HasFriend(Canonical))
	{
		NotifyOwner(FString::Printf(TEXT("%s zaten arkadas listende."), *Canonical));
		return;
	}
	Friends.Add(Canonical);
	NotifyOwner(FString::Printf(TEXT("%s arkadas listene eklendi."), *Canonical));
	SaveThrottled(Me);
}

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
		if (C)
		{
			const int32 Lv = C->GetStats() ? C->GetStats()->Level : 0;
			ListText += FString::Printf(TEXT("\n%s - cevrimici (Lv%d)"), *F, Lv);
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
	static TMap<FString, TPair<bool, double>> GAnnounced;
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
	GAnnounced.Add(Key, TPair<bool, double>(bOnline, NowSec));
	const FString Text = FString::Printf(TEXT("%s %s"), *MyName, bOnline ? TEXT("cevrimici oldu.") : TEXT("cevrimdisi oldu."));
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const AMMOCharacter* Other = PC ? Cast<AMMOCharacter>(PC->GetPawn()) : nullptr;
		const UMMOFriendsComponent* FC = FriendsCompOf(Other);
		if (FC && Other != Me && FC->HasFriend(MyName))
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

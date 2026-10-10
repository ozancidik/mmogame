// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMOMailComponent.h"
#include "Characters/MMOCharacter.h"
#include "Characters/MMOMob.h"
#include "Components/MMOInventoryComponent.h"
#include "Core/MMONotify.h"
#include "Player/MMOPlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"

UMMOMailComponent::UMMOMailComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UMMOMailComponent::HasAuthorityInternal() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

AMMOCharacter* UMMOMailComponent::GetOwnerCharacter() const
{
	return Cast<AMMOCharacter>(GetOwner());
}

double UMMOMailComponent::Now() const
{
	return bTestMode ? TestClock : FPlatformTime::Seconds();
}

FString UMMOMailComponent::GetMyName() const
{
	if (bTestMode)
	{
		return TestName;
	}
	if (const AMMOCharacter* C = GetOwnerCharacter())
	{
		if (const APlayerState* PS = C->GetPlayerState())
		{
			return PS->GetPlayerName();
		}
	}
	return FString();
}

bool UMMOMailComponent::RecipientExists(const FString& Name) const
{
	const FString Wanted = FMMOMailStore::SanitizeName(Name);
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AMMOCharacter> It(World); It; ++It)
		{
			const AMMOCharacter* C = *It;
			if (!IsValid(C) || Cast<AMMOMob>(C))
			{
				continue;
			}
			if (const APlayerState* PS = C->GetPlayerState())
			{
				if (FMMOMailStore::SanitizeName(PS->GetPlayerName()).Equals(Wanted, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
		}
	}
	// Cevrimdisi: karakter kayit dosyasi var mi (yerel depo; bulut depoya gecince degistirilmeli).
	const FString SavePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), FString(TEXT("MMOSave_")) + Wanted + TEXT(".sav"));
	return IFileManager::Get().FileExists(*SavePath);
}

void UMMOMailComponent::BeginPlay()
{
	Super::BeginPlay();
	if (Cast<AMMOMob>(GetOwner()) || !HasAuthorityInternal())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(PeriodicTimer, this, &UMMOMailComponent::OnPeriodic, CollectSeconds, true, CollectSeconds);
	}
}

void UMMOMailComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PeriodicTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void UMMOMailComponent::OnPeriodic()
{
	CollectMail();
}

FString UMMOMailComponent::ResultText(EMMOMailResult R)
{
	switch (R)
	{
	case EMMOMailResult::Ok:            return TEXT("Posta gonderildi.");
	case EMMOMailResult::BadTarget:     return TEXT("Gecersiz alici adi.");
	case EMMOMailResult::Self:          return TEXT("Kendine posta gonderemezsin.");
	case EMMOMailResult::UnknownTarget: return TEXT("Bu isimde bir karakter bulunamadi.");
	case EMMOMailResult::NoGold:        return TEXT("Yeterli altinin yok (ek + gonderim ucreti).");
	case EMMOMailResult::BadGold:       return TEXT("Gecersiz altin miktari (en fazla 1.000.000).");
	case EMMOMailResult::EmptyMail:     return TEXT("Bos posta gonderilemez: mesaj ya da altin ekle.");
	case EMMOMailResult::TextTooLong:   return TEXT("Mesaj en fazla 200 karakter olabilir.");
	case EMMOMailResult::Cooldown:      return TEXT("Cok hizli: iki posta arasinda 5 saniye bekle.");
	case EMMOMailResult::BoxFull:       return TEXT("Alicinin posta kutusu dolu.");
	case EMMOMailResult::StoreError:    return TEXT("Posta su an gonderilemedi; altinin iade edildi / alinmadi.");
	}
	return FString();
}

void UMMOMailComponent::NotifyChat(const FString& Text) const
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

EMMOMailResult UMMOMailComponent::SendMail(const FString& ToName, int32 Gold, const FString& Text, bool bRecipientExists)
{
	AMMOCharacter* Owner = GetOwnerCharacter();
	UMMOInventoryComponent* Inv = Owner ? Owner->GetInventory() : nullptr;
	const FString Me = GetMyName();
	if (!HasAuthorityInternal() || !Inv || !FMMOMailStore::IsDeliverableName(Me))
	{
		return EMMOMailResult::StoreError;
	}
	const FString To = ToName.TrimStartAndEnd();
	if (To.IsEmpty() || To.Len() > FMMOMailStore::MaxNameLength || !FMMOMailStore::IsDeliverableName(To))
	{
		return EMMOMailResult::BadTarget;
	}
	if (FMMOMailStore::SanitizeName(To).Equals(FMMOMailStore::SanitizeName(Me), ESearchCase::IgnoreCase))
	{
		return EMMOMailResult::Self;
	}
	if (Gold < 0 || Gold > FMMOMailStore::MaxGoldPerMail)
	{
		return EMMOMailResult::BadGold;
	}
	const FString Body = Text.TrimStartAndEnd();
	if (Body.Len() > FMMOMailStore::MaxTextLength)
	{
		return EMMOMailResult::TextTooLong;
	}
	if (Body.IsEmpty() && Gold <= 0)
	{
		return EMMOMailResult::EmptyMail;
	}
	if (Now() - LastSendTime < CooldownSeconds)
	{
		return EMMOMailResult::Cooldown;
	}
	if (!bRecipientExists)
	{
		return EMMOMailResult::UnknownTarget;
	}
	const int64 Total = static_cast<int64>(Gold) + FeeFor(Gold);
	if (Inv->Gold < Total)
	{
		return EMMOMailResult::NoGold;
	}
	if (FMMOMailStore::Count(To) >= FMMOMailStore::MaxMailsPerBox)
	{
		return EMMOMailResult::BoxFull;
	}

	LastSendTime = Now(); // altin callback'i bu fonksiyonu yeniden cagirsa bile ikinci gonderim cooldown'a takilir
	const int32 Charge = static_cast<int32>(Total);
	Inv->AddGoldFrom(-Charge, TEXT("posta"));
	FMMOMail M;
	M.From = Me.Left(FMMOMailStore::MaxNameLength);
	M.Text = Body;
	M.Gold = Gold;
	M.SentUnix = FDateTime::UtcNow().ToUnixTimestamp();
	if (!FMMOMailStore::Append(To, M))
	{
		if (IsValid(Owner) && Owner->GetInventory())
		{
			Owner->GetInventory()->AddGoldFrom(Charge, TEXT("posta-iade"));
		}
		return EMMOMailResult::StoreError;
	}
	return EMMOMailResult::Ok;
}

int32 UMMOMailComponent::CollectMail()
{
	AMMOCharacter* Owner = GetOwnerCharacter();
	if (!HasAuthorityInternal() || bCollecting || !Owner)
	{
		return 0;
	}
	const FString Me = GetMyName();
	if (!FMMOMailStore::IsDeliverableName(Me))
	{
		return 0;
	}
	// Karakter kaydi yuklenmeden altin verme: kayit yuklenirken altin ustune yazilir ve posta kutusu bosaldigi icin kaybolurdu.
	if (!bTestMode && !Owner->IsInitialLoadComplete())
	{
		return 0;
	}
	TArray<FMMOMail> Box;
	if (!FMMOMailStore::TakeAll(Me, Box) || Box.IsEmpty())
	{
		return 0;
	}
	TGuardValue<bool> Guard(bCollecting, true);
	const TWeakObjectPtr<AMMOCharacter> WeakOwner(Owner);
	int32 Delivered = 0;
	for (const FMMOMail& M : Box)
	{
		if (!WeakOwner.IsValid())
		{
			break;
		}
		if (M.Gold > 0)
		{
			if (UMMOInventoryComponent* Inv = WeakOwner->GetInventory())
			{
				Inv->AddGoldFrom(M.Gold, TEXT("posta"));
			}
		}
		++Delivered;
		FString Line = FString::Printf(TEXT("Posta [%s]: %s"), *M.From, M.Text.IsEmpty() ? TEXT("(mesaj yok)") : *M.Text);
		if (M.Gold > 0)
		{
			Line += FString::Printf(TEXT("  (+%d altin)"), M.Gold);
		}
		NotifyChat(Line);
	}
	if (Delivered > 0 && WeakOwner.IsValid())
	{
		MMONotify::ToOwner(WeakOwner.Get(), FString::Printf(TEXT("Yeni posta: %d"), Delivered), FColor(255, 230, 140), 4.f);
	}
	return Delivered;
}

void UMMOMailComponent::CmdSend(const FString& Args)
{
	if (!HasAuthorityInternal())
	{
		return;
	}
	FString Name, Rest;
	const FString Trimmed = Args.TrimStartAndEnd();
	if (!Trimmed.Split(TEXT(" "), &Name, &Rest))
	{
		Name = Trimmed;
		Rest.Reset();
	}
	Rest = Rest.TrimStartAndEnd();
	FString GoldStr, Text;
	if (!Rest.Split(TEXT(" "), &GoldStr, &Text))
	{
		GoldStr = Rest;
		Text.Reset();
	}
	int32 Gold = 0;
	if (!GoldStr.IsEmpty() && GoldStr.IsNumeric() && !GoldStr.Contains(TEXT(".")) && !GoldStr.Contains(TEXT("-")))
	{
		const int64 G = FCString::Atoi64(*GoldStr);
		Gold = static_cast<int32>(FMath::Min<int64>(G, static_cast<int64>(FMMOMailStore::MaxGoldPerMail) + 1));
	}
	else
	{
		Text = Rest; // ikinci sozcuk sayi degil: tum kalan mesajdir, ek yok
	}
	const bool bExists = !Name.IsEmpty() && RecipientExists(Name);
	const EMMOMailResult R = SendMail(Name, Gold, Text, bExists);
	if (R == EMMOMailResult::Ok)
	{
		NotifyChat(FString::Printf(TEXT("Posta gonderildi: %s  (ek %d altin, ucret %d altin)"), *Name.TrimStartAndEnd(), Gold, FeeFor(Gold)));
	}
	else
	{
		NotifyChat(ResultText(R));
	}
}

void UMMOMailComponent::CmdInfo()
{
	if (!HasAuthorityInternal())
	{
		return;
	}
	const FString Me = GetMyName();
	const int32 Pending = FMMOMailStore::IsDeliverableName(Me) ? FMMOMailStore::Count(Me) : 0;
	NotifyChat(FString::Printf(
		TEXT("Posta: /posta gonder isim [altin] mesaj\nUcret: %d altin + eke %%2. En fazla 1.000.000 altin, 200 karakter; aliciya 20 posta. Bekleyen postan: %d (10 sn'de bir otomatik gelir)."),
		FlatFee, Pending));
}

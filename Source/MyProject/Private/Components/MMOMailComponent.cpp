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
	// Ayni metin (B3): ayri mesajlar oyuncu adlarinin varligini sorgulamaya (oracle) yarardi.
	case EMMOMailResult::UnknownTarget:
	case EMMOMailResult::NoGold:        return TEXT("Posta gonderilemedi: alici bulunamadi ya da altin yetersiz (ek + gonderim ucreti).");
	case EMMOMailResult::NameChanged:   return TEXT("Adin degisti: posta icin yeniden gir.");
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

bool UMMOMailComponent::IsOwnerLocked(const AMMOCharacter& Owner) const
{
	if (bTestMode)
	{
		return bTestTravelLocked;
	}
	// Yuklenmemis karakter kaydedilemez (altin dusup kalici olmazdi); gecis onayindan sonra EndPlay kaydi da atlanir.
	return !Owner.IsInitialLoadComplete() || Owner.IsTravelSaveCommitted();
}

bool UMMOMailComponent::IsNameChanged(const AMMOCharacter& Owner) const
{
	if (bTestMode)
	{
		return bTestNameChanged;
	}
	// B2: ad degistirilip karakter yeni yuvaya yazilmadan posta islemi = eski ve yeni yuvada ayni altin (catallama).
	return !Owner.GetSaveSlotName().Equals(Owner.GetActiveSaveSlot(), ESearchCase::IgnoreCase);
}

bool UMMOMailComponent::SaveOwner()
{
	if (bTestMode)
	{
		++TestSaveCalls;
		return bTestSaveResult;
	}
	AMMOCharacter* C = GetOwnerCharacter();
	return IsValid(C) && !C->IsActorBeingDestroyed() && C->HasAuthority() && C->SaveCharacterForTravel();
}

EMMOMailResult UMMOMailComponent::SendMail(const FString& ToName, int32 Gold, const FString& Text, bool bRecipientExists)
{
	AMMOCharacter* Owner = GetOwnerCharacter();
	UMMOInventoryComponent* Inv = Owner ? Owner->GetInventory() : nullptr;
	const FString Me = GetMyName();
	if (!HasAuthorityInternal() || !Inv || !FMMOMailStore::IsDeliverableName(Me) || IsOwnerLocked(*Owner))
	{
		return EMMOMailResult::StoreError;
	}
	if (IsNameChanged(*Owner))
	{
		return EMMOMailResult::NameChanged;
	}
	const FString To = ToName.TrimStartAndEnd();
	if (To.IsEmpty() || To.Len() > FMMOMailStore::MaxNameLength || !FMMOMailStore::IsDeliverableName(To))
	{
		return EMMOMailResult::BadTarget;
	}
	// C1: temizleme adi sessizce degistirmesin ("Al!i" -> "Ali" baska oyuncuya gitmesin). Yalniz bosluk -> '_' esdegerdir.
	if (!FMMOMailStore::SanitizeName(To).Equals(To.Replace(TEXT(" "), TEXT("_")), ESearchCase::CaseSensitive))
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
	// B3: altin kontrolu varlik kontrolunden ONCE ve basarisiz varlik denemesi cooldown'a yazilir (ad tarama oracle'i olmasin).
	const int64 Total = static_cast<int64>(Gold) + FeeFor(Gold);
	if (Inv->Gold < Total)
	{
		return EMMOMailResult::NoGold;
	}
	if (!bRecipientExists)
	{
		LastSendTime = Now();
		return EMMOMailResult::UnknownTarget;
	}
	if (FMMOMailStore::Count(To) >= FMMOMailStore::MaxMailsPerBox)
	{
		return EMMOMailResult::BoxFull;
	}

	LastSendTime = Now(); // altin callback'i bu fonksiyonu yeniden cagirsa bile ikinci gonderim cooldown'a takilir
	const int32 Charge = static_cast<int32>(Total);
	const TWeakObjectPtr<AMMOCharacter> WeakOwner(Owner);
	const auto Refund = [&WeakOwner, Charge]()
	{
		if (AMMOCharacter* C = WeakOwner.Get())
		{
			if (UMMOInventoryComponent* I = C->GetInventory())
			{
				I->AddGoldFrom(Charge, TEXT("posta-iade"));
			}
		}
	};

	// K1 altin kopyalama: posta kutusu diske HEMEN yazilir; gonderenin dusulmus altini da kutudan ONCE diske yazilmali.
	// Aksi halde sunucu cokunce gonderen eski (yuksek) altinla, alici da postayla geri gelirdi.
	// 1) ucret dus  2) gonderen karakteri kaydet (olmazsa iade, kutuya yazma)  3) kutuya yaz (olmazsa iade + tekrar kaydet).
	// Olcum: ek (aliciya gecen altin) "posta", ucret (altin sink'i) "posta_ucret" kalemiyle ayri [MMOMETRIC] satiri basar.
	if (Gold > 0)
	{
		Inv->AddGoldFrom(-Gold, TEXT("posta"));
	}
	if (AMMOCharacter* C = WeakOwner.Get())
	{
		if (UMMOInventoryComponent* I = C->GetInventory())
		{
			I->AddGoldFrom(-FeeFor(Gold), TEXT("posta_ucret"));
		}
	}
	if (!WeakOwner.IsValid() || !SaveOwner())
	{
		Refund();
		return EMMOMailResult::StoreError;
	}
	FMMOMail M;
	M.From = Me.Left(FMMOMailStore::MaxNameLength);
	M.Text = Body;
	M.Gold = Gold;
	M.SentUnix = FDateTime::UtcNow().ToUnixTimestamp();
	if (!FMMOMailStore::Append(To, M))
	{
		Refund();
		SaveOwner(); // basarisizsa diskte dusulmus altin kalir: altin kaybolur ama kopyalanmaz
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
	// Bolge gecisi kaydi onaylandiysa da verme: sonraki altin kaydedilmez; posta kutuda kalir, yeni bolgede alinir.
	// Ad degistiyse (karakter yeni yuvaya yazilmadi) de verme: eski ve yeni yuvada ayni altin olusurdu (B2).
	UMMOInventoryComponent* Inv = Owner->GetInventory();
	if (!Inv || IsOwnerLocked(*Owner) || IsNameChanged(*Owner))
	{
		return 0;
	}
	TArray<FMMOMail> Box;
	if (!FMMOMailStore::Load(Me, Box) || Box.IsEmpty())
	{
		return 0;
	}
	// Altin tavani: eki tavani asacak posta kutuda KALIR (ticaretteki gibi onceden reddet; AddGoldFrom fazlayi silerdi).
	TArray<FMMOMail> Take, Keep;
	int64 Projected = Inv->Gold;
	for (const FMMOMail& M : Box)
	{
		if (M.Gold > 0 && Projected + M.Gold > UMMOInventoryComponent::MaxGold)
		{
			Keep.Add(M);
			continue;
		}
		Projected += M.Gold;
		Take.Add(M);
	}
	if (Keep.Num() != LastCapWarnKept)
	{
		LastCapWarnKept = Keep.Num();
		if (Keep.Num() > 0)
		{
			NotifyChat(FString::Printf(TEXT("Altin siniri: %d posta kutunda bekliyor (eki siniri asardi). Altin harcayinca gelir."), Keep.Num()));
		}
	}
	if (Take.IsEmpty())
	{
		return 0;
	}
	// Once kutuyu yalniz kalanlarla diske yaz, SONRA ver: yazma basarisizsa hicbir sey verilmez (cift odeme yok).
	if (!FMMOMailStore::Replace(Me, Keep))
	{
		return 0;
	}
	TGuardValue<bool> Guard(bCollecting, true);
	const TWeakObjectPtr<AMMOCharacter> WeakOwner(Owner);
	int32 Delivered = 0;
	int64 GoldDelivered = 0;
	for (int32 i = 0; i < Take.Num(); ++i)
	{
		const FMMOMail& M = Take[i];
		AMMOCharacter* Live = WeakOwner.Get();
		UMMOInventoryComponent* LiveInv = (Live && !Live->IsActorBeingDestroyed()) ? Live->GetInventory() : nullptr;
		if (!LiveInv)
		{
			// Sahip dongu ortasinda dustu: teslim edilmeyenleri kutuya geri yaz (kutu bu arada dolduysa kaybolur; loglanir).
			for (int32 j = i; j < Take.Num(); ++j)
			{
				if (!FMMOMailStore::Append(Me, Take[j]))
				{
					UE_LOG(LogTemp, Warning, TEXT("[MMOMAIL] Teslim edilemeyen posta kutuya geri yazilamadi: %s <- %s (%d altin)"), *Me, *Take[j].From, Take[j].Gold);
				}
			}
			break;
		}
		// Odul callback'i (basarim vb.) arada altin vermis olabilir: tavan tekrar kontrol edilir, asan posta kutuya doner.
		if (M.Gold > 0 && static_cast<int64>(LiveInv->Gold) + M.Gold > UMMOInventoryComponent::MaxGold)
		{
			if (!FMMOMailStore::Append(Me, M))
			{
				UE_LOG(LogTemp, Warning, TEXT("[MMOMAIL] Tavan nedeniyle geri konan posta yazilamadi: %s <- %s (%d altin)"), *Me, *M.From, M.Gold);
			}
			continue;
		}
		if (M.Gold > 0)
		{
			LiveInv->AddGoldFrom(M.Gold, TEXT("posta"));
			GoldDelivered += M.Gold;
		}
		++Delivered;
		FString Line = FString::Printf(TEXT("Posta [%s]: %s"), *M.From, M.Text.IsEmpty() ? TEXT("(mesaj yok)") : *M.Text);
		if (M.Gold > 0)
		{
			Line += FString::Printf(TEXT("  (+%d altin)"), M.Gold);
		}
		NotifyChat(Line);
	}
	// Teslim edilen altini hemen kalici yap (kutu zaten diskte bosaldi; sunucu cokerse altin kaybolur, kopyalanmaz).
	if (GoldDelivered > 0 && WeakOwner.IsValid())
	{
		SaveOwner();
	}
	if (Delivered > 0 && WeakOwner.IsValid())
	{
		MMONotify::ToOwner(WeakOwner.Get(), FString::Printf(TEXT("Yeni posta: %d"), Delivered), FColor(255, 230, 140), 4.f);
	}
	return Delivered;
}

void UMMOMailComponent::ParseSendArgs(const FString& Args, FString& OutName, int32& OutGold, FString& OutText)
{
	OutName.Reset();
	OutGold = 0;
	OutText.Reset();
	FString Rest;
	const FString Trimmed = Args.TrimStartAndEnd();
	if (!Trimmed.Split(TEXT(" "), &OutName, &Rest))
	{
		OutName = Trimmed;
		Rest.Reset();
	}
	Rest = Rest.TrimStartAndEnd();
	FString First, After;
	if (!Rest.Split(TEXT(" "), &First, &After))
	{
		First = Rest;
		After.Reset();
	}
	// Altin yalniz acik bicimde: rakamlar + 'g' ("500g"). Mesajin ilk sozcugu duz sayiysa mesajin parcasidir.
	if (First.Len() >= 2 && First.EndsWith(TEXT("g"), ESearchCase::IgnoreCase))
	{
		const FString Digits = First.LeftChop(1);
		bool bAllDigits = true;
		for (const TCHAR Ch : Digits)
		{
			if (Ch < TEXT('0') || Ch > TEXT('9'))
			{
				bAllDigits = false;
				break;
			}
		}
		if (bAllDigits)
		{
			const int64 Cap = static_cast<int64>(FMMOMailStore::MaxGoldPerMail) + 1;
			OutGold = static_cast<int32>(Digits.Len() > 9 ? Cap : FMath::Min<int64>(FCString::Atoi64(*Digits), Cap));
			OutText = After.TrimStartAndEnd();
			return;
		}
	}
	OutText = Rest; // altin eki yok: isimden sonraki her sey mesajdir
}

void UMMOMailComponent::CmdSend(const FString& Args)
{
	if (!HasAuthorityInternal())
	{
		return;
	}
	FString Name, Text;
	int32 Gold = 0;
	ParseSendArgs(Args, Name, Gold, Text);
	const bool bExists = !Name.IsEmpty() && RecipientExists(Name);
	const EMMOMailResult R = SendMail(Name, Gold, Text, bExists);
	if (R == EMMOMailResult::Ok)
	{
		NotifyChat(FString::Printf(TEXT("Posta gonderildi: %s  (ek %d altin, ucret %d altin)"), *FMMOMailStore::SanitizeName(Name), Gold, FeeFor(Gold))); // gercek hedef (kutu) adi
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
		TEXT("Posta: /posta gonder isim [<miktar>g] mesaj  (ornek: /posta gonder Ali 500g selam; bosluklu ad: Ali_Veli)\nAltin eki yalniz 'g' ile yazilir; duz sayi mesajdir. Ucret: %d altin + eke %%2. En fazla 1.000.000 altin, 200 karakter; aliciya 20 posta. Bekleyen postan: %d (10 sn'de bir otomatik gelir)."),
		FlatFee, Pending));
}

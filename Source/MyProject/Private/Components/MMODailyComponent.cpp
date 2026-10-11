// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMODailyComponent.h"
#include "Characters/MMOCharacter.h"
#include "Characters/MMOMob.h"
#include "Characters/MMOMobArchetype.h"
#include "Components/MMOStatsComponent.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOQuestComponent.h"
#include "Core/MMONotify.h"
#include "Core/MMOMetrics.h"
#include "Player/MMOPlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	constexpr float DailyPeriodicSeconds = 5.f;
	constexpr int32 DailyMaxTarget = 10000;
	constexpr int32 DailyMaxReward = 10000000;

	// Atomik yazim (.tmp -> Move) hedefi once siler: arada cokerse yalniz .tmp kalir. Ana dosya yoksa onu geri koy.
	// false = .tmp var ama geri konamadi: cagiran "dosya yok" sanip ayni gunun gorevlerini sifirdan acmamali, uzerine yazmamali.
	bool DailyRecoverTmp(const FString& Path)
	{
		const FString Tmp = Path + TEXT(".tmp");
		IFileManager& FM = IFileManager::Get();
		if (!FM.FileExists(*Path) && FM.FileExists(*Tmp))
		{
			if (!FM.Move(*Path, *Tmp, true, true))
			{
				UE_LOG(LogTemp, Warning, TEXT("[MMODAILY] Yarim kalan yazim kurtarilamadi (yukleme ertelendi): %s"), *Tmp);
				return false;
			}
			UE_LOG(LogTemp, Warning, TEXT("[MMODAILY] Yarim kalan yazim kurtarildi: %s"), *Tmp);
		}
		return true;
	}
}

UMMODailyComponent::UMMODailyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false); // yalniz sunucu durumu + sohbet komutu; ag maliyeti yok
}

bool UMMODailyComponent::HasAuthorityInternal() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

AMMOCharacter* UMMODailyComponent::GetOwnerCharacter() const
{
	return Cast<AMMOCharacter>(GetOwner());
}

int32 UMMODailyComponent::CurrentDayKey() const
{
	return ForcedDayKey != 0 ? ForcedDayKey : DayKeyOf(FDateTime::UtcNow());
}

void UMMODailyComponent::BeginPlay()
{
	Super::BeginPlay();
	if (Cast<AMMOMob>(GetOwner()) || !HasAuthorityInternal())
	{
		return; // mobda izleme yok
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(PeriodicTimer, this, &UMMODailyComponent::OnPeriodic, DailyPeriodicSeconds, true, DailyPeriodicSeconds);
	}
}

void UMMODailyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PeriodicTimer);
	}
	// K-1 F2: once TryLoad, sonra yaz: karakter yuvasi degistiyse gunluk dosyasi yeni yuvaya tasinir (aksi halde yeni adla
	// ayni gunun gorevleri sifirdan acilirdi). Siralama MMOAchievementComponent::EndPlay'deki gibi: karakter kaydi
	// (UnPossessed / AMMOCharacter::EndPlay) bilesen EndPlay'inden ONCE calisir, ActiveSaveSlot burada gunceldir.
	if (HasAuthorityInternal() && bLoaded)
	{
		TryLoad();
	}
	if (HasAuthorityInternal() && bDirty)
	{
		SaveNow();
	}
	Super::EndPlay(EndPlayReason);
}

void UMMODailyComponent::OnPeriodic()
{
	EvaluateAll();
}

// ---------------- Saf mantik ----------------

void UMMODailyComponent::BuildBounties(int32 Level, int32 ExpToNextLevel, TArray<FMMODailyBounty>& Out)
{
	Out.Reset();
	const int32 L = FMath::Max(1, Level);
	const int64 BaseXP = FMath::Max<int64>(1, static_cast<int64>(FMath::Max(0, ExpToNextLevel)) * 2 / 100);
	auto Make = [&](EMMODailyKind Kind, int32 Target, int32 Mult)
	{
		FMMODailyBounty B;
		B.Kind = Kind;
		B.Target = Target;
		B.RewardGold = 15 * L * Mult;
		B.RewardXP = static_cast<int32>(FMath::Min<int64>(BaseXP * Mult, DailyMaxReward));
		Out.Add(B);
	};
	Make(EMMODailyKind::KillAny, 15 + L / 2, 1);
	Make(EMMODailyKind::KillBand, 10 + L / 4, 1);
	Make(EMMODailyKind::ClaimQuests, 1, 2);
}

bool UMMODailyComponent::ToJson(int32 DayKey, int32 InQuestBaseline, const TArray<FMMODailyBounty>& B, FString& OutJson)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetNumberField(TEXT("day"), DayKey);
	Root->SetNumberField(TEXT("questBaseline"), InQuestBaseline);
	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FMMODailyBounty& X : B)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("kind"), static_cast<int32>(X.Kind));
		O->SetNumberField(TEXT("target"), X.Target);
		O->SetNumberField(TEXT("progress"), X.Progress);
		O->SetNumberField(TEXT("gold"), X.RewardGold);
		O->SetNumberField(TEXT("xp"), X.RewardXP);
		O->SetBoolField(TEXT("done"), X.bDone);
		Arr.Add(MakeShared<FJsonValueObject>(O));
	}
	Root->SetArrayField(TEXT("bounties"), Arr);
	OutJson.Reset();
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	return FJsonSerializer::Serialize(Root, Writer);
}

bool UMMODailyComponent::FromJson(const FString& Json, int32& OutDayKey, int32& OutQuestBaseline, TArray<FMMODailyBounty>& OutB)
{
	OutDayKey = 0; OutQuestBaseline = 0; OutB.Reset();
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}
	double Day = 0.0, Base = 0.0;
	if (!Root->TryGetNumberField(TEXT("day"), Day) || Day < 20000101.0 || Day > 99991231.0)
	{
		return false;
	}
	Root->TryGetNumberField(TEXT("questBaseline"), Base);
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Root->TryGetArrayField(TEXT("bounties"), Arr) || !Arr || Arr->Num() != BountyCount)
	{
		return false;
	}
	TArray<FMMODailyBounty> Parsed;
	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject>* O = nullptr;
		if (!V.IsValid() || !V->TryGetObject(O) || !O || !O->IsValid())
		{
			return false;
		}
		double Kind = -1, Target = 0, Progress = 0, Gold = 0, XP = 0;
		bool bDone = false;
		if (!(*O)->TryGetNumberField(TEXT("kind"), Kind) || Kind < 0 || Kind > 2
			|| !(*O)->TryGetNumberField(TEXT("target"), Target) || Target < 1 || Target > DailyMaxTarget)
		{
			return false;
		}
		(*O)->TryGetNumberField(TEXT("progress"), Progress);
		(*O)->TryGetNumberField(TEXT("gold"), Gold);
		(*O)->TryGetNumberField(TEXT("xp"), XP);
		(*O)->TryGetBoolField(TEXT("done"), bDone);
		FMMODailyBounty B;
		B.Kind = static_cast<EMMODailyKind>(static_cast<int32>(Kind));
		B.Target = static_cast<int32>(Target);
		B.Progress = static_cast<int32>(FMath::Clamp(Progress, 0.0, Target));
		B.RewardGold = static_cast<int32>(FMath::Clamp(Gold, 0.0, static_cast<double>(DailyMaxReward)));
		B.RewardXP = static_cast<int32>(FMath::Clamp(XP, 0.0, static_cast<double>(DailyMaxReward)));
		B.bDone = bDone;
		Parsed.Add(B);
	}
	OutDayKey = static_cast<int32>(Day);
	OutQuestBaseline = static_cast<int32>(FMath::Clamp(Base, 0.0, 1000000.0));
	OutB = MoveTemp(Parsed);
	return true;
}

// ---------------- Kalicilik ----------------

bool UMMODailyComponent::GetBoundSlot(FString& OutSlot) const
{
	OutSlot.Reset();
	const AMMOCharacter* C = GetOwnerCharacter();
	if (!C)
	{
		return false;
	}
	// Canli ad DEGIL, karakterin yukledigi/yazdigi yuva: ad degistirip bos dosya yukletmek (gunlukleri tekrar almak) mumkun olmasin.
	OutSlot = C->GetActiveSaveSlot();
	// Bos = karakter henuz yuklenmedi; "Default" yuvasi ortak/yanlis dosyadir, kullanma.
	return !OutSlot.IsEmpty() && !OutSlot.Equals(TEXT("MMOSave_Default"), ESearchCase::IgnoreCase);
}

void UMMODailyComponent::ResetLoadedState()
{
	Bounties.Reset();
	StoredDayKey = 0;
	QuestBaseline = 0;
	StoragePath.Reset();
	LoadedSlot.Reset();
	bLoaded = false;
	bDirty = false;
}

bool UMMODailyComponent::TryLoad()
{
	const AMMOCharacter* C = GetOwnerCharacter();
	if (bLoaded)
	{
		if (!bPersistenceEnabled || !C)
		{
			return true; // test modu: kimlik/disk yok
		}
		if (C->GetLoadSerial() != LoadedSerial)
		{
			// Karakter verisi diskten yeniden yuklendi (MMOLoad, yalniz admin/host): eski durum ESKI yola yazilir,
			// sonra yeni yuklenen karakterin dosyasindan yuklenir ve gorev referansi yeniden alinir.
			UE_LOG(LogTemp, Display, TEXT("[MMODAILY] Karakter yeniden yuklendi (%s): gunluk gorevler yeniden yukleniyor"), *LoadedSlot);
			if (bDirty)
			{
				SaveNow();
			}
			ResetLoadedState();
			bRebaseQuestsOnLoad = true;
		}
		else
		{
			FString Bound;
			if (GetBoundSlot(Bound) && !Bound.Equals(LoadedSlot, ESearchCase::IgnoreCase))
			{
				// Ayni karakter verisi ad degisimi + basarili kayitla yeni yuvaya TASINDI: gunluk durum da onunla tasinir
				// (sifirlanmaz; aksi halde ad degistirip ayni gunlukleri tekrar almak mumkun olurdu).
				UE_LOG(LogTemp, Display, TEXT("[MMODAILY] Karakter yeni yuvaya tasindi (%s -> %s): gunluk gorevler tasiniyor"), *LoadedSlot, *Bound);
				StoragePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MMODaily"), Bound + TEXT(".json"));
				LoadedSlot = Bound;
				bDirty = true;
				SaveNow();
			}
			return true;
		}
	}
	// Gorev sayaci referansi karakter kaydi yuklendikten SONRA alinmali; yoksa kayit yuklenince sayi sicrar ve gorev yanlis tamamlanir.
	FString Slot;
	if (!C || !C->IsInitialLoadComplete() || !GetBoundSlot(Slot))
	{
		return false;
	}
	if (bPersistenceEnabled)
	{
		const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MMODaily"), Slot + TEXT(".json"));
		// Ana dosya ya da .tmp VAR ama okunamiyor/kurtarilamiyorsa yukleme ERTELENIR (false; 5 sn sonra yeniden denenir).
		if (!DailyRecoverTmp(Path))
		{
			return false;
		}
		FString Raw;
		const bool bFileExists = IFileManager::Get().FileExists(*Path);
		if (bFileExists && !FFileHelper::LoadFileToString(Raw, *Path))
		{
			UE_LOG(LogTemp, Warning, TEXT("[MMODAILY] Gunluk gorev dosyasi okunamadi (yukleme ertelendi): %s"), *Path);
			return false;
		}
		if (bFileExists)
		{
			int32 Day = 0, Base = 0;
			TArray<FMMODailyBounty> Loaded;
			if (FromJson(Raw, Day, Base, Loaded))
			{
				StoredDayKey = Day;
				QuestBaseline = Base;
				Bounties = Loaded;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[MMODAILY] Bozuk gunluk gorev dosyasi .bad olarak ayrildi: %s"), *Path);
				IFileManager::Get().Move(*(Path + TEXT(".bad")), *Path, true);
			}
		}
		StoragePath = Path;
	}
	if (bRebaseQuestsOnLoad)
	{
		// Karakter yeniden yuklendi: dosyadaki referans baska bir gorev sayacina gore alinmis olabilir: bugunku ilerlemeyi
		// koruyarak referansi simdiki sayaca gore yeniden al (sayi sicrayip gorev yanlis tamamlanmasin).
		bRebaseQuestsOnLoad = false;
		const UMMOQuestComponent* Q = C->GetQuestComponent();
		const int32 QuestsNow = Q ? Q->CompletedQuests.Num() : 0;
		int32 ClaimProgress = 0;
		for (const FMMODailyBounty& B : Bounties)
		{
			if (B.Kind == EMMODailyKind::ClaimQuests)
			{
				ClaimProgress = B.Progress;
			}
		}
		QuestBaseline = FMath::Max(0, QuestsNow - ClaimProgress);
		bDirty = true;
	}
	LoadedSerial = C->GetLoadSerial(); // yalniz basarili yuklemede (ertelenen yukleme eski seriyi birakmaz)
	LoadedSlot = Slot;
	bLoaded = true;
	return true;
}

bool UMMODailyComponent::SaveNow()
{
	if (bTestFailSaves)
	{
		return false;
	}
	if (!bPersistenceEnabled)
	{
		return true; // test modu: disk yok, basarili say
	}
	// Yol yalniz yuklemede belirlenir: yuklenmemis/kimliksiz durum hicbir dosyaya (ozellikle MMOSave_Default) yazilmaz.
	if (!bLoaded || StoragePath.IsEmpty())
	{
		return false;
	}
	FString Json;
	if (!ToJson(StoredDayKey, QuestBaseline, Bounties, Json))
	{
		return false;
	}
	const FString& Path = StoragePath;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	const FString Tmp = Path + TEXT(".tmp");
	if (FFileHelper::SaveStringToFile(Json, *Tmp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
		&& IFileManager::Get().Move(*Path, *Tmp, true, true))
	{
		bDirty = false;
		return true;
	}
	return false;
}

// ---------------- Olaylar ----------------

void UMMODailyComponent::RollIfNewDay(const AMMOCharacter& Owner)
{
	const int32 Day = CurrentDayKey();
	if (Bounties.Num() == BountyCount && StoredDayKey == Day)
	{
		return;
	}
	const UMMOStatsComponent* S = Owner.GetStats();
	const UMMOQuestComponent* Q = Owner.GetQuestComponent();
	BuildBounties(S ? S->Level : 1, S ? S->ExperienceToNextLevel : 100, Bounties);
	StoredDayKey = Day;
	QuestBaseline = Q ? Q->CompletedQuests.Num() : 0;
	bDirty = true;
	SaveNow();
	NotifyChat(TEXT("Gunluk gorevler yenilendi: /gunluk"));
}

void UMMODailyComponent::NotifyMobKilled(FName ArchetypeId)
{
	if (!HasAuthorityInternal() || bEvaluating || ArchetypeId.IsNone() || !TryLoad())
	{
		return;
	}
	const AMMOCharacter* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		return;
	}
	RollIfNewDay(*Owner);
	const FMMOMobArchetype* Mob = FMMOMobDatabase::Find(ArchetypeId);
	const int32 PlayerLevel = Owner->GetStats() ? Owner->GetStats()->Level : 1;
	for (FMMODailyBounty& B : Bounties)
	{
		if (B.bDone)
		{
			continue;
		}
		const bool bCount = B.Kind == EMMODailyKind::KillAny
			|| (B.Kind == EMMODailyKind::KillBand && Mob && IsKillBandMatch(PlayerLevel, Mob->Level));
		if (bCount && B.Progress < B.Target)
		{
			++B.Progress;
			bDirty = true;
		}
	}
	EvaluateAll();
}

void UMMODailyComponent::EvaluateAll()
{
	if (!HasAuthorityInternal() || bEvaluating || !TryLoad())
	{
		return;
	}
	const AMMOCharacter* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		return;
	}
	RollIfNewDay(*Owner);
	if (const UMMOQuestComponent* Q = Owner->GetQuestComponent())
	{
		const int32 Done = FMath::Max(0, Q->CompletedQuests.Num() - QuestBaseline);
		for (FMMODailyBounty& B : Bounties)
		{
			if (B.Kind == EMMODailyKind::ClaimQuests && !B.bDone && B.Progress != FMath::Min(Done, B.Target))
			{
				B.Progress = FMath::Min(Done, B.Target);
				bDirty = true;
			}
		}
	}
	// Once yan etkisiz isaretle ve diske yaz, SONRA oduller: odul callback'i bileseni tekrar cagirsa da ayni gorev iki kez odenmez.
	TArray<int32> NewlyDone;
	for (int32 i = 0; i < Bounties.Num(); ++i)
	{
		if (!Bounties[i].bDone && Bounties[i].Progress >= Bounties[i].Target)
		{
			Bounties[i].bDone = true;
			NewlyDone.Add(i);
		}
	}
	if (NewlyDone.IsEmpty())
	{
		if (bDirty)
		{
			SaveNow();
		}
		return;
	}
	bDirty = true;
	// Yazilamazsa odul VERME ve bDone'u geri al: bir sonraki degerlendirmede (5 sn) yeniden denenir, odul kaybolmaz.
	if (!SaveNow())
	{
		for (const int32 i : NewlyDone)
		{
			Bounties[i].bDone = false;
		}
		UE_LOG(LogTemp, Warning, TEXT("[MMODAILY] Gunluk gorev dosyasi yazilamadi (%s): %d odul ertelendi"), *StoragePath, NewlyDone.Num());
		return;
	}
	TGuardValue<bool> Guard(bEvaluating, true);
	const TWeakObjectPtr<UMMODailyComponent> Weak(this);
	for (const int32 i : NewlyDone)
	{
		if (!Weak.IsValid())
		{
			return;
		}
		Grant(i);
	}
}

void UMMODailyComponent::Grant(int32 Index)
{
	if (!Bounties.IsValidIndex(Index))
	{
		return;
	}
	const FMMODailyBounty B = Bounties[Index]; // kopya: callback listeyi degistirse bile guvenli
	AMMOCharacter* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		return;
	}
	const TWeakObjectPtr<AMMOCharacter> WeakOwner(Owner);
	if (B.RewardGold > 0)
	{
		if (UMMOInventoryComponent* Inv = Owner->GetInventory())
		{
			Inv->AddGoldFrom(B.RewardGold, TEXT("gunluk"));
		}
	}
	if (WeakOwner.IsValid() && B.RewardXP > 0)
	{
		if (UMMOStatsComponent* S = WeakOwner->GetStats())
		{
			// Olcum: gunluk gorev XP'si ayri kalem (oldurme/gorev XP'sinden ayrilsin). lv = odulden onceki seviye.
			if (WeakOwner->IsPlayerControlled())
			{
				UE_LOG(LogTemp, Display, TEXT("[MMOMETRIC] t=%.1f ev=xp who=%s lv=%d src=gunluk amount=%d"),
					MMOMetrics::Seconds(), *MMOMetrics::WhoName(WeakOwner.Get()), S->Level, B.RewardXP);
			}
			S->GainExperience(B.RewardXP);
		}
	}
	if (WeakOwner.IsValid())
	{
		MMONotify::ToOwner(WeakOwner.Get(), FString::Printf(TEXT("Gunluk gorev tamam! (+%d altin, +%d XP)"), B.RewardGold, B.RewardXP), FColor(120, 220, 255), 5.f);
	}
}

// ---------------- Sohbet ----------------

void UMMODailyComponent::NotifyChat(const FString& Text) const
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

void UMMODailyComponent::CmdList()
{
	if (!HasAuthorityInternal())
	{
		return;
	}
	if (!TryLoad())
	{
		NotifyChat(TEXT("Gunluk gorevler su an yuklenemiyor; birkac saniye sonra tekrar dene."));
		return;
	}
	if (const AMMOCharacter* Owner = GetOwnerCharacter())
	{
		RollIfNewDay(*Owner);
	}
	FString Text = TEXT("Gunluk gorevler (gece yarisi UTC'de yenilenir):");
	for (const FMMODailyBounty& B : Bounties)
	{
		const TCHAR* Name = B.Kind == EMMODailyKind::KillAny ? TEXT("Gunluk Avci: herhangi bir canavar")
			: B.Kind == EMMODailyKind::KillBand ? TEXT("Uygun Av: seviyene uygun canavar") : TEXT("Gorev Adami: gorev tamamla");
		Text += FString::Printf(TEXT("\n%s %s %d/%d  (+%d altin, +%d XP)"), B.bDone ? TEXT("[x]") : TEXT("[ ]"), Name, B.Progress, B.Target, B.RewardGold, B.RewardXP);
	}
	NotifyChat(Text);
}

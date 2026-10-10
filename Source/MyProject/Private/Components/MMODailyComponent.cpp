// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMODailyComponent.h"
#include "Characters/MMOCharacter.h"
#include "Characters/MMOMob.h"
#include "Characters/MMOMobArchetype.h"
#include "Components/MMOStatsComponent.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOQuestComponent.h"
#include "Core/MMONotify.h"
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

FString UMMODailyComponent::GetStoragePath() const
{
	const AMMOCharacter* C = GetOwnerCharacter();
	const FString Slot = C ? C->GetSaveSlotName() : FString(TEXT("Default"));
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MMODaily"), Slot + TEXT(".json"));
}

bool UMMODailyComponent::TryLoad()
{
	if (bLoaded)
	{
		return true;
	}
	const AMMOCharacter* C = GetOwnerCharacter();
	if (!C)
	{
		return false;
	}
	// Gorev sayaci referansi karakter kaydi yuklendikten SONRA alinmali; yoksa kayit yuklenince sayi sicrar ve gorev yanlis tamamlanir.
	const APlayerState* PS = C->GetPlayerState();
	if (!PS || PS->GetPlayerName().IsEmpty() || !C->IsInitialLoadComplete())
	{
		return false;
	}
	if (bPersistenceEnabled)
	{
		const FString Path = GetStoragePath();
		FString Raw;
		if (FFileHelper::LoadFileToString(Raw, *Path))
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
	}
	bLoaded = true;
	return true;
}

void UMMODailyComponent::SaveNow()
{
	if (!bPersistenceEnabled || !bLoaded)
	{
		return;
	}
	FString Json;
	if (!ToJson(StoredDayKey, QuestBaseline, Bounties, Json))
	{
		return;
	}
	const FString Path = GetStoragePath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	const FString Tmp = Path + TEXT(".tmp");
	if (FFileHelper::SaveStringToFile(Json, *Tmp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
		&& IFileManager::Get().Move(*Path, *Tmp, true, true))
	{
		bDirty = false;
	}
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
	TGuardValue<bool> Guard(bEvaluating, true);
	bDirty = true;
	SaveNow();
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

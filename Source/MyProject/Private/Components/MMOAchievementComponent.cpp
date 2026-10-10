// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMOAchievementComponent.h"
#include "Characters/MMOCharacter.h"
#include "Characters/MMOMob.h"
#include "Components/MMOStatsComponent.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOQuestComponent.h"
#include "Core/MMONotify.h"
#include "Player/MMOPlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	constexpr int32 AchMaxKillEntries = 256;     // dosyadaki/bellekteki arketip sayaci ust siniri (kotuye kullanima karsi)
	constexpr float AchPeriodicSeconds = 5.f;    // seviye/altin/gorev taramasi
	constexpr double AchMinSaveGapSeconds = 30.0;

	int32 AchClampCount(double V)
	{
		return static_cast<int32>(FMath::Clamp(V, 0.0, static_cast<double>(UMMOAchievementComponent::MaxCounter)));
	}
}

UMMOAchievementComponent::UMMOAchievementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMMOAchievementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UMMOAchievementComponent, Unlocked, COND_OwnerOnly);
	DOREPLIFETIME(UMMOAchievementComponent, ActiveTitle);
}

bool UMMOAchievementComponent::HasAuthorityInternal() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

AMMOCharacter* UMMOAchievementComponent::GetOwnerCharacter() const
{
	return Cast<AMMOCharacter>(GetOwner());
}

void UMMOAchievementComponent::BeginPlay()
{
	Super::BeginPlay();
	// AMMOCharacter mobun da tabanidir: mobda basarim izlenmez, zamanlayici kurulmaz, replikasyon kapatilir.
	if (Cast<AMMOMob>(GetOwner()))
	{
		SetIsReplicated(false);
		return;
	}
	if (!HasAuthorityInternal())
	{
		return;
	}
	if (const AMMOCharacter* C = GetOwnerCharacter())
	{
		if (UMMOStatsComponent* S = C->GetStats())
		{
			S->OnDeath.AddDynamic(this, &UMMOAchievementComponent::OnOwnerDied);
		}
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(PeriodicTimer, this, &UMMOAchievementComponent::OnPeriodic, AchPeriodicSeconds, true, AchPeriodicSeconds);
	}
}

void UMMOAchievementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

void UMMOAchievementComponent::OnPeriodic()
{
	EvaluateAll();
}

// ---------------- Saf mantik ----------------

int32 UMMOAchievementComponent::ProgressOf(const FMMOAchievementDef& Def, const FMMOAchievementCounters& C, int32 Level, int32 QuestsDone)
{
	switch (Def.Kind)
	{
	case EMMOAchievementKind::KillTotal:  return C.KillTotal;
	case EMMOAchievementKind::KillMob:
	{
		const int32* K = C.Kills.Find(Def.TargetId);
		return K ? *K : 0;
	}
	case EMMOAchievementKind::ReachLevel: return Level;
	case EMMOAchievementKind::GoldHeld:   return C.PeakGold;
	case EMMOAchievementKind::QuestsDone: return QuestsDone;
	case EMMOAchievementKind::Deaths:     return C.Deaths;
	}
	return 0;
}

bool UMMOAchievementComponent::IsSatisfied(const FMMOAchievementDef& Def, const FMMOAchievementCounters& C, int32 Level, int32 QuestsDone)
{
	return Def.Target > 0 && ProgressOf(Def, C, Level, QuestsDone) >= Def.Target;
}

bool UMMOAchievementComponent::ToJson(const FMMOAchievementCounters& C, const TArray<FName>& UnlockedIds, const FString& Title, FString& OutJson)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetNumberField(TEXT("killTotal"), C.KillTotal);
	Root->SetNumberField(TEXT("deaths"), C.Deaths);
	Root->SetNumberField(TEXT("peakGold"), C.PeakGold);
	TSharedRef<FJsonObject> Kills = MakeShared<FJsonObject>();
	for (const TPair<FName, int32>& P : C.Kills)
	{
		Kills->SetNumberField(P.Key.ToString(), P.Value);
	}
	Root->SetObjectField(TEXT("kills"), Kills);
	TArray<TSharedPtr<FJsonValue>> U;
	for (const FName Id : UnlockedIds)
	{
		U.Add(MakeShared<FJsonValueString>(Id.ToString()));
	}
	Root->SetArrayField(TEXT("unlocked"), U);
	Root->SetStringField(TEXT("title"), Title);
	OutJson.Reset();
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	return FJsonSerializer::Serialize(Root, Writer);
}

bool UMMOAchievementComponent::FromJson(const FString& Json, FMMOAchievementCounters& OutC, TArray<FName>& OutUnlocked, FString& OutTitle)
{
	OutC = FMMOAchievementCounters();
	OutUnlocked.Reset();
	OutTitle.Reset();
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}
	double V = 0.0;
	if (Root->TryGetNumberField(TEXT("killTotal"), V)) OutC.KillTotal = AchClampCount(V);
	if (Root->TryGetNumberField(TEXT("deaths"), V)) OutC.Deaths = AchClampCount(V);
	if (Root->TryGetNumberField(TEXT("peakGold"), V)) OutC.PeakGold = AchClampCount(V);
	const TSharedPtr<FJsonObject>* KillsObj = nullptr;
	if (Root->TryGetObjectField(TEXT("kills"), KillsObj) && KillsObj && KillsObj->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& P : (*KillsObj)->Values)
		{
			if (OutC.Kills.Num() >= AchMaxKillEntries)
			{
				break;
			}
			double N = 0.0;
			if (P.Value.IsValid() && P.Value->TryGetNumber(N) && !P.Key.IsEmpty())
			{
				OutC.Kills.Add(FName(*P.Key), AchClampCount(N));
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Root->TryGetArrayField(TEXT("unlocked"), Arr) && Arr)
	{
		for (const TSharedPtr<FJsonValue>& Val : *Arr)
		{
			FString S;
			if (Val.IsValid() && Val->TryGetString(S))
			{
				const FName Id(*S);
				if (FMMOAchievementDatabase::Find(Id))
				{
					OutUnlocked.AddUnique(Id);
				}
			}
		}
	}
	FString T;
	if (Root->TryGetStringField(TEXT("title"), T) && !T.IsEmpty())
	{
		for (const FName Id : OutUnlocked)
		{
			const FMMOAchievementDef* D = FMMOAchievementDatabase::Find(Id);
			if (D && !D->Title.IsEmpty() && D->Title == T)
			{
				OutTitle = T;
				break;
			}
		}
	}
	return true;
}

// ---------------- Kalicilik ----------------

FString UMMOAchievementComponent::GetStoragePath() const
{
	const AMMOCharacter* C = GetOwnerCharacter();
	const FString Slot = C ? C->GetSaveSlotName() : FString(TEXT("Default"));
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MMOAchievements"), Slot + TEXT(".json"));
}

bool UMMOAchievementComponent::TryLoad()
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
	// Kayit yuvasi oyuncu adina dayanir; PlayerState adi gelmeden yukleme yapma (yanlis "Default" yuvasi).
	const APlayerState* PS = C->GetPlayerState();
	if (!PS || PS->GetPlayerName().IsEmpty())
	{
		return false;
	}
	if (bPersistenceEnabled)
	{
		const FString Path = GetStoragePath();
		FString Raw;
		if (FFileHelper::LoadFileToString(Raw, *Path))
		{
			FMMOAchievementCounters Loaded;
			TArray<FName> LoadedUnlocked;
			FString LoadedTitle;
			if (FromJson(Raw, Loaded, LoadedUnlocked, LoadedTitle))
			{
				Counters = Loaded;
				Unlocked = LoadedUnlocked;
				ActiveTitle = LoadedTitle;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[MMOACH] Bozuk basarim dosyasi .bad olarak ayrildi: %s"), *Path);
				IFileManager::Get().Move(*(Path + TEXT(".bad")), *Path, true);
			}
		}
	}
	bLoaded = true;
	return true;
}

void UMMOAchievementComponent::SaveNow()
{
	if (!bPersistenceEnabled || !bLoaded)
	{
		return;
	}
	FString Json;
	if (!ToJson(Counters, Unlocked, ActiveTitle, Json))
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
	LastSaveTime = FPlatformTime::Seconds();
}

void UMMOAchievementComponent::SaveIfDirty()
{
	if (bDirty && FPlatformTime::Seconds() - LastSaveTime >= AchMinSaveGapSeconds)
	{
		SaveNow();
	}
}

// ---------------- Olaylar ----------------

void UMMOAchievementComponent::NotifyMobKilled(FName ArchetypeId)
{
	if (!HasAuthorityInternal() || !bLoaded || ArchetypeId.IsNone())
	{
		return;
	}
	Counters.KillTotal = FMath::Min(Counters.KillTotal + 1, MaxCounter);
	if (int32* K = Counters.Kills.Find(ArchetypeId))
	{
		*K = FMath::Min(*K + 1, MaxCounter);
	}
	else if (Counters.Kills.Num() < AchMaxKillEntries)
	{
		Counters.Kills.Add(ArchetypeId, 1);
	}
	bDirty = true;
	EvaluateAll();
}

void UMMOAchievementComponent::OnOwnerDied()
{
	if (!HasAuthorityInternal() || !bLoaded)
	{
		return;
	}
	Counters.Deaths = FMath::Min(Counters.Deaths + 1, MaxCounter);
	bDirty = true;
	EvaluateAll();
}

void UMMOAchievementComponent::EvaluateAll()
{
	if (!HasAuthorityInternal() || bEvaluating || !TryLoad())
	{
		return;
	}
	AMMOCharacter* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		return;
	}
	const UMMOStatsComponent* S = Owner->GetStats();
	const UMMOInventoryComponent* Inv = Owner->GetInventory();
	const UMMOQuestComponent* Q = Owner->GetQuestComponent();
	const int32 Level = S ? S->Level : 0;
	const int32 QuestsDone = Q ? Q->CompletedQuests.Num() : 0;
	if (Inv && Inv->Gold > Counters.PeakGold)
	{
		Counters.PeakGold = FMath::Min(Inv->Gold, MaxCounter);
		bDirty = true;
	}

	// Once yan etkisiz belirle, sonra isle: odul callback'leri (altin degisimi) bu bileseni yeniden cagirabilir.
	TArray<FName> Newly;
	for (const FMMOAchievementDef& D : FMMOAchievementDatabase::GetAll())
	{
		if (!Unlocked.Contains(D.Id) && IsSatisfied(D, Counters, Level, QuestsDone))
		{
			Newly.Add(D.Id);
		}
	}
	if (Newly.IsEmpty())
	{
		SaveIfDirty();
		return;
	}
	TGuardValue<bool> Guard(bEvaluating, true);
	for (const FName Id : Newly)
	{
		Unlocked.Add(Id);
	}
	bDirty = true;
	SaveNow(); // acilma kaydi oduller dagitilmadan once diske gider: coken/atlanan callback ayni basarimi ikinci kez vermez
	const TWeakObjectPtr<UMMOAchievementComponent> Weak(this);
	for (const FName Id : Newly)
	{
		if (!Weak.IsValid())
		{
			return;
		}
		if (const FMMOAchievementDef* D = FMMOAchievementDatabase::Find(Id))
		{
			Grant(*D);
		}
	}
}

void UMMOAchievementComponent::Grant(const FMMOAchievementDef& Def)
{
	AMMOCharacter* Owner = GetOwnerCharacter();
	if (!Owner)
	{
		return;
	}
	const TWeakObjectPtr<AMMOCharacter> WeakOwner(Owner);
	FString Msg = FString::Printf(TEXT("Basarim: %s"), *Def.Name);
	if (Def.RewardGold > 0)
	{
		if (UMMOInventoryComponent* Inv = Owner->GetInventory())
		{
			Inv->AddGoldFrom(Def.RewardGold, TEXT("basarim"));
			Msg += FString::Printf(TEXT(" (+%d altin)"), Def.RewardGold);
		}
	}
	if (!Def.Title.IsEmpty())
	{
		Msg += FString::Printf(TEXT("  - unvan: %s"), *Def.Title);
	}
	if (WeakOwner.IsValid())
	{
		MMONotify::ToOwner(WeakOwner.Get(), Msg, FColor(255, 215, 0), 5.f);
	}
}

// ---------------- Sohbet komutlari ----------------

void UMMOAchievementComponent::NotifyChat(const FString& Text) const
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

void UMMOAchievementComponent::CmdList()
{
	if (!HasAuthorityInternal())
	{
		return;
	}
	if (!bLoaded)
	{
		NotifyChat(TEXT("Basarimlar su an yuklenemiyor; birkac saniye sonra tekrar dene."));
		return;
	}
	const AMMOCharacter* Owner = GetOwnerCharacter();
	const int32 Level = (Owner && Owner->GetStats()) ? Owner->GetStats()->Level : 0;
	const int32 QuestsDone = (Owner && Owner->GetQuestComponent()) ? Owner->GetQuestComponent()->CompletedQuests.Num() : 0;
	const TArray<FMMOAchievementDef>& All = FMMOAchievementDatabase::GetAll();

	FString Titles;
	TArray<TPair<float, const FMMOAchievementDef*>> Next;
	for (const FMMOAchievementDef& D : All)
	{
		if (Unlocked.Contains(D.Id))
		{
			if (!D.Title.IsEmpty())
			{
				Titles += (Titles.IsEmpty() ? TEXT("") : TEXT(", ")) + D.Title;
			}
		}
		else
		{
			const int32 P = FMath::Min(ProgressOf(D, Counters, Level, QuestsDone), D.Target);
			Next.Add(TPair<float, const FMMOAchievementDef*>(D.Target > 0 ? static_cast<float>(P) / D.Target : 0.f, &D));
		}
	}
	Next.Sort([](const TPair<float, const FMMOAchievementDef*>& A, const TPair<float, const FMMOAchievementDef*>& B) { return A.Key > B.Key; });

	FString Text = FString::Printf(TEXT("Basarimlar (%d/%d)  Toplam oldurme: %d, olum: %d"), Unlocked.Num(), All.Num(), Counters.KillTotal, Counters.Deaths);
	Text += FString::Printf(TEXT("\nSecili unvan: %s"), ActiveTitle.IsEmpty() ? TEXT("yok") : *ActiveTitle);
	if (!Titles.IsEmpty())
	{
		Text += FString::Printf(TEXT("\nUnvanlar: %s  (/basarim unvan ad)"), *Titles);
	}
	const int32 Shown = FMath::Min(5, Next.Num());
	if (Shown > 0)
	{
		Text += TEXT("\nSonraki hedefler:");
	}
	for (int32 i = 0; i < Shown; ++i)
	{
		const FMMOAchievementDef& D = *Next[i].Value;
		const int32 P = FMath::Min(ProgressOf(D, Counters, Level, QuestsDone), D.Target);
		Text += FString::Printf(TEXT("\n%s: %d/%d - %s"), *D.Name, P, D.Target, *D.Description);
	}
	NotifyChat(Text);
}

void UMMOAchievementComponent::CmdTitle(const FString& TitleName)
{
	if (!HasAuthorityInternal() || !bLoaded)
	{
		return;
	}
	const FString Wanted = TitleName.TrimStartAndEnd();
	if (Wanted.IsEmpty())
	{
		ActiveTitle.Reset();
		bDirty = true;
		SaveNow();
		NotifyChat(TEXT("Unvan kaldirildi."));
		return;
	}
	for (const FName Id : Unlocked)
	{
		const FMMOAchievementDef* D = FMMOAchievementDatabase::Find(Id);
		if (D && !D->Title.IsEmpty() && D->Title.Equals(Wanted, ESearchCase::IgnoreCase))
		{
			ActiveTitle = D->Title;
			bDirty = true;
			SaveNow();
			NotifyChat(FString::Printf(TEXT("Unvan secildi: %s"), *ActiveTitle));
			return;
		}
	}
	NotifyChat(TEXT("Bu unvana sahip degilsin. /basarim ile sahip olduklarini gor."));
}

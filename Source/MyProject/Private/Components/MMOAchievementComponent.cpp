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

	// Atomik yazim (.tmp -> Move) hedefi once siler: arada cokerse yalniz .tmp kalir. Ana dosya yoksa onu geri koy.
	// false = .tmp var ama geri konamadi: cagiran "dosya yok" sanip odulleri yeniden vermemeli ve uzerine yazmamali.
	bool AchRecoverTmp(const FString& Path)
	{
		const FString Tmp = Path + TEXT(".tmp");
		IFileManager& FM = IFileManager::Get();
		if (!FM.FileExists(*Path) && FM.FileExists(*Tmp))
		{
			if (!FM.Move(*Path, *Tmp, true, true))
			{
				UE_LOG(LogTemp, Warning, TEXT("[MMOACH] Yarim kalan yazim kurtarilamadi (yukleme ertelendi): %s"), *Tmp);
				return false;
			}
			UE_LOG(LogTemp, Warning, TEXT("[MMOACH] Yarim kalan yazim kurtarildi: %s"), *Tmp);
		}
		return true;
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
	// K-1 F2: once TryLoad, sonra yaz. Karakter yuvasi degistiyse (ad degisimi + kayit) dosya yeni yuvaya tasinir; aksi halde
	// eski yolda kalir ve yeni adla girince bos dosya okunup seviye/gorev basarim altini ikinci kez verilirdi.
	// Siralama (motor kodundan dogrulandi, UE 5.7): Destroy -> APawn::Destroyed -> UnPossessed (karakter yeni yuvaya yazilir,
	// ActiveSaveSlot guncellenir) -> AActor::Destroyed -> RouteEndPlay -> AMMOCharacter::EndPlay (kendi kaydi, sonra
	// Super::EndPlay) -> AActor::EndPlay -> bilesen EndPlay'leri. Yani bu noktada ActiveSaveSlot karakter kaydindan SONRAKI
	// guncel yuvadir. Yuklenmemis bilesende TryLoad cagrilmaz (cikista dosya okumaya gerek yok).
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

bool UMMOAchievementComponent::GetBoundSlot(FString& OutSlot) const
{
	OutSlot.Reset();
	const AMMOCharacter* C = GetOwnerCharacter();
	if (!C)
	{
		return false;
	}
	// Canli ad DEGIL, karakterin yukledigi/yazdigi yuva: ad degistirip bos dosya yukletmek (odulleri tekrar kazanmak) mumkun olmasin.
	OutSlot = C->GetActiveSaveSlot();
	// Bos = karakter henuz yuklenmedi; "Default" yuvasi ortak/yanlis dosyadir, kullanma (posta da bu ada teslim etmez).
	return !OutSlot.IsEmpty() && !OutSlot.Equals(TEXT("MMOSave_Default"), ESearchCase::IgnoreCase);
}

void UMMOAchievementComponent::ResetLoadedState()
{
	Counters = FMMOAchievementCounters();
	Unlocked.Reset();
	ActiveTitle.Reset();
	StoragePath.Reset();
	LoadedSlot.Reset();
	bLoaded = false;
	bDirty = false;
}

bool UMMOAchievementComponent::TryLoad()
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
			// sonra yeni yuklenen karakterin dosyasindan yuklenir (asagida, yukleme tamamlaninca).
			UE_LOG(LogTemp, Display, TEXT("[MMOACH] Karakter yeniden yuklendi (%s): basarimlar yeniden yukleniyor"), *LoadedSlot);
			if (bDirty)
			{
				SaveNow();
			}
			ResetLoadedState();
		}
		else
		{
			FString Bound;
			if (GetBoundSlot(Bound) && !Bound.Equals(LoadedSlot, ESearchCase::IgnoreCase))
			{
				// Ayni karakter verisi ad degisimi + basarili kayitla yeni yuvaya TASINDI: basarimlar da onunla tasinir
				// (sifirlanmaz; aksi halde ad degistirip ayni odulleri tekrar almak mumkun olurdu).
				UE_LOG(LogTemp, Display, TEXT("[MMOACH] Karakter yeni yuvaya tasindi (%s -> %s): basarimlar tasiniyor"), *LoadedSlot, *Bound);
				StoragePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MMOAchievements"), Bound + TEXT(".json"));
				LoadedSlot = Bound;
				bDirty = true;
				SaveNow();
			}
			return true;
		}
	}
	// Yalniz karakter kaydi yuklendikten sonra ve yuvasi belliyken (PlayerState adi gelmeden "Default" yuvasi yanlis olurdu).
	FString Slot;
	if (!C || !C->IsInitialLoadComplete() || !GetBoundSlot(Slot))
	{
		return false;
	}
	if (bPersistenceEnabled)
	{
		const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MMOAchievements"), Slot + TEXT(".json"));
		// Ana dosya ya da .tmp VAR ama okunamiyor/kurtarilamiyorsa yukleme ERTELENIR (false; 5 sn sonra yeniden denenir).
		// "Dosya yok" sanilirsa odul tekrar verilir ve diskteki gercek durumun ustune yazilirdi.
		if (!AchRecoverTmp(Path))
		{
			return false;
		}
		bool bHadValidFile = false;
		FString Raw;
		if (IFileManager::Get().FileExists(*Path))
		{
			if (!FFileHelper::LoadFileToString(Raw, *Path))
			{
				UE_LOG(LogTemp, Warning, TEXT("[MMOACH] Basarim dosyasi okunamadi (yukleme ertelendi): %s"), *Path);
				return false;
			}
			FMMOAchievementCounters Loaded;
			TArray<FName> LoadedUnlocked;
			FString LoadedTitle;
			if (FromJson(Raw, Loaded, LoadedUnlocked, LoadedTitle))
			{
				Counters = Loaded;
				Unlocked = LoadedUnlocked;
				ActiveTitle = LoadedTitle;
				bHadValidFile = true;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[MMOACH] Bozuk basarim dosyasi .bad olarak ayrildi: %s"), *Path);
				IFileManager::Get().Move(*(Path + TEXT(".bad")), *Path, true);
			}
		}
		StoragePath = Path;
		LoadedSlot = Slot;
		LoadedSerial = C->GetLoadSerial();
		bLoaded = true;
		if (!bHadValidFile)
		{
			MarkCurrentProgressUnlockedSilently();
		}
		return true;
	}
	LoadedSerial = C->GetLoadSerial();
	LoadedSlot = Slot;
	bLoaded = true;
	return true;
}

void UMMOAchievementComponent::MarkCurrentProgressUnlockedSilently()
{
	// K-1 F2 savunma derinligi: Lv>1 karakterin basarim dosyasi HIC yoksa (ya da bozuktu) dosya kaybolmus/tasinmamis
	// sayilir: o an zaten saglanan seviye/gorev/altin basarimlari ODULSUZ acik isaretlenir ve hemen yazilir. Aksi halde
	// bos dosya yuklenip (ad degisimi, F3, silinen dosya) ~26.000 altinlik seviye/gorev odulu tekrar verilirdi.
	// Bedeli: basarim sistemi gelmeden once Lv>1 olmus karakterler bu basarimlarin altinini geriye donuk ALMAZ (unvanlar acilir).
	// Oldurme/olum sayaclari dosyayla birlikte kaybolur, odulleri yeniden kazanilarak alinir (sayac sifirdan baslar).
	const AMMOCharacter* Owner = GetOwnerCharacter();
	const UMMOStatsComponent* S = Owner ? Owner->GetStats() : nullptr;
	const int32 Level = S ? S->Level : 0;
	if (Level <= 1)
	{
		return;
	}
	const UMMOInventoryComponent* Inv = Owner->GetInventory();
	const UMMOQuestComponent* Q = Owner->GetQuestComponent();
	const int32 QuestsDone = Q ? Q->CompletedQuests.Num() : 0;
	FMMOAchievementCounters Probe = Counters;
	Probe.PeakGold = FMath::Max(Probe.PeakGold, Inv ? FMath::Min(Inv->Gold, MaxCounter) : 0);
	int32 Marked = 0;
	for (const FMMOAchievementDef& D : FMMOAchievementDatabase::GetAll())
	{
		const bool bSnapshotKind = D.Kind == EMMOAchievementKind::ReachLevel || D.Kind == EMMOAchievementKind::QuestsDone
			|| D.Kind == EMMOAchievementKind::GoldHeld;
		if (bSnapshotKind && !Unlocked.Contains(D.Id) && IsSatisfied(D, Probe, Level, QuestsDone))
		{
			Unlocked.Add(D.Id);
			++Marked;
		}
	}
	Counters.PeakGold = Probe.PeakGold;
	if (Marked > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MMOACH] Basarim dosyasi yok, Lv%d karakter: %d basarim odulsuz acik isaretlendi (%s)"),
			Level, Marked, *StoragePath);
		bDirty = true;
		SaveNow();
	}
}

bool UMMOAchievementComponent::SaveNow()
{
	++TestSaveCalls; // yalniz test sayaci (C3)
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
	LastSaveTime = FPlatformTime::Seconds();
	FString Json;
	if (!ToJson(Counters, Unlocked, ActiveTitle, Json))
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
	// TryLoad: zamanlayicidan once gelen ilk oldurme de sayilsin; ad degistiyse once yeni yuvaya gecilir.
	if (!HasAuthorityInternal() || ArchetypeId.IsNone() || !TryLoad())
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
	if (!HasAuthorityInternal() || !TryLoad())
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
	// Acilma kaydi oduller dagitilmadan once diske gider: coken/atlanan callback ayni basarimi ikinci kez vermez.
	// Yazilamazsa odul VERME ve acilmayi geri al: bir sonraki degerlendirmede (5 sn) yeniden denenir, odul kaybolmaz.
	if (!SaveNow())
	{
		for (const FName Id : Newly)
		{
			Unlocked.Remove(Id);
		}
		UE_LOG(LogTemp, Warning, TEXT("[MMOACH] Basarim dosyasi yazilamadi (%s): %d acilma ertelendi"), *StoragePath, Newly.Num());
		return;
	}
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
	if (!TryLoad())
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
	if (!HasAuthorityInternal() || !TryLoad())
	{
		return;
	}
	const FString Wanted = TitleName.TrimStartAndEnd();
	// Deger degismediyse diske yazma (komut tekrariyla disk yukunu artirmak mumkun olmasin).
	if (Wanted.IsEmpty())
	{
		if (!ActiveTitle.IsEmpty())
		{
			ActiveTitle.Reset();
			bDirty = true;
			SaveNow();
		}
		NotifyChat(TEXT("Unvan kaldirildi."));
		return;
	}
	for (const FName Id : Unlocked)
	{
		const FMMOAchievementDef* D = FMMOAchievementDatabase::Find(Id);
		if (D && !D->Title.IsEmpty() && D->Title.Equals(Wanted, ESearchCase::IgnoreCase))
		{
			if (!ActiveTitle.Equals(D->Title, ESearchCase::CaseSensitive))
			{
				ActiveTitle = D->Title;
				bDirty = true;
				SaveNow();
			}
			NotifyChat(FString::Printf(TEXT("Unvan secildi: %s"), *ActiveTitle));
			return;
		}
	}
	NotifyChat(TEXT("Bu unvana sahip degilsin. /basarim ile sahip olduklarini gor."));
}

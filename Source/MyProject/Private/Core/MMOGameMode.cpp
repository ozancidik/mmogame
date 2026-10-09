// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/MMOGameMode.h"
#include "Core/MMOMobSpawner.h"
#include "Core/MMOGameState.h"
#include "Core/MMOAdmin.h"
#include "Characters/MMOMob.h"
#include "Components/CapsuleComponent.h"
#include "World/MMOVendorNPC.h"
#include "World/MMOQuestGiverNPC.h"
#include "World/MMOTalkerNPC.h"
#include "World/MMOHealerNPC.h"
#include "World/MMOWarehouseNPC.h"
#include "UI/MMOHUD.h"
#include "Characters/MMOCharacter.h"
#include "Components/MMOStatsComponent.h"
#include "Components/MMOInventoryComponent.h"
#include "Save/MMOCharacterStoreSubsystem.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Components/MMOStatsComponent.h"
#include "Player/MMOPlayerController.h"
#include "TimerManager.h"

AMMOGameMode::AMMOGameMode()
{
	// Varsayilan HUD'u MMO HUD'imizi yap.
	HUDClass = AMMOHUD::StaticClass();
	// Replike oyun durumu (PvP anahtari aynasi). BP_ThirdPersonGameMode GameStateClass'i ezmez
	// (Content taramasi 2026-10-06), bu varsayilan gecerlidir.
	GameStateClass = AMMOGameState::StaticClass();
	bUseSeamlessTravel = true;
}

void AMMOGameMode::InitGameState()
{
	Super::InitGameState();
	if (AMMOGameState* MMOState = GetGameState<AMMOGameState>())
	{
		MMOState->SetPvPEnabled(bAllowPvP);
	}
}

void AMMOGameMode::SetPvPAllowed(bool bOn)
{
	bAllowPvP = bOn;
	if (AMMOGameState* MMOState = GetGameState<AMMOGameState>())
	{
		MMOState->SetPvPEnabled(bOn);
	}
}

void AMMOGameMode::MMOTestZoneTravel()
{
#if !UE_BUILD_SHIPPING
	ScheduleZoneTravelTest(false);
#endif
}

void AMMOGameMode::MMOTestZoneTravelTimeout()
{
#if !UE_BUILD_SHIPPING
	ScheduleZoneTravelTest(true);
#endif
}

void AMMOGameMode::MMOTestZoneTravelStateChange()
{
#if !UE_BUILD_SHIPPING
	ScheduleZoneTravelTest(false, true);
#endif
}

void AMMOGameMode::MMOTestZoneTravelDeferredSaveFailure()
{
#if !UE_BUILD_SHIPPING
	ScheduleZoneTravelTest(false, false, true);
#endif
}

void AMMOGameMode::MMOTestZoneTravelProgress()
{
#if !UE_BUILD_SHIPPING
	ScheduleZoneTravelTest(false, false, false, true);
#endif
}

void AMMOGameMode::MMOTestZoneTravelCallbacks()
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	if (!World || !HasAuthority() || bZoneTravelInProgress || !PendingZoneTravelCharacters.IsEmpty()
		|| !CompletedZoneTravelSaves.IsEmpty() || PendingZoneTravelSaves != 0
		|| World->GetTimerManager().IsTimerActive(ZoneTravelSaveTimeoutHandle))
	{
		UE_LOG(LogTemp, Error, TEXT("[MMO_ZONE_TRAVEL_CALLBACK_TEST] FAIL unavailable or active travel"));
		return;
	}
	const FString OriginalAbortReason = LastZoneTravelAbortReason;
	TArray<AMMOCharacter*> Actors;
	int32 Checks = 0;
	FString Error;
	const bool bPassed = [&]()
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (int32 I = 0; I < 3; ++I)
		{
			AMMOCharacter* Character = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(0, 0, 5000), FRotator::ZeroRotator, Params);
			if (!Character) { Error = TEXT("fixture spawn failed"); return false; }
			Actors.Add(Character);
		}
		const TWeakObjectPtr<AMMOCharacter> A(Actors[0]);
		const TWeakObjectPtr<AMMOCharacter> B(Actors[1]);
		const TWeakObjectPtr<AMMOCharacter> Foreign(Actors[2]);
		auto BeginBarrier = [&]()
		{
			++ZoneTravelRequestSerial;
			bZoneTravelInProgress = true;
			PendingZoneTravelCharacters = { A, B };
			CompletedZoneTravelSaves.Reset();
			PendingZoneTravelSaves = 2;
			PendingZoneTravelSourceMap = TEXT("CallbackTestSource");
			PendingZoneTravelTargetMap = TEXT("CallbackTestTarget");
			World->GetTimerManager().SetTimer(ZoneTravelSaveTimeoutHandle,
				FTimerDelegate::CreateUObject(this, &AMMOGameMode::HandleZoneTravelSaveTimeout, ZoneTravelRequestSerial),
				ZoneTravelSaveTimeoutSeconds, false);
		};
		auto Check = [&](bool Condition, const TCHAR* Label)
		{
			if (!Condition) { Error = Label; return false; }
			++Checks;
			return true;
		};
		auto Waiting = [&](int32 Remaining, int32 Completed)
		{
			return bZoneTravelInProgress && PendingZoneTravelSaves == Remaining
				&& CompletedZoneTravelSaves.Num() == Completed && PendingZoneTravelCharacters.Num() == 2
				&& World->GetTimerManager().IsTimerActive(ZoneTravelSaveTimeoutHandle);
		};
		auto Aborted = [&]()
		{
			return !bZoneTravelInProgress && PendingZoneTravelSaves == 0 && PendingZoneTravelCharacters.IsEmpty()
				&& CompletedZoneTravelSaves.IsEmpty() && PendingZoneTravelSourceMap.IsEmpty()
				&& PendingZoneTravelTargetMap.IsEmpty() && !World->GetTimerManager().IsTimerActive(ZoneTravelSaveTimeoutHandle);
		};
		BeginBarrier();
		HandleZoneTravelSaveComplete(ZoneTravelRequestSerial, Foreign, true);
		if (!Check(Waiting(2, 0), TEXT("foreign success opened barrier"))) { return false; }
		HandleZoneTravelSaveComplete(ZoneTravelRequestSerial, Foreign, false);
		if (!Check(Waiting(2, 0), TEXT("foreign failure aborted"))) { return false; }
		HandleZoneTravelSaveComplete(ZoneTravelRequestSerial - 1, A, false);
		if (!Check(Waiting(2, 0), TEXT("old request aborted"))) { return false; }
		HandleZoneTravelSaveComplete(ZoneTravelRequestSerial, A, true);
		if (!Check(Waiting(1, 1) && CompletedZoneTravelSaves.Contains(A), TEXT("first success not counted"))) { return false; }
		HandleZoneTravelSaveComplete(ZoneTravelRequestSerial, A, true);
		if (!Check(Waiting(1, 1), TEXT("duplicate success counted"))) { return false; }
		HandleZoneTravelSaveComplete(ZoneTravelRequestSerial, A, false);
		if (!Check(Waiting(1, 1), TEXT("duplicate failure revoked success"))) { return false; }
		// FinishZoneTravel sonrasi fazi taklit et: travel kilidi acik, kayit bariyeri bos.
		World->GetTimerManager().ClearTimer(ZoneTravelSaveTimeoutHandle);
		PendingZoneTravelCharacters.Reset();
		CompletedZoneTravelSaves.Reset();
		PendingZoneTravelSaves = 0;
		HandleZoneTravelSaveComplete(ZoneTravelRequestSerial, A, true);
		HandleZoneTravelSaveComplete(ZoneTravelRequestSerial, A, false);
		if (!Check(bZoneTravelInProgress && PendingZoneTravelTargetMap == TEXT("CallbackTestTarget")
			&& PendingZoneTravelSaves == 0 && CompletedZoneTravelSaves.IsEmpty(), TEXT("late callback changed travel phase"))) { return false; }
		BeginBarrier();
		HandleZoneTravelSaveComplete(ZoneTravelRequestSerial, B, false);
		if (!Check(Aborted() && LastZoneTravelAbortReason == TEXT("Bir karakter kaydedilemedi; gecis iptal edildi"), TEXT("first failure not aborted"))) { return false; }
		BeginBarrier();
		Actors[0]->Destroy();
		HandleZoneTravelSaveComplete(ZoneTravelRequestSerial, A, true);
		if (!Check(Aborted() && LastZoneTravelAbortReason == TEXT("Bir oyuncu kayit sirasinda ayrildi; gecis iptal edildi"), TEXT("expired character success counted"))) { return false; }
		return true;
	}();
	// Yalniz fixture bariyeri kurulmustur; canli oyunculara kayit veya ServerTravel uygulanmaz.
	World->GetTimerManager().ClearTimer(ZoneTravelSaveTimeoutHandle);
	bZoneTravelInProgress = false;
	PendingZoneTravelSaves = 0;
	PendingZoneTravelCharacters.Reset();
	CompletedZoneTravelSaves.Reset();
	PendingZoneTravelSourceMap.Reset();
	PendingZoneTravelTargetMap.Reset();
	LastZoneTravelAbortReason = OriginalAbortReason;
	for (AMMOCharacter* Character : Actors) { if (IsValid(Character)) { Character->Destroy(); } }
	UE_LOG(LogTemp, Display, TEXT("[MMO_ZONE_TRAVEL_CALLBACK_TEST] %s checks=%d reason=%s"),
		bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
#endif
}

void AMMOGameMode::ScheduleZoneTravelTest(bool bExpectTimeout, bool bExpectStateChange, bool bExpectFlushFailure, bool bExpectProgress)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}
	const TWeakObjectPtr<AMMOGameMode> WeakThis(this);
	FTimerHandle TestHandle;
	World->GetTimerManager().SetTimer(TestHandle, FTimerDelegate::CreateLambda([WeakThis, bExpectTimeout, bExpectStateChange, bExpectFlushFailure, bExpectProgress]()
	{
		AMMOGameMode* GameMode = WeakThis.Get();
		UWorld* TestWorld = GameMode ? GameMode->GetWorld() : nullptr;
		if (!GameMode || !TestWorld)
		{
			return;
		}
		APlayerController* Host = nullptr;
		for (FConstPlayerControllerIterator It = TestWorld->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Candidate = It->Get();
			if (Candidate && MMOAdmin::IsServerAdmin(Candidate))
			{
				Host = Candidate;
				break;
			}
		}
		if (bExpectTimeout || bExpectStateChange || bExpectFlushFailure || bExpectProgress)
		{
			GameMode->bTestDropNextZoneTravelSaveCallback = true;
			GameMode->bTestRetryAfterZoneTravelTimeout = bExpectTimeout;
			GameMode->TestZoneTravelTimeoutOverrideSeconds = 0.75f;
		}
		FString FailureReason;
		const bool bAccepted = GameMode->RequestZoneTravel(Host, FailureReason);
		UE_LOG(LogTemp, Display, TEXT("[%s] request=%s reason=%s"),
			bExpectTimeout ? TEXT("MMO_ZONE_TRAVEL_TIMEOUT_TEST") : TEXT("MMO_ZONE_TRAVEL_TEST"),
			bAccepted ? TEXT("ACCEPTED") : TEXT("REJECTED"), *FailureReason);
		if (bExpectTimeout && bAccepted && Host)
		{
			// Gecis kaydi ucustayken (callback dusuruldu) baska bir kayit istegi ERTELENMELI (false doner); gecis
			// zaman asimiyla iptal olunca ertelenen kayit yapilir (FlushDeferredSave log'u).
			if (AMMOCharacter* HostCharacter = Cast<AMMOCharacter>(Host->GetPawn()))
			{
				const bool bSaved = HostCharacter->SaveCharacterForTravel();
				UE_LOG(LogTemp, Display, TEXT("[MMO_ZONE_TRAVEL_TIMEOUT_TEST] deferred-save-check saved=%d deferred=%d inflight=%d"),
					bSaved ? 1 : 0, HostCharacter->IsSaveDeferredByTravel() ? 1 : 0,
					GameMode->IsZoneTravelSaveInFlight(HostCharacter) ? 1 : 0);
			}
		}
		if (bExpectStateChange && bAccepted && Host)
		{
			AMMOCharacter* Character = Cast<AMMOCharacter>(Host->GetPawn());
			if (Character && Character->GetStats() && GameMode->IsZoneTravelSaveInFlight(Character))
			{
				// Yerel kayit tamamlandi ama test kancasi onayi dusurdu; bekleme penceresini taklit et.
				Character->GetStats()->MarkCombatActivity();
				GameMode->HandleZoneTravelSaveComplete(GameMode->ZoneTravelRequestSerial, Character, true);
				const bool bAborted = !GameMode->bZoneTravelInProgress
					&& GameMode->LastZoneTravelAbortReason == TEXT("Kayit beklerken bir oyuncu oldu veya savasa girdi; gecis iptal edildi")
					&& GameMode->PendingZoneTravelCharacters.IsEmpty() && GameMode->CompletedZoneTravelSaves.IsEmpty()
					&& !TestWorld->GetTimerManager().IsTimerActive(GameMode->ZoneTravelSaveTimeoutHandle);
				UE_LOG(LogTemp, Display, TEXT("[MMO_ZONE_TRAVEL_STATE_TEST] %s combat=%d lock=%d pending=%d"),
					bAborted ? TEXT("PASS") : TEXT("FAIL"), Character->GetStats()->IsInCombat() ? 1 : 0,
					GameMode->bZoneTravelInProgress ? 1 : 0, GameMode->PendingZoneTravelCharacters.Num());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[MMO_ZONE_TRAVEL_STATE_TEST] FAIL no pending character"));
			}
		}
		if (bExpectFlushFailure && bAccepted && Host)
		{
			AMMOCharacter* Character = Cast<AMMOCharacter>(Host->GetPawn());
			if (Character && GameMode->IsZoneTravelSaveInFlight(Character))
			{
				// Ek kayit istegi YOK: son snapshot yenilemesi de hatada isaret koymali ve gecisi iptal etmeli.
				const bool bNoExtraRequest = !Character->IsSaveDeferredByTravel();
				Character->FailDeferredSavesForTest(2); // Finish ve Abort sonrasi tekrar deneme de basarisiz.
				GameMode->HandleZoneTravelSaveComplete(GameMode->ZoneTravelRequestSerial, Character, true);
				const bool bAborted = !GameMode->bZoneTravelInProgress && GameMode->PendingZoneTravelCharacters.IsEmpty()
					&& !TestWorld->GetTimerManager().IsTimerActive(GameMode->ZoneTravelSaveTimeoutHandle)
					&& GameMode->LastZoneTravelAbortReason == TEXT("Ertelenen karakter kaydi yazilamadi; gecis iptal edildi");
				const bool bKept = Character->IsSaveDeferredByTravel();
				const bool bRetrySaved = Character->SaveCharacterForTravel(); // Elle kayit da isareti temizler.
				const bool bCleared = !Character->IsSaveDeferredByTravel();
				UE_LOG(LogTemp, Display, TEXT("[MMO_ZONE_TRAVEL_DEFERRED_TEST] %s lock=%d kept=%d retry=%d cleared=%d"),
					bNoExtraRequest && bAborted && bKept && bRetrySaved && bCleared ? TEXT("PASS") : TEXT("FAIL"),
					GameMode->bZoneTravelInProgress ? 1 : 0, bKept ? 1 : 0, bRetrySaved ? 1 : 0, bCleared ? 1 : 0);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[MMO_ZONE_TRAVEL_DEFERRED_TEST] FAIL no pending character"));
			}
		}
		if (bExpectProgress)
		{
			AMMOCharacter* Character = Host ? Cast<AMMOCharacter>(Host->GetPawn()) : nullptr;
			IMMOCharacterStore* Store = Character ? UMMOCharacterStoreSubsystem::Get(Character) : nullptr;
			FMMOCharacterSaveData Before, Expected, After;
			if (!bAccepted || !Character || !Store || !Character->GetStats() || !Character->GetInventory()
				|| !GameMode->IsZoneTravelSaveInFlight(Character) || !Store->Load(Character->GetSaveSlotName(), Before))
			{
				UE_LOG(LogTemp, Error, TEXT("[MMO_ZONE_TRAVEL_PROGRESS_TEST] FAIL setup"));
				return;
			}
			// Ilk yerel snapshot yazildi, onayi test kancasi dusurdu. Ek SaveCharacter cagrisi YOK:
			// pasif parti XP'si/ganimet gibi yalniz ilerleme mutasyonlarini taklit eder.
			Character->GetStats()->GainExperience(37);
			Character->GetInventory()->AddGold(23);
			Character->GetStats()->FillSaveData(Expected);
			Character->GetInventory()->FillSaveData(Expected);
			const bool bChanged = (Before.Level != Expected.Level || Before.Experience != Expected.Experience)
				&& Before.Gold != Expected.Gold;
			GameMode->HandleZoneTravelSaveComplete(GameMode->ZoneTravelRequestSerial, Character, true);
			const bool bLoaded = Store->Load(Character->GetSaveSlotName(), After);
			const bool bXP = bLoaded && After.Level == Expected.Level && After.Experience == Expected.Experience;
			const bool bGold = bLoaded && After.Gold == Expected.Gold;
			const bool bStarted = GameMode->bZoneTravelInProgress && GameMode->PendingZoneTravelCharacters.IsEmpty();
			UE_LOG(LogTemp, Display, TEXT("[MMO_ZONE_TRAVEL_PROGRESS_TEST] %s changed=%d xp=%d gold=%d started=%d cleared=%d"),
				bChanged && bXP && bGold && bStarted && !Character->IsSaveDeferredByTravel() ? TEXT("PASS") : TEXT("FAIL"),
				bChanged ? 1 : 0, bXP ? 1 : 0, bGold ? 1 : 0, bStarted ? 1 : 0, Character->IsSaveDeferredByTravel() ? 0 : 1);
		}
	}), 1.5f, false);
#endif
}

void AMMOGameMode::MMOProbeSpawn(float X, float Y, float Z, float Radius)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	const AMMOMob* MobDefaults = AMMOMob::StaticClass()->GetDefaultObject<AMMOMob>();
	const UCapsuleComponent* Capsule = MobDefaults ? MobDefaults->GetCapsuleComponent() : nullptr;
	if (!World || !Capsule || !HasAuthority())
	{
		return;
	}
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	constexpr int32 Samples = 64;
	int32 Ok = 0;
	AMMOMobSpawner::FSpawnProbeStats Stats;
	for (int32 i = 0; i < Samples; ++i)
	{
		FVector Out;
		if (AMMOMobSpawner::FindSafeSpawnLocation(World, nullptr, FVector(X, Y, Z), Radius, HalfHeight, 1, Out, Stats))
		{
			++Ok;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("[MMOPROBE] (%.0f,%.0f,%.0f) r=%.0f ok=%d/%d trace_miss=%d egim_yukseklik=%d [son zemin Z=%.1f nz=%.2f] nav=%d"),
		X, Y, Z, Radius, Ok, Samples, Stats.TraceMiss, Stats.SlopeOrHeight, Stats.LastGroundZ, Stats.LastNormalZ, Stats.Nav);
#endif
}

void AMMOGameMode::MMOProbeSurface(float X, float Y, float Radius)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}
	// Yukaridan asagi ilk engelleyen yuzey; yuzey Z + 50 spawner taban yuksekligidir.
	FHitResult Top;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MMOProbeSurface), false);
	if (!World->LineTraceSingleByChannel(Top, FVector(X, Y, 4000.f), FVector(X, Y, -4000.f), ECC_Visibility, Params))
	{
		UE_LOG(LogTemp, Display, TEXT("[MMOPROBE] (%.0f,%.0f) yuzey yok"), X, Y);
		return;
	}
	MMOProbeSpawn(X, Y, Top.ImpactPoint.Z + 50.f, Radius);
#endif
}

namespace
{
	// Baglanti secenegindeki (?Name=) baslangic adini ServerSetName ile AYNI karakter kuralina sokar
	// (harf/rakam, '_', '-', bosluk; en cok 20 karakter: motor ?Name= degerini 20'ye keser). Kontrol karakterleri (satir sonu vb.) sohbet
	// satirlarini taklit etmek icin kullanilabildiginden atilir.
	FString SanitizeInitialName(const FString& In)
	{
		FString Out;
		for (const TCHAR Ch : In.TrimStartAndEnd())
		{
			if (FChar::IsAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('-') || Ch == TEXT(' '))
			{
				Out.AppendChar(Ch);
			}
		}
		return Out.Left(20).TrimStartAndEnd();
	}

	// "<ad>-<12+ onaltilik hane>" kalibi: Null OSS'in uretilen varsayilan oyuncu adi
	// (gercek ornek "CIDIK-<14 hex>"). Mesru adlar ("Ozan-20261005", "Kral-12345678", "Ali-deadbeef")
	// 12 haneden kisa kuyruk tasidigi icin eslesmez.
	bool LooksLikeOsDefaultName(const FString& Name)
	{
		int32 Dash = INDEX_NONE;
		if (!Name.FindLastChar(TEXT('-'), Dash) || Dash <= 0)
		{
			return false;
		}
		const FString Suffix = Name.Mid(Dash + 1);
		// Motor ?Name= degerini 20 karakterle keser: uzun on ekte (bilgisayar adi) onaltilik kuyruk kesilmis olabilir.
		// Kesik kalip: tam 20 karakter + en az 4 hane kuyruk + kuyrukta hem en az bir rakam hem en az bir
		// a-f harfi (rastgele onaltilik kuyruk neredeyse hep ikisini de icerir; "...-Fade"/"...-Dead" gibi
		// yalniz harf ve "...-2024" gibi yalniz rakam kuyruklar mesru ad sayilir).
		// Ornekler (ad -> sonuc):
		//   "CIDIK-3F9A1B7C2D4E5F"        (20)  -> OS adi   (kuyruk 14 hane, hex)
		//   "DESKTOP-ABCDEFG-1A2B"        (20)  -> OS adi   (kesik: rakam + harf var)
		//   "MehmetCelikoglu-2024"        (20)  -> mesru    (kuyruk yalniz rakam)
		//   "KralAslanSavasci-Fade"       (21)  -> mesru    (20 karakter degil)
		//   "UzunBirIsimYazdim-Dead"      (22)  -> mesru    (20 karakter degil)
		//   "Ozan-20261005", "Kral-12345678", "Ali-deadbeef" -> mesru (kuyruk < 12 hane, kesik kalip degil)
		const bool bSuffixHasDigit = Suffix.FindLastCharByPredicate([](TCHAR C) { return FChar::IsDigit(C); }) != INDEX_NONE;
		const bool bSuffixHasHexLetter = Suffix.FindLastCharByPredicate([](TCHAR C)
		{
			return (C >= TEXT('a') && C <= TEXT('f')) || (C >= TEXT('A') && C <= TEXT('F'));
		}) != INDEX_NONE;
		const bool bTruncatedByEngine = Name.Len() == 20 && Suffix.Len() >= 4 && bSuffixHasDigit && bSuffixHasHexLetter;
		if (Suffix.Len() < 12 && !bTruncatedByEngine)
		{
			return false;
		}
		for (const TCHAR Ch : Suffix)
		{
			if (!FChar::IsHexDigit(Ch))
			{
				return false;
			}
		}
		return true;
	}
}

void AMMOGameMode::ChangeName(AController* Controller, const FString& NewName, bool bNameChange)
{
	// Motorun ServerChangeName/SetName yolu (bNameChange == true) PlayerState adini dogrudan yazar ve
	// tum ad korumalarini (karakter kurali, 20 sinir, cevrimici/yuva cakismasi, soguma, K-1) atlar.
	// Uzak istemcinin elle cagirabildigi bu yolu karakterin kendi ServerSetName yoluna (AMMOCharacter::MMOSetName)
	// yonlendiriyoruz: ayni kapilardan gecer. Pawn yoksa istek yok sayilir. Baslangic adi (false) asagida.
	if (bNameChange)
	{
		if (AMMOCharacter* Char = Controller ? Cast<AMMOCharacter>(Controller->GetPawn()) : nullptr)
		{
			Char->MMOSetName(NewName);
		}
		return;
	}

	FString UseName = NewName;
	if (!bNameChange)
	{
		// Baslangic adi: guvensiz karakterleri at; bos kalirsa ya da OS kullanici adi kalibindaysa rastgele ad ver.
		const FString Clean = SanitizeInitialName(NewName);
		UseName = Clean;
		// PreLogin yalniz o an var olan PlayerController'lara bakar; iki istemci ayni anda ayni ?Name= ile
			// girerse ikisi de gecer (ayni kayit yuvasi = esya cogaltma). Burada diger denetleyicilerle tekrar kontrol et.
			bool bNameInUse = false;
			if (!Clean.IsEmpty())
			{
				const FString CleanKey = Clean.Replace(TEXT(" "), TEXT("_"));
				if (const UWorld* CheckWorld = GetWorld())
				{
					for (FConstPlayerControllerIterator CIt = CheckWorld->GetPlayerControllerIterator(); CIt; ++CIt)
					{
						const APlayerController* Other = CIt->Get();
						if (Other && Other != Controller && Other->PlayerState)
						{
							const FString OtherKey = Other->PlayerState->GetPlayerName().TrimStartAndEnd().Replace(TEXT(" "), TEXT("_"));
							if (OtherKey.Equals(CleanKey, ESearchCase::IgnoreCase))
							{
								bNameInUse = true;
								break;
							}
						}
					}
				}
			}
			if (Clean.IsEmpty() || bNameInUse || LooksLikeOsDefaultName(Clean))
		{
			// Cevrimici baska bir oyuncuyla VE diskteki bir kayit yuvasiyla cakismayan rastgele ad
			// (kayit yuvasi = ad; var olan bir yuva baska birinin karakterini yukler/ezer).
			const UWorld* World = GetWorld();
			bool bAssigned = false;
			for (int32 Attempt = 0; Attempt < 16; ++Attempt)
			{
				const FString Candidate = FString::Printf(TEXT("Gezgin%06d"), FMath::RandRange(0, 999999));
				// Magaza yoksa yuva dogrulanamaz: aday "alinmis" sayilir (16 deneme sonrasi GUID yedegi devreye girer).
				IMMOCharacterStore* Store = UMMOCharacterStoreSubsystem::Get(this);
				bool bTaken = !Store || Store->Exists(FString(TEXT("MMOSave_")) + Candidate);
				if (!bTaken && World)
				{
					for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
					{
						const APlayerController* PC = It->Get();
						if (PC && PC->PlayerState && PC->PlayerState->GetPlayerName().Equals(Candidate, ESearchCase::IgnoreCase))
						{
							bTaken = true;
							break;
						}
					}
				}
				if (!bTaken)
				{
					UseName = Candidate;
					bAssigned = true;
					break;
				}
			}
			if (!bAssigned)
			{
				// 16 deneme de cakistiysa (cok olasi degil): GUID tabanli benzersiz yedek ad.
				UseName = FString(TEXT("Gezgin")) + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(10);
			}
		}
	}
	Super::ChangeName(Controller, UseName, bNameChange);

	// Baglanti adi degistirildiyse (gecersiz karakter, kullanimda, OS varsayilan adi) oyuncu kayit yuvasinin
	// neden baska oldugunu bilsin. Istemci HUD'u bu anda henuz hazir degil: kisa gecikmeyle sohbet satiri.
	if (UseName != NewName && Controller)
	{
		if (UWorld* NoticeWorld = GetWorld())
		{
			const TWeakObjectPtr<AController> WeakController(Controller);
			const FString Notice = FString::Printf(TEXT("Baglanti adin uygun olmadigi icin adin '%s' olarak ayarlandi. Kayitli karakterin icin gecerli bir ?Name= ile yeniden baglan."), *UseName);
			FTimerHandle NoticeHandle;
			NoticeWorld->GetTimerManager().SetTimer(NoticeHandle, FTimerDelegate::CreateWeakLambda(this, [WeakController, Notice]()
			{
				if (AMMOPlayerController* PC = Cast<AMMOPlayerController>(WeakController.Get()))
				{
					PC->ClientReceiveChat(FString(), Notice, 2);
				}
			}), 3.f, false);
		}
	}
}

void AMMOGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}
	// Istege bagli katilim parolasi (SECURITY_AUDIT K-1/B-13): sunucu komut satirina -MMOPassword=xxx
	// verilirse baglanti URL'inde ?Password=xxx zorunlu olur. Parola kodda/repoda tutulmaz; verilmezse
	// davranis degismez. Kimlik dogrulamanin yerini TUTMAZ; yalniz tanimadigin kisilerin girmesini zorlastirir.
	FString RequiredPassword;
	if (FParse::Value(FCommandLine::Get(), TEXT("MMOPassword="), RequiredPassword) && !RequiredPassword.IsEmpty())
	{
		const FString Given = UGameplayStatics::ParseOption(Options, TEXT("Password"));
		if (!Given.Equals(RequiredPassword, ESearchCase::CaseSensitive))
		{
			ErrorMessage = TEXT("Sunucu parolasi gerekli veya yanlis.");
			return;
		}
	}
	const UWorld* World = GetWorld();
	const FString Requested = UGameplayStatics::ParseOption(Options, TEXT("Name"));
	if (!World || Requested.IsEmpty())
	{
		return;
	}
	// Kayit yuvasi adiyla ayni normallestirme: bosluk -> '_', buyuk/kucuk harf duyarsiz.
	// Motor (AGameModeBase::InitNewPlayer) ?Name= degerini 20 karakterle keser; ayni kesimle karsilastir.
	const FString RequestedKey = SanitizeInitialName(Requested.Left(20)).Replace(TEXT(" "), TEXT("_"));
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (!PC || !PC->PlayerState)
		{
			continue;
		}
		const FString OnlineKey = PC->PlayerState->GetPlayerName().TrimStartAndEnd().Replace(TEXT(" "), TEXT("_"));
		if (OnlineKey.Equals(RequestedKey, ESearchCase::IgnoreCase))
		{
			ErrorMessage = TEXT("Bu ad su an baska bir oyuncu tarafindan kullaniliyor.");
			return;
		}
	}
}

void AMMOGameMode::StartPlay()
{
	Super::StartPlay();
	// Blueprint CDO'su eski false degerini saklasa bile aktif GameMode'da gecis kesintisiz olsun.
	bUseSeamlessTravel = true;
	UE_LOG(LogTemp, Log, TEXT("[MMODBG] AMMOGameMode::StartPlay called"));
	SpawnMobArea();
}

bool AMMOGameMode::RequestZoneTravel(APlayerController* Requester, FString& OutFailureReason)
{
	OutFailureReason.Reset();
	UWorld* World = GetWorld();
	if (!World || !HasAuthority() || bZoneTravelInProgress || !Requester
		|| Requester->GetWorld() != World || !MMOAdmin::IsServerAdmin(Requester))
	{
		OutFailureReason = TEXT("Gecis su anda kullanilamiyor");
		// Devam eden bir gecisin test kancalarina dokunma; yalniz bu reddedilen istegin kancalarini birak.
		if (!bZoneTravelInProgress)
		{
			ClearZoneTravelTestHooks();
		}
		return false;
	}
	LastZoneTravelAbortReason.Reset();

	const FString CurrentMap = UGameplayStatics::GetCurrentLevelName(World, true);
	FString TargetMap;
	if (CurrentMap == TEXT("Summer_scene"))
	{
		TargetMap = TEXT("/Game/HandPaintedEnvironment/Maps/Winter_scene/Winter_scene");
	}
	else if (CurrentMap == TEXT("Winter_scene"))
	{
		TargetMap = TEXT("/Game/HandPaintedEnvironment/Maps/Summer_scene/Summer_scene");
	}
	else
	{
		OutFailureReason = TEXT("Bu haritadan bolge gecisi yok");
		ClearZoneTravelTestHooks();
		return false;
	}

	TArray<AMMOCharacter*> Characters;
	TSet<FString> SaveSlots;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		AMMOCharacter* Character = PC ? Cast<AMMOCharacter>(PC->GetPawn()) : nullptr;
		if (!Character || !Character->GetStats() || Character->GetStats()->bIsDead
			|| Character->GetStats()->IsInCombat())
		{
			UE_LOG(LogTemp, Warning, TEXT("Zone travel rejected: a player is missing, dead or in combat"));
			OutFailureReason = TEXT("Tum oyuncular canli ve savas disinda olmali");
			ClearZoneTravelTestHooks();
			return false;
		}
		const FString Slot = Character->GetSaveSlotName();
		// Windows save dosyalari buyuk/kucuk harf ayirt etmez.
		const FString NormalizedSlot = Slot.ToLower();
		if (SaveSlots.Contains(NormalizedSlot))
		{
			UE_LOG(LogTemp, Error, TEXT("Zone travel rejected: duplicate save slot %s"), *Slot);
			OutFailureReason = TEXT("Iki oyuncunun kayit adi ayni");
			ClearZoneTravelTestHooks();
			return false;
		}
		SaveSlots.Add(NormalizedSlot);
		Characters.Add(Character);
	}
	if (Characters.IsEmpty())
	{
		OutFailureReason = TEXT("Tasinacak oyuncu bulunamadi");
		ClearZoneTravelTestHooks();
		return false;
	}

	bZoneTravelInProgress = true;
	const uint64 RequestSerial = ++ZoneTravelRequestSerial;
	PendingZoneTravelSaves = Characters.Num();
	CompletedZoneTravelSaves.Reset();
	PendingZoneTravelTargetMap = TargetMap;
	PendingZoneTravelSourceMap = CurrentMap;
	PendingZoneTravelCharacters.Reset(Characters.Num());
	for (AMMOCharacter* Character : Characters)
	{
		PendingZoneTravelCharacters.Add(Character);
	}
	const float SaveTimeoutSeconds = TestZoneTravelTimeoutOverrideSeconds > 0.f
		? TestZoneTravelTimeoutOverrideSeconds : ZoneTravelSaveTimeoutSeconds;
	TestZoneTravelTimeoutOverrideSeconds = 0.f;
	World->GetTimerManager().SetTimer(ZoneTravelSaveTimeoutHandle,
		FTimerDelegate::CreateUObject(this, &AMMOGameMode::HandleZoneTravelSaveTimeout, RequestSerial),
		SaveTimeoutSeconds, false);
	for (AMMOCharacter* Character : Characters)
	{
		// Yerel (senkron) magazada ilk karakter kaydi basarisiz olursa gecis burada iptal olmus olabilir:
		// kalan karakterleri gereksiz yere yazma.
		if (!bZoneTravelInProgress || ZoneTravelRequestSerial != RequestSerial)
		{
			break;
		}
		const TWeakObjectPtr<AMMOGameMode> WeakThis(this);
		const TWeakObjectPtr<AMMOCharacter> WeakCharacter(Character);
		Character->SaveCharacterForTravelAsync([WeakThis, WeakCharacter, RequestSerial](bool bSuccess)
		{
			if (AMMOGameMode* GameMode = WeakThis.Get())
			{
				GameMode->HandleZoneTravelSaveComplete(RequestSerial, WeakCharacter, bSuccess);
			}
		});
	}
	// Yerel dosya callback'leri ayni karede biter: gecis zaten iptal olduysa (kayit hatasi) cagirana
	// false + neden doner. Bulut magazasinda true yalniz istegin kabul edildigini belirtir; travel tum
	// kayit callback'leri basarili olduktan sonra baslar (iptal ederse log + ekran mesaji).
	if (!bZoneTravelInProgress || ZoneTravelRequestSerial != RequestSerial)
	{
		OutFailureReason = LastZoneTravelAbortReason.IsEmpty() ? FString(TEXT("Gecis iptal edildi")) : LastZoneTravelAbortReason;
		return false;
	}
	return true;
}

void AMMOGameMode::ClearZoneTravelTestHooks()
{
	bTestDropNextZoneTravelSaveCallback = false;
	bTestRetryAfterZoneTravelTimeout = false;
	TestZoneTravelTimeoutOverrideSeconds = 0.f;
}

void AMMOGameMode::HandleZoneTravelSaveComplete(uint64 RequestSerial, const TWeakObjectPtr<AMMOCharacter>& Character, bool bSuccess)
{
	check(IsInGameThread()); // IMMOCharacterStore callback sozlesmesi; bariyer kilitsizdir.
	if (!bZoneTravelInProgress || RequestSerial != ZoneTravelRequestSerial
		|| !PendingZoneTravelCharacters.Contains(Character))
	{
		return;
	}
	// Ilk sonuc kesindir: yinelenen BASARISIZLIK da tamamlanmis oyuncunun onayini geri alamaz.
	// Pending kumesi ServerTravel'dan sonra bosalir; eski callback'ler gecis kilidini acamaz.
	if (CompletedZoneTravelSaves.Contains(Character))
	{
		UE_LOG(LogTemp, Warning, TEXT("Zone travel: duplicate save callback ignored"));
		return;
	}
#if !UE_BUILD_SHIPPING
	if (bTestDropNextZoneTravelSaveCallback)
	{
		bTestDropNextZoneTravelSaveCallback = false;
		UE_LOG(LogTemp, Display, TEXT("[MMO_ZONE_TRAVEL_TIMEOUT_TEST] callback intentionally dropped"));
		return;
	}
#endif
	if (!bSuccess)
	{
		AbortZoneTravel(RequestSerial, TEXT("Bir karakter kaydedilemedi; gecis iptal edildi"));
		return;
	}
	if (!Character.IsValid())
	{
		AbortZoneTravel(RequestSerial, TEXT("Bir oyuncu kayit sirasinda ayrildi; gecis iptal edildi"));
		return;
	}
	CompletedZoneTravelSaves.Add(Character);
	PendingZoneTravelSaves = FMath::Max(0, PendingZoneTravelCharacters.Num() - CompletedZoneTravelSaves.Num());
	if (PendingZoneTravelSaves <= 0)
	{
		FinishZoneTravel(RequestSerial);
	}
}

void AMMOGameMode::HandleZoneTravelSaveTimeout(uint64 RequestSerial)
{
	if (!bZoneTravelInProgress || RequestSerial != ZoneTravelRequestSerial)
	{
		return;
	}
	const int32 TimedOutSaveCount = PendingZoneTravelSaves;
	const bool bRetryTestAfterTimeout = bTestRetryAfterZoneTravelTimeout;
	AbortZoneTravel(RequestSerial, FString::Printf(
		TEXT("Karakter kaydi zaman asimina ugradi (%d bekliyor); gecis iptal edildi"), TimedOutSaveCount));
#if !UE_BUILD_SHIPPING
	if (bRetryTestAfterTimeout)
	{
		// Ayni GameMode'da yeni istegin kabul edilmesi timeout'un kilidi gercekten temizledigini kanitlar.
		ScheduleZoneTravelTest(false);
	}
#endif
}

void AMMOGameMode::FinishZoneTravel(uint64 RequestSerial)
{
	if (!bZoneTravelInProgress || RequestSerial != ZoneTravelRequestSerial)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		AbortZoneTravel(RequestSerial, TEXT("Dunya gecis tamamlanmadan kapandi"));
		return;
	}
	// Bulut callback'i gecikebilir: ilk istekteki oyuncu/savas kosullari artik gecerli olmayabilir.
	// Yeni katilan veya pawn'i degisen oyuncu kaydedilmeden tasinmamali; canli/savas disi kurali
	// ServerTravel'dan hemen once sunucuda tekrar dogrulanir.
	TSet<TWeakObjectPtr<AMMOCharacter>> CurrentCharacters;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		AMMOCharacter* Character = PC ? Cast<AMMOCharacter>(PC->GetPawn()) : nullptr;
		if (!Character || !PendingZoneTravelCharacters.Contains(Character))
		{
			AbortZoneTravel(RequestSerial, TEXT("Kayit beklerken oyuncu grubu degisti; gecis iptal edildi"));
			return;
		}
		if (!Character->GetStats() || Character->GetStats()->bIsDead || Character->GetStats()->IsInCombat())
		{
			AbortZoneTravel(RequestSerial, TEXT("Kayit beklerken bir oyuncu oldu veya savasa girdi; gecis iptal edildi"));
			return;
		}
		CurrentCharacters.Add(Character);
	}
	if (CurrentCharacters.Num() != PendingZoneTravelCharacters.Num())
	{
		AbortZoneTravel(RequestSerial, TEXT("Kayit beklerken oyuncu grubu degisti; gecis iptal edildi"));
		return;
	}
	World->GetTimerManager().ClearTimer(ZoneTravelSaveTimeoutHandle);
	// Tum onaylar tamamlandi: ek kayit istemeyen XP/altin dahil son snapshot'i her oyuncu icin yaz.
	// Yalniz ertelenen istekleri yazmak, ilk snapshot sonrasi pasif ilerlemeyi kaybettiriyordu.
	if (!FlushDeferredCharacterSaves(PendingZoneTravelCharacters, true))
	{
		AbortZoneTravel(RequestSerial, TEXT("Ertelenen karakter kaydi yazilamadi; gecis iptal edildi"));
		return;
	}
	for (const TWeakObjectPtr<AMMOCharacter>& WeakCharacter : PendingZoneTravelCharacters)
	{
		AMMOCharacter* Character = WeakCharacter.Get();
		if (!Character)
		{
			AbortZoneTravel(RequestSerial, TEXT("Bir oyuncu kayit sirasinda ayrildi; gecis iptal edildi"));
			return;
		}
		Character->SetTravelSaveCommitted(true);
	}

	if (!World->ServerTravel(PendingZoneTravelTargetMap))
	{
		for (const TWeakObjectPtr<AMMOCharacter>& WeakCharacter : PendingZoneTravelCharacters)
		{
			if (AMMOCharacter* Character = WeakCharacter.Get())
			{
				Character->SetTravelSaveCommitted(false);
			}
		}
		AbortZoneTravel(RequestSerial, TEXT("Harita gecisi baslatilamadi"));
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("Zone travel started: %s -> %s (%d players)"),
		*PendingZoneTravelSourceMap, *PendingZoneTravelTargetMap, PendingZoneTravelCharacters.Num());
	PendingZoneTravelCharacters.Reset();
	CompletedZoneTravelSaves.Reset();
	PendingZoneTravelSaves = 0;
}

void AMMOGameMode::AbortZoneTravel(uint64 RequestSerial, const FString& Reason)
{
	if (RequestSerial != ZoneTravelRequestSerial)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ZoneTravelSaveTimeoutHandle);
	}
	for (const TWeakObjectPtr<AMMOCharacter>& WeakCharacter : PendingZoneTravelCharacters)
	{
		if (AMMOCharacter* Character = WeakCharacter.Get())
		{
			Character->SetTravelSaveCommitted(false);
		}
	}
	const TArray<TWeakObjectPtr<AMMOCharacter>> AbortedCharacters = MoveTemp(PendingZoneTravelCharacters);
	bZoneTravelInProgress = false;
	PendingZoneTravelSaves = 0;
	CompletedZoneTravelSaves.Reset();
	PendingZoneTravelCharacters.Reset();
	PendingZoneTravelTargetMap.Reset();
	PendingZoneTravelSourceMap.Reset();
	ClearZoneTravelTestHooks();
	LastZoneTravelAbortReason = Reason;
	UE_LOG(LogTemp, Error, TEXT("Zone travel aborted: %s"), *Reason);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, Reason);
	}
	// Pencere kapandi: ertelenen kayitlar artik gecis yazimini bozamaz.
	FlushDeferredCharacterSaves(AbortedCharacters);
}

bool AMMOGameMode::IsZoneTravelSaveInFlight(const AMMOCharacter* Character) const
{
	if (!bZoneTravelInProgress || !Character)
	{
		return false;
	}
	const TWeakObjectPtr<AMMOCharacter> Weak(const_cast<AMMOCharacter*>(Character));
	return PendingZoneTravelCharacters.Contains(Weak) && !CompletedZoneTravelSaves.Contains(Weak);
}

bool AMMOGameMode::FlushDeferredCharacterSaves(const TArray<TWeakObjectPtr<AMMOCharacter>>& Characters, bool bRefreshSnapshot)
{
	bool bAllSaved = true;
	for (const TWeakObjectPtr<AMMOCharacter>& Weak : Characters)
	{
		if (AMMOCharacter* Character = Weak.Get())
		{
			if (!Character->FlushDeferredSave(bRefreshSnapshot)) bAllSaved = false;
		}
	}
	return bAllSaved;
}

void AMMOGameMode::SpawnMobArea()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Spawner ve hub yerlesimi PlayerStart'a bagli; yoksa rastgele orijinde uretme.
	FVector Origin(0.f, 0.f, 200.f);
	FRotator Facing(0.f, 0.f, 0.f);
	bool bFoundStart = false;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		Origin = It->GetActorLocation();
		Facing = It->GetActorRotation();
		bFoundStart = true;
		break;
	}
	if (!bFoundStart)
	{
		UE_LOG(LogTemp, Error, TEXT("[MMODBG] SpawnMobArea skipped: map %s has no PlayerStart"), *World->GetMapName());
		return;
	}
	// Winter_scene ayri bir harita ve ayri bir Lv42-50 yerlesimi kullanir. Kaydedilmis
	// Recast navmesh uzerinde (-3000,-1500) ve (-3000,0) ceplerinin 150/225u
	// halkalarindaki 64 ornegin tamami hub'a tam yolla bagli; zemin farki <10u.
	// Dogudaki (3000,-1500) yol kismi oldugu icin oraya mob yerlestirme.
	if (World->GetMapName().EndsWith(TEXT("Winter_scene")))
	{
		const FVector WinterMobLoc(-3000.f, -1500.f, 42.f); // zemin ~-8u, taban +50u
		const FTransform WinterMobTM(FRotator::ZeroRotator, WinterMobLoc);
		if (AMMOMobSpawner* WinterSpawner = World->SpawnActorDeferred<AMMOMobSpawner>(
			AMMOMobSpawner::StaticClass(), WinterMobTM))
		{
			WinterSpawner->MaxMobs = 6;
			WinterSpawner->SpawnRadius = 225.f;
			WinterSpawner->MinMobLevel = 42;
			WinterSpawner->MaxMobLevel = 49; // W1 yedek paketi: WinterWolf (47) + WinterBehemoth (49) bu cepte; ayri cep sonra probla
			WinterSpawner->bValidateSpawnOnNavGround = true;
			UGameplayStatics::FinishSpawningActor(WinterSpawner, WinterMobTM);
		}

		const FVector WinterBossLoc(-3000.f, 0.f, 50.f); // zemin ~0u
		const FTransform WinterBossTM(FRotator::ZeroRotator, WinterBossLoc);
		if (AMMOMobSpawner* WinterBoss = World->SpawnActorDeferred<AMMOMobSpawner>(
			AMMOMobSpawner::StaticClass(), WinterBossTM))
		{
			WinterBoss->MaxMobs = 1;
			WinterBoss->SpawnRadius = 150.f;
			WinterBoss->TopUpInterval = 60.f;
			WinterBoss->ForcedArchetypeId = FName(TEXT("WinterSovereign"));
			WinterBoss->bValidateSpawnOnNavGround = true;
			UGameplayStatics::FinishSpawningActor(WinterBoss, WinterBossTM);
		}

		// Lv51-57 Frostbound dilimi. Headless Winter grid probu (spawner-esdeger kontrol:
		// zemin +-60u, egim, nav 50/35u sapma, hub'a TAM yol) ile secildi. Z = zemin + 50u.
		// QA (627e79a): ilk Nobetci (-3800,1200) ve Muhafiz (-2000,1000) cepleri Kis
		// Hukumdari'na ~1400u idi; gezinme 450u + aggro 600u ile mob'lar 200-500u'ya kadar
		// yaklasip istenmeyen cekisler yaratabiliyordu, Nobetci ayrica eski navmesh
		// kenarindaydi. Navmesh genisletmesinden sonra ikisi de >=2400u uzaga tasindi.
		//  - Gozcu (Lv51) dogu (3000,0): zemin 141..178u.
		//  - Nobetci (Lv55) kuzey (-3500,3000): ilk (-2800,4400) konumu calisma zamaninda
		//    2/64 verdi, asagidaki 2026-10-03 notuna bak (yeni konum 64/64).
		//  - Muhafiz elit (Lv57) (2000,2800): zemin ~186u, halka 16/16.
		auto SpawnFrostbound = [&](const FVector& Loc, int32 MinLv, int32 MaxLv, float Radius,
			int32 MaxMobs, FName Forced, float TopUp)
		{
			const FTransform TM(FRotator::ZeroRotator, Loc);
			if (AMMOMobSpawner* S = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), TM))
			{
				S->MaxMobs = MaxMobs;
				S->SpawnRadius = Radius;
				if (Forced.IsNone())
				{
					S->MinMobLevel = MinLv;
					S->MaxMobLevel = MaxLv;
				}
				else
				{
					S->ForcedArchetypeId = Forced;
					S->TopUpInterval = TopUp;
				}
				S->bValidateSpawnOnNavGround = true;
				UGameplayStatics::FinishSpawningActor(S, TM);
			}
		};
		// 2026-10-03 calisma zamani duman testi (Scripts/SmokeTest.ps1 + MMOProbeSpawn): editor probu
		// "gecti" dese de Nobetci (-2800,4400,-120) cebi oyunda 2/64, Ucurum Gezgini (-5200,3600,-115)
		// 23/64 verdi (iz surme baslangicinda cokus: dag cikintisinin basit carpismasi). Yeni konumlar
		// MMOProbeSurface gridiyle olculdu: Nobetci (-3500,3000) 64/64, Gezgin (-5500,4500) 59/64;
		// hub'dan tam navmesh yolu var; Kis Hukumdari'ndan 3041u, birbirinden 2500u.
		SpawnFrostbound(FVector(3000.f, 0.f, 210.f), 51, 51, 225.f, 6, NAME_None, 0.f);
		SpawnFrostbound(FVector(-3500.f, 3000.f, -120.f), 55, 55, 225.f, 6, NAME_None, 0.f);
		SpawnFrostbound(FVector(2000.f, 2800.f, 236.f), 0, 0, 150.f, 1, FName(TEXT("FrostboundWarden")), 60.f);

		// Lv66-80 Kul + Ucurum dilimleri. Winter NavMeshBoundsVolume 2026-10-03'te
		// Scripts/ExtendWinterNavigation.py ile X+-8000 / Y -8500..5500'e genisletildi.
		// Yeni grid probunda (spawner-esdeger kontrol, hub'a tam yol) halkalar 16/16;
		// tum cepler birbirinden ve onceki Winter ceplerinden >=1800u, hub'dan >=3800u.
		//  Kul Oncusu Lv66 (1200,-5200) | Kul Baglayici Lv68 (-2000,-6800) | Kul Tirani elit Lv70 (4400,-5200)
		//  Ucurum Gezgini Lv72 (-5500,4500) | Ucurum Koruyucusu Lv76 (4400,3600) | Ucurum Naibi elit Lv80 (6800,2000, Z~580)
		//  (Naib ilk (-7600,-1200)'deydi; gezinmeyle navmesh sinirini -8000 asiyordu, QA ile tasindi.)
		SpawnFrostbound(FVector(1200.f, -5200.f, 50.f), 66, 66, 225.f, 6, NAME_None, 0.f);
		SpawnFrostbound(FVector(-2000.f, -6800.f, 49.f), 68, 68, 225.f, 6, NAME_None, 0.f);
		SpawnFrostbound(FVector(4400.f, -5200.f, 340.f), 0, 0, 150.f, 1, FName(TEXT("AshTyrant")), 60.f);
		SpawnFrostbound(FVector(-5500.f, 4500.f, -92.f), 72, 72, 225.f, 6, NAME_None, 0.f);
		SpawnFrostbound(FVector(4400.f, 3600.f, 550.f), 76, 76, 225.f, 6, NAME_None, 0.f);
		SpawnFrostbound(FVector(6800.f, 2000.f, 629.f), 0, 0, 150.f, 1, FName(TEXT("AbyssRegent")), 60.f);

		// Kis hub'inda gorev kabul/teslimi, satis ve iyilesme ayni servislerle
		// acilir. Kayitli PlayerStart merkezinden 250u ote, mob cebinden >=2750u.
		FActorSpawnParameters WinterNpcParams;
		WinterNpcParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		World->SpawnActor<AMMOVendorNPC>(AMMOVendorNPC::StaticClass(),
			Origin + FVector(250.f, 200.f, 60.f), FRotator(0.f, 180.f, 0.f), WinterNpcParams);
		World->SpawnActor<AMMOQuestGiverNPC>(AMMOQuestGiverNPC::StaticClass(),
			Origin + FVector(250.f, -200.f, 60.f), FRotator(0.f, 180.f, 0.f), WinterNpcParams);
		World->SpawnActor<AMMOHealerNPC>(AMMOHealerNPC::StaticClass(),
			Origin + FVector(250.f, -350.f, 60.f), FRotator(0.f, 180.f, 0.f), WinterNpcParams);
		World->SpawnActor<AMMOWarehouseNPC>(AMMOWarehouseNPC::StaticClass(),
			Origin + FVector(250.f, 350.f, 60.f), FRotator(0.f, 180.f, 0.f), WinterNpcParams);
		UE_LOG(LogTemp, Log, TEXT("[MMODBG] Winter_scene Lv42-80 layout spawned"));
		return;
	}
	// Bundan sonraki konumlar Summer_scene'e ozgu. Yeni bir harita PlayerStart
	// kazandiginda Summer moblarini ve hub NPC'lerini oraya tasima.
	// PIE'deki UEDPIE_0_ on eki nedeniyle harita adinin sonunu karsilastir.
	if (!World->GetMapName().EndsWith(TEXT("Summer_scene")))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MMODBG] SpawnMobArea skipped: map %s has no configured gameplay layout"), *World->GetMapName());
		return;
	}

	// Ana mob alani: NPC hub'inin (Kael +400u) belirgin disinda kalsin diye 1.5x mesafede
	// ve dar yaricapla (350) — moblar meydanda/NPC'lerin arasinda degil, disarida cikar.
	// Ana spawner Lv1-5 normalleriyle sinirli (MaxMobLevel=5): yeni Lv6-7 moblar baslangic havuzuna sizmasin.
	const FVector SpawnerLoc = Origin + Facing.Vector() * (MobAreaDistance * 1.5f) + FVector(0.f, 0.f, 50.f);
	const FTransform SpawnerTM(FRotator::ZeroRotator, SpawnerLoc);
	AMMOMobSpawner* Spawner = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), SpawnerTM);
	if (Spawner)
	{
		Spawner->MaxMobLevel = 5;
		// QA ilk-10-dk MEDIUM-3: 3 canli mob + 5 esit olasilikli tur = 5 Goblin icin ort. 23 kill,
		// ilk gorevler cok yavas ilerliyordu. 6 mob kill hizini ikiye katlar.
		Spawner->MaxMobs = 6;
		Spawner->SpawnRadius = 350.f;
		UGameplayStatics::FinishSpawningActor(Spawner, SpawnerTM);
	}

	UE_LOG(LogTemp, Log, TEXT("[MMODBG] SpawnMobArea: playerStart=%d origin=%s spawnerLoc=%s spawner=%s"),
		bFoundStart ? 1 : 0, *Origin.ToCompactString(), *SpawnerLoc.ToCompactString(),
		Spawner ? *Spawner->GetName() : TEXT("NULL"));

	// World boss: mob alaninin biraz otesine, tek bir Ork Reisi tutan spawner (olunce ~30sn sonra respawn).
	const FVector BossLoc = Origin + Facing.Vector() * (MobAreaDistance * 2.2f) + FVector(0.f, 0.f, 50.f);
	const FTransform BossTM(FRotator::ZeroRotator, BossLoc);
	if (AMMOMobSpawner* BossSpawner = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), BossTM))
	{
		BossSpawner->MaxMobs = 1;
		BossSpawner->SpawnRadius = 150.f;
		BossSpawner->TopUpInterval = 30.f;
		BossSpawner->ForcedArchetypeId = FName(TEXT("OrcWarlord"));
		UGameplayStatics::FinishSpawningActor(BossSpawner, BossTM);
	}

	// Elit: Orumcek Ana — mob alaninin yan kosesine, tek bir elit tutan spawner (boss'tan ayri yerde).
	// Facing yonunun saginda, mob alani mesafesinin biraz otesinde konumlandir.
	const FVector RightDir = FRotator(0.f, Facing.Yaw + 90.f, 0.f).Vector();
	const FVector EliteLoc = Origin + Facing.Vector() * (MobAreaDistance * 1.6f) + RightDir * 700.f + FVector(0.f, 0.f, 50.f);
	const FTransform EliteTM(FRotator::ZeroRotator, EliteLoc);
	if (AMMOMobSpawner* EliteSpawner = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), EliteTM))
	{
		EliteSpawner->MaxMobs = 1;
		EliteSpawner->SpawnRadius = 150.f;
		EliteSpawner->TopUpInterval = 45.f;
		EliteSpawner->ForcedArchetypeId = FName(TEXT("SpiderQueen"));
		UGameplayStatics::FinishSpawningActor(EliteSpawner, EliteTM);
	}

	// Elit: Kadim Golem (tank) — mob alaninin SOL kosesine (Facing yonunun soluna), Orumcek Ana'nin karsi tarafi.
	const FVector GolemLoc = Origin + Facing.Vector() * (MobAreaDistance * 1.6f) - RightDir * 700.f + FVector(0.f, 0.f, 50.f);
	const FTransform GolemTM(FRotator::ZeroRotator, GolemLoc);
	if (AMMOMobSpawner* GolemSpawner = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), GolemTM))
	{
		GolemSpawner->MaxMobs = 1;
		GolemSpawner->SpawnRadius = 150.f;
		GolemSpawner->TopUpInterval = 50.f;
		GolemSpawner->ForcedArchetypeId = FName(TEXT("StoneGolem"));
		UGameplayStatics::FinishSpawningActor(GolemSpawner, GolemTM);
	}

	// KRITIK duzeltme (2026-09-15, qa-kontrolcu OFPA-analizi + kendi dogrulamam): haritanin
	// (Summer_scene) gercek zemini tek bir SM_Template_Map_Floor aktoru, olcek(4,4,1), yani
	// zemin SADECE X/Y in [-2000,+2000] (4000x4000u) araliginda var; disari yerlestirilen
	// HERHANGI bir spawner havada/bosllukta kalir, moblar dusup yok olur veya hicbir zaman
	// gorunmez, o zincirin gorevleri KALICI OLARAK TAMAMLANAMAZ olur. Eski multiplier tabanli
	// (F*3.2, F*4, F*6, F*8) yerleşim bunu asiyordu (6/10 spawner disardaydi). Ilk duzeltmem
	// merkez noktalarini sinir icine aldi ama spawner'in KENDI SpawnRadius'unu (bolge
	// spawner'larinda 500u) hesaba katmamisti - moblar yine bosluga dusebiliyordu. Su andan
	// itibaren her spawner icin |F|+SpawnRadius<=1850 VE |R|+SpawnRadius<=1850 kurali uygulanir
	// (150u duvar payi, kare zemin oldugu icin F/R bagimsiz). F negatif = PlayerStart'in ARKASI,
	// o da zeminin bir parcasi, kullanilabilir alani ikiye katlar.

	// Elit: Golge Suikastci (assassin) — on-sag, Orumcek Ana'dan yeterince uzak (r=150).
	const FVector AssassinLoc = Origin + Facing.Vector() * 1400.f + RightDir * 1350.f + FVector(0.f, 0.f, 50.f);
	const FTransform AssassinTM(FRotator::ZeroRotator, AssassinLoc);
	if (AMMOMobSpawner* AssassinSpawner = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), AssassinTM))
	{
		AssassinSpawner->MaxMobs = 1;
		AssassinSpawner->SpawnRadius = 150.f;
		AssassinSpawner->TopUpInterval = 45.f;
		AssassinSpawner->ForcedArchetypeId = FName(TEXT("ShadowAssassin"));
		UGameplayStatics::FinishSpawningActor(AssassinSpawner, AssassinTM);
	}

	// --- IKINCI SAVAS BOLGESI (Lv6-7 NORMAL moblar) ---
	// GUVENLIK duzeltmesi (2026-09-18, QA HIGH #1): SpawnRadius 500->400'e indirildi (bkz.
	// MMOMobArchetype.cpp'deki AggroRange notu - ikisi birlikte dogus noktasi/NPC'lerde
	// >=300u guvenlik payi sagliyor, python ile dogrulandi).
	// Arka-sag (F negatif = PlayerStart'in arkasi). ForcedArchetypeId YOK; MinMobLevel=6/MaxMobLevel=7.
	const FVector SecondZoneLoc = Origin - Facing.Vector() * 650.f + RightDir * 1300.f + FVector(0.f, 0.f, 50.f);
	const FTransform SecondZoneTM(FRotator::ZeroRotator, SecondZoneLoc);
	if (AMMOMobSpawner* SecondZone = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), SecondZoneTM))
	{
		SecondZone->MaxMobs = 5;
		SecondZone->SpawnRadius = 400.f;
		SecondZone->MinMobLevel = 6;
		SecondZone->MaxMobLevel = 7;
		UGameplayStatics::FinishSpawningActor(SecondZone, SecondZoneTM);
		UE_LOG(LogTemp, Log, TEXT("[MMODBG] SecondZone spawner @ %s"), *SecondZoneLoc.ToCompactString());
	}

	// --- UCUNCU SAVAS BOLGESI (Lv8-12 NORMAL moblar) ---
	// Tam arka-orta (F=-1300, R=0), zeminin kullanilmayan arka yarisinin merkezi.
	// ForcedArchetypeId YOK; band Lv8-12 normalleri secer.
	const FVector ThirdZoneLoc = Origin - Facing.Vector() * 1300.f + FVector(0.f, 0.f, 50.f);
	const FTransform ThirdZoneTM(FRotator::ZeroRotator, ThirdZoneLoc);
	if (AMMOMobSpawner* ThirdZone = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), ThirdZoneTM))
	{
		ThirdZone->MaxMobs = 5;
		ThirdZone->SpawnRadius = 400.f;
		ThirdZone->MinMobLevel = 8;
		ThirdZone->MaxMobLevel = 12;
		UGameplayStatics::FinishSpawningActor(ThirdZone, ThirdZoneTM);
		UE_LOG(LogTemp, Log, TEXT("[MMODBG] ThirdZone spawner @ %s"), *ThirdZoneLoc.ToCompactString());
	}

	// Elit: Buz Lordu (Lv13, ucuncu bolge kademe-sonu) — arka-sag kose (r=150).
	const FVector LichLoc = Origin - Facing.Vector() * 1650.f + RightDir * 1300.f + FVector(0.f, 0.f, 50.f);
	const FTransform LichTM(FRotator::ZeroRotator, LichLoc);
	if (AMMOMobSpawner* LichSpawner = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), LichTM))
	{
		LichSpawner->MaxMobs = 1;
		LichSpawner->SpawnRadius = 150.f;
		LichSpawner->TopUpInterval = 60.f;
		LichSpawner->ForcedArchetypeId = FName(TEXT("FrostLich"));
		UGameplayStatics::FinishSpawningActor(LichSpawner, LichTM);
	}

	// --- DORDUNCU SAVAS BOLGESI (Lv16-22 NORMAL moblar, Alev Vadisi) ---
	// Arka-sol (F=-650, R=-1300), Ikinci bolgenin R-aynasi. SpawnRadius=500 hesaba katildi.
	// ForcedArchetypeId YOK; band Lv16-22 normallerini secer.
	const FVector FourthZoneLoc = Origin - Facing.Vector() * 650.f - RightDir * 1300.f + FVector(0.f, 0.f, 50.f);
	const FTransform FourthZoneTM(FRotator::ZeroRotator, FourthZoneLoc);
	if (AMMOMobSpawner* FourthZone = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), FourthZoneTM))
	{
		FourthZone->MaxMobs = 5;
		FourthZone->SpawnRadius = 400.f;
		FourthZone->MinMobLevel = 16;
		FourthZone->MaxMobLevel = 22;
		UGameplayStatics::FinishSpawningActor(FourthZone, FourthZoneTM);
		UE_LOG(LogTemp, Log, TEXT("[MMODBG] FourthZone spawner @ %s"), *FourthZoneLoc.ToCompactString());
	}

	// Elit: Magma Tirani (Lv24, dorduncu bolge kademe-sonu) — on-sol, Golge Suikastci'nin
	// R-aynasi, Kadim Golem'den yeterince uzak (r=150).
	const FVector TyrantLoc = Origin + Facing.Vector() * 1400.f - RightDir * 1350.f + FVector(0.f, 0.f, 50.f);
	const FTransform TyrantTM(FRotator::ZeroRotator, TyrantLoc);
	if (AMMOMobSpawner* TyrantSpawner = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), TyrantTM))
	{
		TyrantSpawner->MaxMobs = 1;
		TyrantSpawner->SpawnRadius = 150.f;
		TyrantSpawner->TopUpInterval = 60.f;
		TyrantSpawner->ForcedArchetypeId = FName(TEXT("MagmaTyrant"));
		UGameplayStatics::FinishSpawningActor(TyrantSpawner, TyrantTM);
	}

	// Besinci bolge Summer_scene navmesh'i uzerinde olculen mutlak koordinatlara baglidir.
	// Headless harita probu: (-2800,-1800) merkezinin 150/225u halkalarindaki 64 nokta
	// zeminde ve bagli navmesh'te; PlayerStart ve dorduncu bolgeye tam yol var.
	// (-4000,0) elit alaninin 150/200u halkalarindaki 64 nokta da bagli ve
	// normal alandan 2163u uzakta. Baska haritalarda bu konumlar kullanilmaz.
	if (World->GetMapName().Contains(TEXT("Summer_scene")))
	{
		// Bu iki merkezde olculen zemin Z yaklasik 0; spawner tabani zeminden 50u yukarida.
		const FVector ObsidianLoc(-2800.f, -1800.f, 50.f);
		const FTransform ObsidianTM(FRotator::ZeroRotator, ObsidianLoc);
		if (AMMOMobSpawner* ObsidianSpawner = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), ObsidianTM))
		{
			ObsidianSpawner->MaxMobs = 5;
			ObsidianSpawner->SpawnRadius = 225.f;
			ObsidianSpawner->MinMobLevel = 25;
			ObsidianSpawner->MaxMobLevel = 29;
			ObsidianSpawner->bValidateSpawnOnNavGround = true;
			UGameplayStatics::FinishSpawningActor(ObsidianSpawner, ObsidianTM);
		}

		// Cozulme Yolu (Lv13-15): ROTA ACIKLIGI olcutuyle yerlestirildi (QA NIGHT2 ORTA-1).
		// Eski konum (-5000,-3000): cep-cep mesafe tutuyordu ama hub'dan duz hat Obsidyen (Lv25-29)
		// merkezinden 288u, Obsidyen Reisi'ne (aggro 1200) 676u geciyordu; Lv11-15 oyuncu yuksek
		// seviyeli cebin icinden geciyordu. Yeni konum (-1875,5125,-114), kuzey (zemin Z~-164).
		// Olcum 2026-10-06 (Scripts/ProbeThawRoute.py = editor Recast navmesh, 250u grid, 1877 tam-yollu
		// hucre; oyun ici dogrulama SmokeTest.ps1 + MMOProbeSurface/MMOProbeSpawn):
		//  - Cep merkezlerine en yakin: Yarik Bekcisi (-4800,4800) 2943u; hepsi >=2200u (erken cepler
		//    PlayerStart (-816,-47) yaw 0'a gore). Hub'a 5280u. Navmesh siniri (+-8500) icinde.
		//  - Hub -> cep NAV yolu tam (kismi degil): (-816,-47) (-2413,3363) (-2375,4427) (-2223,4712)
		//    (-2128,4845) (-1875,5125), uzunluk 5694u; halkanin 32/32 noktasi da tam yolla bagli.
		//  - Rota acikligi (yol cokluchizgisine en kisa mesafe), seviyesi >=20 cepler:
		//      Alacakaranlik Hukumdari (-3200,1800) 1375 | Magma Tirani (elit Lv24, aggro 1350) 1945 |
		//      Obsidyen (-2800,-1800) 2647 | Obsidyen Reisi (-4000,0) 2863 | Alacakaranlik (1000,3200) 3022 |
		//      Yarik Bekcisi 2454 | Yarik Arayicisi 5020 | Yarik Habercisi 6769  (eski rota: 288 / 676).
		//    Hedef 1500u KARSILANMADI: hub cikislari iki koridora sabit (dogu: (0,0)->(988,-437), Magma
		//    Tirani'na 1042u; kuzeybati: duz hat (-816,-47)->(-2413,3363), Alacakaranlik Hukumdari'na
		//    1375u); 1877 hucrenin hicbiri >1375 vermedi (guney/bati rotalari Obsidyen'e 1064u, Reis'e 807u).
		//    Kuzey en az kotu: tek >=20 cep Hukumdari (aggro 600 -> fark etme zarfi ~1200u < 1375u, pay 175u);
		//    dogu/bati rotalari ise Magma Tirani/Obsidyen zarfinin icinde kaliyor.
		//  - Kalan risk: yol Ikinci bolgeden (Lv6-7, 37u) ve Buz Lordu'ndan (Lv13 elit, aggro 1300, 943u) geciyor.
		//  - Oyun ici: MMOProbeSpawn -1875 5125 -114 225 x10 = 59..64/64 (ort 62,4; egim reddi nz=0.64 yamaci,
		//    trace_miss 0, nav 0). Yakin komsular dar duz seritte: (-2000,5125) 43/64, (-1875,5000) 45/64.
		const FVector ThawLoc(-1875.f, 5125.f, -114.f);
		const FTransform ThawTM(FRotator::ZeroRotator, ThawLoc);
		if (AMMOMobSpawner* ThawSpawner = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), ThawTM))
		{
			ThawSpawner->MaxMobs = 5;
			ThawSpawner->SpawnRadius = 225.f;
			ThawSpawner->MinMobLevel = 13;
			ThawSpawner->MaxMobLevel = 15;
			ThawSpawner->bValidateSpawnOnNavGround = true;
			UGameplayStatics::FinishSpawningActor(ThawSpawner, ThawTM);
		}

		const FVector ObsidianBossLoc(-4000.f, 0.f, 50.f);
		const FTransform ObsidianBossTM(FRotator::ZeroRotator, ObsidianBossLoc);
		if (AMMOMobSpawner* ObsidianBossSpawner = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), ObsidianBossTM))
		{
			ObsidianBossSpawner->MaxMobs = 1;
			ObsidianBossSpawner->SpawnRadius = 150.f;
			ObsidianBossSpawner->TopUpInterval = 60.f;
			ObsidianBossSpawner->ForcedArchetypeId = FName(TEXT("ObsidianWarlord"));
			ObsidianBossSpawner->bValidateSpawnOnNavGround = true;
			UGameplayStatics::FinishSpawningActor(ObsidianBossSpawner, ObsidianBossTM);
		}

		// Headless Summer_scene probu: (1000,3200) 225/250u halkalarinin 64/64 noktasi
		// zeminde ve bagli navmesh'te; PlayerStart'a tam yol var. Zemin Z~188,
		// taban Z=238. Onceki (-4100,-1400) merkezi Obsidyen Reisi'nin 1200u aggro
		// alaniyla cakisiyordu. Elit (-3200,1800) alaninin zemin Z~-33.2.
		// Dogrulama her spawn'da tekrar calisir.
		const FVector DuskLoc(1000.f, 3200.f, 238.f);
		const FTransform DuskTM(FRotator::ZeroRotator, DuskLoc);
		if (AMMOMobSpawner* DuskSpawner = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), DuskTM))
		{
			DuskSpawner->MaxMobs = 5;
			DuskSpawner->SpawnRadius = 225.f;
			DuskSpawner->MinMobLevel = 31;
			DuskSpawner->MaxMobLevel = 39;
			DuskSpawner->bValidateSpawnOnNavGround = true;
			UGameplayStatics::FinishSpawningActor(DuskSpawner, DuskTM);
		}

		const FVector DuskBossLoc(-3200.f, 1800.f, 50.f);
		const FTransform DuskBossTM(FRotator::ZeroRotator, DuskBossLoc);
		if (AMMOMobSpawner* DuskBossSpawner = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), DuskBossTM))
		{
			DuskBossSpawner->MaxMobs = 1;
			DuskBossSpawner->SpawnRadius = 150.f;
			DuskBossSpawner->TopUpInterval = 60.f;
			DuskBossSpawner->ForcedArchetypeId = FName(TEXT("DuskOverlord"));
			DuskBossSpawner->bValidateSpawnOnNavGround = true;
			UGameplayStatics::FinishSpawningActor(DuskBossSpawner, DuskBossTM);
		}

		// Lv58-65 Rift dilimi (PROGRESSION_PLAN). 2026-10-03 headless Summer grid probu
		// (spawner'in kendi zemin+-60u / egim / nav 50/35u / PlayerStart'a tam yol kontrolu):
		// QA (527ca1f): ilk konumlar erken bolgelere ~1450-1660u'daydi; Rift mob'u
		// spawn 225 + gezinme 450 + aggro 600 ile ~1275u'dan oyuncu fark ediyor, Lv7-40
		// oyuncular Lv58+ mob cekebiliyordu. Summer NavMeshBoundsVolume
		// (Scripts/ExtendSummerNavigation.py) X/Y +-8500'e genisletildi, cepler uzaklastirildi:
		//  - Arayici (Lv58) (800,-4800): zemin ~0, halka 16/16, en yakin eski cep ~3500u.
		//  - Bekci (Lv62) (-4800,4800): zemin ~-170, halka 16/16, ~3400u.
		//  - Haberci elit (Lv65) (4800,4000): zemin ~500, halka 16/16, ~3880u.
		auto SpawnRift = [&](const FVector& Loc, int32 Lv, float Radius, int32 MaxMobs, FName Forced)
		{
			const FTransform TM(FRotator::ZeroRotator, Loc);
			if (AMMOMobSpawner* S = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), TM))
			{
				S->MaxMobs = MaxMobs;
				S->SpawnRadius = Radius;
				if (Forced.IsNone())
				{
					S->MinMobLevel = Lv;
					S->MaxMobLevel = Lv;
				}
				else
				{
					S->ForcedArchetypeId = Forced;
					S->TopUpInterval = 60.f;
				}
				S->bValidateSpawnOnNavGround = true;
				UGameplayStatics::FinishSpawningActor(S, TM);
			}
		};
		SpawnRift(FVector(800.f, -4800.f, 50.f), 58, 225.f, 6, NAME_None);
		SpawnRift(FVector(-4800.f, 4800.f, -120.f), 62, 225.f, 6, NAME_None);
		SpawnRift(FVector(4800.f, 4000.f, 550.f), 0, 150.f, 1, FName(TEXT("RiftHerald")));
	}

	// Satici NPC'yi oyuncu dogus noktasinin yanina koy (yandan biraz, mob alanina ters yon).
	FActorSpawnParameters NpcParams;
	NpcParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	const FVector VendorLoc = Origin + FVector(250.f, 200.f, 60.f);
	World->SpawnActor<AMMOVendorNPC>(AMMOVendorNPC::StaticClass(), VendorLoc, FRotator(0.f, 180.f, 0.f), NpcParams);

	// Gorevli NPC (saticinin yaninda).
	const FVector QuestLoc = Origin + FVector(250.f, -200.f, 60.f);
	World->SpawnActor<AMMOQuestGiverNPC>(AMMOQuestGiverNPC::StaticClass(), QuestLoc, FRotator(0.f, 180.f, 0.f), NpcParams);

	// Konusan NPC: Muhafiz Kael (kapi muhafizi) — tiklaninca LLM'den replik gelir.
	// SpawnActorDeferred ile ad/rol/yedek replikleri sunucuda BeginPlay'den once ayarla.
	const FVector TalkerLoc = Origin + FVector(400.f, 0.f, 60.f);
	const FTransform TalkerTM(FRotator(0.f, 180.f, 0.f), TalkerLoc);
	if (AMMOTalkerNPC* Kael = World->SpawnActorDeferred<AMMOTalkerNPC>(AMMOTalkerNPC::StaticClass(), TalkerTM))
	{
		Kael->NpcName = TEXT("Muhafiz Kael");
		Kael->NpcRole = TEXT("kasaba kapisini tutan muhafiz; kisa, disiplinli konusur, tehlikeye karsi net uyarir");
		// LLM devre disiyken kullanilacak yedek replikler (docs/NpcDialogue.md'den).
		Kael->FallbackLines = {
			TEXT("Dur. Adini ve derdini soyle, sonra gec."),
			TEXT("Surlarin disi senin dostun degil; silahini hazir tut."),
			TEXT("Su tepede kurt uludu butun gece. Uzagindan gec, tek basinaysan hic yaklasma."),
			TEXT("Ork izleri gorduk doguda. Hafife alma, suruyle gezerler."),
			TEXT("Haydutlar yollari kesti. Kervanla git, tek atliyi avlarlar."),
			TEXT("Kapidan gecen cok, geri donen az. Sen donenlerden ol."),
			TEXT("Goblin tek basina korkutmaz. Suru bastirirsa geri cekil, dar bir gecit bul.")
		};
		UGameplayStatics::FinishSpawningActor(Kael, TalkerTM);
	}

	// Sifaci NPC: Sifaci Lyra (tam iyilestirme + altin dususu). Saticinin karsi yanina koy.
	// GUVENLIK duzeltmesi (2026-09-18, QA HIGH #2): eskiden F=-250 (PlayerStart'in hafif
	// ARKASI) idi - zararsizdi ta ki bu commit'ten once o taraf bos olana kadar; artik orada
	// Lv8-12/Lv16-22 mob bolgeleri var, Lyra onlarin aggro menziline cok yakin dusuyordu
	// (~569-670u). Diger hub NPC'leriyle (Satici/Gorevli/Kael) AYNI on tarafa (F pozitif)
	// tasindi, R=-350 ile digerleriyle cakismasin diye ayri konumlandirildi.
	const FVector HealerLoc = Origin + FVector(250.f, -350.f, 60.f);
	World->SpawnActor<AMMOHealerNPC>(AMMOHealerNPC::StaticClass(), HealerLoc, FRotator(0.f, 180.f, 0.f), NpcParams);

	// Depocu (KO "Inn" deposu): saticinin karsi tarafinda, sifaciyla simetrik.
	const FVector WarehouseLoc = Origin + FVector(250.f, 350.f, 60.f);
	World->SpawnActor<AMMOWarehouseNPC>(AMMOWarehouseNPC::StaticClass(), WarehouseLoc, FRotator(0.f, 180.f, 0.f), NpcParams);
}

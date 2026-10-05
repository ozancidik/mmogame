// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/MMOGameMode.h"
#include "Core/MMOMobSpawner.h"
#include "Characters/MMOMob.h"
#include "Components/CapsuleComponent.h"
#include "World/MMOVendorNPC.h"
#include "World/MMOQuestGiverNPC.h"
#include "World/MMOTalkerNPC.h"
#include "World/MMOHealerNPC.h"
#include "World/MMOWarehouseNPC.h"
#include "UI/MMOHUD.h"
#include "Characters/MMOCharacter.h"
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
	// Varsay�lan HUD'u MMO HUD'�m�z yap.
	HUDClass = AMMOHUD::StaticClass();
	bUseSeamlessTravel = true;
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
		// Kesik kalip: tam 20 karakter + en az 4 hane kuyruk + kuyrukta en az bir rakam ("...-Fade"/"...-Dead" gibi
		// yalniz harf-hex kuyruklar mesru ad sayilir).
		const bool bTruncatedByEngine = Name.Len() == 20 && Suffix.Len() >= 4
			&& Suffix.FindLastCharByPredicate([](TCHAR C) { return FChar::IsDigit(C); }) != INDEX_NONE;
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
				bool bTaken = UGameplayStatics::DoesSaveGameExist(FString(TEXT("MMOSave_")) + Candidate, 0);
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
		|| Requester->GetWorld() != World || !Requester->IsLocalController())
	{
		OutFailureReason = TEXT("Gecis su anda kullanilamiyor");
		return false;
	}

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
			return false;
		}
		const FString Slot = Character->GetSaveSlotName();
		// Windows save dosyalari buyuk/kucuk harf ayirt etmez.
		const FString NormalizedSlot = Slot.ToLower();
		if (SaveSlots.Contains(NormalizedSlot))
		{
			UE_LOG(LogTemp, Error, TEXT("Zone travel rejected: duplicate save slot %s"), *Slot);
			OutFailureReason = TEXT("Iki oyuncunun kayit adi ayni");
			return false;
		}
		SaveSlots.Add(NormalizedSlot);
		Characters.Add(Character);
	}
	if (Characters.IsEmpty())
	{
		OutFailureReason = TEXT("Tasinacak oyuncu bulunamadi");
		return false;
	}

	bZoneTravelInProgress = true;
	for (AMMOCharacter* Character : Characters)
	{
		if (!Character->SaveCharacterForTravel())
		{
			UE_LOG(LogTemp, Error, TEXT("Zone travel aborted: failed to save %s"), *Character->GetSaveSlotName());
			OutFailureReason = TEXT("Bir karakter kaydedilemedi; gecis iptal edildi");
			bZoneTravelInProgress = false;
			return false;
		}
	}
	for (AMMOCharacter* Character : Characters)
	{
		Character->SetTravelSaveCommitted(true);
	}
	if (!World->ServerTravel(TargetMap))
	{
		for (AMMOCharacter* Character : Characters)
		{
			Character->SetTravelSaveCommitted(false);
		}
		bZoneTravelInProgress = false;
		UE_LOG(LogTemp, Error, TEXT("Zone travel failed to start: %s"), *TargetMap);
		OutFailureReason = TEXT("Harita gecisi baslatilamadi");
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("Zone travel started: %s -> %s (%d players)"),
		*CurrentMap, *TargetMap, Characters.Num());
	return true;
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
			WinterSpawner->MaxMobLevel = 48;
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
			TEXT("Kapidan gecen cok, geri donen az. Sen donenlerden ol.")
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

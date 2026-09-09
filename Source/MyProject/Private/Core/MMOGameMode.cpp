// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/MMOGameMode.h"
#include "Core/MMOMobSpawner.h"
#include "World/MMOVendorNPC.h"
#include "World/MMOQuestGiverNPC.h"
#include "World/MMOTalkerNPC.h"
#include "World/MMOHealerNPC.h"
#include "UI/MMOHUD.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "CollisionQueryParams.h"

namespace
{
	// Greybox prototip kasaba icin tek bir kutu blok spawn eder: olcekli, carpisan, renkli.
	// Runtime (StaticMeshActor Movable). NOT: yalnizca sunucuda (GameMode) spawn olur; listen
	// server host'unda ve tek-oyunculuda gorunur. Tam MP icin gercek harita LEVEL'a baked olacak.
	void SpawnTownBlock(UWorld* World, UStaticMesh* Cube, UMaterialInterface* BaseMat,
		const FVector& Center, const FVector& Scale, const FLinearColor& Color)
	{
		if (!World || !Cube)
		{
			return;
		}
		AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), FTransform(FRotator::ZeroRotator, Center));
		if (!A)
		{
			return;
		}
		A->SetMobility(EComponentMobility::Movable);
		if (UStaticMeshComponent* MC = A->GetStaticMeshComponent())
		{
			MC->SetStaticMesh(Cube);
			MC->SetWorldScale3D(Scale);
			MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			MC->SetCollisionProfileName(TEXT("BlockAll"));
			if (BaseMat)
			{
				if (UMaterialInstanceDynamic* MID = MC->CreateAndSetMaterialInstanceDynamic(0))
				{
					MID->SetVectorParameterValue(TEXT("Color"), Color);
				}
			}
		}
	}

	// PlayerStart cevresine greybox prototip kasaba: surlar + kapi (mob yonune, +X) + bina bloklari.
	// Zemin Z'si asagi line-trace ile bulunur (float/gomulme olmasin). Konumlar Origin'e goredir;
	// NPC hub'i (X in [-250,400], Y in [-200,200]) ve kapi koridoru (+X, |Y|<260) bos birakilir.
	void SpawnTownGeometry(UWorld* World, const FVector& Origin)
	{
		if (!World)
		{
			return;
		}
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!Cube)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MMODBG] Town: Cube mesh yuklenemedi, greybox atlaniyor"));
			return;
		}

		// Zemin Z: Origin'den asagi trace. Bulunamazsa Origin.Z kullan.
		float GroundZ = Origin.Z;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MMOTownGround), false);
		if (World->LineTraceSingleByChannel(Hit, Origin + FVector(0.f, 0.f, 300.f),
			Origin - FVector(0.f, 0.f, 3000.f), ECC_WorldStatic, Params))
		{
			GroundZ = Hit.Location.Z;
		}

		// Yardimci: (X,Y) Origin-goreli, olcek (Sx,Sy,Sz) => boyut 100*S; blok zemine oturur.
		auto Block = [&](float X, float Y, float Sx, float Sy, float Sz, const FLinearColor& C)
		{
			const float SizeZ = 100.f * Sz;
			const FVector Center(Origin.X + X, Origin.Y + Y, GroundZ + SizeZ * 0.5f);
			SpawnTownBlock(World, Cube, Mat, Center, FVector(Sx, Sy, Sz), C);
		};

		const FLinearColor WallCol(0.42f, 0.42f, 0.46f);
		const FLinearColor PostCol(0.28f, 0.26f, 0.24f);
		const FLinearColor House1(0.55f, 0.42f, 0.30f);
		const FLinearColor House2(0.48f, 0.40f, 0.34f);

		// --- Surlar: 2800x2800 kare (kalinlik 80, yukseklik 400) ---
		Block(0.f,     1400.f, 28.8f, 0.8f, 4.f, WallCol);  // kuzey (+Y)
		Block(0.f,    -1400.f, 28.8f, 0.8f, 4.f, WallCol);  // guney (-Y)
		Block(-1400.f, 0.f,    0.8f, 28.8f, 4.f, WallCol);  // bati (-X)
		// Dogu duvari (+X, mob yonu) kapi bosluklu: iki segment (bosluk Y in [-260,260]).
		Block(1400.f,  830.f,  0.8f, 11.4f, 4.f, WallCol);  // dogu-kuzey (Y 260..1400)
		Block(1400.f, -830.f,  0.8f, 11.4f, 4.f, WallCol);  // dogu-guney (Y -1400..-260)
		// Kapi direkleri (bosluk kenarlari), daha yuksek + koyu.
		Block(1400.f,  270.f,  1.2f, 1.2f, 6.5f, PostCol);
		Block(1400.f, -270.f,  1.2f, 1.2f, 6.5f, PostCol);

		// --- Bina bloklari (NPC hub'i ve kapi koridoru disinda) ---
		Block(-1050.f,  850.f, 6.f, 5.f, 5.f, House1);
		Block(-1050.f, -850.f, 6.f, 5.f, 6.f, House2);
		Block(-1050.f,    0.f, 5.f, 7.f, 5.f, House1);  // bati buyuk ev
		Block(  150.f, 1050.f, 7.f, 5.f, 5.f, House2);  // kuzey ev (NPC hub Y<=200'un uzerinde)
		Block(  150.f,-1050.f, 7.f, 5.f, 6.f, House1);  // guney ev
		Block(  950.f,  950.f, 5.f, 5.f, 7.f, House2);  // KD kule
		Block(  950.f, -950.f, 5.f, 5.f, 5.f, House1);  // GD ev
	}
}

AMMOGameMode::AMMOGameMode()
{
	// Varsay�lan HUD'u MMO HUD'�m�z yap.
	HUDClass = AMMOHUD::StaticClass();
}

void AMMOGameMode::StartPlay()
{
	Super::StartPlay();
	UE_LOG(LogTemp, Log, TEXT("[MMODBG] AMMOGameMode::StartPlay called"));
	SpawnMobArea();
}

void AMMOGameMode::SpawnMobArea()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Bir PlayerStart bul; yoksa orijini kullan.
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

	// Greybox prototip kasaba (surlar + kapi + binalar) — gercek harita gelene kadar iskelet.
	SpawnTownGeometry(World, Origin);

	// Oyuncunun baktigi yonde biraz ilerisine spawner koy (test icin yakin).
	// Ana spawner Lv1-5 normalleriyle sinirli (MaxMobLevel=5): yeni Lv6-7 moblar baslangic havuzuna sizmasin.
	const FVector SpawnerLoc = Origin + Facing.Vector() * MobAreaDistance + FVector(0.f, 0.f, 50.f);
	const FTransform SpawnerTM(FRotator::ZeroRotator, SpawnerLoc);
	AMMOMobSpawner* Spawner = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), SpawnerTM);
	if (Spawner)
	{
		Spawner->MaxMobLevel = 5;
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

	// Elit: Golge Suikastci (assassin) — diger elit/boss'lardan daha ileride, eksende uzak nokta.
	const FVector AssassinLoc = Origin + Facing.Vector() * (MobAreaDistance * 3.2f) + FVector(0.f, 0.f, 50.f);
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
	// Baslangictan belirgin biçimde uzakta ve yana kaydirilmis ayri bir alan: eksende MobAreaDistance*4 (2800u)
	// + sag yone 1600u. En yakin mevcut spawner (Golge Suikastci, eksende 3.2x) ile merkez arasi ~1695u (>600u).
	// ForcedArchetypeId YOK; MinMobLevel=6/MaxMobLevel=7 ile banddan rastgele Lv6-7 normal secilir.
	const FVector SecondZoneLoc = Origin + Facing.Vector() * (MobAreaDistance * 4.f) + RightDir * 1600.f + FVector(0.f, 0.f, 50.f);
	const FTransform SecondZoneTM(FRotator::ZeroRotator, SecondZoneLoc);
	if (AMMOMobSpawner* SecondZone = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), SecondZoneTM))
	{
		SecondZone->MaxMobs = 5;
		SecondZone->SpawnRadius = 500.f;
		SecondZone->MinMobLevel = 6;
		SecondZone->MaxMobLevel = 7;
		UGameplayStatics::FinishSpawningActor(SecondZone, SecondZoneTM);
		UE_LOG(LogTemp, Log, TEXT("[MMODBG] SecondZone spawner @ %s"), *SecondZoneLoc.ToCompactString());
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
	const FVector HealerLoc = Origin + FVector(-250.f, 200.f, 60.f);
	World->SpawnActor<AMMOHealerNPC>(AMMOHealerNPC::StaticClass(), HealerLoc, FRotator(0.f, 180.f, 0.f), NpcParams);
}

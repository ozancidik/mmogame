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
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"

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

	// Oyuncunun baktigi yonde biraz ilerisine spawner koy (test icin yakin).
	const FVector SpawnerLoc = Origin + Facing.Vector() * MobAreaDistance + FVector(0.f, 0.f, 50.f);

	AMMOMobSpawner* Spawner = World->SpawnActor<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), SpawnerLoc, FRotator::ZeroRotator);

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

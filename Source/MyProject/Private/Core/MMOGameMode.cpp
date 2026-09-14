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

	// Ana mob alani: NPC hub'inin (Kael +400u) belirgin disinda kalsin diye 1.5x mesafede
	// ve dar yaricapla (350) — moblar meydanda/NPC'lerin arasinda degil, disarida cikar.
	// Ana spawner Lv1-5 normalleriyle sinirli (MaxMobLevel=5): yeni Lv6-7 moblar baslangic havuzuna sizmasin.
	const FVector SpawnerLoc = Origin + Facing.Vector() * (MobAreaDistance * 1.5f) + FVector(0.f, 0.f, 50.f);
	const FTransform SpawnerTM(FRotator::ZeroRotator, SpawnerLoc);
	AMMOMobSpawner* Spawner = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), SpawnerTM);
	if (Spawner)
	{
		Spawner->MaxMobLevel = 5;
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

	// --- UCUNCU SAVAS BOLGESI (Lv8-12 NORMAL moblar) ---
	// Ikinci bolgenin AYNASI: eksende MobAreaDistance*4 (2800u) + SOL yone 1600u.
	// Mevcut spawner'lara merkez-merkez mesafeler (MobAreaDistance=700, F=Facing, R=RightDir):
	//   Ikinci bolge (F*2800 + R*1600) : 3200u
	//   Golge Suikastci (F*2240)       : ~1695u
	//   Kadim Golem (F*1120 - R*700)   : ~1906u
	//   Ork Reisi (F*1540)             : ~2037u
	//   Ana alan (F*1050)              : ~2371u
	//   Orumcek Ana (F*1120 + R*700)   : ~2848u
	// Hepsi >700u (QA kurali). ForcedArchetypeId YOK; band Lv8-12 normalleri secer.
	const FVector ThirdZoneLoc = Origin + Facing.Vector() * (MobAreaDistance * 4.f) - RightDir * 1600.f + FVector(0.f, 0.f, 50.f);
	const FTransform ThirdZoneTM(FRotator::ZeroRotator, ThirdZoneLoc);
	if (AMMOMobSpawner* ThirdZone = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), ThirdZoneTM))
	{
		ThirdZone->MaxMobs = 5;
		ThirdZone->SpawnRadius = 500.f;
		ThirdZone->MinMobLevel = 8;
		ThirdZone->MaxMobLevel = 12;
		UGameplayStatics::FinishSpawningActor(ThirdZone, ThirdZoneTM);
		UE_LOG(LogTemp, Log, TEXT("[MMODBG] ThirdZone spawner @ %s"), *ThirdZoneLoc.ToCompactString());
	}

	// Elit: Buz Lordu (Lv13, ucuncu bolge kademe-sonu) — eksende en uzak nokta (MobAreaDistance*6 = 4200u).
	// En yakin spawner'lara mesafe: Golge Suikastci ~1960u, Ikinci/Ucuncu bolge ~2126u (>700u).
	const FVector LichLoc = Origin + Facing.Vector() * (MobAreaDistance * 6.f) + FVector(0.f, 0.f, 50.f);
	const FTransform LichTM(FRotator::ZeroRotator, LichLoc);
	if (AMMOMobSpawner* LichSpawner = World->SpawnActorDeferred<AMMOMobSpawner>(AMMOMobSpawner::StaticClass(), LichTM))
	{
		LichSpawner->MaxMobs = 1;
		LichSpawner->SpawnRadius = 150.f;
		LichSpawner->TopUpInterval = 60.f;
		LichSpawner->ForcedArchetypeId = FName(TEXT("FrostLich"));
		UGameplayStatics::FinishSpawningActor(LichSpawner, LichTM);
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

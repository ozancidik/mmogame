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
}

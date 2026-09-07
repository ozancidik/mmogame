// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/MMOMob.h"
#include "Characters/MMOMobArchetype.h"
#include "Components/MMOStatsComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DamageEvents.h"
#include "World/MMOLootPickup.h"
#include "Items/MMOItemDatabase.h"
#include "Components/MMOQuestComponent.h"
#include "Components/MMOPartyComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

AMMOMob::AMMOMob()
{
	PrimaryActorTick.bCanEverTick = true;

	// Yerlestirilince/spawn olunca varsayilan AI controller onu ele gecirsin (hareket icin).
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Hareket yonune donsun.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;
		Move->MaxWalkSpeed = 300.f;
		Move->RotationRate = FRotator(0.f, 360.f, 0.f);
	}
	bUseControllerRotationYaw = false;

	// Gorunur govde: Manny mannequin + yurume animasyonu (oyuncu Quinn kullaniyor, ayrisir).
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
		if (MannyMesh.Succeeded())
		{
			MeshComp->SetSkeletalMesh(MannyMesh.Object);
		}

		static ConstructorHelpers::FClassFinder<UAnimInstance> UnarmedAnim(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
		if (UnarmedAnim.Succeeded())
		{
			MeshComp->SetAnimInstanceClass(UnarmedAnim.Class);
		}

		// Standart mannequin hizalamasi (BP_ThirdPersonCharacter ile ayni).
		MeshComp->SetRelativeLocation(FVector(0.f, 0.f, -89.f));
		MeshComp->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

		// Fiziksel carpisma yok ama fare imleci trace'i (Visibility) icin tiklanabilir olsun.
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
		MeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	// Warrok (Mixamo ork) mesh + animasyonlarini yukle (orc arketipleri kullanir).
	// AnimBP yok; single-node ile duruma gore oynatilir.
	{
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> WarrokMeshRef(TEXT("/Game/Mixamo/Warrok/Warrok.Warrok"));
		if (WarrokMeshRef.Succeeded()) { WarrokMesh = WarrokMeshRef.Object; }
		static ConstructorHelpers::FObjectFinder<UAnimSequence> WIdle(TEXT("/Game/Mixamo/Warrok/Anims/Warrok_Idle.Warrok_Idle"));
		if (WIdle.Succeeded()) { WarrokIdle = WIdle.Object; }
		static ConstructorHelpers::FObjectFinder<UAnimSequence> WRun(TEXT("/Game/Mixamo/Warrok/Anims/Warrok_Run.Warrok_Run"));
		if (WRun.Succeeded()) { WarrokRun = WRun.Object; }
		static ConstructorHelpers::FObjectFinder<UAnimSequence> WAtk(TEXT("/Game/Mixamo/Warrok/Anims/Warrok_Attack.Warrok_Attack"));
		if (WAtk.Succeeded()) { WarrokAttack = WAtk.Object; }
		static ConstructorHelpers::FObjectFinder<UAnimSequence> WDeath(TEXT("/Game/Mixamo/Warrok/Anims/Warrok_Death.Warrok_Death"));
		if (WDeath.Succeeded()) { WarrokDeath = WDeath.Object; }
	}

	// Moblar oyuncudan biraz daha zayif olsun (stat bileseni temel sinifta olusturuldu).
	if (Stats)
	{
		Stats->Strength = 8;
		Stats->Vitality = 5;
	}
}

void AMMOMob::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMMOMob, ArchetypeId);
	DOREPLIFETIME(AMMOMob, MobName);
	DOREPLIFETIME(AMMOMob, MobLevel);
}

void AMMOMob::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		// Spawn noktasini ev olarak kaydet (leash / eve donus icin).
		HomeLocation = GetActorLocation();

		// Arketip sec (belirtilmemisse rastgele) ve uygula.
		const FMMOMobArchetype* Arch = FMMOMobDatabase::Find(ArchetypeId);
		if (!Arch)
		{
			Arch = &FMMOMobDatabase::GetRandom();
		}
		ApplyArchetype(*Arch);
	}
	else
	{
		// Istemci: replike gelen arketiple gorseli uygula (henuz gelmediyse OnRep_Archetype halleder).
		if (const FMMOMobArchetype* Arch = FMMOMobDatabase::Find(ArchetypeId))
		{
			ApplyArchetypeVisuals(*Arch);
		}
		else
		{
			ApplyTint(FLinearColor(0.7f, 0.08f, 0.08f));
		}
	}
}

float AMMOMob::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// Saldirgan bir oyuncuysa onu zorla hedefle (uzaktan vurulunca da mob tepki versin).
	if (HasAuthority() && Stats && !Stats->bIsDead && !bReturningHome)
	{
		APawn* Attacker = Cast<APawn>(DamageCauser);
		if (!Attacker && EventInstigator)
		{
			Attacker = EventInstigator->GetPawn();
		}
		if (Attacker && Attacker->IsPlayerControlled())
		{
			ForcedTarget = Attacker;
			if (const UWorld* W = GetWorld())
			{
				ForcedAggroUntil = W->GetTimeSeconds() + ForcedAggroDuration;
			}
		}
	}

	return Applied;
}

void AMMOMob::OnRep_Archetype()
{
	if (const FMMOMobArchetype* Arch = FMMOMobDatabase::Find(ArchetypeId))
	{
		ApplyArchetypeVisuals(*Arch);
	}
}

void AMMOMob::ApplyArchetype(const FMMOMobArchetype& Arch)
{
	// Kimlik / gorunum bilgisi (replike olur).
	ArchetypeId = Arch.ArchetypeId;
	MobName = Arch.Name;
	MobLevel = Arch.Level;

	// Stat / davranis.
	AggroRange = Arch.AggroRange;
	MobAttackDamage = Arch.AttackDamage;
	XPReward = Arch.XPReward;
	MinGold = Arch.MinGold;
	MaxGold = Arch.MaxGold;

	// Yetenekler.
	bIsElite = Arch.bIsElite;
	OnHitEffectId = Arch.OnHitEffectId;
	OnHitEffectChance = Arch.OnHitEffectChance;
	EnrageEffectId = Arch.EnrageEffectId;
	EnrageHealthPct = Arch.EnrageHealthPct;
	bEnraged = false;

	// Elit ganimet ayarlari (default = normal loot).
	EliteEquipDropChance = Arch.EliteEquipDropChance;
	EliteEquipDropCount = Arch.EliteEquipDropCount;
	EliteMinPlus = Arch.EliteMinItemPlus;
	EliteMaxPlus = Arch.EliteMaxItemPlus;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = Arch.MoveSpeed;
	}

	if (Stats)
	{
		Stats->Level = Arch.Level;
		// Seviye/ana statlari uygula ve turetilmisleri (MaxHealth vb.) tam canla hesapla.
		Stats->InitializePrimaryStats(Arch.Strength, 10, 6, Arch.Vitality);
	}

	ApplyArchetypeVisuals(Arch);
}

void AMMOMob::ApplyArchetypeVisuals(const FMMOMobArchetype& Arch)
{
	SetActorScale3D(FVector(Arch.Scale));

	if (Arch.bUseWarrokMesh && WarrokMesh)
	{
		// Ork mesh'ine cevir: kendi materyalini kullan (tint YOK) + AnimBP'yi birak,
		// single-node animasyona gec. Durum animasyonu Tick/UpdateWarrokAnimation ile surer.
		bWarrokVisual = true;
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			MeshComp->SetSkeletalMesh(WarrokMesh);
			MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			CurrentWarrokAnim = nullptr;
			if (WarrokIdle)
			{
				MeshComp->PlayAnimation(WarrokIdle, /*bLooping=*/true);
				CurrentWarrokAnim = WarrokIdle;
			}
		}
		return;
	}

	// Varsayilan: tint'li Manny.
	bWarrokVisual = false;
	ApplyTint(Arch.Tint);
}

void AMMOMob::UpdateWarrokAnimation()
{
	if (!bWarrokVisual)
	{
		return;
	}
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	const bool bDead = (Stats && Stats->bIsDead);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	// Olum her seyi ezer (tek sefer, dongusuz; son karede kalir).
	if (bDead)
	{
		if (WarrokDeath && CurrentWarrokAnim != WarrokDeath)
		{
			CurrentWarrokAnim = WarrokDeath;
			MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			MeshComp->PlayAnimation(WarrokDeath, /*bLooping=*/false);
		}
		return;
	}

	// Saldiri animasyonu oynuyorsa bitene kadar durum animasyonunu ezme.
	if (Now < WarrokAttackAnimUntil)
	{
		return;
	}

	// Hiz'a gore idle/kosu (hareket tum makinelerde replike; yerel velocity yeterli).
	UAnimSequence* Desired = (GetVelocity().Size2D() > 12.f) ? WarrokRun : WarrokIdle;
	if (Desired && Desired != CurrentWarrokAnim)
	{
		CurrentWarrokAnim = Desired;
		MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		MeshComp->PlayAnimation(Desired, /*bLooping=*/true);
	}
}

void AMMOMob::MulticastPlayWarrokAttack_Implementation()
{
	if (!bWarrokVisual || !WarrokAttack)
	{
		return;
	}
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}
	MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	MeshComp->PlayAnimation(WarrokAttack, /*bLooping=*/false);
	CurrentWarrokAnim = nullptr; // saldiri bitince durum animasyonu yeniden secilsin
	const float Len = WarrokAttack->GetPlayLength();
	WarrokAttackAnimUntil = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f) + Len;
}

void AMMOMob::ApplyTint(const FLinearColor& Color)
{
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		const int32 NumMats = MeshComp->GetNumMaterials();
		for (int32 i = 0; i < NumMats; ++i)
		{
			if (UMaterialInstanceDynamic* MID = MeshComp->CreateAndSetMaterialInstanceDynamic(i))
			{
				MID->SetVectorParameterValue(TEXT("Tint"), Color);
				MID->SetVectorParameterValue(TEXT("Color"), Color);
			}
		}
	}
}

void AMMOMob::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Warrok animasyonu tum makinelerde (istemci dahil) ve olu iken de guncellenmeli.
	UpdateWarrokAnimation();

	if (!HasAuthority() || !Stats || Stats->bIsDead)
	{
		return;
	}

	// Enrage: can esigin altina dusunce mob bir defaya mahsus kendine buff (or. Ofke) uygular.
	if (!bEnraged && EnrageHealthPct > 0.f && !EnrageEffectId.IsNone()
		&& Stats->GetHealthPercent() <= EnrageHealthPct)
	{
		bEnraged = true;
		Stats->ApplyEffect(EnrageEffectId, this);
	}

	AAIController* AICtrl = Cast<AAIController>(GetController());
	UWorld* TheWorld = GetWorld();
	const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(TheWorld);
	const bool bHasNavmesh = AICtrl && Nav && Nav->GetMainNavData() != nullptr;
	const float DistToHome = FVector::Dist(GetActorLocation(), HomeLocation);

	// --- Leash / eve donus ---
	if (bReturningHome)
	{
		// Eve varinca kovalamayi birak ve tam canlan (klasik MMO reset).
		if (DistToHome < 140.f)
		{
			bReturningHome = false;
			bEnraged = false;
			Stats->ClearAllEffects();
			Stats->Heal(Stats->MaxHealth);
		}
		else if (bHasNavmesh)
		{
			const float NowH = TheWorld->GetTimeSeconds();
			if (NowH - LastRepathTime > 0.35f)
			{
				LastRepathTime = NowH;
				AICtrl->MoveToLocation(HomeLocation, 100.f, /*bStopOnOverlap=*/true, /*bUsePathfinding=*/true);
			}
		}
		else
		{
			const FVector Dir = (HomeLocation - GetActorLocation()).GetSafeNormal2D();
			AddMovementInput(Dir, 1.f);
		}
		return;
	}

	// Evden cok uzaklastiysa kovalamayi birak, eve don.
	if (DistToHome > LeashRange)
	{
		bReturningHome = true;
		if (AICtrl)
		{
			AICtrl->StopMovement();
		}
		return;
	}

	// En yakin CANLI oyuncuyu bul (MP: birden fazla oyuncu olabilir).
	APawn* Player = nullptr;
	float Dist = TNumericLimits<float>::Max();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		APawn* Candidate = PC ? PC->GetPawn() : nullptr;
		if (!Candidate)
		{
			continue;
		}
		if (const AMMOCharacter* PChar = Cast<AMMOCharacter>(Candidate))
		{
			if (PChar->GetStats() && PChar->GetStats()->bIsDead)
			{
				continue; // olu oyuncuya saldirma
			}
		}
		const float D = FVector::Dist(GetActorLocation(), Candidate->GetActorLocation());
		if (D < Dist)
		{
			Dist = D;
			Player = Candidate;
		}
	}

	// Zorla hedef (hasar alindiginda ayarlanir) aktifse onu kullan; AggroRange sinirini asar.
	const float NowSel = TheWorld ? TheWorld->GetTimeSeconds() : 0.f;
	APawn* Forced = ForcedTarget.Get();
	bool bForcedActive = false;
	if (Forced && NowSel < ForcedAggroUntil)
	{
		if (const AMMOCharacter* FC = Cast<AMMOCharacter>(Forced))
		{
			bForcedActive = FC->GetStats() && !FC->GetStats()->bIsDead;
		}
	}

	if (bForcedActive)
	{
		Player = Forced;
		Dist = FVector::Dist(GetActorLocation(), Forced->GetActorLocation());
	}
	else if (!Player || Dist > AggroRange)
	{
		// Menzilde dusman yok: ev cevresinde rastgele gezin (dunya canli dursun).
		IdleWander();
		return;
	}

	AAIController* AI = AICtrl;
	UWorld* World = TheWorld;

	if (Dist > AttackRange)
	{
		// Navmesh varsa yol bulma (engelleri asar); yoksa duz-cizgi harekete dus (fallback).
		const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		const bool bHasNav = AI && NavSys && NavSys->GetMainNavData() != nullptr;

		if (bHasNav)
		{
			const float Now = World->GetTimeSeconds();
			// Sik sik yeniden yol cizmeyi engelle (hareketli hedefi periyodik takip et).
			if (Now - LastRepathTime > 0.35f)
			{
				LastRepathTime = Now;
				AI->MoveToActor(Player, AttackRange * 0.8f, /*bStopOnOverlap=*/true, /*bUsePathfinding=*/true);
			}
		}
		else
		{
			const FVector Dir = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			AddMovementInput(Dir, 1.f);
		}
	}
	else
	{
		// Menzilde: yol bulmayi durdur, oyuncuya don ve periyodik vur.
		if (AI)
		{
			AI->StopMovement();
		}

		const FVector Dir = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (!Dir.IsNearlyZero())
		{
			SetActorRotation(Dir.Rotation());
		}

		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - LastMobAttackTime >= MobAttackCooldown)
		{
			LastMobAttackTime = Now;
			UGameplayStatics::ApplyDamage(Player, MobAttackDamage, GetController(), this, nullptr);

			// Mob da vururken animasyon oynatsin (oyuncuyla ayni Mannequin animasyon seti).
			MulticastMeleeFX(Player->GetActorLocation() + FVector(0.f, 0.f, 40.f), FMath::RandRange(0, 2));
			// Warrok mob'lari (ork) icin kendi saldiri animasyonu (single-node, tum makinelerde).
			MulticastPlayWarrokAttack();

			// Vurusta sansa bagli debuff uygula (or. zehir tukuren mob, yavaslatan orumcek).
			if (!OnHitEffectId.IsNone() && OnHitEffectChance > 0.f && FMath::FRand() < OnHitEffectChance)
			{
				if (AMMOCharacter* PChar = Cast<AMMOCharacter>(Player))
				{
					if (PChar->GetStats() && !PChar->GetStats()->bIsDead)
					{
						PChar->GetStats()->ApplyEffect(OnHitEffectId, this);
					}
				}
			}
		}
	}
}

void AMMOMob::IdleWander()
{
	UWorld* World = GetWorld();
	AAIController* AI = Cast<AAIController>(GetController());
	if (!World || !AI)
	{
		return;
	}

	// Navmesh yoksa gezinme (duz-cizgi surukleme istemiyoruz).
	const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Nav || !Nav->GetMainNavData())
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (Now < NextWanderTime)
	{
		return; // hedefe dogru gitmeye devam / bekle
	}
	NextWanderTime = Now + FMath::FRandRange(4.f, 8.f);

	// Ev cevresinde rastgele bir noktaya yuru.
	const float Ang = FMath::FRandRange(0.f, 2.f * PI);
	const float Rad = FMath::FRandRange(0.f, WanderRadius);
	const FVector Dest = HomeLocation + FVector(FMath::Cos(Ang) * Rad, FMath::Sin(Ang) * Rad, 0.f);
	AI->MoveToLocation(Dest, 40.f, /*bStopOnOverlap=*/true, /*bUsePathfinding=*/true);
}

void AMMOMob::HandleDeath()
{
	Super::HandleDeath(); // Blueprint OnCharacterDied olayini tetikler.

	if (bDeathHandled)
	{
		return;
	}
	bDeathHandled = true;

	// Olduren oyuncuya (ve partisi varsa menzildeki uyelere) XP + gorev ilerlemesi ver.
	if (AMMOCharacter* Killer = Cast<AMMOCharacter>(LastDamageInstigator.Get()))
	{
		const float PartyXPShareRange = 3000.f;

		TArray<AMMOCharacter*> Recipients;
		if (UMMOPartyComponent* Party = Killer->GetPartyComponent())
		{
			if (Party->IsInParty())
			{
				Party->GetPartyInRange(GetActorLocation(), PartyXPShareRange, Recipients);
			}
		}
		Recipients.AddUnique(Killer); // olduren her zaman kredi alir

		for (AMMOCharacter* R : Recipients)
		{
			if (!R)
			{
				continue;
			}
			if (R->GetStats())
			{
				R->GetStats()->GainExperience(XPReward);
			}
			if (R->GetQuestComponent())
			{
				R->GetQuestComponent()->NotifyMobKilled(ArchetypeId);
			}
		}
	}

	// Loot dusur (altin + sansa bagli item).
	if (UWorld* World = GetWorld())
	{
		TSubclassOf<AMMOLootPickup> LootClass = LootPickupClass;
		if (!LootClass)
		{
			LootClass = AMMOLootPickup::StaticClass();
		}
		const FVector Base = GetActorLocation();

		// Altin
		{
			const FVector Loc = Base + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), -40.f);
			const FTransform SpawnTM(FRotator::ZeroRotator, Loc);
			if (AMMOLootPickup* P = World->SpawnActorDeferred<AMMOLootPickup>(LootClass, SpawnTM))
			{
				P->SetupAsGold(FMath::RandRange(MinGold, MaxGold));
				UGameplayStatics::FinishSpawningActor(P, SpawnTM);
			}
		}

		// Sansa bagli item
		if (FMath::FRand() < ItemDropChance)
		{
			const FVector Loc = Base + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), -40.f);
			const FTransform SpawnTM(FRotator::ZeroRotator, Loc);
			if (AMMOLootPickup* P = World->SpawnActorDeferred<AMMOLootPickup>(LootClass, SpawnTM))
			{
				P->SetupAsItem(DropItemId, 1);
				UGameplayStatics::FinishSpawningActor(P, SpawnTM);
			}
		}

		// Sansa bagli EKIPMAN. Normal mob: mevcut davranis (rastgele item, kucuk sansla +1).
		// Elit (EliteEquipDropChance>0): garantili/yuksek sans, seviyeye uygun ust-tier havuz,
		// arketip +N araligi ve (elit oldugu icin) iki nadirlik zarindan iyisi. Boss birden fazla dusurebilir.
		const bool bEliteLoot = bIsElite && EliteEquipDropChance > 0.f;
		const float EquipChance = bEliteLoot ? EliteEquipDropChance : EquipDropChance;
		const int32 EquipDropCount = bEliteLoot ? FMath::Max(1, EliteEquipDropCount) : 1;

		const TArray<FMMOItemDef>& AllItems = FMMOItemDatabase::GetAll();

		for (int32 DropIdx = 0; DropIdx < EquipDropCount; ++DropIdx)
		{
			if (AllItems.Num() == 0 || FMath::FRand() >= EquipChance)
			{
				continue;
			}

			// Item sec: elit ise mob seviyesine uygun (RequiredLevel <= MobLevel) ust-tier bandindan,
			// aksi halde tum havuzdan rastgele (mevcut davranis).
			const FMMOItemDef* Picked = nullptr;
			if (bEliteLoot)
			{
				int32 MaxReq = 0;
				for (const FMMOItemDef& It : AllItems)
				{
					if (It.RequiredLevel <= MobLevel)
					{
						MaxReq = FMath::Max(MaxReq, It.RequiredLevel);
					}
				}
				TArray<const FMMOItemDef*> TopTier;
				for (const FMMOItemDef& It : AllItems)
				{
					if (It.RequiredLevel <= MobLevel && It.RequiredLevel >= MaxReq - 2)
					{
						TopTier.Add(&It);
					}
				}
				if (TopTier.Num() > 0)
				{
					Picked = TopTier[FMath::RandRange(0, TopTier.Num() - 1)];
				}
			}
			if (!Picked)
			{
				Picked = &AllItems[FMath::RandRange(0, AllItems.Num() - 1)];
			}

			// Plus: elitse arketip araligindan, degilse mevcut kucuk sans (+1).
			int32 Plus;
			if (bEliteLoot)
			{
				const int32 MinP = FMath::Max(0, EliteMinPlus);
				const int32 MaxP = FMath::Clamp(FMath::Max(MinP, EliteMaxPlus), MinP, FMMOItemDatabase::MaxPlus);
				Plus = FMath::RandRange(MinP, MaxP);
			}
			else
			{
				Plus = (FMath::FRand() < 0.15f) ? 1 : 0;
			}

			// Nadirlik zar at (elit/boss daha iyi loot: bir kez daha atip iyisini sec).
			EMMOItemRarity Rarity = FMMOItemDatabase::RollRarity();
			if (bIsElite)
			{
				const EMMOItemRarity Second = FMMOItemDatabase::RollRarity();
				if ((uint8)Second > (uint8)Rarity)
				{
					Rarity = Second;
				}
			}

			const FVector Loc = Base + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), -40.f);
			const FTransform SpawnTM(FRotator::ZeroRotator, Loc);
			if (AMMOLootPickup* P = World->SpawnActorDeferred<AMMOLootPickup>(LootClass, SpawnTM))
			{
				P->SetupAsEquipment(Picked->ItemId, Plus, Rarity);
				UGameplayStatics::FinishSpawningActor(P, SpawnTM);
			}
		}
	}

	// Hareketi ve carpismayi durdur, govdeyi gizle.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		// Warrok (ork) mob'lari: mesh'i gizleme; olum animasyonu yok edilene kadar (2sn)
		// gorunsun. Digerleri (Manny) eskisi gibi aninda gizlenir.
		if (!bWarrokVisual)
		{
			MeshComp->SetVisibility(false);
		}
	}

	// Kisa gecikmeyle yok et; spawner top-up ile yerine yenisini uretir.
	FTimerHandle DestroyTimer;
	GetWorldTimerManager().SetTimer(DestroyTimer, this, &AMMOMob::DestroySelf, 2.0f, false);
}

void AMMOMob::DestroySelf()
{
	Destroy();
}

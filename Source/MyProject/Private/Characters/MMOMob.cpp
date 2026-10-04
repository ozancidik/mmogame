// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/MMOMob.h"
#include "Core/MMOMetrics.h"
#include "Characters/MMOMobArchetype.h"
#include "Components/MMOStatsComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DamageEvents.h"
#include "World/MMOLootPickup.h"
#include "Items/MMOItemDatabase.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOSkillComponent.h"
#include "Components/MMOQuestComponent.h"
#include "Components/MMOPartyComponent.h"
#include "Components/MMOTradeComponent.h"
#include "Components/MMOClanComponent.h"
#include "Components/MMOFriendsComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/Engine.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/PlayerState.h"
#include "Animation/AnimSequence.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UI/MMOHUD.h"
#include "GameFramework/PlayerController.h"

AMMOMob::AMMOMob()
{
	PrimaryActorTick.bCanEverTick = true;

	// Yerlestirilince/spawn olunca varsayilan AI controller onu ele gecirsin (hareket icin).
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Mob oyuncu ticaret/klan bilesenlerini tasimaz (QA): gereksiz replike alt nesneleri kapat.
	if (GetTradeComponent()) { GetTradeComponent()->SetIsReplicated(false); }
	if (GetClanComponent()) { GetClanComponent()->SetIsReplicated(false); }
	if (GetFriendsComponent()) { GetFriendsComponent()->SetIsReplicated(false); }

	// NETWORK_AUDIT Y5: moblarda Inventory/Skill/Quest/Party bilesenleri de replike edilmez.
	// Bunlar yalniz sunucuda kullanilir (mob becerisi/loot sunucuda calisir; istemci mobun
	// envanterini/gorevini/partisini okumaz). Sadece replikasyon kapanir, sunucu mantigi aynidir.
	// Stats replike kalir (can cubugu, olum, seviye, efektler).
	if (GetInventory()) { GetInventory()->SetIsReplicated(false); }
	if (GetSkillComponent()) { GetSkillComponent()->SetIsReplicated(false); }
	if (GetQuestComponent()) { GetQuestComponent()->SetIsReplicated(false); }
	if (GetPartyComponent()) { GetPartyComponent()->SetIsReplicated(false); }

	// Mob aktoru dusuk frekansla replike edilir: oyuncu basina 45-55 mob her karede ~100 Hz
	// guncellenirse bant bosa gider (motor varsayilani NetUpdateFrequency ~100). Hareket
	// yumusatmasi istemcide yapildigi icin 30 Hz yeterli; hareketsiz moblar MinNetUpdateFrequency
	// (10 Hz) tabanina duser. Cull mesafesi AggroRange'in (~1200uu) cok ustunde ama harita
	// genisliginden (~17000uu) kucuk: uzaktaki moblar ilgili listesinden duser.
	// DOGRULANMADI: degerler PIE/Network Profiler ile olculmedi; "stat net" ile ayarlanmali.
	SetNetUpdateFrequency(30.f);
	SetMinNetUpdateFrequency(10.f);
	SetNetCullDistanceSquared(12000.f * 12000.f);

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

	// Ozel yaratik mesh'i + animasyonlari artik SABIT degil: arketip verisinden (MeshPath /
	// Anim*Path) ApplyArchetypeVisuals icinde on-demand yuklenir. Boylece yeni yaratik saf
	// veri satiri olarak eklenebilir (kod degismeden).

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

		// Hub (PlayerStart) konumunu bul: guvenli bolge merkezi.
		for (TActorIterator<APlayerStart> HubIt(GetWorld()); HubIt; ++HubIt)
		{
			HubLocation = HubIt->GetActorLocation();
			bHasHub = true;
			break;
		}

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
	// Leash sirasinda (eve donerken) hasara baglisiklik: mob vurulup can kaybedebiliyordu
	// ama saldirganini hic hedeflemiyordu (!bReturningHome guard'i asagida) — risksiz
	// "bedava hasar" penceresi aciyordu (menzilli sinif uzaktan vurup mob eve varana kadar
	// bedava DPS alabiliyordu). Standart MMO leash-reset davranisi: eve donerken dokunulmazlik.
	if (bReturningHome)
	{
		return 0.f;
	}
	APawn* Attacker = Cast<APawn>(DamageCauser);
	if (!Attacker && EventInstigator)
	{
		Attacker = EventInstigator->GetPawn();
	}
	// Hub'da mob oyuncuya vuramiyorsa oyuncu da hub'dan menzilli hasar/DoT
	// verip karsiliksiz XP veya ganimet toplayamamali.
	if (Attacker && Attacker->IsPlayerControlled() && bHasHub && HubSafeRadius > 0.f
		&& FVector::Dist2D(Attacker->GetActorLocation(), HubLocation) < HubSafeRadius)
	{
		return 0.f;
	}

	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// Hasar katkisi: oyuncu saldirganlarin mobda biraktigi toplam hasar (oldurme kredisi icin).
	if (HasAuthority() && Applied > 0.f)
	{
		if (AMMOCharacter* AttackerChar = Cast<AMMOCharacter>(Attacker); AttackerChar && Attacker->IsPlayerControlled())
		{
			DamageContributions.FindOrAdd(AttackerChar) += Applied;
		}
	}

	// Saldirgan bir oyuncuysa onu zorla hedefle (uzaktan vurulunca da mob tepki versin).
	if (HasAuthority() && Applied > 0.f && Stats && !Stats->bIsDead && !bReturningHome)
	{
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
		// KRITIK: MaxWalkSpeed'i az once arketipe gore degistirdik; Stats bunu
		// yeniden cache'lemezse (BeginPlay'de ONCEKI/varsayilan hizi cache'lemisti)
		// bir sonraki hiz-efekti (Slow/Haste) devreye girip biterken mob hizi bu
		// arketipe-ozgu degeri degil, eski varsayilani taban alir (gozle gorulur
		// hiz sicramasi/kalici yavaslama bug'i).
		Stats->RefreshBaseWalkSpeed();

		Stats->Level = Arch.Level;
		// Seviye/ana statlari uygula ve turetilmisleri (MaxHealth vb.) tam canla hesapla.
		Stats->InitializePrimaryStats(Arch.Strength, 10, 6, Arch.Vitality);
	}

	ApplyArchetypeVisuals(Arch);
}

void AMMOMob::ApplyArchetypeVisuals(const FMMOMobArchetype& Arch)
{
	// Olcek: temel Scale * ozel-mesh carpani (Manny/tint icin carpan default 1 => degismez).
	SetActorScale3D(FVector(Arch.Scale * Arch.MeshScaleMultiplier));

	if (Arch.UsesCustomMesh())
	{
		// Mesh'i arketip verisinden on-demand yukle (statik C++ tabloda asset ref tutulmaz).
		USkeletalMesh* LoadedMesh = LoadObject<USkeletalMesh>(nullptr, *Arch.MeshPath);
		if (LoadedMesh)
		{
			// Ozel mesh'e cevir: kendi materyalini kullan (tint YOK) + AnimBP'yi birak,
			// single-node animasyona gec. Durum animasyonu Tick/UpdateCreatureAnimation ile surer.
			bCustomMeshVisual = true;

			// Anim setini arketip verisinden yukle (bos path => o durum atlanir, guvenli).
			AnimIdle = Arch.AnimIdlePath.IsEmpty() ? nullptr : LoadObject<UAnimSequence>(nullptr, *Arch.AnimIdlePath);
			AnimRun = Arch.AnimRunPath.IsEmpty() ? nullptr : LoadObject<UAnimSequence>(nullptr, *Arch.AnimRunPath);
			AnimAttack = Arch.AnimAttackPath.IsEmpty() ? nullptr : LoadObject<UAnimSequence>(nullptr, *Arch.AnimAttackPath);
			AnimDeath = Arch.AnimDeathPath.IsEmpty() ? nullptr : LoadObject<UAnimSequence>(nullptr, *Arch.AnimDeathPath);

			if (USkeletalMeshComponent* MeshComp = GetMesh())
			{
				MeshComp->SetSkeletalMesh(LoadedMesh);
				// Arketip-basina ayarlanabilir hizalama (ayak ofseti + bakis yonu duzeltmesi).
				MeshComp->SetRelativeLocation(FVector(0.f, 0.f, Arch.MeshOffsetZ));
				MeshComp->SetRelativeRotation(FRotator(0.f, Arch.MeshYaw, 0.f));
				MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
				CurrentAnim = nullptr;
				if (AnimIdle)
				{
					MeshComp->PlayAnimation(AnimIdle, /*bLooping=*/true);
					CurrentAnim = AnimIdle;
				}
			}
			return;
		}

		// Yukleme basarisiz (path yanlis/asset yok): guvenle tint'li Manny'ye dus.
		UE_LOG(LogTemp, Warning, TEXT("AMMOMob: '%s' mesh yuklenemedi (%s), tint'li Manny'ye dusuluyor."),
			*Arch.Name, *Arch.MeshPath);
	}

	// Varsayilan: tint'li Manny.
	bCustomMeshVisual = false;
	ApplyTint(Arch.Tint);
}

void AMMOMob::UpdateCreatureAnimation()
{
	if (!bCustomMeshVisual)
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
		if (AnimDeath && CurrentAnim != AnimDeath)
		{
			CurrentAnim = AnimDeath;
			MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			MeshComp->PlayAnimation(AnimDeath, /*bLooping=*/false);
		}
		return;
	}

	// Saldiri animasyonu oynuyorsa bitene kadar durum animasyonunu ezme.
	if (Now < AttackAnimUntil)
	{
		return;
	}

	// Hiz'a gore idle/kosu (hareket tum makinelerde replike; yerel velocity yeterli).
	UAnimSequence* Desired = (GetVelocity().Size2D() > 12.f) ? AnimRun : AnimIdle;
	if (Desired && Desired != CurrentAnim)
	{
		CurrentAnim = Desired;
		MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		MeshComp->PlayAnimation(Desired, /*bLooping=*/true);
	}
}

void AMMOMob::MulticastPlayCreatureAttack_Implementation()
{
	if (!bCustomMeshVisual || !AnimAttack)
	{
		return;
	}
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}
	MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	MeshComp->PlayAnimation(AnimAttack, /*bLooping=*/false);
	CurrentAnim = nullptr; // saldiri bitince durum animasyonu yeniden secilsin
	const float Len = AnimAttack->GetPlayLength();
	AttackAnimUntil = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f) + Len;
}

void AMMOMob::MulticastEliteDeathAnnounce_Implementation(const FString& MobDisplayName, const FString& KillerDisplayName)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor(255, 160, 40),
			FString::Printf(TEXT("[DUNYA] %s, %s tarafindan devrildi!"), *MobDisplayName, *KillerDisplayName));
	}
	// Sohbet kaydina da sistem satiri olarak dus (debug mesajlari paketli oyunda gorunmez).
	if (APlayerController* LocalPC = GEngine ? GEngine->GetFirstLocalPlayerController(GetWorld()) : nullptr)
	{
		if (AMMOHUD* HUD = Cast<AMMOHUD>(LocalPC->GetHUD()))
		{
			HUD->AddChatLine(FString::Printf(TEXT("[Dunya] %s, %s tarafindan devrildi!"), *MobDisplayName, *KillerDisplayName),
				FLinearColor(1.f, 0.65f, 0.2f));
		}
	}
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

	// Ozel yaratik animasyonu tum makinelerde (istemci dahil) ve olu iken de guncellenmeli.
	UpdateCreatureAnimation();

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
			DamageContributions.Reset(); // tam canlandi: onceki hasar katkilari gecersiz
			// Zorla-hedefleme'yi de temizle: yoksa ForcedAggroDuration (8sn) dolmadiysa
			// (donus yolu genelde daha kisa surdugu icin) mob eve varir varmaz kalan
			// sure boyunca AggroRange'i yok sayip oyuncuya tekrar kosuyordu (yo-yo).
			ForcedTarget.Reset();
			ForcedAggroUntil = -1.f;
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
	bool bSkippedProtected = false;
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
			if (PChar->IsSpawnProtected())
			{
				bSkippedProtected = true;
				continue; // canlanma korumasindaki oyuncuyu hedef alma
			}
		}
		if (bHasHub && HubSafeRadius > 0.f
			&& FVector::Dist2D(Candidate->GetActorLocation(), HubLocation) < HubSafeRadius)
		{
			bSkippedProtected = true;
			continue; // hub guvenli bolgesindeki oyuncuya saldirma
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
			bForcedActive = FC->GetStats() && !FC->GetStats()->bIsDead && !FC->IsSpawnProtected();
		}
	}

	if (bForcedActive && bHasHub && HubSafeRadius > 0.f
		&& FVector::Dist2D(Forced->GetActorLocation(), HubLocation) < HubSafeRadius)
	{
		bForcedActive = false; // guvenli bolgeye siginan oyuncuyu kovalamayi birak
		bSkippedProtected = true;
	}

	if (bForcedActive)
	{
		Player = Forced;
		Dist = FVector::Dist(GetActorLocation(), Forced->GetActorLocation());
	}
	else if (!Player || Dist > AggroRange)
	{
		// Korumali oyuncu yuzunden hedef birakildiysa eski MoveToActor komutunu durdur
		// (yoksa mob oyuncunun dibine gelip bekler, koruma bitince hemen vurur).
		if (bSkippedProtected && AICtrl)
		{
			AICtrl->StopMovement();
		}
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
			// QA MEDIUM-4: Enrage (Rage = AttackBuff) sadece AttackPower'a ekleniyordu, mob
			// vurusu ise sabit MobAttackDamage kullaniyordu -> ofke hasari hic artirmiyordu.
			const float BuffBonus = Stats ? Stats->SumEffectMagnitude(EMMOEffectType::AttackBuff) : 0.f;
			const float HitDamage = FMath::Max(1.f, MobAttackDamage + BuffBonus);
			UGameplayStatics::ApplyDamage(Player, HitDamage, GetController(), this, nullptr);

			// Mob da vururken animasyon oynatsin (oyuncuyla ayni Mannequin animasyon seti).
			MulticastMeleeFX(Player->GetActorLocation() + FVector(0.f, 0.f, 40.f), FMath::RandRange(0, 2));
			// Ozel mesh'li mob'lar (or. Warrok ork) icin kendi saldiri animasyonu (single-node, tum makinelerde).
			MulticastPlayCreatureAttack();

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

int32 AMMOMob::ScaleXPForLevelGap(int32 BaseXP, int32 PlayerLevel, int32 InMobLevel)
{
	const int32 GrayGap = PlayerLevel - InMobLevel - 5;
	const int32 LeechGap = InMobLevel - PlayerLevel - 10;
	float Mult = 1.f;
	if (GrayGap > 0)
	{
		Mult = FMath::Max(0.1f, 1.f - 0.1f * GrayGap);
	}
	else if (LeechGap > 0)
	{
		Mult = FMath::Max(0.2f, 1.f - 0.1f * LeechGap);
	}
	return FMath::Max(1, FMath::RoundToInt(BaseXP * Mult));
}

void AMMOMob::HandleDeath()
{
	Super::HandleDeath(); // Blueprint OnCharacterDied olayini tetikler.

	if (bDeathHandled)
	{
		return;
	}
	bDeathHandled = true;

	// XP/loot/yok-etme YALNIZCA sunucuda. Bu fonksiyon istemcide de calisir
	// (bIsDead replike -> OnRep_IsDead -> OnDeath.Broadcast -> HandleDeath), o yuzden
	// authority guard'i olmadan uzak istemcide hayalet loot ve istemci-Destroy olusur.

	// Olduren oyuncuya (ve partisi varsa menzildeki uyelere) XP + gorev ilerlemesi ver.
	if (HasAuthority())
	if (AMMOCharacter* Killer = Cast<AMMOCharacter>(LastDamageInstigator.Get()))
	{
		const float PartyXPShareRange = 3000.f;

		// XP payi: oyuncu basina verilen hasarin toplam hasara orani (%1 hasar = %1 XP). Parti tek birim
		// sayilir: partinin toplam hasar payi parti UYE SAYISINA bolunur (8 kisilik partide herkes payin
		// 1/8'ini alir); payi alan uyeler menzildeki (3000u) uyelerdir. Gorev ilerlemesi (kredi) paya bagli
		// degildir: son vurusu yapan ve payi olan herkes tam kredi alir.
		TArray<TPair<AMMOCharacter*, float>> Shares;
		auto AddShare = [&Shares](AMMOCharacter* C, float Share)
		{
			for (TPair<AMMOCharacter*, float>& E : Shares)
			{
				if (E.Key == C)
				{
					E.Value = FMath::Max(E.Value, Share);
					return;
				}
			}
			Shares.Add(TPair<AMMOCharacter*, float>(C, Share));
		};

		float TotalDamage = 0.f;
		for (const TPair<TWeakObjectPtr<AMMOCharacter>, float>& C : DamageContributions)
		{
			TotalDamage += C.Value; // baglantisi kopan/gecersiz katki sahiplerinin payi da toplama girer (pay kaybolur)
		}

		if (TotalDamage <= 0.f)
		{
			// Katki kaydi yok (olum oyuncu hasari disindan geldi): olduren (veya partisi) tum payi alir;
			// parti ise pay uye sayisina bolunur.
			float KillerShare = 1.f;
			if (UMMOPartyComponent* Party = Killer->GetPartyComponent())
			{
				if (Party->IsInParty())
				{
					TArray<AMMOCharacter*> AllMembers;
					Party->GetPartyInRange(GetActorLocation(), 1.0e9f, AllMembers);
					KillerShare = 1.f / FMath::Max(1, AllMembers.Num());
					TArray<AMMOCharacter*> InRange;
					Party->GetPartyInRange(GetActorLocation(), PartyXPShareRange, InRange);
					for (AMMOCharacter* M : InRange)
					{
						AddShare(M, KillerShare);
					}
				}
			}
			AddShare(Killer, KillerShare);
		}
		else
		{
			TArray<AMMOCharacter*> Assigned;
			for (const TPair<TWeakObjectPtr<AMMOCharacter>, float>& C : DamageContributions)
			{
				AMMOCharacter* Who = C.Key.Get();
				if (!Who || Assigned.Contains(Who))
				{
					continue;
				}
				// Grup = katki sahibi + (varsa) tum parti uyeleri; grup payi = grubun toplam hasari / toplam hasar.
				TArray<AMMOCharacter*> Group;
				if (UMMOPartyComponent* Party = Who->GetPartyComponent())
				{
					if (Party->IsInParty())
					{
						Party->GetPartyInRange(GetActorLocation(), 1.0e9f, Group);
					}
				}
				Group.AddUnique(Who);
				float GroupDamage = 0.f;
				for (AMMOCharacter* G : Group)
				{
					Assigned.AddUnique(G);
					if (const float* D = DamageContributions.Find(G))
					{
						GroupDamage += *D;
					}
				}
				// Grubun payi uye sayisina bolunur (tek basina oynayan icin grup = 1 kisi, bolunme yok).
				const float GroupShare = FMath::Clamp(GroupDamage / TotalDamage, 0.f, 1.f);
				const float PerMemberShare = GroupShare / FMath::Max(1, Group.Num());
				for (AMMOCharacter* G : Group)
				{
					if (FVector::Dist(G->GetActorLocation(), GetActorLocation()) <= PartyXPShareRange)
					{
						AddShare(G, PerMemberShare);
					}
				}
			}
			AddShare(Killer, 0.f); // olduren her zaman gorev kredisi alir (payi yoksa XP almaz)
		}

		for (const TPair<AMMOCharacter*, float>& Entry : Shares)
		{
			AMMOCharacter* R = Entry.Key;
			const float Share = Entry.Value;
			// Oluyken XP verilmiyor; ayni oldurmeden gorev kredisi de verilmesin.
			if (!R || !R->GetStats() || R->GetStats()->bIsDead)
			{
				continue;
			}
			int32 ScaledXP = 0;
			if (Share > 0.f)
			{
				const int32 ShareXP = FMath::Max(1, FMath::RoundToInt(XPReward * Share));
				ScaledXP = ScaleXPForLevelGap(ShareXP, R->GetStats()->Level, MobLevel);
				// Parti power-leveling tavani (QA): mob alicidan >10 seviye yuksekse tek oldurmeden
				// alinan XP, alicinin sonraki seviye esiginin %25'ini gecemez (Lv1 -> tek kill'de Lv18 olmasin).
				if (MobLevel - R->GetStats()->Level > 10)
				{
					ScaledXP = FMath::Min(ScaledXP, FMath::Max(1, R->GetStats()->ExperienceToNextLevel / 4));
				}
				if (R->GetStats()->Level < R->GetStats()->MaxLevel)
				{
					R->ClientNotifyXPGain(ScaledXP, XPReward > 0 ? FMath::RoundToInt(100.f * ScaledXP / XPReward) : 100);
				}
			}
			// Olcum gunlugu: oldurme (seviye = XP verilmeden onceki seviye; party=1 -> son vurusu yapan degil).
			UE_LOG(LogTemp, Display, TEXT("[MMOMETRIC] t=%.1f ev=kill who=%s lv=%d mob=%s mlv=%d xp=%d party=%d elite=%d"),
				MMOMetrics::Seconds(), *MMOMetrics::WhoName(R), R->GetStats()->Level, *ArchetypeId.ToString(),
				MobLevel, ScaledXP, R != Killer ? 1 : 0, bIsElite ? 1 : 0);
			if (ScaledXP > 0)
			{
				R->GetStats()->GainExperience(ScaledXP);
			}
			if (R->GetQuestComponent())
			{
				R->GetQuestComponent()->NotifyMobKilled(ArchetypeId);
			}
		}

		// Elit/dunya boss'u dustugunde TUM sunucuya duyur (bug-hunt: onceden sadece
		// olduren ve menzildeki parti uyeleri gorurdu - onemli bir sunucu-genelinde
		// olay hic bildirilmiyordu).
		if (bIsElite)
		{
			FString KillerName = TEXT("bilinmeyen bir maceraci");
			if (const APlayerState* KillerPS = Killer->GetPlayerState())
			{
				KillerName = KillerPS->GetPlayerName();
			}
			MulticastEliteDeathAnnounce(MobName, KillerName);
		}
	}

	// Loot dusur (altin + sansa bagli item) — YALNIZCA sunucuda (hayalet istemci loot'u olmasin).
	if (HasAuthority())
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
				// QA gozlemi (2026-09-14, ilerleyen tier'lerde buyuyen bir sorun olarak isaretlendi):
				// eskiden TUM item havuzundan (Lv1'den o anki en yuksek tier'e kadar) tamamen
				// rastgele seciliyordu, yani bir Lv1 Goblin ile bir Lv16 mob'un ust-tier (Mithril/
				// Adamantin) esya dusurme sansi AYNIYDI. Item sayisi arttikca (9->29) bu "piyango
				// tavani" surekli yukseldi. Normal (elit olmayan) mob'lar artik kendi seviyesinin
				// biraz UZERINDEKI (heyecan verici ama absurt olmayan) esyalarla sinirlandirildi.
				TArray<const FMMOItemDef*> LevelAppropriate;
				for (const FMMOItemDef& It : AllItems)
				{
					if (It.RequiredLevel <= MobLevel + 3)
					{
						LevelAppropriate.Add(&It);
					}
				}
				Picked = LevelAppropriate.Num() > 0
					? LevelAppropriate[FMath::RandRange(0, LevelAppropriate.Num() - 1)]
					: &AllItems[FMath::RandRange(0, AllItems.Num() - 1)];
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
		// Ozel mesh'li mob'lar (or. Warrok ork): mesh'i gizleme; olum animasyonu yok
		// edilene kadar (2sn) gorunsun. Digerleri (Manny) eskisi gibi aninda gizlenir.
		if (!bCustomMeshVisual)
		{
			MeshComp->SetVisibility(false);
		}
	}

	// Kisa gecikmeyle yok et; spawner top-up ile yerine yenisini uretir. YALNIZCA sunucuda:
	// aktoru sunucu yok eder, replikasyon istemcilerde de siler (istemci Destroy'u yanlis).
	if (HasAuthority())
	{
		FTimerHandle DestroyTimer;
		GetWorldTimerManager().SetTimer(DestroyTimer, this, &AMMOMob::DestroySelf, 2.0f, false);
	}
}

void AMMOMob::DestroySelf()
{
	Destroy();
}

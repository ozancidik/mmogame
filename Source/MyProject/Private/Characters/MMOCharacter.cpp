// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/MMOCharacter.h"
#include "Core/MMONotify.h"
#include "Characters/MMOMob.h"
#include "Components/MMOStatsComponent.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOSkillComponent.h"
#include "Components/MMOQuestComponent.h"
#include "Components/MMOPartyComponent.h"
#include "Components/MMOTradeComponent.h"
#include "Components/MMOClanComponent.h"
#include "Components/MMOFriendsComponent.h"
#include "Core/MMOTargeting.h"
#include "Core/MMOMetrics.h"
#include "Player/MMOPlayerController.h"
#include "Core/MMOGameMode.h"
#include "Items/MMOConsumableTypes.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/DamageEvents.h"
#include "Components/InputComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "UI/MMOHUD.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AI/MMODialogueSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Save/MMOSaveGame.h"
#include "Net/UnrealNetwork.h"

namespace
{
	// Hile/debug komutlari (MMOGiveItem, MMOGainXP, MMOSpawnMob vb.) Shipping derlemede
	// calismaz (SECURITY_AUDIT Y-6). Gelistirme ve editor derlemelerinde davranis ayni.
	bool MMODebugCommandsEnabled()
	{
#if UE_BUILD_SHIPPING
		return false;
#else
		return true;
#endif
	}
}

namespace
{
	// PvP kurallarinda "oyuncu" = mob olmayan AMMOCharacter. IsPlayerControlled() kontrolcusu
	// o an olmayan (travel/yeniden sahiplenme) oyuncu pawn'ini mob sanip korumayi atlardi.
	bool IsPlayerPawn(const AMMOCharacter* C)
	{
		return C && !Cast<AMMOMob>(C);
	}
}

AMMOCharacter::AMMOCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	Stats = CreateDefaultSubobject<UMMOStatsComponent>(TEXT("Stats"));
	Inventory = CreateDefaultSubobject<UMMOInventoryComponent>(TEXT("Inventory"));
	SkillComponent = CreateDefaultSubobject<UMMOSkillComponent>(TEXT("SkillComponent"));
	QuestComponent = CreateDefaultSubobject<UMMOQuestComponent>(TEXT("QuestComponent"));
	PartyComponent = CreateDefaultSubobject<UMMOPartyComponent>(TEXT("PartyComponent"));
	TradeComponent = CreateDefaultSubobject<UMMOTradeComponent>(TEXT("TradeComponent"));
	ClanComponent = CreateDefaultSubobject<UMMOClanComponent>(TEXT("ClanComponent"));
	FriendsComponent = CreateDefaultSubobject<UMMOFriendsComponent>(TEXT("FriendsComponent"));

	// NETWORK_AUDIT Y5: oyuncu aktoru icin makul replikasyon sikligi (motor varsayilani ~100 Hz
	// gereksiz; 60 Hz akici hareket icin yeterli, hareketsizken 20 Hz tabanina duser). Cull
	// mesafesi motor varsayilaninda birakildi (oyuncular/parti uyeleri uzaktan da gorunmeli).
	// DOGRULANMADI: degerler PIE/Network Profiler ile olculmedi. AMMOMob kurucusu bunu ezer.
	SetNetUpdateFrequency(60.f);
	SetMinNetUpdateFrequency(20.f);

	// Saldiri animasyonlari. Sira onemli: 0..2 = yakin dovus varyasyonlari, 3 = buyu okuma.
	// Kilic savurma Mixamo'dan alinip Manny iskeletine retarget edildi (RT_ onekli).
	{
		static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Sword(TEXT("/Game/RT_Great_Sword_Slash_Anim.RT_Great_Sword_Slash_Anim"));
		static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Atk1(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"));
		static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Atk2(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02.MM_Attack_02"));
		static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> Charged(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_ChargedAttack.MM_ChargedAttack"));

		UAnimSequenceBase* SwordAnim = nullptr;
		if (Sword.Succeeded()) { SwordAnim = Sword.Object; }

		UAnimSequenceBase* Fallback1 = nullptr;
		if (Atk1.Succeeded()) { Fallback1 = Atk1.Object; }

		UAnimSequenceBase* Fallback2 = nullptr;
		if (Atk2.Succeeded()) { Fallback2 = Atk2.Object; }

		// Kilic animasyonu varsa yakin dovus slotlarini o doldurur; yoksa eski Unarmed setine duser.
		AttackAnims.Add(SwordAnim ? SwordAnim : Fallback1);
		AttackAnims.Add(SwordAnim ? SwordAnim : Fallback2);
		AttackAnims.Add(SwordAnim ? SwordAnim : Fallback1);

		// 3 = buyu okuma / buyuk vurus (henuz ozel bir cast animasyonu yok).
		UAnimSequenceBase* ChargedAnim = nullptr;
		if (Charged.Succeeded()) { ChargedAnim = Charged.Object; }
		AttackAnims.Add(ChargedAnim);
	}
}

void AMMOCharacter::PlayAttackAnimLocal(int32 AnimIndex)
{
	if (!AttackAnims.IsValidIndex(AnimIndex) || !AttackAnims[AnimIndex])
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInst = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!AnimInst)
	{
		return;
	}

	UAnimSequenceBase* Anim = AttackAnims[AnimIndex];
	if (UAnimMontage* AsMontage = Cast<UAnimMontage>(Anim))
	{
		AnimInst->Montage_Play(AsMontage);
	}
	else
	{
		// Duz AnimSequence: ABP_Unarmed'daki "DefaultSlot" uzerinde dinamik montaj olarak calinir
		// (yurume/kosma alt katmani bozulmadan ustune biner).
		AnimInst->PlaySlotAnimationAsDynamicMontage(Anim, TEXT("DefaultSlot"), 0.1f, 0.15f, 1.f, 1);
	}
}

void AMMOCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (Stats)
	{
		Stats->OnDeath.AddDynamic(this, &AMMOCharacter::HandleDeath);
		Stats->OnRevived.AddDynamic(this, &AMMOCharacter::HandleRevived);
		Stats->OnLevelUp.AddDynamic(this, &AMMOCharacter::OnLevelUpFeedback);
	}

	ApplyCameraSettings();
}

void AMMOCharacter::ApplyCameraSettings()
{
	// BP'deki Camera Boom'u (spring arm) bul ve uzaklik ayarla (yerel kameraya etki eder).
	if (USpringArmComponent* Boom = FindComponentByClass<USpringArmComponent>())
	{
		Boom->TargetArmLength = CameraDistance;
	}

	// Baslangic bakis acisini biraz yukaridan yap (sadece yerel oyuncu; kamera control rotation'i takip eder).
	if (IsLocallyControlled())
	{
		if (AController* C = GetController())
		{
			FRotator R = C->GetControlRotation();
			R.Pitch = CameraStartPitch;
			C->SetControlRotation(R);
		}
	}
}

void AMMOCharacter::OnLevelUpFeedback(int32 NewLevel)
{
	// OnLevelUp hem sunucuda (broadcast) hem istemcide (OnRep_Level) tetiklenir; sadece yerel oyuncuya goster.
	if (IsLocallyControlled() && IsPlayerControlled() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor(255, 230, 90),
			FString::Printf(TEXT("*** LEVEL UP!  Seviye %d ***"), NewLevel));
	}
}

void AMMOCharacter::HandleRevived()
{
	// Hem sunucuda hem sahibi istemcide calisir: kilitlenen hareketi geri ac.
	if (IsPlayerControlled())
	{
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}

	// Bu diriltme sifaci NPC'den (erken, bedelli) geldiyse HandleDeath'in kurdugu
	// respawn zamanlayicisi hala bekliyor olabilir; temizlenmezse birkac saniye sonra
	// zaten dirilmis/oynayan karakteri zorla baslangic noktasina isinlar. Zamanlayici
	// zaten dogal respawn'dan geldiyse (RespawnAtStart calismis) bu cagri zaten bosa
	// giden bir temizlik (handle gecersiz), zararsiz. Sadece sunucuda anlamli, ama
	// istemcide bos bir handle'i temizlemek de zararsiz.
	GetWorldTimerManager().ClearTimer(RespawnTimerHandle);

	// Canlanma korumasi: dogal respawn (RespawnAtStart -> Revive) ve sifaci diriltmesi
	// ikisi de buradan gecer. Dogus noktasinin yakininda agresif mob varsa oyuncu hemen
	// tekrar olup olum dongusune girmesin diye kisa sure hasar/aggro'dan muaf tut.
	if (HasAuthority() && IsPlayerControlled())
	{
		StartSpawnProtection();
	}
}

void AMMOCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMMOCharacter, bSpawnProtected);
	DOREPLIFETIME_CONDITION(AMMOCharacter, bClassChoicePending, COND_OwnerOnly);
}

void AMMOCharacter::StartSpawnProtection()
{
	if (!HasAuthority() || SpawnProtectionDuration <= 0.f)
	{
		return;
	}

	bSpawnProtected = true;
	GetWorldTimerManager().SetTimer(SpawnProtectionTimerHandle, this,
		&AMMOCharacter::EndSpawnProtection, SpawnProtectionDuration, false);
	ForceNetUpdate();
	OnRep_SpawnProtected(); // OnRep sunucuda calismaz; host kendi geri bildirimini burada alir
}

void AMMOCharacter::EndSpawnProtection()
{
	if (!HasAuthority() || !bSpawnProtected)
	{
		return;
	}

	bSpawnProtected = false;
	GetWorldTimerManager().ClearTimer(SpawnProtectionTimerHandle);
	ForceNetUpdate();
	OnRep_SpawnProtected();
}

void AMMOCharacter::OnRep_SpawnProtected()
{
	// HUD etiketi bayragi her karede okur; burada sadece korumanin bittigini yerel oyuncuya bildir.
	if (!bSpawnProtected && IsLocallyControlled() && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor(150, 200, 255), TEXT("Canlanma korumasi bitti"));
	}
}

void AMMOCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Sunucu tarafi: oyuncu possess ettiginde sinif kurulumu (starter skill + stat dagilimi).
	// BeginPlay'de degil burada — possession BeginPlay'den sonra gelebilir (ozellikle MP'de).
	if (Cast<APlayerController>(NewController) && SkillComponent)
	{
		SkillComponent->SetupForClass(SkillComponent->CharacterClass);
	}

	// Otorite + oyuncu: kaydi (varsa) guvenli zamanlamada yukle + periyodik otomatik kaydet.
	if (HasAuthority() && Cast<APlayerController>(NewController))
	{
		GetWorldTimerManager().SetTimer(AutoLoadTimerHandle, this, &AMMOCharacter::AutoLoadDeferred, 0.6f, false);
		GetWorldTimerManager().SetTimer(AutoSaveTimerHandle, this, &AMMOCharacter::SaveCharacter, 60.f, true);

		// Ilk giris de bir "dogus": kisa canlanma korumasi ver.
		StartSpawnProtection();
	}
}

void AMMOCharacter::AutoLoadDeferred()
{
	// bInitialLoadComplete artik LoadCharacter() icinde (her cagiran yoldan) set edilir.
	LoadCharacter();

	// Giris tamam: listesinde bu oyuncu olan cevrimici arkadaslara haber ver.
	if (HasAuthority() && IsPlayerControlled() && FriendsComponent)
	{
		FriendsComponent->AnnouncePresence(true);
	}
}

void AMMOCharacter::MoveToPlayerStart()
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			SetActorLocation(It->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
			SetActorRotation(It->GetActorRotation());
			return;
		}
	}
}

bool AMMOCharacter::TryRestoreSavedLocation(const FVector& SavedLocation, const FRotator& SavedRotation, bool& bNavPending)
{
	bNavPending = false;
	if (SavedLocation.ContainsNaN())
	{
		return false;
	}
	UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!World || !Capsule)
	{
		return false;
	}
	const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Nav || !Nav->GetMainNavData())
	{
		// Harita yuklenirken navmesh henuz hazir olmayabilir. PlayerStart'a donup
		// autosave ile eski koordinati ezmek yerine kisa araliklarla yeniden dene.
		bNavPending = true;
		return false;
	}

	FHitResult GroundHit;
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(MMOCharacterLoadGround), false, this);
	// Baslangici kapsul merkezinde tut: ustteki cati/kopru, ayak altindaki
	// destek zeminden once vurulup gecerli kaydi yanlis reddetmesin.
	const FVector TraceEnd(SavedLocation.X, SavedLocation.Y, SavedLocation.Z - 500.f);
	if (!World->LineTraceSingleByChannel(GroundHit, SavedLocation, TraceEnd, ECC_Visibility, TraceParams)
		|| GroundHit.ImpactNormal.Z < 0.7f
		|| FMath::Abs((SavedLocation.Z - GroundHit.ImpactPoint.Z) - Capsule->GetScaledCapsuleHalfHeight()) > 120.f)
	{
		return false;
	}
	FNavLocation NavLocation;
	if (!Nav->ProjectPointToNavigation(GroundHit.ImpactPoint + FVector(0.f, 0.f, 20.f),
			NavLocation, FVector(75.f, 75.f, 80.f))
		|| FVector::Dist2D(NavLocation.Location, GroundHit.ImpactPoint) > 75.f
		|| FMath::Abs(NavLocation.Location.Z - GroundHit.ImpactPoint.Z) > 35.f)
	{
		return false;
	}
	const FVector Target = NavLocation.Location + FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + 2.f);
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
	if (World->OverlapBlockingTestByChannel(Target, FQuat::Identity, ECC_Pawn, CapsuleShape, TraceParams))
	{
		return false;
	}
	SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
	if (!SavedRotation.ContainsNaN())
	{
		SetActorRotation(SavedRotation);
	}
	return true;
}

void AMMOCharacter::RetrySavedLocationRestore()
{
	if (!HasAuthority())
	{
		return;
	}
	bool bNavPending = false;
	if (TryRestoreSavedLocation(PendingSavedLocation, PendingSavedRotation, bNavPending))
	{
		bInitialLoadComplete = true;
	}
	else if (bNavPending && ++SavedLocationRetryCount < MaxSavedLocationRetries)
	{
		GetWorldTimerManager().SetTimer(SavedLocationRetryTimerHandle, this,
			&AMMOCharacter::RetrySavedLocationRestore, 1.f, false);
		return;
	}
	else
	{
		if (bNavPending)
		{
			UE_LOG(LogTemp, Warning, TEXT("Character navigation unavailable after %d retries; using PlayerStart and enabling saves"),
				SavedLocationRetryCount);
		}
		MoveToPlayerStart();
		bInitialLoadComplete = true;
	}
	GetWorldTimerManager().ClearTimer(SavedLocationRetryTimerHandle);
}

void AMMOCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Oyundan cikis/level bitiminde otomatik kaydet (otorite + oyuncu).
	if (HasAuthority() && IsPlayerControlled())
	{
		// bInitialLoadComplete SART: yukleme (0.6sn gecikmeli AutoLoadDeferred) henuz
		// calismadiysa component'ler hala varsayilan/bos durumda — bu haliyle kaydetmek
		// gercek kayit dosyasini bos bir karakterle EZER (bkz. bInitialLoadComplete yorumu,
		// bug-hunt bulgusu: possess sonrasi ~0.6sn icinde disconnect = ilerlemenin silinmesi).
		if (bInitialLoadComplete && !bTravelSaveCommitted)
		{
			SaveCharacter();
		}

		// Partideyse hemen ayril: yoksa kalan uyelerin Members listesi, bu aktor GC ile
		// null'lanana kadar gecerliligini yitirmis (pending-kill) bir isaretci tutar ve
		// "PARTI" HUD'u/uye sayisi/paylasilan XP kisa sure (ya da GC tetiklenene kadar)
		// yanlis/eksik gorunur. Yukleme tamamlanmamis olsa bile parti temizligi guvenlidir
		// (yeni-possess bir karakter zaten partide olamaz), o yuzden bagimsiz calisir.
		if (PartyComponent)
		{
			PartyComponent->RequestLeave();
		}

		// Gercek cikis (Destroyed; bolge gecisi/kapanis degil): arkadaslara cevrimdisi haberi.
		if (FriendsComponent && EndPlayReason == EEndPlayReason::Destroyed && !bTravelSaveCommitted)
		{
			FriendsComponent->AnnouncePresence(false);
		}
	}
	Super::EndPlay(EndPlayReason);
}

FString AMMOCharacter::GetSaveSlotName() const
{
	FString Name = TEXT("Default");
	if (const APlayerState* PS = GetPlayerState())
	{
		const FString PlayerName = PS->GetPlayerName();
		if (!PlayerName.IsEmpty())
		{
			Name = PlayerName;
		}
	}
	// PlayerState adi bir platformdan da gelebilir. Yol ayirici/nokta gibi karakterleri
	// save dosya adina tasima; bosluklarin eski kayitlardaki '_' eslemesini koru.
	FString SafeName;
	SafeName.Reserve(FMath::Min(Name.Len(), 32));
	for (const TCHAR Ch : Name)
	{
		if (SafeName.Len() >= 32)
		{
			break;
		}
		if (FChar::IsAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('-'))
		{
			SafeName.AppendChar(Ch);
		}
		else if (Ch == TEXT(' '))
		{
			SafeName.AppendChar(TEXT('_'));
		}
	}
	return FString(TEXT("MMOSave_")) + (SafeName.IsEmpty() ? TEXT("Default") : SafeName);
}

void AMMOCharacter::SaveCharacter()
{
	SaveCharacterForTravel();
}

void AMMOCharacter::SetTravelSaveCommitted(bool bCommitted)
{
	if (HasAuthority())
	{
		bTravelSaveCommitted = bCommitted;
	}
}

bool AMMOCharacter::SaveCharacterForTravel()
{
	if (!HasAuthority() || !bInitialLoadComplete)
	{
		return false;
	}

	// SECURITY_AUDIT K-1 (kayit ezme): yuklenen/olusturulan yuva ile simdiki yuva (ad degisti) farkliysa
	// ve hedef yuvada baska birine ait bir kayit zaten varsa YAZMA. Yeni (bos) bir yuvaya yazmak serbest;
	// mevcut bir kaydi sahiplenmek icin once o adla MMOLoad yapilmalidir (ActiveSaveSlot guncellenir).
	const FString TargetSlot = GetSaveSlotName();
	if (!ActiveSaveSlot.IsEmpty() && !ActiveSaveSlot.Equals(TargetSlot, ESearchCase::IgnoreCase)
		&& UGameplayStatics::DoesSaveGameExist(TargetSlot, 0))
	{
		UE_LOG(LogTemp, Warning, TEXT("Save refused: %s -> %s already exists (name changed since load)"), *ActiveSaveSlot, *TargetSlot);
		// Uzak oyuncuya da ulasir (B5): aksi halde cikista ilerlemesini kaybettigini hic ogrenmezdi.
		MMONotify::ToOwner(this,
			TEXT("Bu ad baska bir kayda ait: kaydedilmedi. Once MMOLoad ile o kaydi yukle ya da farkli bir ad sec."),
			FColor::Orange, 5.f);
		return false;
	}

	UMMOSaveGame* SG = Cast<UMMOSaveGame>(UGameplayStatics::CreateSaveGameObject(UMMOSaveGame::StaticClass()));
	if (!SG)
	{
		return false;
	}

	FMMOCharacterSaveData& D = SG->Data;
	D.bValid = true;
	if (Stats)
	{
		Stats->FillSaveData(D);
	}
	if (Inventory)
	{
		Inventory->FillSaveData(D);
	}
	if (SkillComponent)
	{
		SkillComponent->FillSaveData(D);
	}
	if (QuestComponent)
	{
		QuestComponent->FillSaveData(D);
	}
	if (ClanComponent)
	{
		ClanComponent->FillSaveData(D);
	}
	if (FriendsComponent)
	{
		FriendsComponent->FillSaveData(D);
	}
	D.MapName = UGameplayStatics::GetCurrentLevelName(this, true);
	D.Location = GetActorLocation();
	D.Rotation = GetActorRotation();

	const bool bOk = UGameplayStatics::SaveGameToSlot(SG, TargetSlot, 0);
	if (bOk)
	{
		ActiveSaveSlot = TargetSlot;
	}

	// "Kaydedildi" mesaji burada DEGIL: bu yol otomatik kayitta (60 sn), klan/ticaret/seyahat
	// kaydinda da calisir ve oyuncuya her seferinde mesaj yagmamali. Yalniz elle kayit
	// (MMOSave / ServerSaveCharacter) ToOwner ile bildirir.
	return bOk;
}

void AMMOCharacter::GrantStarterPack()
{
	// Yalniz sunucu + oyuncu. Ayni pawn icin bir kez (MMOLoad vb. ile yeni-karakter dali tekrar
	// calisirsa cift vermesin). Kayit formatina alan eklenmez: "kayit dosyasi yoktu" bilgisi yeterli.
	if (!HasAuthority() || bStarterPackGranted || !Inventory || !IsPlayerControlled())
	{
		return;
	}
	bStarterPackGranted = true;

	// Ilk silah + ilk zirh (VERTICAL_SLICE_LV1_30 madde 2) cantaya girip kusanilir; 3 Can Iksiri.
	static const FName StarterEquipment[] = { FName(TEXT("ShortSword")), FName(TEXT("LeatherArmor")) };
	for (const FName& ItemId : StarterEquipment)
	{
		const int32 BagBefore = Inventory->EquipmentBag.Num();
		Inventory->AddEquipmentInstance(ItemId);
		if (Inventory->EquipmentBag.Num() > BagBefore)
		{
			Inventory->EquipByInstanceId(Inventory->EquipmentBag.Last().InstanceId);
		}
	}
	Inventory->AddItem(FName(TEXT("HealthPotion")), 3);

	UE_LOG(LogTemp, Display, TEXT("[MMODBG] Starter pack granted to %s weapon=%d armor=%d bag=%d potions=%d"),
		*MMOMetrics::WhoName(this), Inventory->GetEquipped(EMMOEquipSlot::Weapon).IsValidInstance() ? 1 : 0,
		Inventory->GetEquipped(EMMOEquipSlot::Armor).IsValidInstance() ? 1 : 0, Inventory->EquipmentBag.Num(),
		Inventory->GetItemCount(FName(TEXT("HealthPotion"))));
}

bool AMMOCharacter::LoadCharacter()
{
	if (!HasAuthority())
	{
		return false;
	}

	// Manuel MMOLoad ve otomatik yukleme ayni guvenlik kapisindan gecer. Onceki bir
	// yukleme basarili olsa bile simdiki dosya okunamazsa autosave onu ezmesin.
	bInitialLoadComplete = false;
	GetWorldTimerManager().ClearTimer(SavedLocationRetryTimerHandle);
	SavedLocationRetryCount = 0;

	const FString Slot = GetSaveSlotName();
	if (!UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		// Yeni karakter: hicbir kayit olmadigi icin PossessedBy'daki SetupForClass
		// varsayilan (Warrior) sinifi sessizce uygulamis durumda. Oyuncunun bunu
		// degistirmenin tek yolunun gizli bir konsol komutu oldugunu bilmesinin
		// hicbir yolu yoktu (icerik-erisilebilirligi eksigi, "PVP acik/kapali" gibi
		// diger konsol-yonlendirmeli bildirimlerle ayni desen kullanildi).
		// Eskiden 8 sn'lik bir ekran mesajiydi (kacirilabiliyordu, istemcide hic cikmiyordu);
		// artik replike bayrakla HUD'da sinif secilene kadar kalici bir ipucu paneli cizilir.
		bClassChoicePending = true;
		bInitialLoadComplete = true;
		ActiveSaveSlot = Slot;
		// Baslangic paketi YALNIZ bu dalda (kayit dosyasi yok); kayit yuklenen karakterlere hic verilmez.
		GrantStarterPack();
		return false; // yeni karakter
	}
	bClassChoicePending = false;

	UMMOSaveGame* SG = Cast<UMMOSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!SG || !SG->Data.bValid)
	{
		UE_LOG(LogTemp, Error, TEXT("Character save %s exists but could not be loaded; saving disabled"), *Slot);
		return false;
	}

	const FMMOCharacterSaveData& D = SG->Data;

	// Once sinif/skiller (stat reset yapmadan), sonra statlar, sonra envanter (ekipman bonusu).
	if (SkillComponent)
	{
		SkillComponent->ApplySaveData(D);
	}
	if (Stats)
	{
		Stats->ApplySaveData(D);
	}
	if (Inventory)
	{
		Inventory->ApplySaveData(D);
	}
	if (QuestComponent)
	{
		QuestComponent->ApplySaveData(D);
	}
	if (ClanComponent)
	{
		ClanComponent->ApplySaveData(D);
	}
	if (FriendsComponent)
	{
		FriendsComponent->ApplySaveData(D);
	}

	// Kayit verisi uygulandi: yuvayi hemen simdi sahiplen. Konum-bekleme (navmesh) dalindan ONCE olmali;
	// aksi halde ActiveSaveSlot bos/bayat kalir ve K-1 korumasi pasif olur ya da mesru kayitlar reddedilir.
	ActiveSaveSlot = Slot;

	// Eski/bozuk kayit koordinati harita disinda, boslukta veya navmesh disinda olabilir.
	// Boyle bir konuma isiklanmak oyuncuyu kalici olarak kilitler. Ilk dogus konumu
	// guvenli geri donus noktasidir; yeni kayitlarda harita adi da eslesmelidir.
	bool bNavPending = false;
	const FString CurrentMap = UGameplayStatics::GetCurrentLevelName(this, true);
	// Harita adi olmayan eski kaydin hangi map'e ait oldugu bilinmiyor. Konumunu
	// yalniz mevcut ana harita Summer_scene'de dene; baska map'te ayni XY'yi kullanma.
	const bool bMatchingMap = D.MapName.IsEmpty()
		? CurrentMap == TEXT("Summer_scene") : D.MapName == CurrentMap;
	const bool bRestored = bMatchingMap && TryRestoreSavedLocation(D.Location, D.Rotation, bNavPending);
	if (!bRestored && bNavPending)
	{
		PendingSavedLocation = D.Location;
		PendingSavedRotation = D.Rotation;
		GetWorldTimerManager().SetTimer(SavedLocationRetryTimerHandle, this,
			&AMMOCharacter::RetrySavedLocationRestore, 1.f, false);
		UE_LOG(LogTemp, Warning, TEXT("Character navigation data unavailable; saved location restore deferred"));
		return true;
	}
	if (!bRestored)
	{
		MoveToPlayerStart();
		UE_LOG(LogTemp, Warning, TEXT("Saved character position rejected for map %s; moved to PlayerStart"),
			*UGameplayStatics::GetCurrentLevelName(this, true));
	}
	bInitialLoadComplete = true;

	// LoadCharacter yalniz sunucuda calisir: uzak istemciye de ToOwner ile (B5).
	MMONotify::ToOwner(this, TEXT("Karakter yuklendi"), FColor(120, 200, 255), 2.5f);
	return true;
}

void AMMOCharacter::MMOSave()
{
	if (HasAuthority())
	{
		if (SaveCharacterForTravel())
		{
			MMONotify::ToOwner(this, TEXT("Kaydedildi"), FColor(120, 220, 120), 2.f);
		}
	}
	else
	{
		ServerSaveCharacter();
	}
}

void AMMOCharacter::MMOLoad()
{
	if (HasAuthority())
	{
		LoadCharacter();
	}
	else
	{
		ServerLoadCharacter();
	}
}

void AMMOCharacter::ServerSaveCharacter_Implementation()
{
	// Reliable RPC spam'i sunucuyu surekli diske yazmaya zorlamasin.
	const UWorld* World = GetWorld();
	if (!bInitialLoadComplete || !World
		|| World->GetTimeSeconds() - LastRemoteManualSaveTime < RemoteManualSaveCooldown)
	{
		return;
	}
	LastRemoteManualSaveTime = World->GetTimeSeconds();
	// Elle kayit: uzak sahibine de bildir (otomatik kayit sessiz kalir; bkz. SaveCharacterForTravel).
	if (SaveCharacterForTravel())
	{
		MMONotify::ToOwner(this, TEXT("Kaydedildi"), FColor(120, 220, 120), 2.f);
	}
}

void AMMOCharacter::ServerLoadCharacter_Implementation()
{
	// Manuel yukleme debug islemidir. Uzak oyuncu eski disk durumuna donerek
	// tuketim, olum cezasi veya gorev sonuclarini geri alamaz.
	const AController* C = GetController();
	if (!C || !C->IsLocalController())
	{
		return;
	}

	LoadCharacter();
}

void AMMOCharacter::MMOSetName(const FString& NewName)
{
	if (HasAuthority())
	{
		ServerSetName_Implementation(NewName);
	}
	else
	{
		ServerSetName(NewName);
	}
}

void AMMOCharacter::ServerSetName_Implementation(const FString& NewName)
{
	const FString Trimmed = NewName.TrimStartAndEnd();
	if (Trimmed.IsEmpty() || Trimmed.Len() > 24)
	{
		return;
	}
	for (const TCHAR Ch : Trimmed)
	{
		if (!FChar::IsAlnum(Ch) && Ch != TEXT('_') && Ch != TEXT('-') && Ch != TEXT(' '))
		{
			return;
		}
	}
	// Soguma yalniz uzak istemcilere uygulanir: host/yerel konsol (test akisi) kisitlanmaz.
	const UWorld* CooldownWorld = GetWorld();
	if (CooldownWorld && !IsLocallyControlled())
	{
		if (CooldownWorld->GetTimeSeconds() - LastSetNameServerTime < SetNameCooldown)
		{
			return;
		}
	}
	// Ayni oturumdaki iki karakter ayni save dosyasina yazmasin. Windows dosya
	// adlari buyuk/kucuk harf ayirt etmez; bosluk ve '_' da ayni slotu uretir.
	const FString CandidateSlot = FString(TEXT("MMOSave_")) + Trimmed.Replace(TEXT(" "), TEXT("_"));
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AMMOCharacter> It(World); It; ++It)
		{
			const AMMOCharacter* Other = *It;
			if (Other != this && Other->IsPlayerControlled()
				&& Other->GetSaveSlotName().Equals(CandidateSlot, ESearchCase::IgnoreCase))
			{
				return;
			}
		}
	}
	// Uzak istemci diskte kaydi olan (baska birine ait) bir adi alamaz: aksi halde SaveCharacterForTravel
	// bu karakter icin surekli reddedilir ve host'un bolge gecisi kilitlenir (QA O-3). Kendi yuvasina donus serbest.
	if (!IsLocallyControlled() && !ActiveSaveSlot.Equals(CandidateSlot, ESearchCase::IgnoreCase)
		&& UGameplayStatics::DoesSaveGameExist(CandidateSlot, 0))
	{
		MMONotify::ToOwner(this, TEXT("Bu ad kayitli bir karaktere ait: degistirilemedi. Kendi karakterin icin ?Name= ile yeniden baglan."), FColor::Orange, 6.f);
		return;
	}
	if (CooldownWorld)
	{
		LastSetNameServerTime = CooldownWorld->GetTimeSeconds();
	}
	if (APlayerState* PS = GetPlayerState())
	{
		const FString OldName = PS->GetPlayerName();
		PS->SetPlayerName(Trimmed);
		// Ad degisince arkadaslar eski adi cevrimdisi, yeni adi cevrimici gorsun (QA ORTA-2).
		if (FriendsComponent && IsPlayerControlled() && !OldName.Equals(Trimmed, ESearchCase::IgnoreCase))
		{
			FriendsComponent->AnnouncePresence(false, OldName);
			FriendsComponent->AnnouncePresence(true);
		}
	}
	MMONotify::ToOwner(this,
		FString::Printf(TEXT("Adin '%s' olarak ayarlandi. Kalici kayit icin simdi MMOSave/MMOLoad kullan."), *Trimmed),
		FColor(120, 200, 255), 3.f);
}

void AMMOCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	// Sahibi istemci: BP'nin ekledigi Enhanced Input mapping'lerini temizle.
	// (Tikla-git kontrol semasi AMMOPlayerController'da; eski WASD/fare-bak cakismasin.)
	ClearBPInputMappings();

	// Kamera uzakligini/acisini bu istemcide de uygula (controller artik hazir).
	ApplyCameraSettings();

	// BP'nin BeginPlay'i mapping'i BIZDEN SONRA tekrar ekleyebilir (net/possess sirasina gore);
	// bu yuzden ilk ~1.5 sn boyunca periyodik temizle. Aksi halde fare-bak RMB olmadan kamerayi cevirir.
	ClearMappingsAttempts = 0;
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(ClearMappingsTimerHandle, this,
			&AMMOCharacter::ClearBPInputMappings, 0.2f, true);
	}
}

void AMMOCharacter::ClearBPInputMappings()
{
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Subsystem->ClearAllMappings();
			}
		}
	}

	// ~1.5 sn (0.2 x 8) sonra timer'i durdur; BP BeginPlay'i coktan calismis olur.
	if (++ClearMappingsAttempts >= 8)
	{
		if (UWorld* W = GetWorld())
		{
			W->GetTimerManager().ClearTimer(ClearMappingsTimerHandle);
		}
	}
}

float AMMOCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	// Canlanma korumasi (sunucu-otoriter): koruma suresince hicbir kaynaktan (mob, PvP,
	// DoT) hasar islenmez. Moblarda bayrak hic set edilmez, onlari etkilemez.
	if (bSpawnProtected && HasAuthority())
	{
		return 0.f;
	}

	// Tum hasar yollari buradan gecer. Yeni bir PvP skill'i AttackTarget'in on
	// kontrolunu atlayip ApplyDamage cagirsa bile parti/klan/hub kurallari korunur.
	if (HasAuthority() && IsPlayerPawn(this))
	{
		AMMOCharacter* Attacker = EventInstigator
			? Cast<AMMOCharacter>(EventInstigator->GetPawn()) : nullptr;
		if (!Attacker)
		{
			Attacker = Cast<AMMOCharacter>(DamageCauser);
		}
		if (Attacker && Attacker != this && IsPlayerPawn(Attacker))
		{
			const TCHAR* BlockReason = nullptr;
			if (!Attacker->IsPvPAllowed() || !Attacker->CanHarmPlayer(this, BlockReason))
			{
				return 0.f;
			}
		}
	}

	const float BaseHandled = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (!HasAuthority() || !Stats)
	{
		return BaseHandled;
	}

	LastDamageInstigator = DamageCauser;
	const float ActualDamage = Stats->ApplyDamage(DamageAmount, DamageCauser);

	// Yerel HUD'a havada ucan hasar sayisi ekle (standalone/listen icin yeterli).
	if (ActualDamage > 0.f)
	{
		if (UWorld* W = GetWorld())
		{
			if (APlayerController* PC = W->GetFirstPlayerController())
			{
				if (AMMOHUD* HUD = Cast<AMMOHUD>(PC->GetHUD()))
				{
					HUD->AddDamageNumber(GetActorLocation() + FVector(0.f, 0.f, 90.f), ActualDamage);
				}
			}
		}
	}

	return ActualDamage;
}

void AMMOCharacter::HandleDeath()
{
	// Ortak ölüm mantığı (hareketi durdur, çarpışmayı kapat) — sunucuda çalışır,
	// görsel/animasyon için Blueprint olayını tetikleriz.
	OnCharacterDied();

	// Oyuncu: hareketi kilitle ve respawn geri sayimini baslat (moblar kendi HandleDeath'inde yok olur).
	if (IsPlayerControlled())
	{
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->StopMovementImmediately();
			Move->DisableMovement();
		}

		if (HasAuthority())
		{
			// Olum cezasi: bir miktar exp kaybi (KO tarzi).
			if (Stats)
			{
				const int32 Lost = Stats->ApplyDeathPenalty();

				// Olcum gunlugu: oyuncu olumu (by = son hasar veren mob arketipi, yoksa none).
				const AMMOMob* KillerMob = Cast<AMMOMob>(LastDamageInstigator.Get());
				const FVector DeathLoc = GetActorLocation();
				UE_LOG(LogTemp, Display, TEXT("[MMOMETRIC] t=%.1f ev=death who=%s lv=%d x=%.0f y=%.0f z=%.0f by=%s xploss=%d"),
					MMOMetrics::Seconds(), *MMOMetrics::WhoName(this), Stats->Level, DeathLoc.X, DeathLoc.Y, DeathLoc.Z,
					KillerMob ? *KillerMob->ArchetypeId.ToString() : TEXT("none"), Lost);

				if (Lost > 0)
				{
					// Sunucu dali: uzak oyuncu da XP kaybini gorsun (B5).
					MMONotify::ToOwner(this, FString::Printf(TEXT("Oldun! -%d EXP"), Lost), FColor(255, 120, 120), 3.f);
				}
			}

			GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &AMMOCharacter::RespawnAtStart, RespawnDelay, false);
		}
	}
}

void AMMOCharacter::RespawnAtStart()
{
	// Baslangic noktasini bul (yoksa oldugu yerde diril).
	FVector SpawnLoc = GetActorLocation();
	FRotator SpawnRot = GetActorRotation();
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			SpawnLoc = It->GetActorLocation();
			SpawnRot = It->GetActorRotation();
			break;
		}
	}

	SetActorLocation(SpawnLoc, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(SpawnRot);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
	}

	if (Stats)
	{
		Stats->Revive();
	}
}

float AMMOCharacter::GetRespawnTimeRemaining() const
{
	if (!Stats || !Stats->bIsDead)
	{
		return 0.f;
	}
	return FMath::Max(0.f, GetWorldTimerManager().GetTimerRemaining(RespawnTimerHandle));
}

void AMMOCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (PlayerInputComponent)
	{
		// Enhanced Input hareket bindingi BP'de; bunlar sadece test tuslari (eklemeli).
		// Not: sol tik AMMOPlayerController'da (tikla-git/tikla-saldir); skill kitabi (K) da orada.
		// Hotbar: 1..9,0 tuslari -> slot 0..9. (Debug: MMODamage/MMOHeal/MMOGainXP konsol komutlari.)
		PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AMMOCharacter::UseHotbar0);
		PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AMMOCharacter::UseHotbar1);
		PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AMMOCharacter::UseHotbar2);
		PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AMMOCharacter::UseHotbar3);
		PlayerInputComponent->BindKey(EKeys::Five, IE_Pressed, this, &AMMOCharacter::UseHotbar4);
		PlayerInputComponent->BindKey(EKeys::Six, IE_Pressed, this, &AMMOCharacter::UseHotbar5);
		PlayerInputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &AMMOCharacter::UseHotbar6);
		PlayerInputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &AMMOCharacter::UseHotbar7);
		PlayerInputComponent->BindKey(EKeys::Nine, IE_Pressed, this, &AMMOCharacter::UseHotbar8);
		PlayerInputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &AMMOCharacter::UseHotbar9);

		// F = can iksiri kullan.
		PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AMMOCharacter::UseHealthPotion);
		PlayerInputComponent->BindKey(EKeys::G, IE_Pressed, this, &AMMOCharacter::UseManaPotion);
	}
}

void AMMOCharacter::MMOAttack()
{
	// En yakin dusmana vur (hedef null -> otomatik en yakin mob).
	AttackTarget(nullptr);
}

void AMMOCharacter::AttackTarget(AMMOCharacter* Target)
{
	UWorld* World = GetWorld();
	if (!World || !Stats || Stats->bIsDead)
	{
		return;
	}
	if (MMOTargeting::IsInsideHubSafeZone(this, GetActorLocation()))
	{
		// Bu kontrol istemcide de calisir; uzak oyuncu sunucu yanitini beklemeden
		// neden saldiramadigini gorur. Tikla-saldir dongusu her karede cagirdigi icin
		// sabit pozitif anahtar kullanilir (ayni satir yenilenir, yigilmaz).
		if (GEngine && IsLocallyControlled())
		{
			GEngine->AddOnScreenDebugMessage(4101, 1.f, FColor::Silver, TEXT("Guvenli bolgede saldiri yok"));
		}
		return;
	}

	// Yerel cooldown kapisi (istemcide RPC spamini, sunucuda cift vurusu engeller).
	const float Now = World->GetTimeSeconds();
	if (Now - LastMeleeTime < MeleeCooldown)
	{
		return;
	}
	LastMeleeTime = Now;

	// Istemci: istegi hedefle sunucuya gonder (hasar sadece otoritede islenir).
	if (!HasAuthority())
	{
		ServerAttackTarget(Target);
		return;
	}

	const bool bLocalFeedback = IsLocallyControlled();
	auto SendAttackFeedback = [this, bLocalFeedback](const FString& Text)
	{
		if (bLocalFeedback)
		{
			if (GEngine)
			{
				// Sabit anahtar: tikla-saldir dongusunde satir yigilmaz, yenilenir.
				GEngine->AddOnScreenDebugMessage(4102, 1.f, FColor::Silver, Text);
			}
			return;
		}

		// Listen-server'daki uzak pawn sunucuda locally controlled degildir.
		// Ret sebebi gecici ipucu olarak (karakter ustunde kisa balon) saldirana iletilir;
		// kalici sohbet satiri birakmaz. Ayni metin 3 sn icinde tekrar gonderilmez.
		const float FeedbackNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		if (Text == LastAttackFeedbackText && FeedbackNow - LastAttackFeedbackTime < 3.f)
		{
			return;
		}
		if (Cast<AMMOPlayerController>(GetController()))
		{
			LastAttackFeedbackText = Text;
			LastAttackFeedbackTime = FeedbackNow;
			ClientShowHint(Text);
		}
	};

	// Hedef gecersizse (yok/olu) en yakin canli mob'a dus.
	AMMOCharacter* Victim = Target;
	if (!Victim || !Victim->GetStats() || Victim->GetStats()->bIsDead)
	{
		Victim = MMOTargeting::FindNearestMob(this, GetActorLocation(), MeleeRange);
	}

	if (!Victim || Victim == this)
	{
		SendAttackFeedback(TEXT("Menzilde dusman yok - yaklas"));
		return;
	}

	// Menzil dogrulama (anti-cheat: istemci yalan soylemesin).
	if (FVector::Dist(GetActorLocation(), Victim->GetActorLocation()) > MeleeRange * 1.3f)
	{
		return;
	}

	// PvP kontrolu: hedef oyuncuysa PvP acik olmali.
	if (IsPlayerPawn(Victim) && !IsPvPAllowed())
	{
		SendAttackFeedback(TEXT("PvP kapali"));
		return;
	}
	// Dostu-ates ve guvenli bolge kurallari (parti / klan / hub).
	if (const TCHAR* BlockReason = nullptr; IsPlayerPawn(Victim) && !CanHarmPlayer(Victim, BlockReason))
	{
		SendAttackFeedback(BlockReason ? FString(BlockReason) : TEXT("Bu hedefe saldiramazsin"));
		return;
	}

	// Saldiran oyuncu canlanma korumasini kaybeder (dokunulmazken vurmasin).
	EndSpawnProtection();
	// Uzak menzilden hasar almadan saldiran oyuncu da savastadir; sinif degistiremez.
	Stats->MarkCombatActivity();

	// Hedefe don ve vur.
	const FVector Dir = (Victim->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (!Dir.IsNearlyZero())
	{
		SetActorRotation(Dir.Rotation());
	}

	const float ActualDamage = UGameplayStatics::ApplyDamage(Victim, Stats->AttackPower, GetController(), this, nullptr);
	// Vurus animasyonu varyasyonunu sunucuda sec ki tum makineler ayni hareketi gorsun.
	MulticastMeleeFX(Victim->GetActorLocation() + FVector(0.f, 0.f, 40.f), FMath::RandRange(0, 2));

	if (Victim->GetStats())
	{
		const bool bKilled = Victim->GetStats()->bIsDead;
		const FString HitText = FString::Printf(TEXT("VURDUN: %.0f hasar | hedef HP %.0f/%.0f%s"),
			ActualDamage,
			Victim->GetStats()->Health, Victim->GetStats()->MaxHealth,
			bKilled ? TEXT("  >>> OLDU!") : TEXT(""));
		if (GEngine && bLocalFeedback)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, bKilled ? FColor::Red : FColor::Yellow, HitText);
		}
		else if (!bLocalFeedback && bKilled)
		{
			// Uzak istemciye yalniz OLDURUCU vurus (ToOwner sohbet satiri birakir; her vurusta
			// gondermek sohbeti doldururdu). Hasar sayilari/kritik geri bildirimi ayri yoldan gider.
			MMONotify::ToOwner(this, HitText, FColor::Red, 1.5f);
		}
	}
}

void AMMOCharacter::ServerAttackTarget_Implementation(AMMOCharacter* Target)
{
	// Sunucu-OTORITER cooldown: istemcinin dogru cadence'te gonderdigine GUVENME.
	// (Eskiden LastMeleeTime=-100 yapiliyordu => kapi hic tetiklenmiyor, hilelı/
	// laglı istemci sinirsiz DPS atabiliyordu.) Ag jitter'i icin %10 tolerans birak;
	// cok erken gelen istegi sessizce reddet.
	const UWorld* W = GetWorld();
	const float Now = W ? W->GetTimeSeconds() : 0.f;
	if (Now - LastMeleeTime < MeleeCooldown * 0.9f)
	{
		return;
	}
	// AttackTarget kendi kapisini (Now - LastMeleeTime < MeleeCooldown) yeniden uygular;
	// tam gecmesi icin LastMeleeTime'i bir cooldown geriye al, o da LastMeleeTime=Now yapar.
	LastMeleeTime = Now - MeleeCooldown;
	AttackTarget(Target);
}

bool AMMOCharacter::CanHarmPlayer(const AMMOCharacter* Victim, const TCHAR*& OutReason) const
{
	if (!IsPlayerPawn(Victim))
	{
		return true; // mob / oyuncu olmayan hedef: kurallar uygulanmaz
	}

	// Parti uyesine vurulmaz.
	if (PartyComponent)
	{
		for (const TObjectPtr<AMMOCharacter>& M : PartyComponent->Members)
		{
			if (M.Get() == Victim)
			{
				OutReason = TEXT("Parti uyesine vuramazsin");
				return false;
			}
		}
	}

	// Ayni klan uyesine vurulmaz.
	if (ClanComponent && Victim->GetClanComponent()
		&& ClanComponent->IsInClan()
		&& ClanComponent->ClanName.Equals(Victim->GetClanComponent()->ClanName, ESearchCase::IgnoreCase))
	{
		OutReason = TEXT("Klan uyesine vuramazsin");
		return false;
	}

	// Hub guvenli bolgesi: saldiran ya da hedef PlayerStart'a 700u icindeyse PvP yok.
	constexpr float SafeRadius = 700.f;
	if (MMOTargeting::IsInsideHubSafeZone(this, GetActorLocation(), SafeRadius)
		|| MMOTargeting::IsInsideHubSafeZone(this, Victim->GetActorLocation(), SafeRadius))
	{
		OutReason = TEXT("Guvenli bolgede PvP yok");
		return false;
	}
	return true;
}

bool AMMOCharacter::IsPvPAllowed() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AMMOGameMode* GM = World->GetAuthGameMode<AMMOGameMode>())
		{
			return GM->bAllowPvP;
		}
	}
	return false;
}

void AMMOCharacter::MMOPvP(int32 bOn)
{
	if (!HasAuthority())
	{
		ServerSetPvP(bOn != 0);
		return;
	}

	if (AMMOGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMMOGameMode>() : nullptr)
	{
		GM->bAllowPvP = (bOn != 0);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow,
				FString::Printf(TEXT("PvP %s"), GM->bAllowPvP ? TEXT("ACIK") : TEXT("KAPALI")));
		}
	}
}

void AMMOCharacter::ServerSetPvP_Implementation(bool bOn)
{
	// GUVENLIK: bAllowPvP TUM sunucu icin gecerli paylasilan bir kuraldir, kisisel
	// bir ayar degil. Bu kontrol olmadan HERHANGI BIR uzak istemci kendi konsoluna
	// "MMOPvP 1" yazip ev sahibinin onayi olmadan butun oyuncular icin PvP'yi
	// acabiliyordu (PvP'nin kasitli olarak kapali tutulmasi tasarim kararini bypass
	// eden ciddi bir acik). Sadece HOST'un (listen server'i baslatan oyuncunun)
	// karakteri bu RPC'yi kendi yerel controller'i uzerinden cagirabilir; uzak bir
	// istemcinin karakterinin sunucudaki controller'i asla "local" degildir.
	const AController* C = GetController();
	if (!C || !C->IsLocalController())
	{
		return;
	}

	if (AMMOGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMMOGameMode>() : nullptr)
	{
		GM->bAllowPvP = bOn;
	}
}

void AMMOCharacter::MMOBuff(const FString& EffectId)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	const FName Id(*EffectId);
	if (HasAuthority())
	{
		if (Stats)
		{
			Stats->ApplyEffect(Id, this);
		}
	}
	else
	{
		ServerApplyBuff(Id);
	}
}

void AMMOCharacter::ServerApplyBuff_Implementation(FName EffectId)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	// Hata ayiklama komutu: uzak istemci kendi RPC'sini dogrudan cagirip buff alamaz.
	const AController* C = GetController();
	if (!C || !C->IsLocalController())
	{
		return;
	}

	if (Stats)
	{
		Stats->ApplyEffect(EffectId, this);
	}
}

void AMMOCharacter::MMOSpawnMob(const FString& ArchetypeId)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	const FName Id(*ArchetypeId);
	if (HasAuthority())
	{
		ServerSpawnMob_Implementation(Id);
	}
	else
	{
		ServerSpawnMob(Id);
	}
}

void AMMOCharacter::MMOInvite()
{
	if (PartyComponent)
	{
		PartyComponent->RequestInviteNearest();
	}
}

void AMMOCharacter::MMOPartyAccept()
{
	if (PartyComponent)
	{
		PartyComponent->RequestAccept();
	}
}

void AMMOCharacter::MMOPartyLeave()
{
	if (PartyComponent)
	{
		PartyComponent->RequestLeave();
	}
}

void AMMOCharacter::MMOPartyKick()
{
	if (!PartyComponent)
	{
		return;
	}

	// Hedef = oyuncunun secili tuttugu karakter (Z / sol tik ile secilen).
	AMMOPlayerController* MPC = Cast<AMMOPlayerController>(GetController());
	AMMOCharacter* Target = MPC ? MPC->GetSelectedTarget() : nullptr;
	if (!Target || Target == this)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Once cikarilacak parti uyesini sec"));
		}
		return;
	}

	// Yetki/uyelik dogrulamasini sunucu yapar; buradaki kontrol sadece erken geri bildirim.
	if (!PartyComponent->Members.Contains(Target))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("Secili oyuncu partinde degil"));
		}
		return;
	}

	PartyComponent->RequestKick(Target);
}

void AMMOCharacter::MMOTrade()
{
	if (TradeComponent)
	{
		TradeComponent->RequestInviteNearest();
	}
}

void AMMOCharacter::MMOTradeAccept()
{
	if (TradeComponent)
	{
		TradeComponent->RequestAccept();
	}
}

void AMMOCharacter::MMOTradeCancel()
{
	if (TradeComponent)
	{
		TradeComponent->RequestCancel();
	}
}

void AMMOCharacter::MMOTradeGold(int32 Amount)
{
	if (TradeComponent)
	{
		TradeComponent->RequestSetGold(Amount);
	}
}

void AMMOCharacter::MMOTradeConfirm()
{
	if (TradeComponent)
	{
		TradeComponent->RequestConfirm();
	}
}

namespace
{
	// "STR"/"str"/"strength" -> "Strength" (SpendStatPoint'in bekledigi ad).
	FName MapMMOStatName(FName In)
	{
		const FString L = In.ToString().ToLower();
		if (L == TEXT("str") || L == TEXT("strength"))     { return FName(TEXT("Strength")); }
		if (L == TEXT("dex") || L == TEXT("dexterity"))    { return FName(TEXT("Dexterity")); }
		if (L == TEXT("int") || L == TEXT("intelligence")) { return FName(TEXT("Intelligence")); }
		if (L == TEXT("vit") || L == TEXT("vitality"))     { return FName(TEXT("Vitality")); }
		return NAME_None;
	}
}

void AMMOCharacter::RequestSpendStat(FName StatName)
{
	const FName Mapped = MapMMOStatName(StatName);
	if (Mapped.IsNone())
	{
		return;
	}
	if (HasAuthority())
	{
		if (Stats)
		{
			Stats->SpendStatPoint(Mapped, 1);
		}
	}
	else
	{
		ServerSpendStat(Mapped);
	}
}

void AMMOCharacter::ServerSpendStat_Implementation(FName StatName)
{
	if (Stats)
	{
		Stats->SpendStatPoint(StatName, 1); // zaten eslendi
	}
}

void AMMOCharacter::MMOStat(const FString& StatName)
{
	RequestSpendStat(FName(*StatName));
}

void AMMOCharacter::ClientNotifyXPGain_Implementation(int32 Amount, int32 Percent)
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	AMMOHUD* HUD = PC ? Cast<AMMOHUD>(PC->GetHUD()) : nullptr;
	if (!HUD || Amount <= 0)
	{
		return;
	}
	const FString Label = Percent < 100
		? FString::Printf(TEXT("+%d EXP (%%%d)"), Amount, Percent)
		: FString::Printf(TEXT("+%d EXP"), Amount);
	HUD->AddFloatingLabel(GetActorLocation() + FVector(0.f, 0.f, 130.f), Label);
}

void AMMOCharacter::ClientShowHint_Implementation(const FString& Text)
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	AMMOHUD* HUD = PC ? Cast<AMMOHUD>(PC->GetHUD()) : nullptr;
	if (!HUD || Text.IsEmpty())
	{
		return;
	}
	HUD->AddFloatingLabel(GetActorLocation() + FVector(0.f, 0.f, 170.f), Text);
}

void AMMOCharacter::MMONpcTalk()
{
	// Bu Exec komutu client-side calisir (RPC degil), o yuzden kendi cooldown'unu
	// tutar — yoksa konsoldan spamlanirsa sinirsiz eszamanli/ucretli LLM istegi atilir.
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	if (Now - LastNpcTalkDebugTime < NpcTalkDebugCooldown)
	{
		return;
	}
	LastNpcTalkDebugTime = Now;

	UGameInstance* GI = GetGameInstance();
	UMMODialogueSubsystem* Dialogue = GI ? GI->GetSubsystem<UMMODialogueSubsystem>() : nullptr;
	if (!Dialogue)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MMONpcTalk] Dialogue subsystem yok."));
		return;
	}

	// Oyuncu baglami (seviye) — replige renk katsin.
	FString PlayerInfo = TEXT("bir maceraci");
	if (Stats)
	{
		PlayerInfo = FString::Printf(TEXT("seviye %d bir maceraci"), Stats->Level);
	}

	FMMOOnDialogueReady OnReady;
	OnReady.BindLambda([](bool bSuccess, const FString& Text)
	{
		UE_LOG(LogTemp, Display, TEXT("[NPC] %s"), *Text);
		if (GEngine)
		{
			const FColor Col = bSuccess ? FColor::Green : FColor::Orange;
			GEngine->AddOnScreenDebugMessage(-1, 8.f, Col, FString::Printf(TEXT("NPC: %s"), *Text));
		}
	});

	Dialogue->RequestNpcLine(
		TEXT("Satici Merek"),
		TEXT("kasabanin aksi ama durust silah saticisi"),
		PlayerInfo,
		OnReady);
}

void AMMOCharacter::ServerSpawnMob_Implementation(FName ArchetypeId)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	// Sunucunun yerel oyuncusu disindakiler mob olusturup XP/ganimet uretemez.
	const AController* C = GetController();
	if (!C || !C->IsLocalController())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Oyuncunun ~3m onune, hafif yukariya spawn'la.
	const FVector Loc = GetActorLocation() + GetActorForwardVector() * 300.f + FVector(0.f, 0.f, 60.f);
	const FTransform SpawnTM(FRotator::ZeroRotator, Loc);

	if (AMMOMob* Mob = World->SpawnActorDeferred<AMMOMob>(AMMOMob::StaticClass(), SpawnTM))
	{
		// ArchetypeId'yi BeginPlay'den once ata ki ApplyArchetype dogru arketipi kullansin.
		Mob->ArchetypeId = ArchetypeId;
		UGameplayStatics::FinishSpawningActor(Mob, SpawnTM);
	}
}

void AMMOCharacter::MulticastMeleeFX_Implementation(FVector ImpactLocation, int32 AnimIndex)
{
	// Vurus animasyonu (her makinede yerel oynatilir; sunucu hangi varyasyon oldugunu bildirir).
	PlayAttackAnimLocal(AnimIndex);

	if (UWorld* World = GetWorld())
	{
		// Kisa kirmizi darbe patlamasi (her makinede yerel cizilir).
		DrawDebugSphere(World, ImpactLocation, 35.f, 8, FColor(255, 90, 60), false, 0.2f, 0, 2.5f);
	}
}

void AMMOCharacter::MMODamage(float Amount)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	// Standart hasar akisindan gecir: TakeDamage -> Stats->ApplyDamage.
	UGameplayStatics::ApplyDamage(this, Amount, GetController(), this, nullptr);
}

void AMMOCharacter::MMOHeal(float Amount)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	if (Stats)
	{
		Stats->Heal(Amount);
	}
}

void AMMOCharacter::MMOGainXP(int32 Amount)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	if (Stats)
	{
		Stats->GainExperience(Amount);
	}
}

void AMMOCharacter::MMOUseMana(float Amount)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	if (Stats)
	{
		Stats->ConsumeMana(Amount);
	}
}

void AMMOCharacter::UseHotbar0() { if (SkillComponent) { SkillComponent->UseHotbarSlot(0); } }
void AMMOCharacter::UseHotbar1() { if (SkillComponent) { SkillComponent->UseHotbarSlot(1); } }
void AMMOCharacter::UseHotbar2() { if (SkillComponent) { SkillComponent->UseHotbarSlot(2); } }
void AMMOCharacter::UseHotbar3() { if (SkillComponent) { SkillComponent->UseHotbarSlot(3); } }
void AMMOCharacter::UseHotbar4() { if (SkillComponent) { SkillComponent->UseHotbarSlot(4); } }
void AMMOCharacter::UseHotbar5() { if (SkillComponent) { SkillComponent->UseHotbarSlot(5); } }
void AMMOCharacter::UseHotbar6() { if (SkillComponent) { SkillComponent->UseHotbarSlot(6); } }
void AMMOCharacter::UseHotbar7() { if (SkillComponent) { SkillComponent->UseHotbarSlot(7); } }
void AMMOCharacter::UseHotbar8() { if (SkillComponent) { SkillComponent->UseHotbarSlot(8); } }
void AMMOCharacter::UseHotbar9() { if (SkillComponent) { SkillComponent->UseHotbarSlot(9); } }

void AMMOCharacter::UseHealthPotion()
{
	static const FName PotionId(TEXT("HealthPotion"));

	if (!Stats || !Inventory || Stats->bIsDead)
	{
		return;
	}

	const bool bLocalFeedback = IsLocallyControlled();

	// On kontroller replike verilerle her iki tarafta da yapilabilir.
	if (Stats->Health >= Stats->MaxHealth)
	{
		if (GEngine && bLocalFeedback)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("Canin zaten dolu"));
		}
		return;
	}

	if (Inventory->GetItemCount(PotionId) <= 0)
	{
		if (GEngine && bLocalFeedback)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("Iksir yok!"));
		}
		return;
	}

	// Istemci: sunucuya istek gonder (envanter/heal sadece otoritede degisir).
	if (!HasAuthority())
	{
		ServerUseHealthPotion();
		return;
	}

	if (Inventory->RemoveItem(PotionId, 1))
	{
		Stats->Heal(PotionHealAmount);
		// Otorite dali (istemci yukarida RPC'ye doner): uzak istemciye de ToOwner ile (B5).
		MMONotify::ToOwner(this,
			FString::Printf(TEXT("Iksir icildi: +%.0f HP (kalan %d)"), PotionHealAmount, Inventory->GetItemCount(PotionId)),
			FColor(255, 80, 80), 2.f);
	}
}

void AMMOCharacter::ServerUseHealthPotion_Implementation()
{
	UseHealthPotion();
}

void AMMOCharacter::UseManaPotion()
{
	UseConsumable(FName(TEXT("ManaPotion")));
}

void AMMOCharacter::MMOUse(const FString& ItemId)
{
	UseConsumable(FName(*ItemId));
}

void AMMOCharacter::UseConsumable(FName ItemId)
{
	if (!Stats || !Inventory || Stats->bIsDead)
	{
		return;
	}

	const FMMOConsumableDef* Def = FMMOConsumableDatabase::Find(ItemId);
	if (!Def)
	{
		return;
	}

	const bool bLocalFeedback = IsLocallyControlled();

	if (Inventory->GetItemCount(ItemId) <= 0)
	{
		if (GEngine && bLocalFeedback)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver,
				FString::Printf(TEXT("%s yok!"), *Def->Name));
		}
		return;
	}

	// Istemci: istegi sunucuya gonder (esya harcamasi + etki sadece otoritede).
	if (!HasAuthority())
	{
		ServerUseConsumable(ItemId);
		return;
	}

	if (Inventory->RemoveItem(ItemId, 1))
	{
		if (Def->Heal > 0.f)
		{
			Stats->Heal(Def->Heal);
		}
		if (Def->ManaRestore > 0.f)
		{
			Stats->RestoreMana(Def->ManaRestore);
		}
		if (!Def->EffectId.IsNone())
		{
			Stats->ApplyEffect(Def->EffectId, this);
		}

		MMONotify::ToOwner(this,
			FString::Printf(TEXT("Kullanildi: %s (kalan %d)"), *Def->Name, Inventory->GetItemCount(ItemId)),
			FColor(120, 200, 255), 2.f);
	}
}

void AMMOCharacter::ServerUseConsumable_Implementation(FName ItemId)
{
	UseConsumable(ItemId);
}

void AMMOCharacter::MMOGiveItem(const FString& ItemId, int32 Count)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	const FName Id(*ItemId);
	const int32 C = FMath::Max(1, Count);
	if (HasAuthority())
	{
		if (Inventory)
		{
			Inventory->AddItem(Id, C);
		}
	}
	else
	{
		ServerGiveItem(Id, C);
	}
}

void AMMOCharacter::ServerGiveItem_Implementation(FName ItemId, int32 Count)
{
	if (!MMODebugCommandsEnabled())
	{
		return;
	}
	// Debug item komutunu yalnizca host kullanabilir; sayi siniri tek basina yeterli degil.
	const AController* C = GetController();
	if (!C || !C->IsLocalController())
	{
		return;
	}

	// GUVENLIK (qa-kontrolcu bulgusu): Count sadece MMOGiveItem'in istemci-tarafi
	// yolunda clamp'leniyordu (FMath::Max(1,Count)) - bu RPC gövdesi ise ham geleni
	// dogrudan kullaniyordu. Hileli bir istemci ServerGiveItem'i DOGRUDAN (istemci
	// fonksiyonunu hic cagirmadan) devasa/negatif bir Count ile cagirabilirdi:
	// Items dizisindeki Stack.Count int32'yi tasip negatife donerdi, o item o
	// oyuncu icin KALICI OLARAK harcanamaz hale gelirdi. Ustten de sinirla
	// (BuyPotion/BuyConsumable'daki ayni desen).
	if (Inventory)
	{
		Inventory->AddItem(ItemId, FMath::Clamp(Count, 1, 999));
	}
}

void AMMOCharacter::MMOClass(const FString& ClassName)
{
	if (!SkillComponent)
	{
		return;
	}

	const FString C = ClassName.TrimStartAndEnd().ToLower();
	EMMOClass NewClass = EMMOClass::Warrior;
	if (C == TEXT("warrior") || C == TEXT("savasci") || C == TEXT("savaşçı"))
	{
		NewClass = EMMOClass::Warrior;
	}
	else if (C == TEXT("archer") || C == TEXT("okcu") || C == TEXT("okçu"))
	{
		NewClass = EMMOClass::Archer;
	}
	else if (C == TEXT("mage") || C == TEXT("buyucu") || C == TEXT("büyücü"))
	{
		NewClass = EMMOClass::Mage;
	}
	else if (C == TEXT("priest") || C == TEXT("rahip"))
	{
		NewClass = EMMOClass::Priest;
	}
	else
	{
		if (GEngine && IsLocallyControlled())
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Silver,
				TEXT("Gecersiz sinif. Kullanim: MMOClass savasci|okcu|buyucu|rahip"));
		}
		return;
	}

	SkillComponent->RequestSetupForClass(NewClass);
}

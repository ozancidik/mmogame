// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/MMOPlayerController.h"
#include "Characters/MMOCharacter.h"
#include "Characters/MMOMob.h"
#include "World/MMOVendorNPC.h"
#include "World/MMOQuestGiverNPC.h"
#include "Components/MMOStatsComponent.h"
#include "Core/MMOTargeting.h"
#include "UI/MMOHUD.h"
#include "Components/InputComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NavigationSystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

AMMOPlayerController::AMMOPlayerController()
{
	// OS imlecini gizle (kendi imlecimizi HUD'da ciziyoruz) ama fare takibini surdur.
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::None;
}

void AMMOPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// OS imleci gizli ama fare serbest ve konumu takip edilir; kendi imlecimizi HUD cizer.
	bShowMouseCursor = true;
	CurrentMouseCursor = EMouseCursor::None;
}

void AMMOPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AMMOPlayerController::OnLeftClickPressed);
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AMMOPlayerController::OnLeftClickReleased);
		InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AMMOPlayerController::OnRightMousePressed);
		InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AMMOPlayerController::OnRightMouseReleased);
		InputComponent->BindAxisKey(EKeys::MouseX, this, &AMMOPlayerController::OnMouseX);
		InputComponent->BindAxisKey(EKeys::MouseY, this, &AMMOPlayerController::OnMouseY);
		// K = skill kitabi, I = envanter.
		InputComponent->BindKey(EKeys::K, IE_Pressed, this, &AMMOPlayerController::OnToggleSkillBook);
		InputComponent->BindKey(EKeys::I, IE_Pressed, this, &AMMOPlayerController::OnToggleInventory);
		InputComponent->BindKey(EKeys::C, IE_Pressed, this, &AMMOPlayerController::OnToggleCharacter);

		// Z = en yakin dusmani sec, R = saldiri ac/kapa (KO tarzi).
		InputComponent->BindKey(EKeys::Z, IE_Pressed, this, &AMMOPlayerController::OnTargetNearestEnemy);
		InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AMMOPlayerController::OnToggleAttack);

		// Q / E = kamerayi sola / saga cevir (touchpad dostu; PlayerTick'te islenir).
		// (Eski E=dukkani kapat kaldirildi; artik pencereler sag ustteki X ile kapatiliyor.)

		// Space = ziplama.
		InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AMMOPlayerController::OnJumpPressed);
		InputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &AMMOPlayerController::OnJumpReleased);
	}
}

void AMMOPlayerController::OnJumpPressed()
{
	// UI penceresi acikken ziplama.
	if (IsAnyUIWindowOpen())
	{
		return;
	}
	if (ACharacter* Ch = Cast<ACharacter>(GetPawn()))
	{
		Ch->Jump();
	}
}

void AMMOPlayerController::OnJumpReleased()
{
	if (ACharacter* Ch = Cast<ACharacter>(GetPawn()))
	{
		Ch->StopJumping();
	}
}

void AMMOPlayerController::OnCloseShop()
{
	if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		if (H->IsShopOpen())
		{
			H->CloseShop();
		}
		if (H->IsQuestWindowOpen())
		{
			H->CloseQuestWindow();
		}
	}
}

AMMOCharacter* AMMOPlayerController::GetSelectedTarget() const
{
	AMMOCharacter* Target = SelectedTarget.Get();
	if (!Target || !Target->GetStats() || Target->GetStats()->bIsDead)
	{
		return nullptr;
	}
	return Target;
}

void AMMOPlayerController::SelectTarget(AMMOCharacter* NewTarget)
{
	// KO TargetSelect: hedef degistiyse auto-attack iptal edilir.
	if (SelectedTarget.Get() != NewTarget)
	{
		TargetEnemy.Reset();
	}

	SelectedTarget = NewTarget;

	// KO RotateTo: karakter hedefe doner (yurumez, saldirmaz).
	APawn* P = GetPawn();
	if (P && NewTarget)
	{
		const FVector Dir = (NewTarget->GetActorLocation() - P->GetActorLocation()).GetSafeNormal2D();
		if (!Dir.IsNearlyZero())
		{
			P->SetActorRotation(Dir.Rotation());
		}
	}
}

void AMMOPlayerController::OnTargetNearestEnemy()
{
	APawn* P = GetPawn();
	UWorld* World = GetWorld();
	if (!P || !World)
	{
		return;
	}

	// En yakin canli mobu bul (KO: CharacterGetByNearestEnemy).
	AMMOMob* Nearest = MMOTargeting::FindNearestMob(this, P->GetActorLocation(), TargetSearchRange);

	SelectTarget(Nearest);

	if (!Nearest && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Silver, TEXT("Yakinlarda dusman yok"));
	}
}

void AMMOPlayerController::OnToggleAttack()
{
	// KO KM_TOGGLE_ATTACK: saldiriyorsa durdur...
	if (TargetEnemy.IsValid())
	{
		TargetEnemy.Reset();
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Silver, TEXT("Saldiri durduruldu"));
		}
		return;
	}

	// ...degilse secili hedefe saldiriya gec (TryStartAttack).
	AMMOCharacter* Target = GetSelectedTarget();
	if (Target)
	{
		TargetEnemy = Target;
		bHasMoveTarget = false;
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Silver, TEXT("Hedef yok - Z ile sec"));
	}
}

bool AMMOPlayerController::IsSkillBookOpen() const
{
	if (const AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		return H->IsSkillBookOpen();
	}
	return false;
}

bool AMMOPlayerController::IsAnyUIWindowOpen() const
{
	if (const AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		return H->IsSkillBookOpen() || H->IsInventoryOpen() || H->IsShopOpen() || H->IsQuestWindowOpen() || H->IsStatWindowOpen();
	}
	return false;
}

void AMMOPlayerController::OnToggleSkillBook()
{
	if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		H->ToggleSkillBook();
	}
}

void AMMOPlayerController::OnToggleInventory()
{
	if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		H->ToggleInventory();
	}
}

void AMMOPlayerController::OnToggleCharacter()
{
	if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		H->ToggleStatWindow();
	}
}

bool AMMOPlayerController::IsInAttackMode() const
{
	const AMMOCharacter* Enemy = TargetEnemy.Get();
	return Enemy && Enemy->GetStats() && !Enemy->GetStats()->bIsDead;
}

void AMMOPlayerController::OnLeftClickPressed()
{
	// UI penceresi acikken tiklama pencereye gider, karakteri hareket ettirmez.
	if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		if (H->IsAnyWindowOpen())
		{
			// Tiklama, imlecin ustundeki acik pencereye gider (X ile kapatma dahil); karakter hareket etmez.
			float MX = 0.f, MY = 0.f;
			if (GetMousePosition(MX, MY))
			{
				H->HandleWindowClick(FVector2D(MX, MY));
			}
			return;
		}
	}

	// Pencere yok: sol tus basildi. Basili kaldigi surece her kare imlecin oldugu yere git
	// (basili-tut-git). Sol tus kamerayi cevirmez.
	bLeftPressed = true;
}

void AMMOPlayerController::DoLeftClickAction()
{
	// Bir pencere acikken dunyaya tiklama gitmesin (dukkan/gorev/envanter vs.).
	if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		if (H->IsAnyWindowOpen())
		{
			return;
		}
	}

	// Fare altindaki dunyayi trace et.
	FHitResult Hit;
	if (!GetHitResultUnderCursor(ECC_Visibility, false, Hit) || !Hit.bBlockingHit)
	{
		return;
	}

	// Saticiya tiklandi mi? (menzil icindeyse dukkani ac)
	if (AMMOVendorNPC* Vendor = Cast<AMMOVendorNPC>(Hit.GetActor()))
	{
		APawn* MyPawn = GetPawn();
		if (MyPawn && FVector::Dist(MyPawn->GetActorLocation(), Vendor->GetActorLocation()) <= 500.f)
		{
			if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
			{
				H->OpenShop();
			}
		}
		else if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("Saticiya yaklas"));
		}
		return;
	}

	// Gorevliye tiklandi mi? (menzil icindeyse gorev penceresini ac)
	if (AMMOQuestGiverNPC* QuestGiver = Cast<AMMOQuestGiverNPC>(Hit.GetActor()))
	{
		APawn* MyPawn = GetPawn();
		if (MyPawn && FVector::Dist(MyPawn->GetActorLocation(), QuestGiver->GetActorLocation()) <= 500.f)
		{
			if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
			{
				H->OpenQuestWindow();
			}
		}
		else if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("Gorevliye yaklas"));
		}
		return;
	}

	// Bir karaktere (mob veya diger oyuncu) tiklandi mi? Kendine tiklama sayilmaz.
	// (Oyuncuya vurma PvP kontrolu sunucuda yapilir; NPC'ler AMMOCharacter degil, otomatik haric.)
	AMMOCharacter* HitChar = Cast<AMMOCharacter>(Hit.GetActor());
	if (HitChar && HitChar != GetPawn() && HitChar->GetStats() && !HitChar->GetStats()->bIsDead)
	{
		// Hedefe git ve saldir.
		SelectedTarget = HitChar;
		TargetEnemy = HitChar;
		bHasMoveTarget = false;
	}
	else
	{
		// Zemine tiklandi: oraya yuru.
		MoveToLocation = Hit.ImpactPoint;
		bHasMoveTarget = true;
		TargetEnemy.Reset();
	}
}

void AMMOPlayerController::OnLeftClickReleased()
{
	// Surukle-birak: skill kitabindan tasinan skill bir hotbar slotuna birakildiysa ata.
	if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		float MX = 0.f, MY = 0.f;
		if (GetMousePosition(MX, MY))
		{
			H->HandleSkillBookRelease(FVector2D(MX, MY));
		}
	}

	// Birakildi: tikla-git/saldiri/NPC eylemini garantile (cok kisa dokunuslar tick'e denk gelmemis olabilir).
	if (bLeftPressed)
	{
		DoLeftClickAction();
	}
	bLeftPressed = false;
}

void AMMOPlayerController::OnRightMousePressed()
{
	// Pencere acikken sag tik, imlecin ustundeki pencereye calisir (kitap: slota koy; envanter: +1 dene), kamera donmez.
	if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		if (H->IsAnyWindowOpen())
		{
			float MX = 0.f, MY = 0.f;
			if (GetMousePosition(MX, MY))
			{
				H->HandleWindowRightClick(FVector2D(MX, MY));
			}
			return;
		}
	}

	bRotatingCamera = true;
	// Imleci gizle/yakala ki fare kenara dayanmadan sinirsiz donebilsin.
	bShowMouseCursor = false;
}

void AMMOPlayerController::OnRightMouseReleased()
{
	bRotatingCamera = false;
	// Imleci geri getir (tikla-git icin).
	bShowMouseCursor = true;
}

void AMMOPlayerController::OnMouseX(float Value)
{
	if (Value == 0.f || IsAnyUIWindowOpen())
	{
		return;
	}

	// Kamera cevirme SADECE sag tus basiliyken. Sol tus kamerayi cevirmez.
	if (bRotatingCamera)
	{
		const float LookSensitivity = 1.6f;
		AddYawInput(Value * LookSensitivity);
	}
}

void AMMOPlayerController::OnMouseY(float Value)
{
	if (Value == 0.f || IsAnyUIWindowOpen())
	{
		return;
	}

	// Kamera cevirme SADECE sag tus basiliyken (ters-Y duzeltmesi). Sol tus kamerayi cevirmez.
	if (bRotatingCamera)
	{
		const float LookSensitivity = 1.6f;
		AddPitchInput(-Value * LookSensitivity);
	}
}

void AMMOPlayerController::SetOrientToMovement(bool bOrient)
{
	if (ACharacter* Ch = Cast<ACharacter>(GetPawn()))
	{
		if (UCharacterMovementComponent* Move = Ch->GetCharacterMovement())
		{
			if (Move->bOrientRotationToMovement != bOrient)
			{
				Move->bOrientRotationToMovement = bOrient;
			}
		}
	}
}

bool AMMOPlayerController::HasNavmesh() const
{
	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	return NavSys && NavSys->GetMainNavData() != nullptr;
}

void AMMOPlayerController::DrivePawnTowardLocation(const FVector& Dest)
{
	APawn* P = GetPawn();
	if (!P)
	{
		return;
	}

	// Tikla-git: karakter gittigi yone donsun (WASD strafe'i kapatmis olabilir).
	SetOrientToMovement(true);

	if (HasNavmesh())
	{
		// Yol bulma: cok sik yeniden hesaplama; aradaki karelerde path takibi surer.
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - LastPlayerRepathTime > 0.2f)
		{
			LastPlayerRepathTime = Now;
			UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, Dest);
		}
	}
	else
	{
		// Fallback: duz-cizgi.
		const FVector Dir = (Dest - P->GetActorLocation()).GetSafeNormal2D();
		P->AddMovementInput(Dir, 1.f);
	}
}

void AMMOPlayerController::DrivePawnTowardActor(AActor* Goal)
{
	APawn* P = GetPawn();
	if (!P || !Goal)
	{
		return;
	}

	// Hedefe kosarken de yone donsun.
	SetOrientToMovement(true);

	if (HasNavmesh())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - LastPlayerRepathTime > 0.2f)
		{
			LastPlayerRepathTime = Now;
			UAIBlueprintHelperLibrary::SimpleMoveToActor(this, Goal);
		}
	}
	else
	{
		const FVector Dir = (Goal->GetActorLocation() - P->GetActorLocation()).GetSafeNormal2D();
		P->AddMovementInput(Dir, 1.f);
	}
}

void AMMOPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	APawn* P = GetPawn();
	if (!P)
	{
		return;
	}

	// UI penceresi acikken: kamera donusunu zorla kapat + imleci serbest birak
	// (pencere sag-tik basiliyken acilmis olabilir; yoksa imlec takilir ve fare kamerayi cevirir).
	const bool bBookOpen = IsAnyUIWindowOpen();
	if (bBookOpen)
	{
		bRotatingCamera = false;
		if (!bShowMouseCursor)
		{
			bShowMouseCursor = true;
		}
	}

	// --- WASD klavye hareketi (KARAKTERIN baktigi yona gore, yonu degistirmeden) ---
	// Enhanced Input mapping'lerini temizledigimiz icin WASD'yi burada C++ ile suruyoruz.
	// W/S = karakterin baktigi yonde ileri/geri, A/D = yana kayma (strafe). Karakter DONMEZ.
	if (!bBookOpen)
	{
		float ForwardAxis = 0.f;
		float RightAxis = 0.f;
		if (IsInputKeyDown(EKeys::W)) { ForwardAxis += 1.f; }
		if (IsInputKeyDown(EKeys::S)) { ForwardAxis -= 1.f; }
		if (IsInputKeyDown(EKeys::D)) { RightAxis += 1.f; }
		if (IsInputKeyDown(EKeys::A)) { RightAxis -= 1.f; }

		if (ForwardAxis != 0.f || RightAxis != 0.f)
		{
			// Karakterin mevcut bakis yonunu temel al (kamera degil).
			const FRotator YawRot(0.f, P->GetActorRotation().Yaw, 0.f);
			const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
			const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

			// Hareket yonune donmeyi kapat ki bakis yonu sabit kalsin (yana kayarken de).
			SetOrientToMovement(false);

			// Klavye hareketi tikla-git yolunu ve hedefi iptal eder (manuel kontrol oncelikli).
			StopMovement();
			bHasMoveTarget = false;
			TargetEnemy.Reset();

			P->AddMovementInput(Forward, ForwardAxis);
			P->AddMovementInput(Right, RightAxis);
			return;
		}
	}

	// Sol tus basili: her kare imlecin oldugu yere git/saldir (tikla-git / basili-tut-git).
	if (bLeftPressed && !bBookOpen)
	{
		DoLeftClickAction();
	}

	// Dusman hedefi varsa: ona yol bul, menzilde saldir.
	if (TargetEnemy.IsValid())
	{
		AMMOCharacter* Enemy = TargetEnemy.Get();
		if (!Enemy->GetStats() || Enemy->GetStats()->bIsDead)
		{
			TargetEnemy.Reset();
			StopMovement();
			return;
		}

		const FVector ToEnemy = Enemy->GetActorLocation() - P->GetActorLocation();
		if (ToEnemy.Size2D() > AttackReach)
		{
			DrivePawnTowardActor(Enemy);
		}
		else if (AMMOCharacter* MyChar = Cast<AMMOCharacter>(P))
		{
			// Menzilde: yol bulmayi durdur, saldir (MMOAttack kendi cooldown'u ile vurur).
			StopMovement();
			MyChar->MMOAttack();
		}
	}
	// Aksi halde zemin hedefi varsa oraya yol bul.
	else if (bHasMoveTarget)
	{
		const FVector ToLoc = MoveToLocation - P->GetActorLocation();
		if (ToLoc.Size2D() > MoveAcceptanceRadius)
		{
			DrivePawnTowardLocation(MoveToLocation);
		}
		else
		{
			bHasMoveTarget = false;
			StopMovement();
		}
	}
}

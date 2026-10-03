// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/MMOPlayerController.h"
#include "Characters/MMOCharacter.h"
#include "Characters/MMOMob.h"
#include "World/MMOVendorNPC.h"
#include "World/MMOQuestGiverNPC.h"
#include "World/MMOTalkerNPC.h"
#include "World/MMOHealerNPC.h"
#include "World/MMOWarehouseNPC.h"
#include "AI/MMODialogueSubsystem.h"
#include "Components/MMOStatsComponent.h"
#include "Components/MMOInventoryComponent.h"
#include "Engine/GameInstance.h"
#include "Core/MMOTargeting.h"
#include "Core/MMOGameMode.h"
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
#include "Components/MMOPartyComponent.h"
#include "Components/MMOTradeComponent.h"
#include "Components/MMOClanComponent.h"
#include "Components/MMOFriendsComponent.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SEditableTextBox.h"

AMMOPlayerController::AMMOPlayerController()
{
	// OS imlecini gizle (kendi imlecimizi HUD'da ciziyoruz) ama fare takibini surdur.
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::None;
}

void AMMOPlayerController::MMOTravel()
{
	// Uzak istemci bu komutla tum oturumu tasiyamaz. Host kontrolu GameMode'da da var.
	if (!HasAuthority() || !IsLocalController())
	{
		return;
	}
	AMMOGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AMMOGameMode>() : nullptr;
	FString FailureReason;
	if (!GM || !GM->RequestZoneTravel(this, FailureReason))
	{
		if (FailureReason.IsEmpty())
		{
			FailureReason = TEXT("Oyun modu gecis icin hazir degil");
		}
		UE_LOG(LogTemp, Warning, TEXT("MMOTravel rejected: %s"), *FailureReason);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FailureReason);
		}
	}
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

		// Enter = sohbet yazi kutusu.
		InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AMMOPlayerController::OnChatKey);
	}
}

void AMMOPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseChatInput();
	Super::EndPlay(EndPlayReason);
}

void AMMOPlayerController::SeamlessTravelTo(APlayerController* NewPC)
{
	// Seamless travel'da kontrolcu EndPlay almayabilir; acik sohbet kutusu yeni haritada
	// asili kalip hareketi kilitlemesin.
	CloseChatInput();
	Super::SeamlessTravelTo(NewPC);
}

void AMMOPlayerController::Say(const FString& Message)
{
	ServerSendChat(Message);
}

void AMMOPlayerController::ServerSendChat_Implementation(const FString& Message)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	if (Now - LastChatServerTime < 0.7f)
	{
		return;
	}

	FString Msg = Message.Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" ")).TrimStartAndEnd();
	uint8 Channel = 0;
	if (Msg.Equals(TEXT("/p"), ESearchCase::IgnoreCase))
	{
		return; // bos parti mesaji: genel sohbete "/p" olarak sizmasin
	}
	if (Msg.Equals(TEXT("/yardim"), ESearchCase::IgnoreCase) || Msg.Equals(TEXT("/help"), ESearchCase::IgnoreCase))
	{
		LastChatServerTime = Now;
		ClientReceiveChat(FString(), TEXT("Komutlar: /p mesaj (parti), /w isim mesaj (fisilti), /who (cevrimici oyuncular), /trade (ticaret; kabul, iptal), /klan (kur ad, davet, kabul, ayril, at isim, liste), /g mesaj (klan sohbeti), /arkadas (ekle isim, sil isim, liste), /yardim"), 2);
		return;
	}
	if (Msg.Equals(TEXT("/who"), ESearchCase::IgnoreCase))
	{
		LastChatServerTime = Now;
		FString List;
		int32 Count = 0;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const AMMOPlayerController* PC = Cast<AMMOPlayerController>(It->Get());
			if (!PC || !PC->PlayerState)
			{
				continue;
			}
			const AMMOCharacter* Ch = Cast<AMMOCharacter>(PC->GetPawn());
			const int32 Lv = (Ch && Ch->GetStats()) ? Ch->GetStats()->Level : 0;
			List += FString::Printf(TEXT("%s%s (Lv%d)"), Count > 0 ? TEXT(", ") : TEXT(""), *PC->PlayerState->GetPlayerName(), Lv);
			++Count;
		}
		ClientReceiveChat(FString(), FString::Printf(TEXT("Cevrimici (%d): %s"), Count, *List), 2);
		return;
	}
	// Ticaret: "/trade" (en yakin oyuncuya davet), "/trade kabul", "/trade iptal".
	if (Msg.Equals(TEXT("/trade"), ESearchCase::IgnoreCase) || Msg.StartsWith(TEXT("/trade "), ESearchCase::IgnoreCase))
	{
		LastChatServerTime = Now;
		UMMOTradeComponent* Trade = nullptr;
		if (const AMMOCharacter* Me = Cast<AMMOCharacter>(GetPawn()))
		{
			Trade = Me->GetTradeComponent();
		}
		const FString Sub = Msg.RightChop(6).TrimStartAndEnd();
		if (!Trade)
		{
			ClientReceiveChat(FString(), TEXT("Ticaret su an kullanilamaz."), 2);
		}
		else if (Sub.IsEmpty())
		{
			Trade->RequestInviteNearest();
		}
		else if (Sub.Equals(TEXT("kabul"), ESearchCase::IgnoreCase) || Sub.Equals(TEXT("accept"), ESearchCase::IgnoreCase))
		{
			Trade->RequestAccept();
		}
		else if (Sub.Equals(TEXT("iptal"), ESearchCase::IgnoreCase) || Sub.Equals(TEXT("cancel"), ESearchCase::IgnoreCase))
		{
			Trade->RequestCancel();
		}
		else
		{
			ClientReceiveChat(FString(), TEXT("Kullanim: /trade, /trade kabul, /trade iptal"), 2);
		}
		return;
	}
	// Klan: "/klan kur <ad>", "/klan davet", "/klan kabul", "/klan ayril", "/klan at <isim>", "/klan liste".
	if (Msg.Equals(TEXT("/klan"), ESearchCase::IgnoreCase) || Msg.StartsWith(TEXT("/klan "), ESearchCase::IgnoreCase))
	{
		LastChatServerTime = Now;
		UMMOClanComponent* Clan = nullptr;
		if (const AMMOCharacter* Me = Cast<AMMOCharacter>(GetPawn()))
		{
			Clan = Me->GetClanComponent();
		}
		const FString Rest = Msg.RightChop(5).TrimStartAndEnd();
		FString Sub = Rest;
		FString Arg;
		Rest.Split(TEXT(" "), &Sub, &Arg);
		Arg = Arg.TrimStartAndEnd();
		if (!Clan)
		{
			ClientReceiveChat(FString(), TEXT("Klan su an kullanilamaz."), 2);
		}
		else if (Sub.Equals(TEXT("kur"), ESearchCase::IgnoreCase))
		{
			Clan->CmdCreate(Arg);
		}
		else if (Sub.Equals(TEXT("davet"), ESearchCase::IgnoreCase))
		{
			Clan->CmdInvite();
		}
		else if (Sub.Equals(TEXT("kabul"), ESearchCase::IgnoreCase))
		{
			Clan->CmdAccept();
		}
		else if (Sub.Equals(TEXT("ayril"), ESearchCase::IgnoreCase))
		{
			Clan->CmdLeave();
		}
		else if (Sub.Equals(TEXT("at"), ESearchCase::IgnoreCase))
		{
			Clan->CmdKick(Arg);
		}
		else if (Sub.Equals(TEXT("liste"), ESearchCase::IgnoreCase))
		{
			Clan->CmdList();
		}
		else
		{
			ClientReceiveChat(FString(), TEXT("Kullanim: /klan kur ad, /klan davet, /klan kabul, /klan ayril, /klan at isim, /klan liste"), 2);
		}
		return;
	}
	// Arkadas listesi: "/arkadas ekle <isim>", "/arkadas sil <isim>", "/arkadas" veya "/arkadas liste".
	if (Msg.Equals(TEXT("/arkadas"), ESearchCase::IgnoreCase) || Msg.StartsWith(TEXT("/arkadas "), ESearchCase::IgnoreCase))
	{
		LastChatServerTime = Now;
		UMMOFriendsComponent* Friends = nullptr;
		if (const AMMOCharacter* Me = Cast<AMMOCharacter>(GetPawn()))
		{
			Friends = Me->GetFriendsComponent();
		}
		const FString Rest = Msg.RightChop(8).TrimStartAndEnd();
		FString Sub = Rest;
		FString Arg;
		Rest.Split(TEXT(" "), &Sub, &Arg);
		Arg = Arg.TrimStartAndEnd();
		if (!Friends)
		{
			ClientReceiveChat(FString(), TEXT("Arkadas listesi su an kullanilamaz."), 2);
		}
		else if (Sub.IsEmpty() || Sub.Equals(TEXT("liste"), ESearchCase::IgnoreCase))
		{
			Friends->CmdList();
		}
		else if (Sub.Equals(TEXT("ekle"), ESearchCase::IgnoreCase))
		{
			Friends->CmdAdd(Arg);
		}
		else if (Sub.Equals(TEXT("sil"), ESearchCase::IgnoreCase))
		{
			Friends->CmdRemove(Arg);
		}
		else
		{
			ClientReceiveChat(FString(), TEXT("Kullanim: /arkadas ekle isim, /arkadas sil isim, /arkadas liste"), 2);
		}
		return;
	}
	// Klan sohbeti: "/g mesaj" (yalniz ayni klan adindaki cevrimici oyunculara).
	if (Msg.Equals(TEXT("/g"), ESearchCase::IgnoreCase))
	{
		return; // bos klan mesaji: genel sohbete "/g" olarak sizmasin
	}
	if (Msg.StartsWith(TEXT("/g "), ESearchCase::IgnoreCase))
	{
		const FString ClanText = Msg.RightChop(3).TrimStart().Left(120);
		if (ClanText.IsEmpty())
		{
			return;
		}
		LastChatServerTime = Now;
		if (const AMMOCharacter* Me = Cast<AMMOCharacter>(GetPawn()))
		{
			if (UMMOClanComponent* Clan = Me->GetClanComponent())
			{
				Clan->CmdChat(ClanText);
				return;
			}
		}
		ClientReceiveChat(FString(), TEXT("Klan su an kullanilamaz."), 2);
		return;
	}
	if (Msg.StartsWith(TEXT("/p "), ESearchCase::IgnoreCase))
	{
		Channel = 1;
		Msg = Msg.RightChop(3).TrimStart();
	}

	// Fisilti: "/w Isim mesaj" (ya da /f). Hedef ada gore bulunur; yalniz hedefe ve gondericiye gider.
	if (Msg.StartsWith(TEXT("/w "), ESearchCase::IgnoreCase) || Msg.StartsWith(TEXT("/f "), ESearchCase::IgnoreCase))
	{
		FString Rest = Msg.RightChop(3).TrimStart();
		FString TargetName, WhisperText;
		if (!Rest.Split(TEXT(" "), &TargetName, &WhisperText))
		{
			ClientReceiveChat(FString(), TEXT("Kullanim: /w Isim mesaj"), 2);
			return;
		}
		WhisperText = WhisperText.TrimStart().Left(120);
		if (TargetName.IsEmpty() || WhisperText.IsEmpty())
		{
			ClientReceiveChat(FString(), TEXT("Kullanim: /w Isim mesaj"), 2);
			return;
		}
		LastChatServerTime = Now;

		const FString From = PlayerState ? PlayerState->GetPlayerName() : FString(TEXT("?"));
		AMMOPlayerController* Target = nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AMMOPlayerController* PC = Cast<AMMOPlayerController>(It->Get());
			if (PC && PC != this && PC->PlayerState && PC->PlayerState->GetPlayerName().Equals(TargetName, ESearchCase::IgnoreCase))
			{
				Target = PC;
				break;
			}
		}
		if (!Target)
		{
			ClientReceiveChat(FString(), FString::Printf(TEXT("%s adli oyuncu bulunamadi."), *TargetName), 2);
			return;
		}
		Target->ClientReceiveChat(From, WhisperText, 3);
		ClientReceiveChat(Target->PlayerState->GetPlayerName(), WhisperText, 4);
		return;
	}
	Msg = Msg.Left(120);
	if (Msg.IsEmpty())
	{
		return;
	}
	LastChatServerTime = Now;

	const FString Sender = PlayerState ? PlayerState->GetPlayerName() : FString(TEXT("?"));

	if (Channel == 1)
	{
		AMMOCharacter* Me = Cast<AMMOCharacter>(GetPawn());
		UMMOPartyComponent* Party = Me ? Me->GetPartyComponent() : nullptr;
		if (!Party || !Party->IsInParty())
		{
			ClientReceiveChat(FString(), TEXT("Bir partide degilsin; mesaj gonderilmedi."), 2);
			return;
		}
		TArray<AMMOCharacter*> Members;
		Party->GetPartyInRange(Me->GetActorLocation(), 1.0e7f, Members);
		Members.AddUnique(Me);
		for (AMMOCharacter* M : Members)
		{
			if (AMMOPlayerController* PC = M ? Cast<AMMOPlayerController>(M->GetController()) : nullptr)
			{
				PC->ClientReceiveChat(Sender, Msg, 1);
			}
		}
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (AMMOPlayerController* PC = Cast<AMMOPlayerController>(It->Get()))
		{
			PC->ClientReceiveChat(Sender, Msg, 0);
		}
	}
}

void AMMOPlayerController::ClientReceiveChat_Implementation(const FString& Sender, const FString& Message, uint8 Channel)
{
	AMMOHUD* HUD = Cast<AMMOHUD>(GetHUD());
	if (!HUD)
	{
		return;
	}
	FLinearColor Col = FLinearColor::White;
	FString Line;
	if (Channel == 2)
	{
		Col = FLinearColor(1.f, 0.65f, 0.2f);
		Line = Message;
	}
	else if (Channel == 1)
	{
		Col = FLinearColor(0.45f, 0.85f, 1.f);
		Line = FString::Printf(TEXT("[Parti] %s: %s"), *Sender, *Message);
	}
	else if (Channel == 3)
	{
		Col = FLinearColor(1.f, 0.55f, 0.85f);
		Line = FString::Printf(TEXT("[%s fisildiyor] %s"), *Sender, *Message);
	}
	else if (Channel == 4)
	{
		Col = FLinearColor(1.f, 0.55f, 0.85f);
		Line = FString::Printf(TEXT("[%s adli oyuncuya] %s"), *Sender, *Message);
	}
	else if (Channel == 5)
	{
		Col = FLinearColor(0.4f, 0.9f, 0.4f);
		Line = FString::Printf(TEXT("[Klan] %s: %s"), *Sender, *Message);
	}
	else
	{
		Line = FString::Printf(TEXT("%s: %s"), *Sender, *Message);
	}
	HUD->AddChatLine(Line, Col);
}

void AMMOPlayerController::OnChatKey()
{
	if (!ChatBox.IsValid())
	{
		bChatOpenRequested = true;
	}
}

void AMMOPlayerController::OpenChatInput()
{
	if (!IsLocalController() || ChatBox.IsValid() || !FSlateApplication::IsInitialized())
	{
		return;
	}
	UWorld* World = GetWorld();
	UGameViewportClient* VC = World ? World->GetGameViewport() : nullptr;
	if (!VC)
	{
		return;
	}

	SAssignNew(ChatContainer, SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(16.f, 0.f, 0.f, 96.f))
		[
			SNew(SBox)
			.WidthOverride(400.f)
			[
				SAssignNew(ChatBox, SEditableTextBox)
				.HintText(FText::FromString(TEXT("Mesaj yaz - Enter gonder, Esc iptal, /p parti, /w isim mesaj")))
				.ClearKeyboardFocusOnCommit(false)
				.OnTextCommitted(FOnTextCommitted::CreateUObject(this, &AMMOPlayerController::OnChatCommitted))
			]
		];
	VC->AddViewportWidgetContent(ChatContainer.ToSharedRef(), 50);
	FSlateApplication::Get().SetKeyboardFocus(ChatBox, EFocusCause::SetDirectly);
}

void AMMOPlayerController::CloseChatInput()
{
	if (ChatContainer.IsValid())
	{
		UWorld* World = GetWorld();
		if (UGameViewportClient* VC = World ? World->GetGameViewport() : nullptr)
		{
			VC->RemoveViewportWidgetContent(ChatContainer.ToSharedRef());
		}
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		}
	}
	ChatContainer.Reset();
	ChatBox.Reset();
}

void AMMOPlayerController::OnChatCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		const FString Msg = Text.ToString().TrimStartAndEnd();
		if (!Msg.IsEmpty())
		{
			ServerSendChat(Msg);
		}
	}
	// Enter, Esc (OnCleared) ya da odak kaybi: her durumda kutuyu kapat.
	bChatCloseRequested = true;
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
		return H->IsSkillBookOpen() || H->IsInventoryOpen() || H->IsShopOpen() || H->IsQuestWindowOpen() || H->IsStatWindowOpen() || H->IsWarehouseOpen() || H->IsTradeOpen();
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
		float MX = 0.f, MY = 0.f;
		if (GetMousePosition(MX, MY) && H->HandleClassChoiceClick(FVector2D(MX, MY)))
		{
			return;
		}
		if (GetMousePosition(MX, MY) && H->HandleZoneTravelClick(FVector2D(MX, MY)))
		{
			return;
		}
		if (H->IsAnyWindowOpen())
		{
			// Tiklama, imlecin ustundeki acik pencereye gider (X ile kapatma dahil); karakter hareket etmez.
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
				// Kisa karsilama replikleri (istemci-tarafi flourish; LLM gerektirmez).
				const FString Greet = Vendor->PickGreeting();
				if (!Greet.IsEmpty())
				{
					H->ShowNpcDialogue(Vendor->VendorName, Greet);
				}
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
				const FString Greet = QuestGiver->PickGreeting();
				if (!Greet.IsEmpty())
				{
					H->ShowNpcDialogue(QuestGiver->NpcName, Greet);
				}
			}
		}
		else if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("Gorevliye yaklas"));
		}
		return;
	}

	// Konusan NPC'ye tiklandi mi? (menzil icindeyse LLM'den replik iste)
	if (AMMOTalkerNPC* Talker = Cast<AMMOTalkerNPC>(Hit.GetActor()))
	{
		APawn* MyPawn = GetPawn();
		if (MyPawn && FVector::Dist(MyPawn->GetActorLocation(), Talker->GetActorLocation()) <= 500.f)
		{
			// Kisa spam korumasi (ust uste tiklamada yeni istek atma).
			const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
			if (Now - LastNpcTalkRequestTime >= 1.5f)
			{
				LastNpcTalkRequestTime = Now;
				ServerRequestNpcDialogue(Talker);
			}
		}
		else if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("NPC'ye yaklas"));
		}
		return;
	}

	// Sifaciya tiklandi mi? (menzil icindeyse sunucudan iyilestirme iste — altin duser)
	if (AMMOHealerNPC* Healer = Cast<AMMOHealerNPC>(Hit.GetActor()))
	{
		APawn* MyPawn = GetPawn();
		if (MyPawn && FVector::Dist(MyPawn->GetActorLocation(), Healer->GetActorLocation()) <= 500.f)
		{
			// Kisa spam korumasi (talker'daki desenin ayni si): sol tus basili tutulurken
			// DoLeftClickAction her karede calisiyor; throttle olmadan DoT ile tam-can
			// arasinda gidip gelen oyuncu her karede tekrar altin dusurebiliyordu.
			const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
			if (Now - LastHealRequestTime >= 1.5f)
			{
				LastHealRequestTime = Now;
				ServerRequestHeal(Healer);
			}
		}
		else if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("Sifaciya yaklas"));
		}
		return;
	}

	// Depocuya tiklandi mi? (menzil icindeyse depo penceresini ac; tasima sunucuda dogrulanir)
	if (AMMOWarehouseNPC* Keeper = Cast<AMMOWarehouseNPC>(Hit.GetActor()))
	{
		APawn* MyPawn = GetPawn();
		if (MyPawn && FVector::Dist(MyPawn->GetActorLocation(), Keeper->GetActorLocation()) <= 500.f)
		{
			if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
			{
				H->OpenWarehouse();
				const FString Greet = Keeper->PickGreeting();
				if (!Greet.IsEmpty())
				{
					H->ShowNpcDialogue(Keeper->WarehouseName, Greet);
				}
			}
		}
		else if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("Depocuya yaklas"));
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

	// Depo penceresi: depocudan uzaklasinca (sunucu tasimayi reddeder) kendiliginden kapansin.
	if (AMMOHUD* WH = Cast<AMMOHUD>(GetHUD()); WH && WH->IsWarehouseOpen())
	{
		bool bNear = false;
		if (const APawn* WP = GetPawn())
		{
			for (TActorIterator<AMMOWarehouseNPC> It(GetWorld()); It; ++It)
			{
				if (FVector::DistSquared(WP->GetActorLocation(), It->GetActorLocation()) <= 700.f * 700.f)
				{
					bNear = true;
					break;
				}
			}
		}
		if (!bNear)
		{
			WH->CloseWarehouse();
		}
	}

	// Sohbet kutusu ac/kapa istekleri (callback/tus olayinin disinda guvenle).
	if (bChatCloseRequested)
	{
		bChatCloseRequested = false;
		CloseChatInput();
	}
	if (bChatOpenRequested)
	{
		bChatOpenRequested = false;
		OpenChatInput();
	}

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

	// Olumden onceki zemin tiklamasi veya otomatik saldiri, canlanmadan sonra
	// oyuncuyu eski tehlikeli konuma geri goturmesin. Her istemci kendi yerel
	// hareket hedefini temizler; sunucu canlanma/hasar kararlarini ayri verir.
	if (const AMMOCharacter* MeChar = Cast<AMMOCharacter>(P))
	{
		if (const UMMOStatsComponent* MyStats = MeChar->GetStats(); MyStats && MyStats->bIsDead)
		{
			TargetEnemy.Reset();
			SelectedTarget.Reset();
			bHasMoveTarget = false;
			bLeftPressed = false;
			StopMovement();
			return;
		}
	}

	// --- WASD klavye hareketi (KO tarzi: W/S ileri-geri, A/D yerinde DONUS) ---
	// Enhanced Input mapping'lerini temizledigimiz icin WASD'yi burada C++ ile suruyoruz.
	// W/S = karakterin baktigi yonde ileri/geri. A/D = karakteri sola/saga dondur (yana KAYMAZ).
	if (!bBookOpen && !ChatBox.IsValid())
	{
		float ForwardAxis = 0.f;
		float TurnAxis = 0.f;
		if (IsInputKeyDown(EKeys::W)) { ForwardAxis += 1.f; }
		if (IsInputKeyDown(EKeys::S)) { ForwardAxis -= 1.f; }
		if (IsInputKeyDown(EKeys::D)) { TurnAxis += 1.f; }  // saga don
		if (IsInputKeyDown(EKeys::A)) { TurnAxis -= 1.f; }  // sola don

		if (ForwardAxis != 0.f || TurnAxis != 0.f)
		{
			// Hareket yonune donmeyi kapat ki donusu biz kontrol edelim, ileri giderken kaymasin.
			SetOrientToMovement(false);

			// Klavye kontrolu tikla-git yolunu ve hedefi iptal eder (manuel kontrol oncelikli).
			StopMovement();
			bHasMoveTarget = false;
			TargetEnemy.Reset();

			// A/D: karakteri yerinde dondur (yaw). Yana kayma yok.
			if (TurnAxis != 0.f)
			{
				const float YawSpeed = 140.f; // derece/sn
				P->AddActorWorldRotation(FRotator(0.f, TurnAxis * YawSpeed * DeltaTime, 0.f));
			}

			// W/S: karakterin (donmus) bakis yonunde ileri/geri.
			if (ForwardAxis != 0.f)
			{
				const FRotator YawRot(0.f, P->GetActorRotation().Yaw, 0.f);
				const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
				P->AddMovementInput(Forward, ForwardAxis);
			}
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
		// Oyuncu oldu/canlanma korumasinda ise otomatik saldiri hedefini birak (yoksa canlanir
		// canlanmaz mob'a yuruyup korumayi kendisi bitirir - olum dongusu geri gelir).
		const AMMOCharacter* MeChar = Cast<AMMOCharacter>(P);
		const bool bMeDeadOrProtected = MeChar && ((MeChar->GetStats() && MeChar->GetStats()->bIsDead) || MeChar->IsSpawnProtected());
		if (!Enemy->GetStats() || Enemy->GetStats()->bIsDead || bMeDeadOrProtected)
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

void AMMOPlayerController::ServerRequestNpcDialogue_Implementation(AMMOTalkerNPC* Npc)
{
	if (!Npc)
	{
		return;
	}

	// Sunucu-otoriter mesafe dogrulamasi (istemciye guvenme).
	APawn* MyPawn = GetPawn();
	if (!MyPawn || FVector::Dist(MyPawn->GetActorLocation(), Npc->GetActorLocation()) > 700.f)
	{
		return;
	}

	// Sunucu-otoriter hiz siniri: istemci throttle'ina guvenme (hileli istemci
	// RPC'yi bombardiman edip her cagrida ucretli bir LLM istegi tetikleyebilir).
	const float ServerNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (ServerNow - LastNpcTalkServerTime < NpcTalkServerCooldown)
	{
		return;
	}
	LastNpcTalkServerTime = ServerNow;

	// Oyuncu baglami (replige renk katsin).
	FString PlayerInfo = TEXT("bir maceraci");
	if (AMMOCharacter* C = Cast<AMMOCharacter>(MyPawn))
	{
		if (UMMOStatsComponent* St = C->GetStats())
		{
			PlayerInfo = FString::Printf(TEXT("seviye %d bir maceraci"), St->Level);
		}
	}

	const FString SpeakerName = Npc->NpcName;
	const FString FallbackLine = Npc->PickFallbackLine();

	UGameInstance* GI = GetGameInstance();
	UMMODialogueSubsystem* Dialogue = GI ? GI->GetSubsystem<UMMODialogueSubsystem>() : nullptr;
	if (!Dialogue)
	{
		ClientReceiveNpcDialogue(SpeakerName, FallbackLine);
		return;
	}

	// LLM cevabi asenkron gelir; basarisizsa (anahtar/kredi yok) NPC'ye ozgu yedek replige duser.
	TWeakObjectPtr<AMMOPlayerController> WeakThis(this);
	FMMOOnDialogueReady OnReady;
	OnReady.BindLambda([WeakThis, SpeakerName, FallbackLine](bool bSuccess, const FString& Text)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		WeakThis->ClientReceiveNpcDialogue(SpeakerName, bSuccess ? Text : FallbackLine);
	});

	Dialogue->RequestNpcLine(Npc->NpcName, Npc->NpcRole, PlayerInfo, OnReady);
}

void AMMOPlayerController::ClientReceiveNpcDialogue_Implementation(const FString& SpeakerName, const FString& Text)
{
	if (AMMOHUD* H = Cast<AMMOHUD>(GetHUD()))
	{
		H->ShowNpcDialogue(SpeakerName, Text);
	}
}

void AMMOPlayerController::ServerRequestHeal_Implementation(AMMOHealerNPC* Npc)
{
	// Sunucu-otoriter: iyilestirme ve altin dususu YALNIZCA burada (authority) yapilir.
	if (!Npc)
	{
		return;
	}

	// Mesafeyi sunucuda dogrula (hileli istemci uzaktan iyilestirme isteyemesin).
	APawn* MyPawn = GetPawn();
	if (!MyPawn || FVector::Dist(MyPawn->GetActorLocation(), Npc->GetActorLocation()) > 700.f)
	{
		return;
	}

	// SUNUCU-tarafi throttle (guvenlik, talker'daki desenle ayni): istemci-tarafi kapiya
	// guvenme — hileli istemci bu RPC'yi bombardiman edip her cagrida gercek altin
	// dususu/iyilestirme tetikleyemesin.
	const UWorld* World = GetWorld();
	const float ServerNow = World ? World->GetTimeSeconds() : 0.f;
	if (ServerNow - LastHealServerTime < HealServerCooldown)
	{
		return;
	}
	LastHealServerTime = ServerNow;

	AMMOCharacter* C = Cast<AMMOCharacter>(MyPawn);
	if (!C)
	{
		return;
	}
	UMMOStatsComponent* St = C->GetStats();
	UMMOInventoryComponent* Inv = C->GetInventory();
	if (!St || !Inv)
	{
		return;
	}

	const FString Speaker = Npc->HealerName;

	// Zaten tam saglikliysa (olu degil, can ve mana dolu) altin alma; kibarca gonder.
	const bool bFullHp = St->GetHealthPercent() >= 0.999f;
	// MaxMana<=0 olan karakterde (mana havuzu yok) iyilestirilecek mana yoktur => "dolu" say.
	// Aksi halde GetManaPercent 0 doner, tam canli oyuncu bedavaya altin kaptirir.
	const bool bFullMp = (St->MaxMana <= 0.f) || (St->GetManaPercent() >= 0.999f);
	if (!St->bIsDead && bFullHp && bFullMp)
	{
		ClientReceiveNpcDialogue(Speaker, Npc->AlreadyHealedLine);
		return;
	}

	// Yeterli altin var mi?
	const int32 Cost = FMath::Max(0, Npc->HealCost);
	if (Inv->Gold < Cost)
	{
		ClientReceiveNpcDialogue(Speaker, Npc->InsufficientGoldLine);
		return;
	}

	// Altini dus ve tam iyilestir (Heal/RestoreMana clamp'li; olu ise dirilt).
	Inv->AddGold(-Cost);
	if (St->bIsDead)
	{
		St->Revive();
	}
	else
	{
		St->Heal(St->MaxHealth);
		St->RestoreMana(St->MaxMana);
	}

	// Onay balonu: sifacinin sabit repliklerinden biri (istemciye ozel gonderilir).
	ClientReceiveNpcDialogue(Speaker, Npc->PickGreeting());
}

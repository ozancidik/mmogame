// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/MMOLootPickup.h"
#include "Characters/MMOCharacter.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOStatsComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/MMOPlayerController.h"
#include "Items/MMOItemDatabase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Core/MMONotify.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#if !UE_BUILD_SHIPPING
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#endif

#if !UE_BUILD_SHIPPING
bool AMMOLootPickup::RunFirstLootSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{ OutError = TEXT("server game world required"); return false; }
	TArray<AActor*> Fixtures;
	TArray<AMMOPlayerController*> Controllers;
	const bool bPassed = [&]()
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AMMOCharacter* Pawns[3] = {};
		AMMOLootPickup* Loot[3] = {};
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const FVector Location(1000000 + Index * 5000, 1000000, 5000);
			AMMOPlayerController* PC = World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(), Location, FRotator::ZeroRotator, Params);
			Pawns[Index] = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
			Loot[Index] = World->SpawnActor<AMMOLootPickup>(AMMOLootPickup::StaticClass(), Location + FVector(2000, 0, 0), FRotator::ZeroRotator, Params);
			if (PC) { Fixtures.Add(PC); Controllers.Add(PC); }
			if (Pawns[Index]) Fixtures.Add(Pawns[Index]);
			if (Loot[Index]) { Fixtures.Add(Loot[Index]); Loot[Index]->SetActorEnableCollision(false); }
			if (!PC || !Pawns[Index] || !Loot[Index]) { OutError = TEXT("fixture spawn failed"); return false; }
			PC->Possess(Pawns[Index]);
		}
		auto Check = [&](bool Condition, const TCHAR* Label)
		{
			if (!Condition) { OutError = Label; return false; }
			++OutChecks; return true;
		};
		UMMOInventoryComponent* GoldInv = Pawns[0]->GetInventory();
		UMMOInventoryComponent* ItemInv = Pawns[1]->GetInventory();
		UMMOInventoryComponent* EquipInv = Pawns[2]->GetInventory();
		if (!GoldInv || !ItemInv || !EquipInv) { OutError = TEXT("fixture inventory missing"); return false; }
		Loot[0]->SetupAsGold(17);
		GoldInv->Gold = UMMOInventoryComponent::MaxGold;
		Loot[0]->TryCollect(Pawns[0]);
		if (!Check(GoldInv->Gold == UMMOInventoryComponent::MaxGold, TEXT("capped gold changed balance"))) return false;
		AMMOLootPickup* SecondGold = World->SpawnActor<AMMOLootPickup>(AMMOLootPickup::StaticClass(), FVector(1020000, 1000000, 5000), FRotator::ZeroRotator, Params);
		if (!SecondGold) { OutError = TEXT("second gold spawn failed"); return false; }
		Fixtures.Add(SecondGold); SecondGold->SetActorEnableCollision(false); SecondGold->SetupAsGold(17);
		GoldInv->Gold -= 2;
		SecondGold->TryCollect(Pawns[0]); SecondGold->TryCollect(Pawns[0]);
		if (!Check(GoldInv->Gold == UMMOInventoryComponent::MaxGold && SecondGold->bCollected, TEXT("delivered gold or duplicate incorrect"))) return false;
		Loot[1]->SetupAsItem(TEXT("HealthPotion"), 3);
		ItemInv->AddItem(TEXT("HealthPotion"), UMMOInventoryComponent::MaxStackSize);
		Loot[1]->TryCollect(Pawns[1]);
		if (!Check(!Loot[1]->bCollected && Loot[1]->ItemCount == 3, TEXT("full stack collected"))) return false;
		ItemInv->RemoveItem(TEXT("HealthPotion"), 1);
		Loot[1]->TryCollect(Pawns[1]);
		const bool bPartialOK = !Loot[1]->bCollected && Loot[1]->ItemCount == 2 && ItemInv->GetItemCount(TEXT("HealthPotion")) == UMMOInventoryComponent::MaxStackSize;
		Loot[1]->TryCollect(Pawns[1]); // No new room.
		ItemInv->RemoveItem(TEXT("HealthPotion"), 2);
		Loot[1]->TryCollect(Pawns[1]); Loot[1]->TryCollect(Pawns[1]);
		if (!Check(bPartialOK && Loot[1]->bCollected && ItemInv->GetItemCount(TEXT("HealthPotion")) == UMMOInventoryComponent::MaxStackSize,
			TEXT("partial pickup remainder or duplicate incorrect"))) return false;
		Loot[2]->SetupAsEquipment(TEXT("ShortSword"), 0);
		for (int32 Index = 0; Index < UMMOInventoryComponent::MaxBagSize; ++Index) EquipInv->AddEquipmentInstance(TEXT("ShortSword"));
		Loot[2]->TryCollect(Pawns[2]);
		if (!Check(!Loot[2]->bCollected && EquipInv->EquipmentBag.Num() == UMMOInventoryComponent::MaxBagSize, TEXT("full equipment bag collected"))) return false;
		EquipInv->EquipmentBag.RemoveAt(0);
		Loot[2]->TryCollect(Pawns[2]); Loot[2]->TryCollect(Pawns[2]);
		return Check(Loot[2]->bCollected && EquipInv->EquipmentBag.Num() == UMMOInventoryComponent::MaxBagSize, TEXT("equipment grant or duplicate incorrect"));
	}();
	for (AMMOPlayerController* PC : Controllers) { if (IsValid(PC)) PC->UnPossess(); }
	for (AActor* Fixture : Fixtures) { if (IsValid(Fixture)) Fixture->Destroy(); }
	return bPassed; // Same frame: no player load timer/disk save or physical overlap.
}

namespace
{
	struct FMMOFirstLootTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestFirstLoot"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = AMMOLootPickup::RunFirstLootSelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_FIRST_LOOT_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOFirstLootTestExec GMMOFirstLootTestExec;
}

bool AMMOLootPickup::RunLifeStateSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0;
	OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{
		OutError = TEXT("server game world required");
		return false;
	}
	TArray<AActor*> Fixtures;
	AMMOPlayerController* Controller = nullptr;
	const bool bPassed = [&]()
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector Location(1000000, 1000000, 5000);
		AMMOCharacter* Character = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
		Controller = World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(), Location, FRotator::ZeroRotator, Params);
		if (Character) Fixtures.Add(Character);
		if (Controller) Fixtures.Add(Controller);
		if (!Character || !Controller) { OutError = TEXT("fixture spawn failed"); return false; }
		Controller->Possess(Character);
		UMMOStatsComponent* Stats = Character->GetStats();
		UMMOInventoryComponent* Inv = Character->GetInventory();
		if (!Stats || !Inv || !Character->IsPlayerControlled()) { OutError = TEXT("fixture possession failed"); return false; }
		auto Check = [&](bool Condition, const TCHAR* Label)
		{
			if (!Condition) { OutError = Label; return false; }
			++OutChecks;
			return true;
		};
		auto SpawnLoot = [&]()
		{
			AMMOLootPickup* Loot = World->SpawnActor<AMMOLootPickup>(AMMOLootPickup::StaticClass(), Location + FVector(5000, 0, 0), FRotator::ZeroRotator, Params);
			if (Loot) { Fixtures.Add(Loot); Loot->SetActorEnableCollision(false); }
			return Loot;
		};
		AMMOLootPickup* Gold = SpawnLoot();
		AMMOLootPickup* Item = SpawnLoot();
		AMMOLootPickup* Equipment = SpawnLoot();
		if (!Gold || !Item || !Equipment) { OutError = TEXT("loot fixture spawn failed"); return false; }
		Gold->SetupAsGold(17);
		Item->SetupAsItem(TEXT("HealthPotion"), 3);
		Equipment->SetupAsEquipment(TEXT("ShortSword"), 0);
		const int32 BeforeGold = Inv->Gold, BeforeItems = Inv->GetItemCount(TEXT("HealthPotion")), BeforeBag = Inv->EquipmentBag.Num();
		Stats->bIsDead = true;
		Gold->TryCollect(Character);
		if (!Check(!Gold->bCollected && IsValid(Gold) && Inv->Gold == BeforeGold, TEXT("dead gold collected"))) return false;
		Item->OnOverlap(nullptr, Character, nullptr, 0, false, FHitResult());
		if (!Check(!Item->bCollected && Item->ItemCount == 3 && Inv->GetItemCount(TEXT("HealthPotion")) == BeforeItems, TEXT("dead item collected"))) return false;
		Equipment->TryCollect(Character);
		if (!Check(!Equipment->bCollected && Inv->EquipmentBag.Num() == BeforeBag, TEXT("dead equipment collected"))) return false;
		// Rezervasyonu bitmis pickup da ayni toplama kapisindan gecer.
		Gold->SetReserved({Character}, 1.f);
		Gold->ReservedUntilTime = World->GetTimeSeconds() - 1.0;
		Gold->TryCollect(Character);
		if (!Check(!Gold->bCollected && Inv->Gold == BeforeGold, TEXT("dead expired reservation collected"))) return false;
		Stats->Revive();
		Gold->TryCollect(Character);
		if (!Check(Gold->bCollected && Inv->Gold == BeforeGold + 17, TEXT("revived gold missing"))) return false;
		Gold->TryCollect(Character);
		if (!Check(Inv->Gold == BeforeGold + 17, TEXT("duplicate gold"))) return false;
		Item->TryCollect(Character);
		if (!Check(Item->bCollected && Inv->GetItemCount(TEXT("HealthPotion")) == BeforeItems + 3, TEXT("revived item missing"))) return false;
		Item->TryCollect(Character);
		if (!Check(Inv->GetItemCount(TEXT("HealthPotion")) == BeforeItems + 3, TEXT("duplicate item"))) return false;
		Equipment->TryCollect(Character);
		if (!Check(Equipment->bCollected && Inv->EquipmentBag.Num() == BeforeBag + 1, TEXT("revived equipment missing"))) return false;
		Equipment->TryCollect(Character);
		return Check(Inv->EquipmentBag.Num() == BeforeBag + 1, TEXT("duplicate equipment"));
	}();
	// Ayni senkron cagri icinde temizle: 0.6sn yukleme/60sn autosave timer'i calismaz.
	if (IsValid(Controller)) Controller->UnPossess();
	for (AActor* Fixture : Fixtures) { if (IsValid(Fixture)) Fixture->Destroy(); }
	return bPassed;
}

namespace
{
	struct FMMOLootLifeStateTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestLootLifeState"))) return false;
			FString Error;
			int32 Checks = 0;
			const bool bPassed = AMMOLootPickup::RunLifeStateSelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_LOOT_LIFE_STATE_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOLootLifeStateTestExec GMMOLootLifeStateTestExec;
}
#endif

namespace
{
	// "+N altin" bildirimi: ayni oyuncunun kisa surede topladigi altinlar TEK mesajda toplanir
	// (ToOwner uzak istemciye sohbet satiri birakir; her pickup'ta ayri satir spam olurdu).
	struct FPendingGoldNotify
	{
		int64 Sum = 0;
	};
	TMap<TWeakObjectPtr<AMMOCharacter>, FPendingGoldNotify> GPendingGoldNotify;
	constexpr float GoldNotifyWindow = 0.5f;

	void QueueGoldNotify(AMMOCharacter* Char, int32 Amount)
	{
		if (!Char || Amount <= 0)
		{
			return;
		}
		const TWeakObjectPtr<AMMOCharacter> Weak(Char);
		const bool bTimerRunning = GPendingGoldNotify.Contains(Weak);
		GPendingGoldNotify.FindOrAdd(Weak).Sum += Amount;
		if (bTimerRunning)
		{
			return; // pencere acik, bu miktar pencere sonunda gonderilir
		}
		UWorld* World = Char->GetWorld();
		if (!World)
		{
			GPendingGoldNotify.Remove(Weak);
			return;
		}
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak]()
		{
			const FPendingGoldNotify Done = GPendingGoldNotify.FindRef(Weak);
			GPendingGoldNotify.Remove(Weak);
			if (AMMOCharacter* C = Weak.Get(); C && Done.Sum > 0)
			{
				MMONotify::ToOwner(C, FString::Printf(TEXT("+%lld altin"), Done.Sum), FColor(255, 210, 0), 2.f);
			}
		}), GoldNotifyWindow, false);
	}
}

AMMOLootPickup::AMMOLootPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// Overlap alani (kok bilesen).
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(90.f);
	Collision->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Collision->SetGenerateOverlapEvents(true);
	RootComponent = Collision;

	// Gorunur mesh.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.4f));
}

void AMMOLootPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMMOLootPickup, bIsGold);
	DOREPLIFETIME(AMMOLootPickup, bIsEquipment);
}

void AMMOLootPickup::OnRep_LootVisual()
{
	ApplyVisual();
}

void AMMOLootPickup::SetReserved(const TArray<AMMOCharacter*>& Owners, float Seconds)
{
	ReservedFor.Reset();
	for (AMMOCharacter* O : Owners)
	{
		if (O)
		{
			ReservedFor.Add(O);
		}
	}
	ReservedSeconds = ReservedFor.Num() > 0 ? FMath::Max(0.f, Seconds) : 0.f;
}

void AMMOLootPickup::SetupAsGold(int32 Amount)
{
	bIsGold = true;
	GoldAmount = FMath::Max(1, Amount);
	ItemId = NAME_None;
}

void AMMOLootPickup::SetupAsItem(FName InItemId, int32 Count)
{
	bIsGold = false;
	bIsEquipment = false;
	ItemId = InItemId;
	ItemCount = FMath::Max(1, Count);
}

void AMMOLootPickup::SetupAsEquipment(FName InItemId, int32 InPlus, EMMOItemRarity InRarity)
{
	bIsGold = false;
	bIsEquipment = true;
	EquipItemId = InItemId;
	EquipPlus = FMath::Max(0, InPlus);
	EquipRarity = InRarity;
}

void AMMOLootPickup::BeginPlay()
{
	Super::BeginPlay();

	ApplyVisual();

	if (HasAuthority())
	{
		Collision->OnComponentBeginOverlap.AddDynamic(this, &AMMOLootPickup::OnOverlap);
		SetLifeSpan(LifeSpanSeconds);
		// Rezervasyon: sure dolunca ortadaki baska oyuncular icin yeniden dene (BeginOverlap tekrar tetiklenmez).
		if (ReservedSeconds > 0.f && GetWorld())
		{
			ReservedUntilTime = GetWorld()->GetTimeSeconds() + ReservedSeconds;
			GetWorldTimerManager().SetTimer(ReservationTimer, this, &AMMOLootPickup::OnReservationExpired,
				ReservedSeconds + 0.1f, false);
		}
	}
}

void AMMOLootPickup::ApplyVisual()
{
	if (!Mesh)
	{
		return;
	}

	// Altin -> sari, ekipman -> yesil, item -> mavi.
	FLinearColor Color;
	if (bIsGold)
	{
		Color = FLinearColor(1.f, 0.82f, 0.f);
	}
	else if (bIsEquipment)
	{
		Color = FLinearColor(0.2f, 0.9f, 0.25f);
	}
	else
	{
		Color = FLinearColor(0.15f, 0.55f, 1.f);
	}
	Mesh->SetRelativeScale3D(bIsGold ? FVector(0.4f) : FVector(0.5f));

	if (UMaterialInterface* Base = Mesh->GetMaterial(0))
	{
		if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
			Mesh->SetMaterial(0, MID);
		}
	}
}

void AMMOLootPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Basit "juice": donme + yukari-asagi salinim.
	BobTime += DeltaSeconds;
	if (Mesh)
	{
		Mesh->AddLocalRotation(FRotator(0.f, 120.f * DeltaSeconds, 0.f));
		const float BobZ = FMath::Sin(BobTime * 3.f) * 8.f;
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, 20.f + BobZ));
	}
}

void AMMOLootPickup::OnOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor, UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	TryCollect(Cast<AMMOCharacter>(OtherActor));
}

void AMMOLootPickup::OnReservationExpired()
{
	if (!HasAuthority() || bCollected || !Collision)
	{
		return;
	}
	// Rezervasyon bitti: pickup'in uzerinde duran oyuncular (BeginOverlap daha once reddedilmisti) simdi toplayabilir.
	TArray<AActor*> Overlapping;
	Collision->GetOverlappingActors(Overlapping, AMMOCharacter::StaticClass());
	for (AActor* A : Overlapping)
	{
		TryCollect(Cast<AMMOCharacter>(A));
		if (bCollected)
		{
			break;
		}
	}
}

void AMMOLootPickup::TryCollect(AMMOCharacter* Char)
{
	if (!HasAuthority())
	{
		return;
	}

	// Sadece oyuncu toplayabilir (moblar degil).
	if (!Char || !Char->IsPlayerControlled())
	{
		return;
	}
	// Ceset overlap'i veya rezervasyon bitimi ganimet veremez. Tum loot turleri
	// ayni sunucu kapisini kullanir; reddedilen pickup normal omru boyunca yerde kalir.
	const UMMOStatsComponent* Stats = Char->GetStats();
	if (!Stats || Stats->bIsDead)
	{
		return;
	}

	// Ganimet rezervasyonu: sure dolana kadar yalniz hak sahipleri (XP payi olan oyuncular) toplayabilir.
	if (ReservedFor.Num() > 0 && GetWorld() && GetWorld()->GetTimeSeconds() < ReservedUntilTime)
	{
		bool bAllowed = false;
		for (const TWeakObjectPtr<AMMOCharacter>& W : ReservedFor)
		{
			if (W.Get() == Char)
			{
				bAllowed = true;
				break;
			}
		}
		if (!bAllowed)
		{
			return;
		}
	}

	UMMOInventoryComponent* Inv = Char->GetInventory();
	if (!Inv)
	{
		return;
	}

	// Tek-sefer guard: Destroy() ayni frame'de senkron degil; grant'tan ONCE isaretle ve
	// bir daha overlap event'i uretmesin diye collision'i kapat (dupe onlemi).
	if (bCollected)
	{
		return;
	}
	// Canta doluysa AddEquipmentInstance esyayi eklemez. Pickup'i yerde birak ki
	// oyuncu yer actiktan sonra tekrar toplayabilsin.
	if (bIsEquipment && Inv->EquipmentBag.Num() >= UMMOInventoryComponent::MaxBagSize)
	{
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		double& LastTime = LastBagFullNotifyTime.FindOrAdd(TWeakObjectPtr<AMMOCharacter>(Char), -BagFullNotifyInterval);
		if (Now - LastTime >= BagFullNotifyInterval)
		{
			LastTime = Now;
			MMONotify::ToOwner(Char, TEXT("Canta dolu!"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		}
		return;
	}
	// Sarf malzemesi: yigin tavani (999) dolarsa fazlasi yok edilmez. Sigdigi kadar alinir,
	// kalan miktar yerde durur (tek-sefer isareti yalniz TAMAMI alininca konur). Ayni fonksiyon
	// icinde senkron oldugu icin dupe riski yok (bCollected yukarida denetlendi).
	if (!bIsGold && !bIsEquipment)
	{
		const int32 Added = Inv->AddItem(ItemId, ItemCount);
		if (Added > 0)
		{
			if (AMMOPlayerController* PC = Cast<AMMOPlayerController>(Char->GetController()))
				PC->RecordFirstLootMetric(TEXT("item"), ItemId, Added);
			MMONotify::ToOwner(Char, FString::Printf(TEXT("+%d %s"), Added, *ItemId.ToString()), FColor(60, 140, 255), 2.f);
		}
		if (Added < ItemCount)
		{
			ItemCount -= Added;
			const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
			double& LastTime = LastBagFullNotifyTime.FindOrAdd(TWeakObjectPtr<AMMOCharacter>(Char), -BagFullNotifyInterval);
			if (Now - LastTime >= BagFullNotifyInterval)
			{
				LastTime = Now;
				MMONotify::ToOwner(Char, FString::Printf(TEXT("%s yigini dolu (en fazla %d)"),
					*ItemId.ToString(), UMMOInventoryComponent::MaxStackSize), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
			}
			return;
		}
	}
	bCollected = true;
	if (Collision)
	{
		Collision->SetGenerateOverlapEvents(false);
	}

	if (bIsGold)
	{
		const int32 BeforeGold = Inv->Gold;
		Inv->AddGoldFrom(GoldAmount, TEXT("loot"));
		if (AMMOPlayerController* PC = Cast<AMMOPlayerController>(Char->GetController()))
			PC->RecordFirstLootMetric(TEXT("gold"), TEXT("Gold"), Inv->Gold - BeforeGold);
		// Yalniz toplayana (yerelde ekran, uzak istemcide sohbet satiri); kisa surede toplananlar tek mesaj.
		QueueGoldNotify(Char, GoldAmount);
	}
	else if (bIsEquipment)
	{
		const int32 BeforeBag = Inv->EquipmentBag.Num();
		Inv->AddEquipmentInstance(EquipItemId, EquipPlus, EquipRarity);
		if (Inv->EquipmentBag.Num() > BeforeBag)
		{
			if (AMMOPlayerController* PC = Cast<AMMOPlayerController>(Char->GetController()))
				PC->RecordFirstLootMetric(TEXT("equipment"), EquipItemId, Inv->EquipmentBag.Num() - BeforeBag);
		}
		const FLinearColor RC = FMMOItemDatabase::GetRarityColor(EquipRarity);
		MMONotify::ToOwner(Char,
			FString::Printf(TEXT("[%s] %s +%d dustu  (I ile envanteri ac)"),
				*FMMOItemDatabase::GetRarityName(EquipRarity), *EquipItemId.ToString(), EquipPlus),
			RC.ToFColor(true), 3.f);
	}
	// (Sarf malzemesi yukarida, tavan denetimiyle verildi.)

	Destroy();
}

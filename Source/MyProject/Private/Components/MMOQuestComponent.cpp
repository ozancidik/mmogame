// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMOQuestComponent.h"
#include "Characters/MMOCharacter.h"
#include "Components/MMOStatsComponent.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOAchievementComponent.h"
#include "Items/MMOItemDatabase.h"
#include "Save/MMOSaveGame.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Pawn.h"
#include "Core/MMONotify.h"
#include "Core/MMOMetrics.h"
#include "Player/MMOPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "World/MMOQuestGiverNPC.h"
#if !UE_BUILD_SHIPPING
#include "Save/MMOCharacterSaveJson.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#endif

#if !UE_BUILD_SHIPPING
bool UMMOQuestComponent::RunRewardCapacitySelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0;
	OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{
		OutError = TEXT("server game world required");
		return false;
	}
	TArray<AActor*> Fixtures;
	const bool bPassed = [&]()
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector Origin(1000000, 1000000, 5000);
		AMMOCharacter* Character = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Origin, FRotator::ZeroRotator, Params);
		AMMOCharacter* ExpectedCharacter = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Origin + FVector(5000, 0, 0), FRotator::ZeroRotator, Params);
		AMMOQuestGiverNPC* Giver = World->SpawnActor<AMMOQuestGiverNPC>(AMMOQuestGiverNPC::StaticClass(), Origin, FRotator::ZeroRotator, Params);
		if (Character) Fixtures.Add(Character);
		if (ExpectedCharacter) Fixtures.Add(ExpectedCharacter);
		if (Giver) Fixtures.Add(Giver);
		if (!Character || !ExpectedCharacter || !Giver) { OutError = TEXT("fixture spawn failed"); return false; }
		UMMOQuestComponent* Quests = Character->GetQuestComponent();
		UMMOStatsComponent* Stats = Character->GetStats();
		UMMOStatsComponent* ExpectedStats = ExpectedCharacter->GetStats();
		UMMOInventoryComponent* Inv = Character->GetInventory();
		const FMMOQuestDef* Consumable = FMMOQuestDatabase::Find(TEXT("Q_Goblins"));
		const FMMOQuestDef* Equipment = FMMOQuestDatabase::Find(TEXT("Q_Boss"));
		if (!Quests || !Stats || !ExpectedStats || !Inv || !Consumable || !Equipment
			|| Consumable->RewardConsumableId.IsNone() || Consumable->RewardConsumableCount < 2
			|| !Consumable->RewardEquipmentId.IsNone() || !FMMOItemDatabase::Find(Equipment->RewardEquipmentId)
			|| !Equipment->RewardConsumableId.IsNone())
		{ OutError = TEXT("fixture data unavailable"); return false; }
		auto Check = [&](bool Condition, const TCHAR* Label)
		{
			if (!Condition) { OutError = Label; return false; }
			++OutChecks;
			return true;
		};
		auto Prepare = [&](const FMMOQuestDef& Def)
		{
			FMMOCharacterSaveData Seed;
			Seed.Level = FMath::Max(1, Def.RequiredLevel);
			Stats->ApplySaveData(Seed);
			ExpectedStats->ApplySaveData(Seed);
			Inv->Items.Reset();
			Inv->EquipmentBag.Reset();
			Inv->Gold = 1000;
			Quests->ActiveQuests.Reset();
			Quests->CompletedQuests.Reset();
			if (!Def.PrereqQuestId.IsNone()) Quests->CompletedQuests.Add(Def.PrereqQuestId);
			FMMOQuestProgress Ready;
			Ready.QuestId = Def.QuestId;
			Ready.KillProgress = Def.TargetCount;
			Quests->ActiveQuests.Add(Ready);
		};
		auto Snapshot = [&]()
		{
			FMMOCharacterSaveData Data;
			Stats->FillSaveData(Data);
			Inv->FillSaveData(Data);
			Quests->FillSaveData(Data);
			return Data;
		};
		auto UnchangedClaim = [&](const FMMOQuestDef& Def, const TCHAR* Label)
		{
			const FMMOCharacterSaveData Before = Snapshot();
			const float Health = Stats->Health, Mana = Stats->Mana;
			const bool bDead = Stats->bIsDead;
			Quests->ClaimReward(Def.QuestId);
			TArray<FString> Differences;
			FMMOCharacterSaveJson::FindDifferences(Before, Snapshot(), Differences);
			return Check(Differences.IsEmpty() && Stats->Health == Health && Stats->Mana == Mana
				&& Stats->bIsDead == bDead, Label);
		};
		auto CompleteClaim = [&](const FMMOQuestDef& Def, int32 ExpectedItems, int32 ExpectedBagSize, const TCHAR* Label)
		{
			ExpectedStats->GainExperience(Def.RewardXP);
			Quests->ClaimReward(Def.QuestId);
			bool bRewardOK = Stats->Level == ExpectedStats->Level && Stats->Experience == ExpectedStats->Experience
				&& Stats->UnspentStatPoints == ExpectedStats->UnspentStatPoints
				&& Stats->UnspentSkillPoints == ExpectedStats->UnspentSkillPoints
				&& Inv->Gold == 1000 + Def.RewardGold && Inv->EquipmentBag.Num() == ExpectedBagSize
				&& Quests->IsCompleted(Def.QuestId) && !Quests->IsQuestActive(Def.QuestId);
			if (!Def.RewardConsumableId.IsNone()) bRewardOK &= Inv->GetItemCount(Def.RewardConsumableId) == ExpectedItems;
			if (!Def.RewardEquipmentId.IsNone() && !Inv->EquipmentBag.IsEmpty())
			{
				const FMMOEquipmentInstance& Granted = Inv->EquipmentBag.Last();
				bRewardOK &= Granted.IsValidInstance() && Granted.ItemId == Def.RewardEquipmentId
					&& Granted.Plus == 0 && Granted.Rarity == EMMOItemRarity::Common;
			}
			return Check(bRewardOK, Label);
		};
		auto FillStacks = [&](int32 Count)
		{
			for (int32 Index = 0; Index < Count; ++Index)
				Inv->AddItem(FName(*FString::Printf(TEXT("QuestCapacityFixture%d"), Index)), 1);
		};
		// Hazir gorev bellekten kurulur; gercek av/kabul/RPC veya oyuncu dosyasi testi degildir.
		Prepare(*Consumable);
		Inv->AddItem(Consumable->RewardConsumableId, UMMOInventoryComponent::MaxStackSize);
		if (!UnchangedClaim(*Consumable, TEXT("full stack changed reward or quest"))) return false;
		Prepare(*Consumable);
		Inv->AddItem(Consumable->RewardConsumableId, UMMOInventoryComponent::MaxStackSize - Consumable->RewardConsumableCount + 1);
		if (!UnchangedClaim(*Consumable, TEXT("partial room granted partial reward"))) return false;
		Inv->RemoveItem(Consumable->RewardConsumableId, 1);
		if (!CompleteClaim(*Consumable, UMMOInventoryComponent::MaxStackSize, 0, TEXT("exact stack fit reward incomplete"))) return false;
		if (!UnchangedClaim(*Consumable, TEXT("duplicate consumable reward changed state"))) return false;
		Prepare(*Consumable);
		FillStacks(UMMOInventoryComponent::MaxItemStacks);
		if (!UnchangedClaim(*Consumable, TEXT("full stack list changed reward or quest"))) return false;
		Inv->Items.RemoveAt(0);
		if (!CompleteClaim(*Consumable, Consumable->RewardConsumableCount, 0, TEXT("freed stack slot reward incomplete"))) return false;
		if (Inv->Items.Num() != UMMOInventoryComponent::MaxItemStacks) { OutError = TEXT("freed slot count incorrect"); return false; }
		Prepare(*Consumable);
		FillStacks(UMMOInventoryComponent::MaxItemStacks - 1);
		Inv->AddItem(Consumable->RewardConsumableId, UMMOInventoryComponent::MaxStackSize - Consumable->RewardConsumableCount);
		if (!CompleteClaim(*Consumable, UMMOInventoryComponent::MaxStackSize, 0, TEXT("existing stack in full list not rewarded"))) return false;
		if (Inv->Items.Num() != UMMOInventoryComponent::MaxItemStacks) { OutError = TEXT("existing stack created extra slot"); return false; }
		Prepare(*Equipment);
		for (int32 Index = 0; Index < UMMOInventoryComponent::MaxBagSize; ++Index) Inv->AddEquipmentInstance(TEXT("ShortSword"));
		if (!UnchangedClaim(*Equipment, TEXT("full equipment bag changed reward or quest"))) return false;
		Inv->EquipmentBag.RemoveAt(0);
		if (!CompleteClaim(*Equipment, 0, UMMOInventoryComponent::MaxBagSize, TEXT("freed equipment slot reward incomplete"))) return false;
		if (!UnchangedClaim(*Equipment, TEXT("duplicate equipment reward changed state"))) return false;

		// Altin sink'i (GoldDonationMinTarget): buyuk HaveGold gorevi teslimde altini dusurur, kucuk olan dusurmez.
		const FMMOQuestDef* Donation = FMMOQuestDatabase::Find(TEXT("Q_Gold5000"));
		const FMMOQuestDef* Hoard = FMMOQuestDatabase::Find(TEXT("Q_Gold1000"));
		if (!Donation || !Hoard || Donation->Type != EMMOQuestType::HaveGold || Hoard->Type != EMMOQuestType::HaveGold
			|| Donation->TargetCount < UMMOQuestComponent::GoldDonationMinTarget || Hoard->TargetCount >= UMMOQuestComponent::GoldDonationMinTarget)
		{ OutError = TEXT("donation fixture data unavailable"); return false; }
		Prepare(*Donation); Inv->Gold = Donation->TargetCount - 1;
		if (!UnchangedClaim(*Donation, TEXT("insufficient gold claimed a donation quest"))) return false;
		Inv->Gold = Donation->TargetCount + 700; Quests->ClaimReward(Donation->QuestId);
		if (!Check(Inv->Gold == 700 + Donation->RewardGold && Quests->IsCompleted(Donation->QuestId), TEXT("donation quest did not deduct the gold"))) return false;
		Prepare(*Hoard); Inv->Gold = Hoard->TargetCount + 5; Quests->ClaimReward(Hoard->QuestId);
		return Check(Inv->Gold == Hoard->TargetCount + 5 + Hoard->RewardGold && Quests->IsCompleted(Hoard->QuestId), TEXT("hoard quest wrongly deducted gold"));
	}();
	for (AActor* Fixture : Fixtures) { if (IsValid(Fixture)) Fixture->Destroy(); }
	// Controllersiz ve tek senkron kare: AutoLoadDeferred/oyuncu kaydi calismaz.
	return bPassed;
}

namespace
{
	struct FMMOQuestRewardCapacityTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestQuestRewardCapacity"))) return false;
			FString Error;
			int32 Checks = 0;
			const bool bPassed = UMMOQuestComponent::RunRewardCapacitySelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_QUEST_CAPACITY_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOQuestRewardCapacityTestExec GMMOQuestRewardCapacityTestExec;
}

bool UMMOQuestComponent::RunRewardLifeStateSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0;
	OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{
		OutError = TEXT("server game world required");
		return false;
	}
	TArray<AActor*> Fixtures;
	const bool bPassed = [&]()
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector TestLocation(1000000, 1000000, 5000);
		AMMOCharacter* Character = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), TestLocation, FRotator::ZeroRotator, Params);
		AMMOCharacter* ExpectedCharacter = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), TestLocation + FVector(5000, 0, 0), FRotator::ZeroRotator, Params);
		AMMOQuestGiverNPC* Giver = World->SpawnActor<AMMOQuestGiverNPC>(AMMOQuestGiverNPC::StaticClass(), TestLocation, FRotator::ZeroRotator, Params);
		if (Character) Fixtures.Add(Character);
		if (ExpectedCharacter) Fixtures.Add(ExpectedCharacter);
		if (Giver) Fixtures.Add(Giver);
		if (!Character || !ExpectedCharacter || !Giver) { OutError = TEXT("fixture spawn failed"); return false; }
		// Tek senkron kare: gercek oyuncuya dokunmadan oyuncu telemetri yolunu da sinar.
		// AutoLoadDeferred timer'i calismadan karakter yok edilir; ilk kayit yuklenmez.
		AMMOPlayerController* Controller = World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(), TestLocation, FRotator::ZeroRotator, Params);
		if (!Controller) { OutError = TEXT("fixture controller spawn failed"); return false; }
		Fixtures.Add(Controller);
		Controller->Possess(Character);
		if (!Character->IsPlayerControlled()) { OutError = TEXT("fixture possession failed"); return false; }
		UMMOQuestComponent* Quests = Character->GetQuestComponent();
		UMMOStatsComponent* Stats = Character->GetStats();
		UMMOInventoryComponent* Inv = Character->GetInventory();
		const FName QuestId(TEXT("Q_Goblins")), OtherId(TEXT("Q_Cleanup"));
		const FMMOQuestDef* Def = FMMOQuestDatabase::Find(QuestId);
		const FMMOQuestDef* OtherDef = FMMOQuestDatabase::Find(OtherId);
		if (!Quests || !Stats || !Inv || !Def || !OtherDef || Def->RewardXP <= 0) { OutError = TEXT("fixture data unavailable"); return false; }
		auto Check = [&](bool Condition, const TCHAR* Label)
		{
			if (!Condition) { OutError = Label; return false; }
			++OutChecks;
			return true;
		};
		const int32 BeforeLevel = Stats->Level, BeforeXP = Stats->Experience, BeforeGold = Inv->Gold;
		const int32 BeforeItems = Inv->GetItemCount(Def->RewardConsumableId);
		auto NoReward = [&]()
		{
			return Stats->Level == BeforeLevel && Stats->Experience == BeforeXP && Inv->Gold == BeforeGold
				&& Inv->GetItemCount(Def->RewardConsumableId) == BeforeItems;
		};
		Quests->AcceptQuest(QuestId);
		if (!Check(Quests->IsQuestActive(QuestId), TEXT("alive acceptance failed"))) return false;
		Quests->ClaimReward(QuestId);
		if (!Check(Quests->IsQuestActive(QuestId) && !Quests->IsCompleted(QuestId) && NoReward(), TEXT("incomplete reward granted"))) return false;
		Quests->FindActive(QuestId)->KillProgress = Def->TargetCount;
		if (!Check(Quests->IsQuestComplete(*Def, *Quests->FindActive(QuestId)), TEXT("completed fixture not ready"))) return false;
		Stats->bIsDead = true; // Gercek olum animasyonu/timer degil; sunucudaki oluluk kosulu.
		Quests->ClaimReward(QuestId);
		if (!Check(Quests->IsQuestActive(QuestId) && !Quests->IsCompleted(QuestId) && NoReward(), TEXT("dead claim consumed quest or granted partial reward"))) return false;
		Quests->AcceptQuest(OtherId);
		if (!Check(!Quests->IsQuestActive(OtherId) && NoReward(), TEXT("dead acceptance succeeded"))) return false;
		Stats->Revive();
		ExpectedCharacter->GetStats()->GainExperience(Def->RewardXP);
		Quests->ClaimReward(QuestId);
		if (!Check(Quests->IsCompleted(QuestId) && !Quests->IsQuestActive(QuestId)
			&& Stats->Level == ExpectedCharacter->GetStats()->Level && Stats->Experience == ExpectedCharacter->GetStats()->Experience
			&& Inv->Gold == BeforeGold + Def->RewardGold && Inv->GetItemCount(Def->RewardConsumableId) == BeforeItems + Def->RewardConsumableCount,
			TEXT("revived reward incomplete"))) return false;
		const int32 AfterLevel = Stats->Level, AfterXP = Stats->Experience, AfterGold = Inv->Gold;
		const int32 AfterItems = Inv->GetItemCount(Def->RewardConsumableId);
		Quests->ClaimReward(QuestId);
		if (!Check(Stats->Level == AfterLevel && Stats->Experience == AfterXP && Inv->Gold == AfterGold
			&& Inv->GetItemCount(Def->RewardConsumableId) == AfterItems, TEXT("duplicate reward granted"))) return false;
		Quests->AcceptQuest(OtherId);
		FMMOQuestProgress* OtherProgress = Quests->FindActive(OtherId);
		if (!OtherProgress) { OutError = TEXT("distance fixture acceptance failed"); return false; }
		OtherProgress->KillProgress = OtherDef->TargetCount;
		Character->SetActorLocation(TestLocation + FVector(5000, 0, 0));
		Quests->ClaimReward(OtherId);
		return Check(Quests->IsQuestActive(OtherId) && !Quests->IsCompleted(OtherId) && Inv->Gold == AfterGold
			&& Stats->Level == AfterLevel && Stats->Experience == AfterXP, TEXT("distant reward granted"));
	}();
	for (AActor* Fixture : Fixtures) { if (IsValid(Fixture)) Fixture->Destroy(); }
	// Ilk kaydi yuklenmemis fixture'lar: disk kaydi ve gercek oyuncu durumu degismez.
	return bPassed;
}

namespace
{
	struct FMMOQuestRewardLifeStateTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestQuestRewardLifeState"))) return false;
			FString Error;
			int32 Checks = 0;
			const bool bPassed = UMMOQuestComponent::RunRewardLifeStateSelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_QUEST_LIFE_STATE_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOQuestRewardLifeStateTestExec GMMOQuestRewardLifeStateTestExec;
}
#endif

UMMOQuestComponent::UMMOQuestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMMOQuestComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// NETWORK_AUDIT O9: gorev durumu yalniz sahibine (HUD gorev takibi/NPC isaretleri kendi
	// pawn'ini okur; baska oyuncunun gorev listesi gizlilik + bant yukudur).
	DOREPLIFETIME_CONDITION(UMMOQuestComponent, ActiveQuests, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMMOQuestComponent, CompletedQuests, COND_OwnerOnly);
}

bool UMMOQuestComponent::HasAuthorityInternal() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->HasAuthority() : false;
}

bool UMMOQuestComponent::IsLocallyControlledOwner() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return OwnerPawn ? OwnerPawn->IsLocallyControlled() : false;
}

bool UMMOQuestComponent::IsOwnerNearQuestGiver(float MaxDist) const
{
	const AActor* OwnerActor = GetOwner();
	const UWorld* World = GetWorld();
	if (!OwnerActor || !World)
	{
		return false;
	}
	const FVector Loc = OwnerActor->GetActorLocation();
	const float MaxDistSq = MaxDist * MaxDist;

	// Hizli yol: en son yakin bulunan gorevli hala menzildeyse dunyayi taramaya gerek yok
	// (oyuncu genelde ayni NPC basinda kabul+odul yapar). Menzilde degilse tam taramaya dus —
	// sonuc tam taramayla birebir ayni kalir.
	if (const AMMOQuestGiverNPC* Cached = CachedNearGiver.Get())
	{
		if (FVector::DistSquared(Loc, Cached->GetActorLocation()) <= MaxDistSq)
		{
			return true;
		}
	}

	for (TActorIterator<AMMOQuestGiverNPC> It(World); It; ++It)
	{
		if (*It && FVector::DistSquared(Loc, (*It)->GetActorLocation()) <= MaxDistSq)
		{
			CachedNearGiver = *It;
			return true;
		}
	}
	return false;
}

FMMOQuestProgress* UMMOQuestComponent::FindActive(FName QuestId)
{
	return ActiveQuests.FindByPredicate([QuestId](const FMMOQuestProgress& P) { return P.QuestId == QuestId; });
}

const FMMOQuestProgress* UMMOQuestComponent::FindActive(FName QuestId) const
{
	return ActiveQuests.FindByPredicate([QuestId](const FMMOQuestProgress& P) { return P.QuestId == QuestId; });
}

bool UMMOQuestComponent::IsQuestActive(FName QuestId) const
{
	return FindActive(QuestId) != nullptr;
}

bool UMMOQuestComponent::IsCompleted(FName QuestId) const
{
	return CompletedQuests.Contains(QuestId);
}

bool UMMOQuestComponent::IsQuestAvailable(FName QuestId) const
{
	const FMMOQuestDef* Def = FMMOQuestDatabase::Find(QuestId);
	if (!Def)
	{
		return false;
	}
	if (IsQuestActive(QuestId) || IsCompleted(QuestId))
	{
		return false;
	}
	// On kosul gorev tamamlanmis olmali.
	if (!Def->PrereqQuestId.IsNone() && !IsCompleted(Def->PrereqQuestId))
	{
		return false;
	}
	// Level yeterli olmali.
	const AMMOCharacter* Owner = Cast<AMMOCharacter>(GetOwner());
	if (Owner && Owner->GetStats() && Owner->GetStats()->Level < Def->RequiredLevel)
	{
		return false;
	}
	return true;
}

int32 UMMOQuestComponent::GetProgress(const FMMOQuestDef& Def) const
{
	const AMMOCharacter* Owner = Cast<AMMOCharacter>(GetOwner());
	switch (Def.Type)
	{
	case EMMOQuestType::KillSpecificMob:
	case EMMOQuestType::KillAnyMob:
	{
		const FMMOQuestProgress* P = FindActive(Def.QuestId);
		return P ? FMath::Min(P->KillProgress, Def.TargetCount) : 0;
	}
	case EMMOQuestType::ReachLevel:
		return (Owner && Owner->GetStats()) ? FMath::Min(Owner->GetStats()->Level, Def.TargetCount) : 0;
	case EMMOQuestType::HaveGold:
		return (Owner && Owner->GetInventory()) ? FMath::Min(Owner->GetInventory()->Gold, Def.TargetCount) : 0;
	}
	return 0;
}

bool UMMOQuestComponent::IsQuestComplete(const FMMOQuestDef& Def, const FMMOQuestProgress& Prog) const
{
	const AMMOCharacter* Owner = Cast<AMMOCharacter>(GetOwner());
	switch (Def.Type)
	{
	case EMMOQuestType::KillSpecificMob:
	case EMMOQuestType::KillAnyMob:
		return Prog.KillProgress >= Def.TargetCount;
	case EMMOQuestType::ReachLevel:
		return Owner && Owner->GetStats() && Owner->GetStats()->Level >= Def.TargetCount;
	case EMMOQuestType::HaveGold:
		return Owner && Owner->GetInventory() && Owner->GetInventory()->Gold >= Def.TargetCount;
	}
	return false;
}

void UMMOQuestComponent::AcceptQuest(FName QuestId)
{
	const FMMOQuestDef* Def = FMMOQuestDatabase::Find(QuestId);
	if (!Def)
	{
		return;
	}

	if (!HasAuthorityInternal())
	{
		ServerAcceptQuest(QuestId);
		return;
	}
	const AMMOCharacter* Owner = Cast<AMMOCharacter>(GetOwner());
	if (!Owner || !Owner->GetStats() || Owner->GetStats()->bIsDead)
	{
		MMONotify::ToOwner(GetOwner(), TEXT("Olduyken gorev alamazsin"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	if (IsQuestActive(QuestId) || IsCompleted(QuestId))
	{
		return;
	}

	// Sunucu-otoriter dogrulama: vendor/healer/talker'in aksine bu akista mesafe kontrolu
	// yoktu — hileli istemci gorevliye hic yaklasmadan gorev kabul edebiliyordu (bug-hunt).
	if (!IsOwnerNearQuestGiver())
	{
		MMONotify::ToOwner(GetOwner(), TEXT("Gorevli yakininda degilsin"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	// On kosul gorev tamamlanmadan alinamaz (zincir).
	if (!Def->PrereqQuestId.IsNone() && !IsCompleted(Def->PrereqQuestId))
	{
		// ToOwner: uzak istemci de gorsun (eskiden yalniz yerel kontrollu sahibe ekran mesajiydi).
		const FMMOQuestDef* Pre = FMMOQuestDatabase::Find(Def->PrereqQuestId);
		MMONotify::ToOwner(GetOwner(), FString::Printf(TEXT("Once '%s' gorevini bitir"), Pre ? *Pre->Name : TEXT("onceki")),
			FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	if (Owner && Owner->GetStats() && Owner->GetStats()->Level < Def->RequiredLevel)
	{
		MMONotify::ToOwner(GetOwner(), FString::Printf(TEXT("Level %d gerekli"), Def->RequiredLevel), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	FMMOQuestProgress P;
	P.QuestId = QuestId;
	P.KillProgress = 0;
	ActiveQuests.Add(P);
	if (Owner->IsPlayerControlled())
	{
		UE_LOG(LogTemp, Display, TEXT("[MMOMETRIC] t=%.1f ev=quest_accept who=%s lv=%d quest=%s"),
			MMOMetrics::Seconds(), *MMOMetrics::WhoName(Owner), Owner->GetStats()->Level, *QuestId.ToString());
		if (AMMOPlayerController* PC = Cast<AMMOPlayerController>(Owner->GetController()))
		{
			PC->RecordFirstQuestMetric(TEXT("quest_accept"), QuestId);
		}
	}

	MMONotify::ToOwner(GetOwner(), FString::Printf(TEXT("Gorev alindi: %s"), *Def->Name), FColor(120, 200, 255), 2.5f);
}

bool UMMOQuestComponent::PassesRpcRateLimit(double& LastTime)
{
	// Monotonik gercek zaman, dunya zaman olceginden ve float hassasiyet kaybindan etkilenmez.
	const double Now = FPlatformTime::Seconds();
	if (Now - LastTime < QuestRpcMinInterval)
	{
		return false;
	}
	LastTime = Now;
	return true;
}

void UMMOQuestComponent::ServerAcceptQuest_Implementation(FName QuestId)
{
	// Reliable RPC spam korumasi: ayni oyuncudan cok sik gelen istekleri yoksay.
	if (!PassesRpcRateLimit(LastAcceptRpcTime))
	{
		return;
	}
	AcceptQuest(QuestId);
}

void UMMOQuestComponent::NotifyMobKilled(FName MobArchetypeId)
{
	if (!HasAuthorityInternal())
	{
		return;
	}

	for (FMMOQuestProgress& P : ActiveQuests)
	{
		const FMMOQuestDef* Def = FMMOQuestDatabase::Find(P.QuestId);
		if (!Def)
		{
			continue;
		}

		bool bCount = false;
		if (Def->Type == EMMOQuestType::KillAnyMob)
		{
			bCount = true;
		}
		else if (Def->Type == EMMOQuestType::KillSpecificMob && Def->TargetMob == MobArchetypeId)
		{
			bCount = true;
		}

		if (bCount && P.KillProgress < Def->TargetCount)
		{
			P.KillProgress++;
			// Her oldurmede gelen ilerleme satiri uzak istemcinin sohbetini doldurmasin: yalniz yerel ekran
			// (HUD gorev takibi zaten uzak istemcide ilerlemeyi gosterir).
			if (GEngine && IsLocallyControlledOwner())
			{
				GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor(200, 200, 120),
					FString::Printf(TEXT("%s: %d/%d"), *Def->Name, P.KillProgress, Def->TargetCount));
			}
		}
	}

	// Basarimlar gorev ilerlemesinden SONRA: odul/altin callback'leri gorev dongusunu etkilemesin.
	if (const AMMOCharacter* Me = Cast<AMMOCharacter>(GetOwner()))
	{
		if (UMMOAchievementComponent* Ach = Me->GetAchievementComponent())
		{
			Ach->NotifyMobKilled(MobArchetypeId);
		}
	}
}

void UMMOQuestComponent::ClaimReward(FName QuestId)
{
	const FMMOQuestDef* Def = FMMOQuestDatabase::Find(QuestId);
	if (!Def)
	{
		return;
	}

	if (!HasAuthorityInternal())
	{
		ServerClaimReward(QuestId);
		return;
	}
	// XP/gold/inventory delegates run before the quest is completed. Reject all
	// nested claims on this component, including a different quest sharing its
	// reward capacity. A later call can retry after this guard leaves scope.
	if (bClaimRewardInProgress)
	{
		return;
	}
	TGuardValue<bool> ClaimGuard(bClaimRewardInProgress, true);

	// GainExperience oluyken XP vermez. Gorevi burada tuketmek altin/esya verip XP'yi
	// kalici kaybettirirdi: tum oduller ve gorev durumu, dirilene kadar oldugu gibi kalmali.
	AMMOCharacter* Owner = Cast<AMMOCharacter>(GetOwner());
	if (!Owner || !Owner->GetStats() || Owner->GetStats()->bIsDead)
	{
		ClientClaimRewardFailed(TEXT("Olduyken gorev odulu alamazsin. Dirilince tekrar dene."));
		return;
	}

	// Zaten tamamlanmis gorevin odulu ikinci kez verilmez (bozuk kayit ayni gorevi hem aktif
	// hem tamamlanmis listede tasiyabilirdi; ApplySaveData bunu da temizler, bu ikinci savunma).
	if (CompletedQuests.Contains(QuestId))
	{
		MMONotify::ToOwner(GetOwner(), TEXT("Bu gorevin odulunu zaten aldin"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}
	FMMOQuestProgress* P = FindActive(QuestId);
	if (!P || !IsQuestComplete(*Def, *P))
	{
		return;
	}

	// Sunucu-otoriter dogrulama: odul alma da gorevliye yakinlik istemeli (bug-hunt).
	if (!IsOwnerNearQuestGiver())
	{
		MMONotify::ToOwner(GetOwner(), TEXT("Gorevli yakininda degilsin"), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	// Tum odulleri vermeden once ekipman icin yer ayir; aksi halde XP/altin verilip
	// gorev tamamlanirken AddEquipmentInstance dolu cantada sessizce basarisiz olur.
	if (!Def->RewardEquipmentId.IsNone())
	{
		UMMOInventoryComponent* Inv = Owner ? Owner->GetInventory() : nullptr;
		if (!Inv || !FMMOItemDatabase::Find(Def->RewardEquipmentId))
		{
			UE_LOG(LogTemp, Error, TEXT("Quest %s has an unavailable equipment reward %s"),
				*QuestId.ToString(), *Def->RewardEquipmentId.ToString());
			ClientClaimRewardFailed(TEXT("Gorev ekipman odulu su an verilemiyor."));
			return;
		}
		if (Inv->EquipmentBag.Num() >= UMMOInventoryComponent::MaxBagSize)
		{
			ClientClaimRewardFailed(TEXT("Ekipman cantasi dolu. Yer acip gorev odulunu tekrar al."));
			return;
		}
	}
	// Sarf odulu icin de yer ayir: yigin tavani (999) asilacaksa fazlasi sessizce yok olmasin;
	// odul alinmaz, oyuncu yer acip tekrar dener (ekipman cantasi kuraliyla ayni).
	if (!Def->RewardConsumableId.IsNone() && Def->RewardConsumableCount > 0)
	{
		const UMMOInventoryComponent* Inv = Owner ? Owner->GetInventory() : nullptr;
		if (Inv && Inv->GetAddRoom(Def->RewardConsumableId) < Def->RewardConsumableCount)
		{
			ClientClaimRewardFailed(TEXT("Sarf esyasi cantasi dolu (yigin tavani). Biraz harcayip gorev odulunu tekrar al."));
			return;
		}
	}
	const int32 RewardLevel = Owner->GetStats()->Level;
	int32 DonatedGold = 0; // bagis gorevinde teslimde dusulen altin (bildirimde gosterilir)
	if (Owner)
	{
		// Altin sink'i (DECISIONS_NEEDED madde 3a): buyuk HaveGold gorevleri artik teslimde altini BAGISLATIR (dusurur);
		// onceden yalnizca tasimayi isterdi, Lv1-30 geliri gideri 12 kat asiyordu. IsQuestComplete bakiyeyi dogruladi.
		// Kucuk (Q_Gold500/1000) gorevler erken oyunda altin kitligi nedeniyle bagis degil, birikim olarak kalir.
		if (Def->Type == EMMOQuestType::HaveGold && Def->TargetCount >= GoldDonationMinTarget && Owner->GetInventory())
		{
			Owner->GetInventory()->AddGoldFrom(-Def->TargetCount, TEXT("gorev_bagis"));
			DonatedGold = Def->TargetCount;
		}
		if (Owner->GetStats() && Def->RewardXP > 0)
		{
			Owner->GetStats()->GainExperience(Def->RewardXP);
		}
		if (Owner->GetInventory() && Def->RewardGold > 0)
		{
			Owner->GetInventory()->AddGoldFrom(Def->RewardGold, TEXT("gorev"));
		}
		// Odul-esyalari (sadece otorite; ClaimReward zaten tek sefer calisir).
		if (UMMOInventoryComponent* Inv = Owner->GetInventory())
		{
			if (!Def->RewardEquipmentId.IsNone() && FMMOItemDatabase::Find(Def->RewardEquipmentId))
			{
				Inv->AddEquipmentInstance(Def->RewardEquipmentId, 0);
			}
			if (!Def->RewardConsumableId.IsNone() && Def->RewardConsumableCount > 0)
			{
				Inv->AddItem(Def->RewardConsumableId, Def->RewardConsumableCount);
			}
		}
	}

	CompletedQuests.AddUnique(QuestId);
	ActiveQuests.RemoveAll([QuestId](const FMMOQuestProgress& X) { return X.QuestId == QuestId; });
	if (Owner->IsPlayerControlled())
	{
		// xp_reward veri tablosundaki nominal odul; seviye tavaninda kaybedilen XP dahil.
		// Altin gercek bakiye degisimi olarak ev=gold'da zaten olculur (cift sayilmaz).
		UE_LOG(LogTemp, Display, TEXT("[MMOMETRIC] t=%.1f ev=quest_claim who=%s lv=%d to=%d quest=%s xp_reward=%d capped=%d"),
			MMOMetrics::Seconds(), *MMOMetrics::WhoName(Owner), RewardLevel, Owner->GetStats()->Level,
			*QuestId.ToString(), FMath::Max(0, Def->RewardXP), Owner->GetStats()->Level >= Owner->GetStats()->MaxLevel ? 1 : 0);
		if (AMMOPlayerController* PC = Cast<AMMOPlayerController>(Owner->GetController()))
		{
			PC->RecordFirstQuestMetric(TEXT("quest_claim"), QuestId);
		}
	}

	const FString DonationNote = DonatedGold > 0 ? FString::Printf(TEXT(", -%d altin bagislandi"), DonatedGold) : FString();
	MMONotify::ToOwner(GetOwner(), FString::Printf(TEXT("Gorev tamam: %s  (+%d XP, +%d altin%s)"), *Def->Name, Def->RewardXP, Def->RewardGold, *DonationNote), FColor(120, 255, 120), 3.f);
}

void UMMOQuestComponent::ServerClaimReward_Implementation(FName QuestId)
{
	if (!PassesRpcRateLimit(LastClaimRpcTime))
	{
		return;
	}
	ClaimReward(QuestId);
}

void UMMOQuestComponent::ClientClaimRewardFailed_Implementation(const FString& Reason)
{
	MMONotify::ToOwner(GetOwner(), Reason, FColor(255, 120, 80), 4.f);
}

void UMMOQuestComponent::FillSaveData(FMMOCharacterSaveData& Out) const
{
	Out.ActiveQuests = ActiveQuests;
	Out.CompletedQuests = CompletedQuests;
}

void UMMOQuestComponent::ApplySaveData(const FMMOCharacterSaveData& In)
{
	if (!HasAuthorityInternal())
	{
		return;
	}
	// GUVENLIK (docs/SECURITY_AUDIT.md Y-2): bos/yinelenen kimlikleri at, sayaci hedef
	// adedine sikistir (elle yazilmis "KillProgress=999999" ile anlik tamamlama olmasin).
	// Bilinmeyen gorev kimlikleri SILINMEZ (surumler arasi yeniden adlandirma ilerlemeyi kaybettirmesin).
	// Once tamamlanmis liste (tekrarsiz), sonra aktif liste: tamamlanmis bir gorev aktifte de
	// gorunuyorsa aktiften dusurulur (aksi halde odulu ikinci kez alinabilirdi).
	CompletedQuests.Reset();
	TSet<FName> SeenDone;
	for (const FName& Id : In.CompletedQuests)
	{
		if (!Id.IsNone() && !SeenDone.Contains(Id))
		{
			SeenDone.Add(Id);
			CompletedQuests.Add(Id);
		}
	}

	ActiveQuests = In.ActiveQuests;
	TSet<FName> SeenActive;
	for (int32 i = ActiveQuests.Num() - 1; i >= 0; --i)
	{
		FMMOQuestProgress& P = ActiveQuests[i];
		if (P.QuestId.IsNone() || SeenActive.Contains(P.QuestId) || SeenDone.Contains(P.QuestId))
		{
			ActiveQuests.RemoveAt(i);
			continue;
		}
		SeenActive.Add(P.QuestId);
		P.KillProgress = FMath::Max(0, P.KillProgress);
		if (const FMMOQuestDef* Def = FMMOQuestDatabase::Find(P.QuestId))
		{
			P.KillProgress = FMath::Min(P.KillProgress, FMath::Max(1, Def->TargetCount));
		}
	}
}

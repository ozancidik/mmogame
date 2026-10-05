// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMOQuestComponent.h"
#include "Characters/MMOCharacter.h"
#include "Components/MMOStatsComponent.h"
#include "Components/MMOInventoryComponent.h"
#include "Items/MMOItemDatabase.h"
#include "Save/MMOSaveGame.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Pawn.h"
#include "Core/MMONotify.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "World/MMOQuestGiverNPC.h"

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

	const AMMOCharacter* Owner = Cast<AMMOCharacter>(GetOwner());
	if (Owner && Owner->GetStats() && Owner->GetStats()->Level < Def->RequiredLevel)
	{
		MMONotify::ToOwner(GetOwner(), FString::Printf(TEXT("Level %d gerekli"), Def->RequiredLevel), FColor::Silver, 2.f, MMONotify::EMMONotifyKind::Hint);
		return;
	}

	FMMOQuestProgress P;
	P.QuestId = QuestId;
	P.KillProgress = 0;
	ActiveQuests.Add(P);

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

	AMMOCharacter* Owner = Cast<AMMOCharacter>(GetOwner());
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
	if (Owner)
	{
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

	MMONotify::ToOwner(GetOwner(), FString::Printf(TEXT("Gorev tamam: %s  (+%d XP, +%d altin)"), *Def->Name, Def->RewardXP, Def->RewardGold), FColor(120, 255, 120), 3.f);
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

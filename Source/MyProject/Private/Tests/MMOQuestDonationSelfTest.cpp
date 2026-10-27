#include "Components/MMOQuestComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOStatsComponent.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Quests/MMOQuestTypes.h"
#include "Save/MMOSaveGame.h"
#include "World/MMOQuestGiverNPC.h"

// Altin bagis gorevleri (HaveGold, TargetCount >= GoldDonationMinTarget) veri-tablosu + teslim testi.
// QA KUCUK-4: Q_Gold41000..1500000 (4ad5092) eklendi ama sink'in bu satirlar icin calistigini kanitlayan
// bir test yoktu. Bellek ici, tek senkron kare, yetkili headless dunya; PIE/RPC/ag/disk kaniti DEGILDIR.
// Hedefler/XP PLACEHOLDER: yeni bagis gorevi eklenince/silinince asagidaki liste ve beklenen sayi bilincli guncellenir.
namespace
{
	// 11 bagis gorevi x 6 kontrol + 2 genel (liste butunlugu, kucuk gorev kontrolu).
	constexpr int32 QuestDonationExpectedChecks = 11 * 6 + 2;
	constexpr int32 QuestDonationExpectedQuests = 11;

	const TCHAR* const QuestDonationIds[QuestDonationExpectedQuests] = {
		TEXT("Q_Gold5000"), TEXT("Q_Gold15000"), TEXT("Q_Gold30000"), TEXT("Q_Gold50000"),
		TEXT("Q_Gold41000"), TEXT("Q_Gold160000"), TEXT("Q_Gold310000"), TEXT("Q_Gold440000"),
		TEXT("Q_Gold710000"), TEXT("Q_Gold1100000"), TEXT("Q_Gold1500000")
	};

	bool RunQuestDonationSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
	{
		OutChecks = 0; OutError.Reset();
		if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client
			|| !FParse::Param(FCommandLine::Get(), TEXT("unattended"))
			|| !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
		{ OutError = TEXT("disposable authority headless game world required"); return false; }
		TArray<AActor*> Fixtures;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector Origin(1000000, 1000000, 5000);
		AMMOCharacter* Pawn = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Origin, FRotator::ZeroRotator, Params);
		if (Pawn) { Fixtures.Add(Pawn); Pawn->SetActorEnableCollision(false); }
		AMMOQuestGiverNPC* Giver = World->SpawnActor<AMMOQuestGiverNPC>(AMMOQuestGiverNPC::StaticClass(), Origin, FRotator::ZeroRotator, Params);
		if (Giver) { Fixtures.Add(Giver); Giver->SetActorEnableCollision(false); }
		int32 Failures = 0;
		const auto Check = [&](const FString& Case, const TCHAR* Label, bool bOK)
		{
			++OutChecks;
			UE_LOG(LogTemp, Display, TEXT("[MMO_QUEST_DONATION_CASE] %s check=%s/%s"), bOK ? TEXT("PASS") : TEXT("FAIL"), *Case, Label);
			if (!bOK) { ++Failures; if (OutError.IsEmpty()) OutError = Case + TEXT("/") + Label; }
		};
		UMMOQuestComponent* Quests = Pawn ? Pawn->GetQuestComponent() : nullptr;
		UMMOStatsComponent* Stats = Pawn ? Pawn->GetStats() : nullptr;
		UMMOInventoryComponent* Inv = Pawn ? Pawn->GetInventory() : nullptr;
		if (!Quests || !Stats || !Inv || !Giver)
		{
			OutError = TEXT("fixture spawn failed");
			for (AActor* Fixture : Fixtures) { if (IsValid(Fixture)) Fixture->Destroy(); }
			return false;
		}
		const auto Prepare = [&](const FMMOQuestDef& Def, int32 Gold)
		{
			FMMOCharacterSaveData Seed;
			Seed.Level = FMath::Clamp(Def.RequiredLevel, 1, Stats->MaxLevel);
			Stats->ApplySaveData(Seed);
			Inv->Items.Reset(); Inv->EquipmentBag.Reset();
			Inv->Gold = Gold;
			Quests->ActiveQuests.Reset(); Quests->CompletedQuests.Reset();
			if (!Def.PrereqQuestId.IsNone()) Quests->CompletedQuests.Add(Def.PrereqQuestId);
			FMMOQuestProgress Ready;
			Ready.QuestId = Def.QuestId;
			Quests->ActiveQuests.Add(Ready);
		};

		// Veri butunlugu: sabit liste ile tablodaki "bagis" kume birebir ayni (sessizce eksilen/eklenen satir yakalanir).
		TSet<FName> Pinned;
		for (const TCHAR* Id : QuestDonationIds) Pinned.Add(FName(Id));
		TSet<FName> Found;
		TMap<FName, int32> IdCounts;
		for (const FMMOQuestDef& Def : FMMOQuestDatabase::GetAll())
		{
			IdCounts.FindOrAdd(Def.QuestId)++;
			if (Def.Type == EMMOQuestType::HaveGold && Def.TargetCount >= UMMOQuestComponent::GoldDonationMinTarget) Found.Add(Def.QuestId);
		}
		Check(TEXT("set"), TEXT("donation_set_matches_pinned_list"), Found.Num() == QuestDonationExpectedQuests && Found.Includes(Pinned) && Pinned.Includes(Found));

		for (const TCHAR* IdText : QuestDonationIds)
		{
			const FName QuestId(IdText);
			const FString Case = IdText;
			const FMMOQuestDef* Def = FMMOQuestDatabase::Find(QuestId);
			if (!Def)
			{
				// Tanim yok: bu gorevin 6 kontrolu de basarisiz sayilir (sayi sabit kalir).
				for (const TCHAR* L : { TEXT("id_unique"), TEXT("gate_level_in_range"), TEXT("reward_gold_below_target"),
					TEXT("insufficient_gold_blocks_claim"), TEXT("claim_deducts_target"), TEXT("reclaim_changes_nothing") })
					Check(Case, L, false);
				continue;
			}
			Check(Case, TEXT("id_unique"), IdCounts.FindRef(QuestId) == 1);
			Check(Case, TEXT("gate_level_in_range"), Def->RequiredLevel >= 1 && Def->RequiredLevel <= Stats->MaxLevel);
			Check(Case, TEXT("reward_gold_below_target"), Def->RewardGold >= 0 && Def->RewardGold < Def->TargetCount);

			// Yetersiz altin: teslim reddedilir, gorev aktif kalir, altin degismez.
			Prepare(*Def, Def->TargetCount - 1);
			Quests->ClaimReward(QuestId);
			Check(Case, TEXT("insufficient_gold_blocks_claim"), Inv->Gold == Def->TargetCount - 1
				&& Quests->IsQuestActive(QuestId) && !Quests->IsCompleted(QuestId));

			// Yeterli altin: hedef kadar dusulur, yalniz RewardGold geri eklenir (net sink).
			const int32 Extra = 700;
			Prepare(*Def, Def->TargetCount + Extra);
			Quests->ClaimReward(QuestId);
			Check(Case, TEXT("claim_deducts_target"), Inv->Gold == Extra + Def->RewardGold
				&& Quests->IsCompleted(QuestId) && !Quests->IsQuestActive(QuestId));

			// Tekrar alinamaz: ikinci teslim ne altini dusurur ne odul verir.
			const int32 GoldAfterFirst = Inv->Gold;
			Inv->Gold = Def->TargetCount + Extra;
			Quests->ClaimReward(QuestId);
			int32 CompletedCount = 0;
			for (const FName& Done : Quests->CompletedQuests) { if (Done == QuestId) ++CompletedCount; }
			Check(Case, TEXT("reclaim_changes_nothing"), Inv->Gold == Def->TargetCount + Extra && CompletedCount == 1 && GoldAfterFirst == Extra + Def->RewardGold);
		}

		// Kontrol: birikim gorevi (hedef < esik) altini dusurmez.
		const FMMOQuestDef* Hoard = FMMOQuestDatabase::Find(TEXT("Q_Gold1000"));
		bool bHoardOK = false;
		if (Hoard && Hoard->Type == EMMOQuestType::HaveGold && Hoard->TargetCount < UMMOQuestComponent::GoldDonationMinTarget)
		{
			Prepare(*Hoard, Hoard->TargetCount + 5);
			Quests->ClaimReward(Hoard->QuestId);
			bHoardOK = Inv->Gold == Hoard->TargetCount + 5 + Hoard->RewardGold && Quests->IsCompleted(Hoard->QuestId);
		}
		Check(TEXT("hoard"), TEXT("below_threshold_quest_keeps_gold"), bHoardOK);

		for (AActor* Fixture : Fixtures) { if (IsValid(Fixture)) Fixture->Destroy(); }
		if (OutChecks != QuestDonationExpectedChecks && OutError.IsEmpty()) OutError = TEXT("check count drifted");
		return Failures == 0 && OutChecks == QuestDonationExpectedChecks;
	}

	struct FQuestDonationTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestQuestDonation"))) return false;
			FString Error;
			int32 Checks = 0;
			const bool bPassed = RunQuestDonationSelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_QUEST_DONATION_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	static FQuestDonationTestExec GQuestDonationTestExec;
}

// Sinirlar: tek senkron kare, bellek ici fixture; gercek RPC, mesafe/oran sinirlari, HUD, disk kaydi ve PIE kaniti degildir.
#endif

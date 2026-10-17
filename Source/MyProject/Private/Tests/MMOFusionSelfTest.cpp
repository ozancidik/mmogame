#include "Items/MMOFusion.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOStatsComponent.h"
#include "Engine/World.h"
#include "Items/MMOItemDatabase.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Save/MMOCharacterSaveJson.h"
#include "Save/MMOSaveGame.h"
#include "Tests/MMOPurchaseCallbackProbe.h"
#include "UObject/StrongObjectPtr.h"
#include "World/MMOVendorNPC.h"

// Ekipman birlestirme native testi (docs/DESIGN_FUSION.md 7.2). Tek kullanimlik headless otorite dunyasi,
// controller'siz ayni-kare aktorler; diske kayit/yukleme yok (FillSaveData/ApplySaveData bellek ici).
namespace
{
	// 5 saf NextRarity + 9 FeeFor + 3x16 basari + 7 FindFromSlot + 14x4 ret + 2x2 sinir.
	constexpr int32 FusionTestExpectedChecks = 129;

	FMMOEquipmentInstance FusionTestInst(int32 Id, const TCHAR* ItemId, EMMOItemRarity Rarity, int32 Plus = 0)
	{
		FMMOEquipmentInstance I;
		I.InstanceId = Id;
		I.ItemId = FName(ItemId);
		I.Rarity = Rarity;
		I.Plus = Plus;
		return I;
	}

	struct FFusionTestRun
	{
		EMMOFusionResult Result = EMMOFusionResult::InternalError;
		int32 NewId = 0;
		int32 InvCalls = 0;
		int32 GoldCalls = 0;
		int32 PublishedGold = -1;
		FMMOCharacterSaveData SeenByInv;
		FMMOCharacterSaveData SeenByGold;
	};

	bool FusionRunSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
	{
		OutChecks = 0;
		OutError.Reset();
		if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client
			|| !FParse::Param(FCommandLine::Get(), TEXT("unattended"))
			|| !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
		{
			OutError = TEXT("disposable authority headless game world required");
			return false;
		}
		using R = EMMOItemRarity;
		using F = EMMOFusionResult;
		TArray<AActor*> Fixtures;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector Origin(1000000, 1000000, 5000);
		AMMOCharacter* Pawn = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Origin, FRotator::ZeroRotator, Params);
		if (Pawn) { Fixtures.Add(Pawn); Pawn->SetActorEnableCollision(false); }
		AMMOCharacter* Restore = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Origin + FVector(0, 5000, 0), FRotator::ZeroRotator, Params);
		if (Restore) { Fixtures.Add(Restore); Restore->SetActorEnableCollision(false); }
		AMMOVendorNPC* Vendor = World->SpawnActor<AMMOVendorNPC>(AMMOVendorNPC::StaticClass(), Origin + FVector(100, 0, 0), FRotator::ZeroRotator, Params);
		if (Vendor) { Fixtures.Add(Vendor); Vendor->SetActorEnableCollision(false); }

		int32 Failures = 0;
		const auto Check = [&](const FString& Case, const FString& Label, bool bOK)
		{
			++OutChecks;
			UE_LOG(LogTemp, Display, TEXT("[MMO_FUSION_CASE] %s case=%s check=%s"), bOK ? TEXT("PASS") : TEXT("FAIL"), *Case, *Label);
			if (!bOK) { ++Failures; if (OutError.IsEmpty()) OutError = Case + TEXT("/") + Label; }
		};
		const auto Same = [](const FMMOCharacterSaveData& A, const FMMOCharacterSaveData& B)
		{
			TArray<FString> Differences; FMMOCharacterSaveJson::FindDifferences(A, B, Differences); return Differences.IsEmpty();
		};

		const bool bCompleted = [&]()
		{
			if (!Pawn || !Restore || !Vendor || !Pawn->GetInventory() || !Restore->GetInventory() || !Pawn->GetStats())
			{
				OutError = TEXT("fixture spawn failed"); return false;
			}
			UMMOInventoryComponent* Inv = Pawn->GetInventory();
			const FMMOItemDef* Sword = FMMOItemDatabase::Find(TEXT("ShortSword"));
			if (!Sword || !FMMOItemDatabase::Find(TEXT("LongSword"))) { OutError = TEXT("fixture items missing"); return false; }
			const TCHAR* Short = TEXT("ShortSword");
			const TCHAR* Long = TEXT("LongSword");

			const auto Snapshot = [&]() { FMMOCharacterSaveData D; Inv->FillSaveData(D); return D; };
			// Kusanilmis (90) ve depodaki (91) esya + sarf yigini her senaryoda var: degismedikleri dogrulanir.
			const auto Seed = [&](const TArray<FMMOEquipmentInstance>& Bag, int32 Gold)
			{
				FMMOCharacterSaveData D;
				D.Gold = Gold;
				D.EquipmentBag = Bag;
				D.EquippedItems.SetNum(static_cast<int32>(EMMOEquipSlot::MAX));
				D.EquippedItems[static_cast<int32>(EMMOEquipSlot::Weapon)] = FusionTestInst(90, Short, R::Common);
				D.Warehouse.Add(FusionTestInst(91, Short, R::Common));
				FMMOItemStack Stack; Stack.ItemId = TEXT("HealthPotion"); Stack.Count = 5; D.Items.Add(Stack);
				D.NextInstanceId = 100;
				Pawn->GetStats()->bIsDead = false;
				Pawn->SetActorLocation(Origin);
				Inv->ApplySaveData(D);
			};
			const auto Run = [&](int32 A, int32 B, int32 C, FFusionTestRun& Out)
			{
				TStrongObjectPtr<UMMOPurchaseCallbackProbe> InvProbe(NewObject<UMMOPurchaseCallbackProbe>());
				TStrongObjectPtr<UMMOPurchaseCallbackProbe> GoldProbe(NewObject<UMMOPurchaseCallbackProbe>());
				InvProbe->Callback = [&](int32) { Out.SeenByInv = Snapshot(); };
				GoldProbe->Callback = [&](int32 Published) { Out.SeenByGold = Snapshot(); Out.PublishedGold = Published; };
				Inv->OnInventoryChanged.AddDynamic(InvProbe.Get(), &UMMOPurchaseCallbackProbe::OnInventory);
				Inv->OnGoldChanged.AddDynamic(GoldProbe.Get(), &UMMOPurchaseCallbackProbe::OnGold);
				Out.Result = FMMOFusion::Execute(*Inv, A, B, C, &Out.NewId);
				Inv->OnInventoryChanged.RemoveDynamic(InvProbe.Get(), &UMMOPurchaseCallbackProbe::OnInventory);
				Inv->OnGoldChanged.RemoveDynamic(GoldProbe.Get(), &UMMOPurchaseCallbackProbe::OnGold);
				InvProbe->Callback = nullptr;
				GoldProbe->Callback = nullptr;
				Out.InvCalls = InvProbe->Calls;
				Out.GoldCalls = GoldProbe->Calls;
			};
			const auto BagHas = [](const FMMOCharacterSaveData& D, int32 Id)
			{
				return D.EquipmentBag.ContainsByPredicate([Id](const FMMOEquipmentInstance& I) { return I.InstanceId == Id; });
			};

			// --- Saf: NextRarity (5) ---
			{
				const FString Case = TEXT("pure_next");
				R Out = R::Common;
				Check(Case, TEXT("common"), FMMOFusion::NextRarity(R::Common, Out) && Out == R::Uncommon);
				Check(Case, TEXT("uncommon"), FMMOFusion::NextRarity(R::Uncommon, Out) && Out == R::Rare);
				Check(Case, TEXT("rare"), FMMOFusion::NextRarity(R::Rare, Out) && Out == R::Epic);
				Check(Case, TEXT("epic_capped"), !FMMOFusion::NextRarity(R::Epic, Out));
				Check(Case, TEXT("legendary_capped"), !FMMOFusion::NextRarity(R::Legendary, Out));
			}

			// --- Saf: FeeFor 3 esya x 3 nadirlik (9), tasarim 6. bolum tablosu ---
			{
				struct FFusionFeeRow { const TCHAR* Id; int32 Fees[3]; };
				const FFusionFeeRow Rows[] = {
					{ TEXT("ShortSword"), { 86, 171, 342 } },
					{ TEXT("ObsidianBlade"), { 794, 1587, 3174 } },
					{ TEXT("FrostboundBlade"), { 1407, 2813, 5626 } },
				};
				for (const FFusionFeeRow& Row : Rows)
				{
					const FMMOItemDef* Def = FMMOItemDatabase::Find(Row.Id);
					if (!Def) { OutError = FString::Printf(TEXT("fixture item %s missing"), Row.Id); return false; }
					for (int32 r = 0; r < 3; ++r)
					{
						Check(TEXT("pure_fee"), FString::Printf(TEXT("%s_%d"), Row.Id, r),
							FMMOFusion::FeeFor(*Def, static_cast<R>(r)) == Row.Fees[r]);
					}
				}
			}

			// --- Basari: C->U, U->R, R->E (3 x 16) ---
			const TCHAR* SuccessCases[] = { TEXT("success_c_to_u"), TEXT("success_u_to_r"), TEXT("success_r_to_e") };
			for (int32 r = 0; r < 3; ++r)
			{
				const FString Case = SuccessCases[r];
				const R From = static_cast<R>(r);
				const R To = static_cast<R>(r + 1);
				const int32 Fee = FMMOFusion::FeeFor(*Sword, From);
				// Girdiler 1,2,4 (bitisik olmayan indeksler), 3 yedek esya.
				Seed({ FusionTestInst(1, Short, From), FusionTestInst(2, Short, From), FusionTestInst(3, Long, From), FusionTestInst(4, Short, From) }, 10000);
				const FMMOCharacterSaveData Before = Snapshot();
				FFusionTestRun Res;
				Run(4, 1, 2, Res);
				const FMMOCharacterSaveData After = Snapshot();
				const FMMOEquipmentInstance* Made = After.EquipmentBag.FindByPredicate([&](const FMMOEquipmentInstance& I) { return I.InstanceId == Res.NewId; });
				Check(Case, TEXT("result_ok"), Res.Result == F::Ok);
				Check(Case, TEXT("inputs_gone"), !BagHas(After, 1) && !BagHas(After, 2) && !BagHas(After, 4));
				Check(Case, TEXT("bag_count"), After.EquipmentBag.Num() == Before.EquipmentBag.Num() - 2);
				Check(Case, TEXT("new_id_fresh"), Made && Res.NewId == Before.NextInstanceId
					&& Res.NewId != 1 && Res.NewId != 2 && Res.NewId != 4 && Res.NewId != 90 && Res.NewId != 91);
				Check(Case, TEXT("next_id_advanced"), After.NextInstanceId == Before.NextInstanceId + 1);
				Check(Case, TEXT("output_item"), Made && Made->ItemId == FName(Short) && Made->Rarity == To && Made->Plus == 0);
				Check(Case, TEXT("gold_exact"), Fee > 0 && After.Gold == Before.Gold - Fee);
				FMMOCharacterSaveData Rest = After;
				Rest.EquipmentBag = Before.EquipmentBag; Rest.Gold = Before.Gold; Rest.NextInstanceId = Before.NextInstanceId;
				Check(Case, TEXT("equipped_stacks_warehouse_unchanged"), Same(Rest, Before));
				Check(Case, TEXT("spare_untouched"), After.EquipmentBag.ContainsByPredicate([&](const FMMOEquipmentInstance& I)
					{ return I.InstanceId == 3 && I.ItemId == FName(Long) && I.Rarity == From && I.Plus == 0; }));
				Check(Case, TEXT("inventory_delegate_once"), Res.InvCalls == 1);
				Check(Case, TEXT("gold_delegate_once"), Res.GoldCalls == 1);
				Check(Case, TEXT("inventory_callback_saw_final"), Res.InvCalls == 1 && Same(Res.SeenByInv, After));
				Check(Case, TEXT("gold_callback_saw_final"), Res.GoldCalls == 1 && Same(Res.SeenByGold, After) && Res.PublishedGold == After.Gold);
				Restore->GetInventory()->ApplySaveData(After);
				FMMOCharacterSaveData Restored; Restore->GetInventory()->FillSaveData(Restored);
				const FMMOEquipmentInstance* Back = Restored.EquipmentBag.FindByPredicate([&](const FMMOEquipmentInstance& I) { return I.InstanceId == Res.NewId; });
				Check(Case, TEXT("save_roundtrip"), Same(After, Restored) && Back && Made
					&& Back->ItemId == Made->ItemId && Back->Rarity == Made->Rarity && Back->Plus == Made->Plus);
				FFusionTestRun Again;
				Run(4, 1, 2, Again);
				Check(Case, TEXT("repeat_not_in_bag"), Again.Result == F::NotInBag);
				Check(Case, TEXT("repeat_no_mutation"), Same(Snapshot(), After) && Again.InvCalls == 0 && Again.GoldCalls == 0);
			}

			// --- FindFromSlot: karisik canta (7) ---
			{
				const FString Case = TEXT("find_from_slot");
				Seed({ FusionTestInst(1, Short, R::Common), FusionTestInst(2, Long, R::Common), FusionTestInst(3, Short, R::Uncommon),
					FusionTestInst(4, Short, R::Common, 2), FusionTestInst(5, Short, R::Common), FusionTestInst(6, Short, R::Common),
					FusionTestInst(7, Short, R::Common) }, 10000);
				const FMMOCharacterSaveData Before = Snapshot();
				const FMMOFusionPreview P = FMMOFusion::FindFromSlot(*Inv, 7);
				Check(Case, TEXT("result_ok"), P.Result == F::Ok);
				Check(Case, TEXT("picked_lowest_ids"), P.InstanceIds[0] == 1 && P.InstanceIds[1] == 5 && P.InstanceIds[2] == 6);
				Check(Case, TEXT("preview_item_fee"), P.ItemId == FName(Short) && P.From == R::Common && P.To == R::Uncommon
					&& P.Fee == FMMOFusion::FeeFor(*Sword, R::Common));
				Check(Case, TEXT("plus_slot_rejected"), FMMOFusion::FindFromSlot(*Inv, 4).Result == F::PlusNotZero);
				Check(Case, TEXT("bad_slot"), FMMOFusion::FindFromSlot(*Inv, 0).Result == F::BadIds
					&& FMMOFusion::FindFromSlot(*Inv, 99).Result == F::BadIds);
				Check(Case, TEXT("too_few"), FMMOFusion::FindFromSlot(*Inv, 3).Result == F::BadIds);
				Check(Case, TEXT("no_mutation"), Same(Snapshot(), Before));
			}

			// --- Ret vakalari (14 x 4): sonuc dogru, anlik goruntu ayni, delege 0 ---
			const TArray<FMMOEquipmentInstance> RejectBag = {
				FusionTestInst(1, Short, R::Common), FusionTestInst(2, Short, R::Common), FusionTestInst(3, Short, R::Common),
				FusionTestInst(4, Long, R::Common), FusionTestInst(5, Short, R::Uncommon), FusionTestInst(6, Short, R::Common, 1),
				FusionTestInst(7, Short, R::Epic), FusionTestInst(8, Short, R::Epic), FusionTestInst(9, Short, R::Epic),
				FusionTestInst(10, Short, R::Legendary), FusionTestInst(11, Short, R::Legendary), FusionTestInst(12, Short, R::Legendary) };
			struct FFusionRejectCase { const TCHAR* Name; int32 A, B, C; F Expected; int32 Setup; };
			// Setup: 0 normal, 1 Gold = Fee-1, 2 saticidan 2000 u uzak, 3 olu, 4 bilinmeyen esya cantasi.
			const FFusionRejectCase Rejects[] = {
				{ TEXT("reject_bad_ids"), 0, 1, 2, F::BadIds, 0 },
				{ TEXT("reject_duplicate_id"), 1, 1, 2, F::DuplicateId, 0 },
				{ TEXT("reject_not_in_bag"), 1, 2, 999, F::NotInBag, 0 },
				{ TEXT("reject_equipped_id"), 1, 2, 90, F::NotInBag, 0 },
				{ TEXT("reject_warehouse_id"), 1, 2, 91, F::NotInBag, 0 },
				{ TEXT("reject_item_mismatch"), 1, 2, 4, F::ItemMismatch, 0 },
				{ TEXT("reject_rarity_mismatch"), 1, 2, 5, F::RarityMismatch, 0 },
				{ TEXT("reject_plus_not_zero"), 1, 2, 6, F::PlusNotZero, 0 },
				{ TEXT("reject_capped_epic"), 7, 8, 9, F::RarityCapped, 0 },
				{ TEXT("reject_capped_legendary"), 10, 11, 12, F::RarityCapped, 0 },
				{ TEXT("reject_not_enough_gold"), 1, 2, 3, F::NotEnoughGold, 1 },
				{ TEXT("reject_not_near_vendor"), 1, 2, 3, F::NotNearVendor, 2 },
				{ TEXT("reject_dead"), 1, 2, 3, F::NotNearVendor, 3 },
				{ TEXT("reject_unknown_item"), 1, 2, 3, F::UnknownItem, 4 },
			};
			for (const FFusionRejectCase& RC : Rejects)
			{
				const FString Case = RC.Name;
				if (RC.Setup == 4)
				{
					const TCHAR* Unknown = TEXT("ZFusionUnknownFixture");
					Seed({ FusionTestInst(1, Unknown, R::Common), FusionTestInst(2, Unknown, R::Common), FusionTestInst(3, Unknown, R::Common) }, 10000);
				}
				else
				{
					Seed(RejectBag, RC.Setup == 1 ? FMMOFusion::FeeFor(*Sword, R::Common) - 1 : 10000);
				}
				if (RC.Setup == 2) Pawn->SetActorLocation(Origin + FVector(2000, 0, 0));
				if (RC.Setup == 3) Pawn->GetStats()->bIsDead = true;
				const FMMOCharacterSaveData Before = Snapshot();
				FFusionTestRun Res;
				Run(RC.A, RC.B, RC.C, Res);
				Check(Case, TEXT("result"), Res.Result == RC.Expected);
				Check(Case, TEXT("no_mutation"), Same(Snapshot(), Before) && Res.NewId == 0);
				Check(Case, TEXT("no_inventory_delegate"), Res.InvCalls == 0);
				Check(Case, TEXT("no_gold_delegate"), Res.GoldCalls == 0);
				Pawn->GetStats()->bIsDead = false;
				Pawn->SetActorLocation(Origin);
			}

			// --- Sinir: Gold == Fee tam (2), canta tam 12 dolu (2) ---
			{
				const FString Case = TEXT("boundary_gold_equals_fee");
				Seed({ FusionTestInst(1, Short, R::Common), FusionTestInst(2, Short, R::Common), FusionTestInst(3, Short, R::Common) },
					FMMOFusion::FeeFor(*Sword, R::Common));
				FFusionTestRun Res;
				Run(1, 2, 3, Res);
				Check(Case, TEXT("result_ok"), Res.Result == F::Ok);
				Check(Case, TEXT("gold_zero"), Inv->Gold == 0);
			}
			{
				const FString Case = TEXT("boundary_full_bag");
				TArray<FMMOEquipmentInstance> Full = { FusionTestInst(1, Short, R::Common), FusionTestInst(2, Short, R::Common), FusionTestInst(3, Short, R::Common) };
				for (int32 Id = 4; Full.Num() < UMMOInventoryComponent::MaxBagSize; ++Id) Full.Add(FusionTestInst(Id, Long, R::Common));
				Seed(Full, 10000);
				const bool bWasFull = Inv->EquipmentBag.Num() == UMMOInventoryComponent::MaxBagSize;
				FFusionTestRun Res;
				Run(1, 2, 3, Res);
				Check(Case, TEXT("result_ok"), bWasFull && Res.Result == F::Ok);
				Check(Case, TEXT("bag_ten"), Inv->EquipmentBag.Num() == UMMOInventoryComponent::MaxBagSize - 2
					&& Inv->EquipmentBag.ContainsByPredicate([&](const FMMOEquipmentInstance& I) { return I.InstanceId == Res.NewId && Res.NewId != 0; }));
			}
			return true;
		}();
		for (AActor* Fixture : Fixtures) if (IsValid(Fixture)) Fixture->Destroy();
		if (!bCompleted) return false;
		if (OutChecks != FusionTestExpectedChecks) { ++Failures; OutError += TEXT(" check_count_mismatch"); }
		if (Failures) OutError = FString::Printf(TEXT("failures=%d first=%s"), Failures, *OutError);
		return Failures == 0;
	}

	struct FMMOFusionTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestFusion"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = FusionRunSelfTest(World, Error, Checks);
			Ar.Logf(TEXT("[MMO_FUSION_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOFusionTestExec GMMOFusionTestExec;
}
#endif

#include "Items/MMOFusion.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Characters/MMOMob.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOStatsComponent.h"
#include "Components/MMOTradeComponent.h"
#include "Player/MMOPlayerController.h"
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

// Ekipman birlestirme native testi (docs/DESIGN_FUSION.md 7.2). Tek kullanimlik headless otorite dunyasi
// (yalniz NM_Standalone), ayni-kare aktorler; test pawn'i gercek bir AMMOPlayerController'a possess edilir
// (oyuncu kapisi), kayit kimligi SetSaveIdentityForTest ile acilir. Diske kayit/yukleme yok
// (FillSaveData/ApplySaveData bellek ici; unpossess oncesi yukleme bayragi kapatilir, ayrilma kaydi atlanir).
namespace
{
	// 5 saf NextRarity + 9 FeeFor + 3x16 basari + 10 FindFromSlot + 20x4 ret + 2 mob + 4x2 sinir
	// + 18 geri cagirim (5 ayni uclu tekrar + 6 baska uclu + 3 altin harcama + 4 sahibi yok etme).
	constexpr int32 FusionTestExpectedChecks = 180;

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
		// Gercek oyunculu sunucuda (dedicated/listen) kosmaz: test pawn'i ve controller'i dunyaya eklenir (D-5).
		if (!World || !World->IsGameWorld() || World->GetNetMode() != NM_Standalone || IsRunningDedicatedServer()
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
		AMMOPlayerController* Controller = World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(), Origin, FRotator::ZeroRotator, Params);
		if (Controller) Fixtures.Add(Controller);
		// Geri cagirim icinde yok edilen ikinci oyuncu (kendi controller'i, satici yaninda): ana pawn'a dokunmaz.
		AMMOCharacter* Victim = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Origin + FVector(0, 200, 0), FRotator::ZeroRotator, Params);
		if (Victim) { Fixtures.Add(Victim); Victim->SetActorEnableCollision(false); }
		AMMOPlayerController* VictimController = World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(), Origin, FRotator::ZeroRotator, Params);
		if (VictimController) Fixtures.Add(VictimController);
		// Mob sahibi (F3): kendi AI controller'i ile, satici yaninda. Ayni kare; tick kapali.
		const FTransform MobTM(FRotator::ZeroRotator, Origin + FVector(0, -300, 0));
		AMMOMob* Mob = World->SpawnActorDeferred<AMMOMob>(AMMOMob::StaticClass(), MobTM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Mob)
		{
			Fixtures.Add(Mob);
			Mob->ArchetypeId = TEXT("Goblin");
			Mob->SetActorEnableCollision(false);
			Mob->FinishSpawning(MobTM);
			Mob->SetActorTickEnabled(false);
		}
		// Possess oncesi/sonrasi: yalniz bellek ici kayit kimligi; unpossess'ten once kapatilir (ayrilma kaydi yazilmaz).
		const auto SetLoaded = [&](bool bLoaded)
		{
			if (Pawn) Pawn->SetSaveIdentityForTest(bLoaded, Pawn->GetActiveSaveSlot(), Pawn->GetLoadSerial());
		};
		if (Pawn && Controller) { Controller->Possess(Pawn); SetLoaded(true); }
		const auto SetVictimLoaded = [&](bool bLoaded)
		{
			if (IsValid(Victim)) Victim->SetSaveIdentityForTest(bLoaded, Victim->GetActiveSaveSlot(), Victim->GetLoadSerial());
		};
		if (Victim && VictimController) { VictimController->Possess(Victim); SetVictimLoaded(true); }

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
			if (!Pawn || !Restore || !Vendor || !Controller || !Mob || !Pawn->GetInventory() || !Restore->GetInventory() || !Pawn->GetStats()
				|| !Pawn->GetTradeComponent() || !Mob->GetInventory() || !Victim || !VictimController || !Victim->GetInventory())
			{
				OutError = TEXT("fixture spawn failed"); return false;
			}
			if (!Pawn->IsPlayerControlled() || !Pawn->IsInitialLoadComplete() || Mob->IsPlayerControlled()
				|| !Victim->IsPlayerControlled() || !Victim->IsInitialLoadComplete())
			{
				OutError = TEXT("fixture possession failed"); return false;
			}
			UMMOTradeComponent* Trade = Pawn->GetTradeComponent();
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
				SetLoaded(true);
				Pawn->SetTravelSaveCommitted(false);
				Trade->bTradeActive = false;
				Trade->MyOffer.Items.Reset();
				Inv->ApplySaveData(D);
			};
			// Takas teklifi: yalniz public alanlar (MMOTradeComponent degismez); Seed temizler.
			const auto Offer = [&](int32 Id)
			{
				Trade->bTradeActive = true;
				Trade->MyOffer.Items.Add(FusionTestInst(Id, Short, R::Common));
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

			// --- FindFromSlot: karisik canta (10) ---
			{
				const FString Case = TEXT("find_from_slot");
				Seed({ FusionTestInst(1, Short, R::Common), FusionTestInst(2, Long, R::Common), FusionTestInst(3, Short, R::Uncommon),
					FusionTestInst(4, Short, R::Common, 2), FusionTestInst(5, Short, R::Common), FusionTestInst(6, Short, R::Common),
					FusionTestInst(7, Short, R::Common) }, 10000);
				const FMMOCharacterSaveData Before = Snapshot();
				const FMMOFusionPreview P = FMMOFusion::FindFromSlot(*Inv, 7);
				Check(Case, TEXT("result_ok"), P.Result == F::Ok);
				// Tiklanan slotun esyasi (7) ilk girdi; kalan ikisi esdegerlerin en kucuk kimlikleri (D-1).
				Check(Case, TEXT("slot_item_first"), P.InstanceIds[0] == 7 && P.InstanceIds[1] == 1 && P.InstanceIds[2] == 5);
				const FMMOFusionPreview P1 = FMMOFusion::FindFromSlot(*Inv, 1);
				Check(Case, TEXT("slot_one_then_lowest"), P1.Result == F::Ok && P1.InstanceIds[0] == 1 && P1.InstanceIds[1] == 5 && P1.InstanceIds[2] == 6);
				Check(Case, TEXT("preview_item_fee"), P.ItemId == FName(Short) && P.From == R::Common && P.To == R::Uncommon
					&& P.Fee == FMMOFusion::FeeFor(*Sword, R::Common));
				Check(Case, TEXT("plus_slot_rejected"), FMMOFusion::FindFromSlot(*Inv, 4).Result == F::PlusNotZero);
				Check(Case, TEXT("bad_slot"), FMMOFusion::FindFromSlot(*Inv, 0).Result == F::BadIds
					&& FMMOFusion::FindFromSlot(*Inv, 99).Result == F::BadIds && FMMOFusion::FindFromSlot(*Inv, 8).Result == F::BadIds);
				// F4/D-2: isaretli tasma yok (INT_MIN - 1), uc degerler reddedilir.
				Check(Case, TEXT("bad_slot_extremes"), FMMOFusion::FindFromSlot(*Inv, MIN_int32).Result == F::BadIds
					&& FMMOFusion::FindFromSlot(*Inv, MAX_int32).Result == F::BadIds && FMMOFusion::FindFromSlot(*Inv, -1).Result == F::BadIds);
				Check(Case, TEXT("too_few"), FMMOFusion::FindFromSlot(*Inv, 3).Result == F::BadIds);
				// D-4: uzaktaki oyuncu once satici mesajini gorur (gecersiz slotta bile).
				Pawn->SetActorLocation(Origin + FVector(2000, 0, 0));
				Check(Case, TEXT("far_gate_before_slot"), FMMOFusion::FindFromSlot(*Inv, 0).Result == F::NotNearVendor
					&& FMMOFusion::FindFromSlot(*Inv, 7).Result == F::NotNearVendor);
				Pawn->SetActorLocation(Origin);
				Check(Case, TEXT("no_mutation"), Same(Snapshot(), Before));
			}

			// --- Ret vakalari (20 x 4): sonuc dogru, anlik goruntu ayni, delege 0 ---
			const TArray<FMMOEquipmentInstance> RejectBag = {
				FusionTestInst(1, Short, R::Common), FusionTestInst(2, Short, R::Common), FusionTestInst(3, Short, R::Common),
				FusionTestInst(4, Long, R::Common), FusionTestInst(5, Short, R::Uncommon), FusionTestInst(6, Short, R::Common, 1),
				FusionTestInst(7, Short, R::Epic), FusionTestInst(8, Short, R::Epic), FusionTestInst(9, Short, R::Epic),
				FusionTestInst(10, Short, R::Legendary), FusionTestInst(11, Short, R::Legendary), FusionTestInst(12, Short, R::Legendary) };
			struct FFusionRejectCase { const TCHAR* Name; int32 A, B, C; F Expected; int32 Setup; };
			// Setup: 0 normal, 1 Gold = Fee-1, 2 saticidan 2000 u uzak, 3 olu, 4 bilinmeyen esya cantasi,
			// 5 saticidan 850 u (800 u esiginin hemen disi; esik genisletilirse yakalanir),
			// 6 controller yok (oyuncu kapisi kapali), 7 kayit yuklenmedi, 8 bolge gecisi kaydi onaylandi,
			// 9 girdi 2 aktif takas teklifinde.
			const FFusionRejectCase Rejects[] = {
				{ TEXT("reject_not_player"), 1, 2, 3, F::NotPlayer, 6 },
				{ TEXT("reject_load_incomplete"), 1, 2, 3, F::Locked, 7 },
				{ TEXT("reject_travel_committed"), 1, 2, 3, F::Locked, 8 },
				{ TEXT("reject_trade_offer"), 1, 2, 3, F::TradeLocked, 9 },
				{ TEXT("reject_far_bad_ids"), 0, 1, 2, F::NotNearVendor, 2 },
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
				{ TEXT("reject_vendor_just_out_of_range"), 1, 2, 3, F::NotNearVendor, 5 },
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
				if (RC.Setup == 5) Pawn->SetActorLocation(Origin + FVector(950, 0, 0)); // satici x=+100: 850 u
				if (RC.Setup == 3) Pawn->GetStats()->bIsDead = true;
				// Unpossess kaydi yazilmasin diye once kapatilir, sonra yeniden acilir: yalniz oyuncu kapisi reddeder
				// (IsPlayerControlled kontrolu silinirse kayit kilidi bunu ortmez, birlestirme gerceklesir).
				if (RC.Setup == 6) { SetLoaded(false); Controller->UnPossess(); SetLoaded(true); }
				if (RC.Setup == 7) SetLoaded(false);
				if (RC.Setup == 8) Pawn->SetTravelSaveCommitted(true);
				if (RC.Setup == 9) Offer(2);
				const FMMOCharacterSaveData Before = Snapshot();
				FFusionTestRun Res;
				Run(RC.A, RC.B, RC.C, Res);
				Check(Case, TEXT("result"), Res.Result == RC.Expected);
				Check(Case, TEXT("no_mutation"), Same(Snapshot(), Before) && Res.NewId == 0);
				Check(Case, TEXT("no_inventory_delegate"), Res.InvCalls == 0);
				Check(Case, TEXT("no_gold_delegate"), Res.GoldCalls == 0);
				Pawn->GetStats()->bIsDead = false;
				Pawn->SetActorLocation(Origin);
				if (RC.Setup == 6) { SetLoaded(false); Controller->Possess(Pawn); SetLoaded(true); }
			}

			// --- Mob sahibi (2): satici yaninda, 3 esdeger esya, yeterli altin; yine de reddedilir (F3) ---
			{
				const FString Case = TEXT("reject_mob_owner");
				UMMOInventoryComponent* MobInv = Mob->GetInventory();
				FMMOCharacterSaveData D;
				D.Gold = 10000;
				D.EquipmentBag = { FusionTestInst(1, Short, R::Common), FusionTestInst(2, Short, R::Common), FusionTestInst(3, Short, R::Common) };
				D.EquippedItems.SetNum(static_cast<int32>(EMMOEquipSlot::MAX));
				D.NextInstanceId = 100;
				MobInv->ApplySaveData(D);
				FMMOCharacterSaveData MobBefore; MobInv->FillSaveData(MobBefore);
				int32 NewId = -1;
				Check(Case, TEXT("result"), FMMOFusion::Execute(*MobInv, 1, 2, 3, &NewId) == F::NotPlayer && NewId == 0);
				FMMOCharacterSaveData MobAfter; MobInv->FillSaveData(MobAfter);
				Check(Case, TEXT("no_mutation"), Same(MobAfter, MobBefore));
			}

			// --- Sinir: Gold == Fee tam (2), canta tam 12 dolu (2), saticiya 750 u (2), takasta baska esya (2) ---
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
			{
				// 800 u esiginin hemen ici: esik daraltilirsa yakalanir.
				const FString Case = TEXT("boundary_vendor_in_range");
				const int32 Fee = FMMOFusion::FeeFor(*Sword, R::Common);
				Seed({ FusionTestInst(1, Short, R::Common), FusionTestInst(2, Short, R::Common), FusionTestInst(3, Short, R::Common) }, 10000);
				Pawn->SetActorLocation(Origin + FVector(-650, 0, 0)); // satici x=+100: 750 u
				FFusionTestRun Res;
				Run(1, 2, 3, Res);
				Check(Case, TEXT("result_ok"), Res.Result == F::Ok);
				Check(Case, TEXT("gold_exact"), Fee > 0 && Inv->Gold == 10000 - Fee);
				Pawn->SetActorLocation(Origin);
			}
			{
				// Takas acik ama teklifte baska esya (4): kilit yalniz teklifteki kimligi tutar.
				const FString Case = TEXT("boundary_trade_other_item");
				Seed({ FusionTestInst(1, Short, R::Common), FusionTestInst(2, Short, R::Common), FusionTestInst(3, Short, R::Common),
					FusionTestInst(4, Short, R::Common) }, 10000);
				Offer(4);
				FFusionTestRun Res;
				Run(1, 2, 3, Res);
				Check(Case, TEXT("result_ok"), Res.Result == F::Ok);
				Check(Case, TEXT("offered_item_kept"), Inv->EquipmentBag.ContainsByPredicate([](const FMMOEquipmentInstance& I) { return I.InstanceId == 4; })
					&& Inv->EquipmentBag.ContainsByPredicate([&](const FMMOEquipmentInstance& I) { return I.InstanceId == Res.NewId && Res.NewId != 0; }));
				Trade->bTradeActive = false;
				Trade->MyOffer.Items.Reset();
			}

			// --- Geri cagirim vakalari (5 + 6 + 3 + 4): OnInventoryChanged icinden ikinci Execute, altin harcama,
			// sahibini yok etme. Altin probu geri cagirimdan SONRA baglanir: yalniz dis Execute'un son
			// OnGoldChanged yayinini gorur (canli bakiye mi, bayat mi).
			const int32 CbFee = FMMOFusion::FeeFor(*Sword, R::Common);
			struct FFusionCbRun { F Result = F::InternalError; int32 NewId = 0; int32 InvCalls = 0; int32 GoldCalls = 0; int32 Published = -1; };
			const auto RunCb = [&](UMMOInventoryComponent* Target, int32 A, int32 B, int32 C, TFunction<void()> OnInv, FFusionCbRun& Out)
			{
				TStrongObjectPtr<UMMOPurchaseCallbackProbe> InvProbe(NewObject<UMMOPurchaseCallbackProbe>());
				TStrongObjectPtr<UMMOPurchaseCallbackProbe> GoldProbe(NewObject<UMMOPurchaseCallbackProbe>());
				GoldProbe->Callback = [&Out](int32 Published) { Out.Published = Published; };
				InvProbe->Callback = [&](int32)
				{
					OnInv();
					Target->OnGoldChanged.AddDynamic(GoldProbe.Get(), &UMMOPurchaseCallbackProbe::OnGold);
				};
				Target->OnInventoryChanged.AddDynamic(InvProbe.Get(), &UMMOPurchaseCallbackProbe::OnInventory);
				Out.Result = FMMOFusion::Execute(*Target, A, B, C, &Out.NewId);
				// Target yok edilmis olabilir: bellek bu karede gecerli (GC yok), baglar yine de sokulur.
				Target->OnInventoryChanged.RemoveDynamic(InvProbe.Get(), &UMMOPurchaseCallbackProbe::OnInventory);
				Target->OnGoldChanged.RemoveDynamic(GoldProbe.Get(), &UMMOPurchaseCallbackProbe::OnGold);
				InvProbe->Callback = nullptr;
				GoldProbe->Callback = nullptr;
				Out.InvCalls = InvProbe->Calls;
				Out.GoldCalls = GoldProbe->Calls;
			};
			const auto Three = [&]()
			{
				return TArray<FMMOEquipmentInstance>{ FusionTestInst(1, Short, R::Common), FusionTestInst(2, Short, R::Common), FusionTestInst(3, Short, R::Common) };
			};
			{
				// Ayni uclu geri cagirimdan tekrar: girdiler zaten gitti, ikinci birlestirme ve ikinci ucret yok.
				const FString Case = TEXT("reentry_same_ids");
				Seed(Three(), 10000);
				F Nested = F::Ok; int32 NestedId = -1;
				FFusionCbRun Res;
				RunCb(Inv, 1, 2, 3, [&]() { Nested = FMMOFusion::Execute(*Inv, 1, 2, 3, &NestedId); }, Res);
				Check(Case, TEXT("outer_ok"), Res.Result == F::Ok && Res.NewId > 0);
				Check(Case, TEXT("nested_not_in_bag"), Nested == F::NotInBag && NestedId == 0);
				Check(Case, TEXT("single_output"), Inv->EquipmentBag.Num() == 1 && Inv->EquipmentBag[0].InstanceId == Res.NewId
					&& Inv->EquipmentBag[0].Rarity == R::Uncommon);
				Check(Case, TEXT("gold_one_fee"), Inv->Gold == 10000 - CbFee);
				Check(Case, TEXT("gold_published_live"), Res.GoldCalls == 1 && Res.Published == Inv->Gold);
			}
			{
				// Baska bir uclu geri cagirimdan: iki ayri cikti, iki ucret; dis yayin ic birlestirmeden sonraki bakiyeyi verir.
				const FString Case = TEXT("reentry_other_ids");
				TArray<FMMOEquipmentInstance> Six = Three();
				Six.Append({ FusionTestInst(4, Short, R::Common), FusionTestInst(5, Short, R::Common), FusionTestInst(6, Short, R::Common) });
				Seed(Six, 10000);
				const int32 NextBefore = Snapshot().NextInstanceId;
				F Nested = F::InternalError; int32 NestedId = -1;
				FFusionCbRun Res;
				RunCb(Inv, 1, 2, 3, [&]() { Nested = FMMOFusion::Execute(*Inv, 4, 5, 6, &NestedId); }, Res);
				const auto HasUncommon = [&](int32 Id)
				{
					return Id > 0 && Inv->EquipmentBag.ContainsByPredicate([Id](const FMMOEquipmentInstance& I) { return I.InstanceId == Id && I.Rarity == R::Uncommon; });
				};
				Check(Case, TEXT("outer_ok"), Res.Result == F::Ok && Res.NewId > 0);
				Check(Case, TEXT("nested_ok"), Nested == F::Ok && NestedId > 0 && NestedId != Res.NewId);
				Check(Case, TEXT("bag_two_outputs"), Inv->EquipmentBag.Num() == 2 && HasUncommon(Res.NewId) && HasUncommon(NestedId));
				Check(Case, TEXT("gold_two_fees"), Inv->Gold == 10000 - 2 * CbFee);
				Check(Case, TEXT("next_id_two"), Snapshot().NextInstanceId == NextBefore + 2);
				Check(Case, TEXT("gold_published_live"), Res.GoldCalls == 1 && Res.Published == Inv->Gold);
			}
			{
				// OnInventoryChanged gozlemcisi altin harcar: OnGoldChanged canli bakiyeyi yayinlar (onceden alinmis degeri degil).
				const FString Case = TEXT("callback_spends_gold");
				Seed(Three(), 10000);
				FFusionCbRun Res;
				RunCb(Inv, 1, 2, 3, [&]() { Inv->AddGold(-500); }, Res);
				Check(Case, TEXT("result_ok"), Res.Result == F::Ok && Res.NewId > 0);
				Check(Case, TEXT("gold_fee_and_spend"), Inv->Gold == 10000 - CbFee - 500);
				Check(Case, TEXT("gold_published_live"), Res.GoldCalls == 1 && Res.Published == Inv->Gold);
			}
			{
				// Gozlemci sahibi yok eder: cokme yok, yok olmus envanterde altin yayini yok. Son vaka (Victim geri gelmez).
				const FString Case = TEXT("callback_destroys_owner");
				UMMOInventoryComponent* VInv = Victim->GetInventory();
				FMMOCharacterSaveData D;
				D.Gold = 10000;
				D.EquipmentBag = Three();
				D.EquippedItems.SetNum(static_cast<int32>(EMMOEquipSlot::MAX));
				D.NextInstanceId = 100;
				VInv->ApplySaveData(D);
				FFusionCbRun Res;
				// Ayrilma kaydi yazilmasin: yukleme bayragi yok etmeden once kapatilir.
				RunCb(VInv, 1, 2, 3, [&]() { SetVictimLoaded(false); Victim->Destroy(); }, Res);
				Check(Case, TEXT("result_ok"), Res.Result == F::Ok && Res.NewId > 0);
				Check(Case, TEXT("owner_destroyed"), !IsValid(Victim));
				Check(Case, TEXT("inventory_delegate_once"), Res.InvCalls == 1);
				Check(Case, TEXT("no_gold_publish_after_destroy"), Res.GoldCalls == 0);
			}
			return true;
		}();
		// Ayrilma kaydi yazilmasin: yukleme bayragi kapali, sonra unpossess, sonra yok et.
		SetLoaded(false);
		if (IsValid(Controller)) Controller->UnPossess();
		SetVictimLoaded(false);
		if (IsValid(VictimController)) VictimController->UnPossess();
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

// Kapsam notu: bu test tek senkron karede, NM_Standalone otorite dunyasinda (istemci baglantisiz bir
// AMMOPlayerController possess eder) FMMOFusion mantigini dogrular. PIE, ag/replikasyon (uzak istemci),
// gercek kayit yukleme/bolge gecisi akisi (bayraklar test kancasiyla), takasin kendi yurutmesi, sohbet
// komutu, HUD ve zamanlayici kaniti DEGILDIR.
// Execute icindeki geri alma dallari (cikti eklenemedi / ucret kismen kesildi) Check'in on kosullari
// yuzunden bu testten ulasilamaz; yalniz "Gold >= Fee kontrolu silinirse" mutasyonuyla dolayli sinandi.
// Ucret geri alma dalinda ApplyGoldInternal yerine dogrudan Gold yazimi ve test exec'teki dedicated/listen
// korumasinin kaldirilmasi bu testten yakalanamaz (dal ulasilamaz; test yalniz NM_Standalone'da kosar).
// Geri cagirim vakalari ayni karede, ayni yigin uzerinde yeniden girisi sinar; GC/sonraki kare degildir.

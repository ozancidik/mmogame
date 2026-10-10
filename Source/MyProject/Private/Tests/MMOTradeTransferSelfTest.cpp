#include "Components/MMOTradeComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOInventoryComponent.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Save/MMOCharacterSaveJson.h"
#include "Save/MMOSaveGame.h"
#include "Tests/MMOPurchaseCallbackProbe.h"
#include "Tests/MMOGoldMetricProbe.h"
#include "Tests/MMOTradeTeardownProbe.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

bool UMMOTradeComponent::RunTradeTransferSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks=0; OutError.Reset();
	if(!World || !World->IsGameWorld() || World->GetNetMode()==NM_Client
		|| !FParse::Param(FCommandLine::Get(),TEXT("unattended")) || !FParse::Param(FCommandLine::Get(),TEXT("NullRHI")))
	{ OutError=TEXT("disposable authority headless world required"); return false; }
	int32 Failures=0; TArray<AMMOCharacter*> Actors;
	const auto Check=[&](const FString& Case,const TCHAR* Label,bool OK)
	{
		++OutChecks; UE_LOG(LogTemp,Display,TEXT("[MMO_TRADE_TRANSFER_CASE] %s case=%s check=%s"),OK?TEXT("PASS"):TEXT("FAIL"),*Case,Label);
		if(!OK) { ++Failures; if(OutError.IsEmpty()) OutError=Case+TEXT("/")+Label; }
	};
	const auto Snapshot=[](UMMOInventoryComponent* Inv) { FMMOCharacterSaveData D; Inv->FillSaveData(D); return D; };
	const auto Equal=[](const FMMOCharacterSaveData& A,const FMMOCharacterSaveData& B)
	{ TArray<FString> D; FMMOCharacterSaveJson::FindDifferences(A,B,D); return D.IsEmpty(); };
	const auto State=[](UMMOTradeComponent* T)
	{
		return FString::Printf(TEXT("%d/%llu/%d/%s/%d/%d/%d/%d/%d/%d"),T->bTradeActive,T->TradeSessionId,T->OfferRevision,
			*GetNameSafe(T->Partner.Get()),T->MyOffer.Gold,T->TheirOffer.Gold,T->MyOffer.bConfirmed,T->TheirOffer.bConfirmed,
			T->MyOffer.Items.Num(),T->TheirOffer.Items.Num());
	};
	const auto Alive=[](AMMOCharacter* P)
	{ return IsValid(P) && !P->IsActorBeingDestroyed() && P->HasActorBegunPlay() && !P->HasStartedEndPlay(); };
	const auto Spawn=[&](int32 I)
	{
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* P=World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(),FVector(2000000+I*100,2000000,5000),FRotator::ZeroRotator,Params);
		if(P) { Actors.Add(P); P->SetTravelSaveCommitted(true); P->SetActorTickEnabled(false); P->SetActorEnableCollision(false); }
		return P;
	};
	const bool Complete=[&]()
	{
		// Gold-only -> gold, equipment-only -> inventory, mixed -> each first publication.
		for(int32 Kind=0;Kind<4;++Kind) for(int32 Side=0;Side<2;++Side) for(int32 Position=0;Position<2;++Position) for(int32 Phase=0;Phase<2;++Phase) for(int32 Mode=0;Mode<10;++Mode)
		{
			const int32 Peer=1-Side; const bool Bag=Kind!=0, Gold=Kind!=1, GoldHook=Kind==0 || Kind==3;
			const int32 Issuer=Position==0?Side:Peer;
			const bool PeerPublished=Phase==0 && Position==1;
			const FString Case=FString::Printf(TEXT("kind%d_side%d_pos%d_phase%d_mode%d"),Kind,Side,Position,Phase,Mode);
			AMMOCharacter* P[3]={}; UMMOTradeComponent* T[3]={}; UMMOInventoryComponent* Inv[3]={}; FMMOCharacterSaveData Expected[3];
			for(int32 I=0;I<3;++I)
			{
				P[I]=Spawn(I); if(!P[I] || !P[I]->GetInventory() || !P[I]->GetTradeComponent()) { OutError=TEXT("fixture spawn"); return false; }
				Inv[I]=P[I]->GetInventory(); T[I]=P[I]->GetTradeComponent();
				FMMOCharacterSaveData Seed; Seed.Gold=1000+I*100; Seed.NextInstanceId=50;
				FMMOEquipmentInstance Item; Item.ItemId=I==Side?TEXT("ShortSword"):TEXT("LeatherArmor"); Item.InstanceId=40; Item.Plus=2; Item.Rarity=EMMOItemRarity::Rare;
				Seed.EquipmentBag.Add(Item); Inv[I]->ApplySaveData(Seed); Expected[I]=Snapshot(Inv[I]);
			}
			StartTradeServer(P[Issuer],P[1-Issuer]);
			for(int32 I=0;I<2;++I)
			{
				T[I]->MyOffer.Gold=Gold?(I==Side?10:20):0;
				if(Bag) T[I]->MyOffer.Items.Add(Inv[I]->EquipmentBag[0]);
				T[I]->MyOffer.bConfirmed=I!=Issuer; T[I]->LastTradeChangeTime=-100.f; T[I]->LastConfirmTime=-100.f;
			}
			T[Side]->TheirOffer=T[Peer]->MyOffer; T[Peer]->TheirOffer=T[Side]->MyOffer;
			if(Gold) { Expected[Side].Gold+=10; Expected[Peer].Gold-=10; }
			if(Bag)
			{
				const auto A=Expected[Side].EquipmentBag[0], B=Expected[Peer].EquipmentBag[0];
				Expected[Side].EquipmentBag={B}; Expected[Peer].EquipmentBag={A};
				for(int32 I=0;I<2;++I) { Expected[I].EquipmentBag[0].InstanceId=50; Expected[I].NextInstanceId=51; }
			}
			TStrongObjectPtr<UMMOPurchaseCallbackProbe> IP[3]={TStrongObjectPtr<UMMOPurchaseCallbackProbe>(NewObject<UMMOPurchaseCallbackProbe>()),TStrongObjectPtr<UMMOPurchaseCallbackProbe>(NewObject<UMMOPurchaseCallbackProbe>()),TStrongObjectPtr<UMMOPurchaseCallbackProbe>(NewObject<UMMOPurchaseCallbackProbe>())};
			TStrongObjectPtr<UMMOGoldMetricProbe> GP[3]={TStrongObjectPtr<UMMOGoldMetricProbe>(NewObject<UMMOGoldMetricProbe>()),TStrongObjectPtr<UMMOGoldMetricProbe>(NewObject<UMMOGoldMetricProbe>()),TStrongObjectPtr<UMMOGoldMetricProbe>(NewObject<UMMOGoldMetricProbe>())};
			TStrongObjectPtr<UMMOTradeTeardownProbe> TP(NewObject<UMMOTradeTeardownProbe>());
			bool Saw=false, Committed=false, Closed=false, ValueOK=false; FString Fresh[3];
			const auto Inject=[&](int32 Value)
			{
				Saw=true; Committed=true; Closed=true;
				for(int32 I=0;I<3;++I) { Committed &= Equal(Expected[I],Snapshot(Inv[I])); Closed &= !T[I]->bTradeActive && !T[I]->Partner.IsValid() && !World->GetTimerManager().IsTimerActive(T[I]->ValidateTimerHandle); }
				ValueOK=!GoldHook || Value==Expected[Side].Gold;
				if(Mode==1) { T[Side]->RequestCancelSeen(1); T[Peer]->RequestConfirmSeen(1,0); }
				if(Mode==2 || Mode==3)
				{
					const int32 N=Mode==2?Peer:2; StartTradeServer(P[Side],P[N]);
					T[Side]->MyOffer.Gold=17; T[N]->MyOffer.Gold=11; T[Side]->MyOffer.bConfirmed=T[N]->MyOffer.bConfirmed=true;
					T[Side]->TheirOffer=T[N]->MyOffer; T[N]->TheirOffer=T[Side]->MyOffer;
				}
				if(Mode==4 || Mode==5) P[Mode==4?Side:Peer]->Destroy();
				if(Mode==6 || Mode==7) P[Mode==6?Side:Peer]->RouteEndPlay(EEndPlayReason::LevelTransition);
				if(Mode==8) Inv[Side]->AddGold(7);
				if(Mode==9) Inv[Peer]->AddGold(7);
				for(int32 I=0;I<3;++I) Fresh[I]=State(T[I]);
			};
			if(Phase==1) { TP->Callback=[&](int32) { if(!Saw && !T[Side]->bTradeActive) Inject(Expected[Side].Gold); }; T[Side]->OnTradeChanged.AddDynamic(TP.Get(),&UMMOTradeTeardownProbe::OnTrade); }
			else if(GoldHook) GP[Side]->Callback=[&](int32 Call) { if(Call==1) Inject(GP[Side]->Values.Last()); };
			else IP[Side]->Callback=Inject;
			for(int32 I=0;I<3;++I) { Inv[I]->OnInventoryChanged.AddDynamic(IP[I].Get(),&UMMOPurchaseCallbackProbe::OnInventory); Inv[I]->OnGoldChanged.AddDynamic(GP[I].Get(),&UMMOGoldMetricProbe::OnGold); }
			T[Issuer]->RequestConfirmSeen(1,0);
			IP[Side]->Callback=nullptr; GP[Side]->Callback=nullptr; TP->Callback=nullptr;
			T[Side]->OnTradeChanged.RemoveDynamic(TP.Get(),&UMMOTradeTeardownProbe::OnTrade);
			Check(Case,TEXT("first_observer_full_commit"),Saw && Committed);
			Check(Case,TEXT("closed_before_observer"),Closed);
			Check(Case,TEXT("gold_event_final_value"),ValueOK);
			if(Mode==8) Expected[Side].Gold+=7;
			if(Mode==9) Expected[Peer].Gold+=7;
			bool Economy=true, Preserved=Saw, Timers=true;
			for(int32 I=0;I<3;++I) { Economy &= Equal(Expected[I],Snapshot(Inv[I])); Preserved &= State(T[I])==Fresh[I]; Timers &= World->GetTimerManager().IsTimerActive(T[I]->ValidateTimerHandle)==(Mode==2 && I!=2 || Mode==3 && (I==Side || I==2)); }
			Check(Case,TEXT("exact_final_economy"),Economy); Check(Case,TEXT("callback_trade_preserved"),Preserved); Check(Case,TEXT("timers"),Timers);
			const bool SelfRetired=Mode==4 || Mode==6, PeerRetired=Mode==5 || Mode==7;
			const bool SelfOuterGold=Gold && (!SelfRetired || (Phase==0 && GoldHook));
			const bool PeerOuter=PeerPublished || !PeerRetired;
			TArray<int32> SelfValues, PeerValues;
			if(Mode==8) SelfValues.Add(Expected[Side].Gold);
			if(SelfOuterGold) SelfValues.Add(Expected[Side].Gold);
			if(Mode==8 && Phase==0 && GoldHook) SelfValues[0]-=7; // Outer event precedes nested credit.
			if(PeerPublished && Gold) PeerValues.Add(Expected[Peer].Gold-(Mode==9?7:0));
			if(Mode==9) PeerValues.Add(Expected[Peer].Gold);
			if(!PeerPublished && Gold && PeerOuter) PeerValues.Add(Expected[Peer].Gold);
			Check(Case,TEXT("exact_notifications"),IP[Side]->Calls==(Bag && !(Phase==1 && SelfRetired)?1:0)
				&& IP[Peer]->Calls==(Bag && PeerOuter?1:0) && IP[2]->Calls==0 && GP[2]->Values.IsEmpty()
				&& GP[Side]->Values==SelfValues && GP[Peer]->Values==PeerValues);
			const FString Final[3]={State(T[0]),State(T[1]),State(T[2])}; const int32 C0=IP[0]->Calls+GP[0]->Values.Num(),C1=IP[1]->Calls+GP[1]->Values.Num(),C2=IP[2]->Calls+GP[2]->Values.Num();
			for(int32 I=0;I<3;++I) if(Alive(P[I])) { T[I]->RequestConfirmSeen(1,0); T[I]->RequestCancelSeen(1); }
			Check(Case,TEXT("old_completed_requests_no_effect"),State(T[0])==Final[0] && State(T[1])==Final[1] && State(T[2])==Final[2]
				&& C0==IP[0]->Calls+GP[0]->Values.Num() && C1==IP[1]->Calls+GP[1]->Values.Num() && C2==IP[2]->Calls+GP[2]->Values.Num()
				&& Equal(Expected[0],Snapshot(Inv[0])) && Equal(Expected[1],Snapshot(Inv[1])) && Equal(Expected[2],Snapshot(Inv[2])));
			for(int32 I=0;I<3;++I) { Inv[I]->OnInventoryChanged.RemoveDynamic(IP[I].Get(),&UMMOPurchaseCallbackProbe::OnInventory); Inv[I]->OnGoldChanged.RemoveDynamic(GP[I].Get(),&UMMOGoldMetricProbe::OnGold); T[I]->EndTradeServer(TEXT("fixture cleanup")); if(IsValid(P[I])) P[I]->Destroy(); }
		}
		// Fail either bag plan without publishing or changing either ID counter.
		for(int32 BadSide=0;BadSide<2;++BadSide) for(int32 Bad=0;Bad<3;++Bad)
		{
			const FString Case=FString::Printf(TEXT("id_reject_side%d_bad%d"),BadSide,Bad);
			AMMOCharacter* P[2]={Spawn(0),Spawn(1)}; UMMOTradeComponent* T[2]={}; UMMOInventoryComponent* Inv[2]={}; FMMOCharacterSaveData Before[2];
			TStrongObjectPtr<UMMOPurchaseCallbackProbe> Probe(NewObject<UMMOPurchaseCallbackProbe>());
			for(int32 I=0;I<2;++I)
			{
				if(!P[I]) { OutError=TEXT("reject fixture spawn"); return false; }
				Inv[I]=P[I]->GetInventory(); T[I]=P[I]->GetTradeComponent();
				FMMOCharacterSaveData Seed; Seed.Gold=1000; Seed.NextInstanceId=50;
				FMMOEquipmentInstance Item; Item.ItemId=TEXT("ShortSword"); Item.InstanceId=40; Seed.EquipmentBag.Add(Item); Inv[I]->ApplySaveData(Seed);
			}
			Inv[BadSide]->NextInstanceId=Bad==0?0:Bad==1?MAX_int32:40;
			StartTradeServer(P[0],P[1]);
			for(int32 I=0;I<2;++I)
			{
				Before[I]=Snapshot(Inv[I]); T[I]->MyOffer.Gold=10; T[I]->MyOffer.Items.Add(Inv[I]->EquipmentBag[0]);
				T[I]->MyOffer.bConfirmed=I==1; T[I]->LastTradeChangeTime=-100.f; T[I]->LastConfirmTime=-100.f;
				Inv[I]->OnInventoryChanged.AddDynamic(Probe.Get(),&UMMOPurchaseCallbackProbe::OnInventory);
				Inv[I]->OnGoldChanged.AddDynamic(Probe.Get(),&UMMOPurchaseCallbackProbe::OnGold);
			}
			T[0]->TheirOffer=T[1]->MyOffer; T[1]->TheirOffer=T[0]->MyOffer;
			T[0]->RequestConfirmSeen(1,0);
			Check(Case,TEXT("rejected_economy_and_counters"),Equal(Before[0],Snapshot(Inv[0])) && Equal(Before[1],Snapshot(Inv[1])));
			Check(Case,TEXT("no_inventory_gold_notifications"),Probe->Calls==0);
			Check(Case,TEXT("pair_closed"),!T[0]->bTradeActive && !T[1]->bTradeActive && !T[0]->Partner.IsValid() && !T[1]->Partner.IsValid());
			Check(Case,TEXT("timers_cleared"),!World->GetTimerManager().IsTimerActive(T[0]->ValidateTimerHandle) && !World->GetTimerManager().IsTimerActive(T[1]->ValidateTimerHandle));
			for(int32 I=0;I<2;++I) { Inv[I]->OnInventoryChanged.RemoveDynamic(Probe.Get(),&UMMOPurchaseCallbackProbe::OnInventory); Inv[I]->OnGoldChanged.RemoveDynamic(Probe.Get(),&UMMOPurchaseCallbackProbe::OnGold); P[I]->Destroy(); }
		}
		return true;
	}();
	for(auto* P:Actors) if(IsValid(P)) { P->SetTravelSaveCommitted(true); P->Destroy(); }
	if(!Complete) return false;
	if(OutChecks!=2584) { ++Failures; OutError+=TEXT(" count_mismatch"); }
	if(Failures) OutError=FString::Printf(TEXT("failures=%d first=%s"),Failures,*OutError);
	return Failures==0;
}

namespace
{
	struct FMMOTradeTransferTestExec:public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* World,const TCHAR* Cmd,FOutputDevice& Ar) override
		{
			if(!FParse::Command(&Cmd,TEXT("MMOTestTradeTransfer"))) return false;
			FString Error; int32 Checks=0; const bool Passed=UMMOTradeComponent::RunTradeTransferSelfTest(World,Error,Checks);
			Ar.Logf(TEXT("[MMO_TRADE_TRANSFER_TEST] %s checks=%d reason=%s"),Passed?TEXT("PASS"):TEXT("FAIL"),Checks,*Error); return true;
		}
	};
	FMMOTradeTransferTestExec GMMOTradeTransferTestExec;
}
#endif

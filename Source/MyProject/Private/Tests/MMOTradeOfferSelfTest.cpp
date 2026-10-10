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
#include "Tests/MMOTradeTeardownProbe.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

bool UMMOTradeComponent::RunTradeOfferSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks=0; OutError.Reset();
	if(!World || !World->IsGameWorld() || World->GetNetMode()==NM_Client
		|| !FParse::Param(FCommandLine::Get(),TEXT("unattended")) || !FParse::Param(FCommandLine::Get(),TEXT("NullRHI")))
	{ OutError=TEXT("disposable authority headless world required"); return false; }
	TArray<AMMOCharacter*> Actors; int32 Failures=0;
	const auto Check=[&](const FString& Case,const TCHAR* Label,bool OK)
	{
		++OutChecks;
		UE_LOG(LogTemp,Display,TEXT("[MMO_TRADE_OFFER_CASE] %s case=%s check=%s"),OK?TEXT("PASS"):TEXT("FAIL"),*Case,Label);
		if(!OK) { ++Failures; if(OutError.IsEmpty()) OutError=Case+TEXT("/")+Label; }
	};
	const auto Snapshot=[](UMMOInventoryComponent* Inv) { FMMOCharacterSaveData D; Inv->FillSaveData(D); return D; };
	const auto Equal=[](const FMMOCharacterSaveData& A,const FMMOCharacterSaveData& B)
	{ TArray<FString> D; FMMOCharacterSaveJson::FindDifferences(A,B,D); return D.IsEmpty(); };
	const auto Offer=[](const FMMOTradeOffer& O)
	{
		FString S=FString::Printf(TEXT("%d/%d"),O.Gold,O.bConfirmed);
		for(const auto& I:O.Items) S+=FString::Printf(TEXT("/%d:%s:%d:%d"),I.InstanceId,*I.ItemId.ToString(),I.Plus,static_cast<int32>(I.Rarity));
		return S;
	};
	const auto State=[&](UMMOTradeComponent* T)
	{
		// Clock strings test same-frame preservation, not bit-exact float identity.
		return FString::Printf(TEXT("%d/%llu/%d/%s/%s/%s/%g/%g/%g/%d"),T->bTradeActive,T->TradeSessionId,T->OfferRevision,
			*GetNameSafe(T->Partner.Get()),*Offer(T->MyOffer),*Offer(T->TheirOffer),T->LastOfferChangeTime,T->LastTradeChangeTime,T->LastConfirmTime,
			World->GetTimerManager().IsTimerActive(T->ValidateTimerHandle));
	};
	const bool Complete=[&]()
	{
		for(int32 API=0;API<3;++API) for(int32 Side=0;Side<2;++Side) for(int32 Mode=0;Mode<10;++Mode)
		{
			const int32 Peer=1-Side;
			const FString Case=FString::Printf(TEXT("api%d_side%d_mode%d"),API,Side,Mode);
			AMMOCharacter* P[3]={}; UMMOTradeComponent* T[3]={}; FMMOCharacterSaveData Before[3];
			FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			for(int32 I=0;I<3;++I)
			{
				P[I]=World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(),FVector(1900000+I*100,1900000,5000),FRotator::ZeroRotator,Params);
				if(P[I]) Actors.Add(P[I]);
				if(!P[I] || !P[I]->GetTradeComponent() || !P[I]->GetInventory()) { OutError=TEXT("fixture spawn"); return false; }
				// Controllerless fixtures never load player slots; also suppress teardown saving.
				P[I]->SetTravelSaveCommitted(true); P[I]->SetActorTickEnabled(false); P[I]->SetActorEnableCollision(false); T[I]=P[I]->GetTradeComponent();
				FMMOCharacterSaveData Seed; Seed.Gold=1000+I*100; Seed.NextInstanceId=50;
				FMMOEquipmentInstance Item; Item.InstanceId=40; Item.ItemId=TEXT("ShortSword"); Item.Plus=2; Item.Rarity=EMMOItemRarity::Rare;
				Seed.EquipmentBag.Add(Item); P[I]->GetInventory()->ApplySaveData(Seed); Before[I]=Snapshot(P[I]->GetInventory());
			}
			StartTradeServer(P[Side],P[Peer]);
			T[Side]->MyOffer.Gold=10; T[Peer]->MyOffer.Gold=20;
			if(API==2) T[Side]->MyOffer.Items.Add(P[Side]->GetInventory()->EquipmentBag[0]);
			// Seed prior confirmations to observe all four flags at the first callout.
			T[Side]->MyOffer.bConfirmed=T[Peer]->MyOffer.bConfirmed=true;
			T[Side]->TheirOffer=T[Peer]->MyOffer; T[Peer]->TheirOffer=T[Side]->MyOffer;
			for(int32 I=0;I<2;++I) { T[I]->LastTradeChangeTime=-100.f; T[I]->LastConfirmTime=-100.f; T[I]->LastOfferChangeTime=-100.f; }
			TStrongObjectPtr<UMMOTradeTeardownProbe> A(NewObject<UMMOTradeTeardownProbe>()), B(NewObject<UMMOTradeTeardownProbe>()), C(NewObject<UMMOTradeTeardownProbe>());
			bool Committed=false, Payload=false, SawCallback=false; FString Fresh[3]; float FreshTimes[3][3]={};
			B->Callback=[&](int32 Call)
			{
				if(Call!=1) return;
				SawCallback=true;
				Committed=T[Side]->OfferRevision==1 && T[Peer]->OfferRevision==1
					&& !T[Side]->MyOffer.bConfirmed && !T[Side]->TheirOffer.bConfirmed && !T[Peer]->MyOffer.bConfirmed && !T[Peer]->TheirOffer.bConfirmed
					&& Offer(T[Side]->MyOffer)==Offer(T[Peer]->TheirOffer) && Offer(T[Peer]->MyOffer)==Offer(T[Side]->TheirOffer)
					&& T[Side]->LastTradeChangeTime==World->GetTimeSeconds() && T[Peer]->LastTradeChangeTime==World->GetTimeSeconds();
				Payload=T[Side]->MyOffer.Gold==(API==0?13:10) && T[Side]->MyOffer.Items.Num()==(API==1?1:0)
					&& T[Peer]->MyOffer.Gold==20 && T[Peer]->MyOffer.Items.IsEmpty();
				if(API==1) Payload &= T[Side]->MyOffer.Items.Num()==1 && T[Side]->MyOffer.Items[0].ItemId==TEXT("ShortSword")
					&& T[Side]->MyOffer.Items[0].InstanceId==40 && T[Side]->MyOffer.Items[0].Plus==2
					&& T[Side]->MyOffer.Items[0].Rarity==EMMOItemRarity::Rare;
				if(Mode>=1 && Mode<=3) T[Side]->RequestCancelSeen(1);
				if(Mode==2 || Mode==3)
				{
					const int32 NewPeer=Mode==2?Peer:2; StartTradeServer(P[Side],P[NewPeer]);
					// Same revision, distinct session; isolate the session publication gate.
					T[Side]->MyOffer.Gold=17; T[NewPeer]->MyOffer.Gold=11;
					T[Side]->MyOffer.bConfirmed=T[NewPeer]->MyOffer.bConfirmed=true;
					T[Side]->TheirOffer=T[NewPeer]->MyOffer; T[NewPeer]->TheirOffer=T[Side]->MyOffer;
					T[Side]->OfferRevision=T[NewPeer]->OfferRevision=1;
				}
				if(Mode==4 || Mode==5)
				{
					const int32 Changed=Mode==4?Side:Peer; T[Changed]->LastOfferChangeTime=-100.f;
					const int32 Other=1-Changed, Requested=Mode==4?23:31;
					FMMOTradeOffer ExpectedChanged=T[Changed]->MyOffer, ExpectedOther=T[Other]->MyOffer;
					ExpectedChanged.Gold=Requested; ExpectedChanged.bConfirmed=false; ExpectedOther.bConfirmed=false;
					T[Changed]->RequestSetGoldSeen(1,1,Requested);
					Payload &= Offer(T[Changed]->MyOffer)==Offer(ExpectedChanged) && Offer(T[Other]->TheirOffer)==Offer(ExpectedChanged)
						&& Offer(T[Other]->MyOffer)==Offer(ExpectedOther) && Offer(T[Changed]->TheirOffer)==Offer(ExpectedOther);
				}
				if(Mode==6 || Mode==7) P[Mode==6?Side:Peer]->Destroy();
				if(Mode==8 || Mode==9) P[Mode==8?Side:Peer]->RouteEndPlay(EEndPlayReason::LevelTransition);
				for(int32 I=0;I<3;++I) { Fresh[I]=State(T[I]); FreshTimes[I][0]=T[I]->LastOfferChangeTime; FreshTimes[I][1]=T[I]->LastTradeChangeTime; FreshTimes[I][2]=T[I]->LastConfirmTime; }
			};
			T[Side]->OnTradeChanged.AddDynamic(A.Get(),&UMMOTradeTeardownProbe::OnTrade);
			T[Peer]->OnTradeChanged.AddDynamic(B.Get(),&UMMOTradeTeardownProbe::OnTrade);
			T[2]->OnTradeChanged.AddDynamic(C.Get(),&UMMOTradeTeardownProbe::OnTrade);
			if(API==0) T[Side]->RequestSetGoldSeen(1,0,13);
			if(API==1) T[Side]->RequestAddItemSeen(1,0,40);
			if(API==2) T[Side]->RequestRemoveItemSeen(1,0,40);
			B->Callback=nullptr; // Keep probes counting through rejected requests.
			Check(Case,TEXT("all_mirrors_committed_before_callback"),Committed);
			Check(Case,TEXT("public_and_nested_api_payload"),Payload);
			const int32 ES[]={1,1,2,2,1,1,0,1,0,1}, EP[]={1,2,3,2,2,2,2,1,2,1};
			Check(Case,TEXT("exact_delegate_counts"),A->Calls==ES[Mode] && B->Calls==EP[Mode] && C->Calls==(Mode==3?1:0));
			bool Preserved=SawCallback, Pair=true, Timers=true, Economy=true;
			const bool Active=Mode==0 || Mode==2 || Mode==3 || Mode==4 || Mode==5;
			const int32 NewPeer=Mode==3?2:Peer;
			for(int32 I=0;I<3;++I)
			{
				Preserved &= State(T[I])==Fresh[I] && T[I]->LastOfferChangeTime==FreshTimes[I][0] && T[I]->LastTradeChangeTime==FreshTimes[I][1] && T[I]->LastConfirmTime==FreshTimes[I][2]; Economy &= Equal(Before[I],Snapshot(P[I]->GetInventory()));
				const bool Endpoint=Active && (I==Side || I==NewPeer);
				Timers &= World->GetTimerManager().IsTimerActive(T[I]->ValidateTimerHandle)==Endpoint;
				if(Endpoint) Pair &= T[I]->GetValidPartnerComp()==T[I==Side?NewPeer:Side]
					&& T[I]->TradeSessionId==(Mode==2 || Mode==3?2:1) && T[I]->OfferRevision==(Mode==4 || Mode==5?2:1);
				else Pair &= !T[I]->bTradeActive && !T[I]->Partner.IsValid() && T[I]->MyOffer.Items.IsEmpty() && T[I]->TheirOffer.Items.IsEmpty()
					&& !T[I]->MyOffer.Gold && !T[I]->TheirOffer.Gold && !T[I]->MyOffer.bConfirmed && !T[I]->TheirOffer.bConfirmed;
			}
			Check(Case,TEXT("callback_state_preserved"),Preserved); Check(Case,TEXT("final_pair_and_revision"),Pair);
			Check(Case,TEXT("timers"),Timers); Check(Case,TEXT("economy_unchanged"),Economy);
			for(int32 I=0;I<3;++I) T[I]->LastOfferChangeTime=-100.f;
			const FString Final[3]={State(T[0]),State(T[1]),State(T[2])};
			const int32 CountA=A->Calls, CountB=B->Calls, CountC=C->Calls;
			for(int32 I=0;I<3;++I) if(IsValid(P[I]) && P[I]->HasActorBegunPlay() && !P[I]->IsActorBeingDestroyed())
			{
				const uint64 OldSession=T[I]->TradeSessionId==2?1:T[I]->TradeSessionId;
				const int32 OldRevision=T[I]->TradeSessionId==2?T[I]->OfferRevision:T[I]->OfferRevision-1;
				T[I]->RequestSetGoldSeen(OldSession,OldRevision,99); T[I]->RequestAddItemSeen(0,T[I]->OfferRevision,40); T[I]->RequestRemoveItemSeen(0,T[I]->OfferRevision,40);
			}
			Check(Case,TEXT("stale_or_zero_offer_no_effect"),State(T[0])==Final[0] && State(T[1])==Final[1] && State(T[2])==Final[2]
				&& A->Calls==CountA && B->Calls==CountB && C->Calls==CountC);
			T[Side]->OnTradeChanged.RemoveDynamic(A.Get(),&UMMOTradeTeardownProbe::OnTrade);
			T[Peer]->OnTradeChanged.RemoveDynamic(B.Get(),&UMMOTradeTeardownProbe::OnTrade);
			T[2]->OnTradeChanged.RemoveDynamic(C.Get(),&UMMOTradeTeardownProbe::OnTrade);
			for(int32 I=0;I<3;++I) { T[I]->EndTradeServer(TEXT("fixture cleanup")); P[I]->SetTravelSaveCommitted(true); if(IsValid(P[I])) P[I]->Destroy(); }
		}
		return true;
	}();
	for(auto* P:Actors) if(IsValid(P)) { P->SetTravelSaveCommitted(true); P->Destroy(); }
	if(!Complete) return false;
	if(OutChecks!=480) { ++Failures; OutError+=TEXT(" count_mismatch"); }
	if(Failures) OutError=FString::Printf(TEXT("failures=%d first=%s"),Failures,*OutError);
	return Failures==0;
}

namespace
{
	struct FMMOTradeOfferTestExec:public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* World,const TCHAR* Cmd,FOutputDevice& Ar) override
		{
			if(!FParse::Command(&Cmd,TEXT("MMOTestTradeOffer"))) return false;
			FString Error; int32 Checks=0; const bool Passed=UMMOTradeComponent::RunTradeOfferSelfTest(World,Error,Checks);
			Ar.Logf(TEXT("[MMO_TRADE_OFFER_TEST] %s checks=%d reason=%s"),Passed?TEXT("PASS"):TEXT("FAIL"),Checks,*Error); return true;
		}
	};
	FMMOTradeOfferTestExec GMMOTradeOfferTestExec;
}
#endif

#include "Components/MMOTradeComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOInventoryComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Player/MMOPlayerController.h"
#include "Save/MMOCharacterSaveJson.h"
#include "Save/MMOSaveGame.h"
#include "Tests/MMOTradeTeardownProbe.h"
#include "TimerManager.h"
#include "UI/MMOHUD.h"
#include "UObject/StrongObjectPtr.h"

bool UMMOTradeComponent::RunTradeConfirmSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks=0; OutError.Reset();
	if(!World || !World->IsGameWorld() || World->GetNetMode()==NM_Client
		|| !FParse::Param(FCommandLine::Get(),TEXT("unattended")) || !FParse::Param(FCommandLine::Get(),TEXT("NullRHI")))
	{ OutError=TEXT("disposable authority headless world required"); return false; }
	TArray<AActor*> Actors; int32 Failures=0;
	const auto Check=[&](const FString& Case,const TCHAR* Label,bool OK)
	{
		++OutChecks;
		UE_LOG(LogTemp,Display,TEXT("[MMO_TRADE_CONFIRM_CASE] %s case=%s check=%s"),OK?TEXT("PASS"):TEXT("FAIL"),*Case,Label);
		if(!OK) { ++Failures; if(OutError.IsEmpty()) OutError=Case+TEXT("/")+Label; }
	};
	const auto Snapshot=[](UMMOInventoryComponent* Inv) { FMMOCharacterSaveData D; Inv->FillSaveData(D); return D; };
	const auto Equal=[](const FMMOCharacterSaveData& A,const FMMOCharacterSaveData& B)
	{ TArray<FString> D; FMMOCharacterSaveJson::FindDifferences(A,B,D); return D.IsEmpty(); };
	const auto State=[&](UMMOTradeComponent* T)
	{
		FString S=FString::Printf(TEXT("%d/%llu/%d/%s/%d/%d/%d/%d/%d"),T->bTradeActive,T->TradeSessionId,T->OfferRevision,
			*GetNameSafe(T->Partner.Get()),T->MyOffer.Gold,T->TheirOffer.Gold,T->MyOffer.bConfirmed,T->TheirOffer.bConfirmed,
			World->GetTimerManager().IsTimerActive(T->ValidateTimerHandle));
		for(const auto* O:{&T->MyOffer,&T->TheirOffer}) for(const auto& I:O->Items)
			S+=FString::Printf(TEXT("/%d:%s:%d:%d"),I.InstanceId,*I.ItemId.ToString(),I.Plus,static_cast<int32>(I.Rarity));
		return S;
	};
	const bool Complete=[&]()
	{
		for(int32 Side=0;Side<2;++Side) for(int32 Position=0;Position<2;++Position) for(int32 Mode=0;Mode<10;++Mode)
		{
			const int32 Peer=1-Side, Trigger=Position==0?Side:Peer;
			const FString Case=FString::Printf(TEXT("side%d_pos%d_mode%d"),Side,Position,Mode);
			AMMOCharacter* P[3]={}; AMMOPlayerController* PC[3]={}; AMMOHUD* H[3]={}; UMMOTradeComponent* T[3]={};
			FMMOCharacterSaveData Before[3]; FString Names[3]; int32 Base[3]={};
			FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			for(int32 I=0;I<3;++I)
			{
				P[I]=World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(),FVector(1700000+I*100,1700000,5000),FRotator::ZeroRotator,Params);
				PC[I]=World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(),Params);
				if(P[I]) Actors.Add(P[I]); if(PC[I]) Actors.Add(PC[I]);
				if(!P[I] || !PC[I] || !P[I]->GetTradeComponent() || !P[I]->GetInventory()) { OutError=TEXT("fixture spawn"); return false; }
				P[I]->SetTravelSaveCommitted(true); P[I]->SetActorTickEnabled(false); P[I]->SetActorEnableCollision(false);
				PC[I]->SetActorTickEnabled(false); PC[I]->Possess(P[I]);
				if(!P[I]->GetPlayerState()) { OutError=TEXT("fixture player state"); return false; }
				Names[I]=FString::Printf(TEXT("O55_%d"),I); P[I]->GetPlayerState()->SetPlayerName(Names[I]);
				PC[I]->ClientSetHUD(AMMOHUD::StaticClass()); H[I]=Cast<AMMOHUD>(PC[I]->GetHUD());
				if(!H[I]) { OutError=TEXT("fixture HUD"); return false; } Actors.Add(H[I]);
				T[I]=P[I]->GetTradeComponent();
				FMMOCharacterSaveData Seed; Seed.Gold=1000+I*100; Seed.NextInstanceId=50;
				FMMOEquipmentInstance Item; Item.InstanceId=40; Item.ItemId=TEXT("ShortSword"); Seed.EquipmentBag.Add(Item);
				P[I]->GetInventory()->ApplySaveData(Seed); Before[I]=Snapshot(P[I]->GetInventory());
			}
			StartTradeServer(P[Side],P[Peer]);
			T[Side]->MyOffer.Gold=10; T[Peer]->TheirOffer=T[Side]->MyOffer;
			for(int32 I=0;I<2;++I) { T[I]->LastTradeChangeTime=-100.f; T[I]->LastConfirmTime=-100.f; T[I]->LastOfferChangeTime=-100.f; }
			for(int32 I=0;I<3;++I) Base[I]=H[I]->GetChatLinesForTests().Num();
			const FString Initial=State(T[Side])+State(T[Peer]);
			T[Side]->RequestConfirmSeen(0,0); T[Side]->RequestConfirmSeen(1,1); T[Side]->RequestConfirmSeen(2,0);
			Check(Case,TEXT("stale_admission_no_state_change"),Initial==State(T[Side])+State(T[Peer]));
			// Ignore rejection feedback; the accepted confirmation must precede its own callouts.
			for(int32 I=0;I<3;++I) Base[I]=H[I]->GetChatLinesForTests().Num();
			TStrongObjectPtr<UMMOTradeTeardownProbe> Probes[3]={TStrongObjectPtr<UMMOTradeTeardownProbe>(NewObject<UMMOTradeTeardownProbe>()),
				TStrongObjectPtr<UMMOTradeTeardownProbe>(NewObject<UMMOTradeTeardownProbe>()),TStrongObjectPtr<UMMOTradeTeardownProbe>(NewObject<UMMOTradeTeardownProbe>())};
			bool SawCommitted=false, SawMessage=false, Changed=false; FString Fresh[3];
			Probes[Trigger]->Callback=[&](int32 Call)
			{
				if(Call!=1) return;
				SawCommitted=T[Side]->MyOffer.bConfirmed && T[Peer]->TheirOffer.bConfirmed && !T[Peer]->MyOffer.bConfirmed;
				SawMessage=H[Peer]->GetChatLinesForTests().Num()==Base[Peer]+1
					&& H[Peer]->GetChatLinesForTests().Last().Text==Names[Side]+TEXT(" teklifi onayladi.");
				if(Mode>=1 && Mode<=3) T[Side]->RequestCancelSeen(1);
				if(Mode==2 || Mode==3)
				{
					const int32 NewPeer=Mode==2?Peer:2; StartTradeServer(P[Side],P[NewPeer]);
					// A fresh confirmed server context must not be executed by the old stack frame.
					T[Side]->MyOffer.Gold=17; T[NewPeer]->MyOffer.Gold=11;
					T[Side]->MyOffer.bConfirmed=T[NewPeer]->MyOffer.bConfirmed=true;
					T[Side]->TheirOffer=T[NewPeer]->MyOffer; T[NewPeer]->TheirOffer=T[Side]->MyOffer;
					T[Side]->OfferRevision=T[NewPeer]->OfferRevision=0;
				}
				if(Mode==4 || Mode==9) T[Side]->RequestSetGoldSeen(1,0,13);
				if(Mode==9)
				{
					// Isolate revision identity from confirmation flags using a seeded
					// newer confirmed context; the old stack must not execute it.
					T[Side]->MyOffer.bConfirmed=T[Peer]->MyOffer.bConfirmed=true;
					T[Side]->TheirOffer=T[Peer]->MyOffer; T[Peer]->TheirOffer=T[Side]->MyOffer;
				}
				if(Mode==5 || Mode==6) P[Mode==5?Side:Peer]->Destroy();
				if(Mode==7 || Mode==8) P[Mode==7?Side:Peer]->RouteEndPlay(EEndPlayReason::LevelTransition);
				for(int32 I=0;I<3;++I) Fresh[I]=State(T[I]); Changed=true;
			};
			for(int32 I=0;I<3;++I) T[I]->OnTradeChanged.AddDynamic(Probes[I].Get(),&UMMOTradeTeardownProbe::OnTrade);
			T[Side]->RequestConfirmSeen(1,0);
			for(int32 I=0;I<3;++I) { T[I]->OnTradeChanged.RemoveDynamic(Probes[I].Get(),&UMMOTradeTeardownProbe::OnTrade); Probes[I]->Callback=nullptr; }
			Check(Case,TEXT("mirrored_confirmation_before_callback"),SawCommitted);
			Check(Case,TEXT("confirmation_message_before_callback"),SawMessage);
			int32 ES=1, EP=1, ET=0;
			if(Mode==1 || Mode==4 || Mode==9) { ES=2; EP=Position==0?1:2; }
			if(Mode==2) { ES=3; EP=Position==0?2:3; }
			if(Mode==3) { ES=3; EP=Position==0?1:2; ET=1; }
			if(Mode==5 || Mode==7) { ES=1; EP=Position==0?1:2; }
			if(Mode==6 || Mode==8) { ES=2; EP=Position==0?0:1; }
			Check(Case,TEXT("exact_delegate_counts"),Probes[Side]->Calls==ES && Probes[Peer]->Calls==EP && Probes[2]->Calls==ET);
			bool StateOK=Changed, Economy=true, Messages=true, Timers=true;
			for(int32 I=0;I<3;++I)
			{
				StateOK &= State(T[I])==Fresh[I]; Economy &= Equal(Before[I],Snapshot(P[I]->GetInventory()));
				TArray<FString> Expected; if(I==Peer) Expected.Add(Names[Side]+TEXT(" teklifi onayladi."));
				if(Mode>=1 && Mode<=3 && I!=2) Expected.Add(TEXT("Ticaret iptal edildi."));
				if(Mode==2 && I!=2 || Mode==3 && (I==Side || I==2))
					Expected.Add(Names[I==Side?(Mode==2?Peer:2):Side]+TEXT(" ile ticaret basladi."));
				if((Mode==5 || Mode==7) && I==Peer || (Mode==6 || Mode==8) && I==Side)
					Expected.Add(TEXT("Ticaret iptal edildi: karsi taraf ayrildi."));
				TArray<FString> Actual; const auto& Lines=H[I]->GetChatLinesForTests();
				for(int32 N=Base[I];N<Lines.Num();++N) Actual.Add(Lines[N].Text);
				Messages &= Actual==Expected;
				if(Actual!=Expected) UE_LOG(LogTemp,Display,TEXT("[MMO_TRADE_CONFIRM_CHAT] case=%s endpoint=%d actual=%s expected=%s"),*Case,I,*FString::Join(Actual,TEXT("|")),*FString::Join(Expected,TEXT("|")));
				const bool Active=Mode==0 || Mode==2 || Mode==3 || Mode==4 || Mode==9;
				Timers &= World->GetTimerManager().IsTimerActive(T[I]->ValidateTimerHandle)==(Active && (I==Side || I==(Mode==3?2:Peer)));
			}
			Check(Case,TEXT("callback_context_preserved"),StateOK); Check(Case,TEXT("economy_unchanged"),Economy);
			Check(Case,TEXT("native_chat_exact_order"),Messages); Check(Case,TEXT("timers"),Timers);
			for(int32 I=0;I<3;++I) T[I]->EndTradeServer(TEXT("fixture cleanup"));
			for(int32 I=0;I<3;++I) { P[I]->SetTravelSaveCommitted(true); if(IsValid(P[I])) P[I]->Destroy(); PC[I]->UnPossess(); PC[I]->Destroy(); H[I]->Destroy(); }
		}
		// Normal and nested peer confirmation must still complete once. Unpossessed
		// pawns never load a player save, so SaveCharacter cannot write a user slot.
		for(int32 Side=0;Side<2;++Side) for(int32 Nested=0;Nested<2;++Nested)
		{
			const int32 Peer=1-Side;
			const FString Case=FString::Printf(TEXT("complete_side%d_nested%d"),Side,Nested);
			AMMOCharacter* P[2]={}; UMMOTradeComponent* T[2]={}; FMMOCharacterSaveData Expected[2];
			FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			for(int32 I=0;I<2;++I)
			{
				P[I]=World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(),FVector(1800000+I*100,1800000,5000),FRotator::ZeroRotator,Params);
				if(P[I]) Actors.Add(P[I]);
				if(!P[I] || !P[I]->GetTradeComponent() || !P[I]->GetInventory()) { OutError=TEXT("completion fixture spawn"); return false; }
				P[I]->SetTravelSaveCommitted(true); P[I]->SetActorTickEnabled(false); P[I]->SetActorEnableCollision(false); T[I]=P[I]->GetTradeComponent();
				FMMOCharacterSaveData Seed; Seed.Gold=1000+I*100; Seed.NextInstanceId=50;
				P[I]->GetInventory()->ApplySaveData(Seed); Expected[I]=Snapshot(P[I]->GetInventory());
			}
			StartTradeServer(P[Side],P[Peer]);
			T[Side]->MyOffer.Gold=10; T[Peer]->MyOffer.Gold=20;
			T[Side]->TheirOffer=T[Peer]->MyOffer; T[Peer]->TheirOffer=T[Side]->MyOffer;
			for(int32 I=0;I<2;++I) { T[I]->LastTradeChangeTime=-100.f; T[I]->LastConfirmTime=-100.f; }
			Check(Case,TEXT("authority_pair_ready"),P[0]->HasAuthority() && P[1]->HasAuthority() && T[Side]->GetValidPartnerComp()==T[Peer]);
			TStrongObjectPtr<UMMOTradeTeardownProbe> A(NewObject<UMMOTradeTeardownProbe>()), B(NewObject<UMMOTradeTeardownProbe>());
			bool Mirrored=false;
			A->Callback=[&](int32 Call)
			{
				if(Call!=1) return;
				Mirrored=T[Side]->MyOffer.bConfirmed && T[Peer]->TheirOffer.bConfirmed && !T[Peer]->MyOffer.bConfirmed;
				// Duplicate issuer approval has no effect even inside its publication.
				T[Side]->RequestConfirmSeen(1,0);
				if(Nested) T[Peer]->RequestConfirmSeen(1,0);
			};
			T[Side]->OnTradeChanged.AddDynamic(A.Get(),&UMMOTradeTeardownProbe::OnTrade);
			T[Peer]->OnTradeChanged.AddDynamic(B.Get(),&UMMOTradeTeardownProbe::OnTrade);
			T[Side]->RequestConfirmSeen(1,0);
			if(!Nested) { T[Side]->RequestConfirmSeen(1,0); T[Peer]->RequestConfirmSeen(1,0); }
			Check(Case,TEXT("first_approval_mirrored"),Mirrored);
			Check(Case,TEXT("exact_completion_delegates"),A->Calls==3 && B->Calls==(Nested?2:3));
			Check(Case,TEXT("closed_pair"),!T[0]->bTradeActive && !T[1]->bTradeActive && !T[0]->Partner.IsValid() && !T[1]->Partner.IsValid());
			Check(Case,TEXT("timers_cleared"),!World->GetTimerManager().IsTimerActive(T[0]->ValidateTimerHandle) && !World->GetTimerManager().IsTimerActive(T[1]->ValidateTimerHandle));
			Expected[Side].Gold+=10; Expected[Peer].Gold-=10;
			Check(Case,TEXT("single_exact_gold_transfer"),Equal(Expected[0],Snapshot(P[0]->GetInventory())) && Equal(Expected[1],Snapshot(P[1]->GetInventory())));
			Check(Case,TEXT("generation_retained"),T[0]->TradeSessionId==1 && T[1]->TradeSessionId==1);
			const FString Final=State(T[0])+State(T[1]); const int32 CountA=A->Calls,CountB=B->Calls;
			for(auto* C:T) { C->RequestConfirmSeen(1,0); C->RequestConfirmSeen(0,0); }
			Check(Case,TEXT("completed_duplicate_no_effect"),Final==State(T[0])+State(T[1]) && CountA==A->Calls && CountB==B->Calls
				&& Equal(Expected[0],Snapshot(P[0]->GetInventory())) && Equal(Expected[1],Snapshot(P[1]->GetInventory())));
			T[Side]->OnTradeChanged.RemoveDynamic(A.Get(),&UMMOTradeTeardownProbe::OnTrade); A->Callback=nullptr;
			T[Peer]->OnTradeChanged.RemoveDynamic(B.Get(),&UMMOTradeTeardownProbe::OnTrade);
			for(auto* C:P) C->Destroy();
		}
		return true;
	}();
	for(AActor* Actor:Actors) if(IsValid(Actor)) { if(auto* P=Cast<AMMOCharacter>(Actor)) P->SetTravelSaveCommitted(true); Actor->Destroy(); }
	if(!Complete) return false;
	if(OutChecks!=352) { ++Failures; OutError+=TEXT(" count_mismatch"); }
	if(Failures) OutError=FString::Printf(TEXT("failures=%d first=%s"),Failures,*OutError);
	return Failures==0;
}

namespace
{
	struct FMMOTradeConfirmTestExec:public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* World,const TCHAR* Cmd,FOutputDevice& Ar) override
		{
			if(!FParse::Command(&Cmd,TEXT("MMOTestTradeConfirm"))) return false;
			FString Error; int32 Checks=0; const bool Passed=UMMOTradeComponent::RunTradeConfirmSelfTest(World,Error,Checks);
			Ar.Logf(TEXT("[MMO_TRADE_CONFIRM_TEST] %s checks=%d reason=%s"),Passed?TEXT("PASS"):TEXT("FAIL"),Checks,*Error); return true;
		}
	};
	FMMOTradeConfirmTestExec GMMOTradeConfirmTestExec;
}
#endif

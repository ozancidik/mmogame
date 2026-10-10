#include "Components/MMOTradeComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOInventoryComponent.h"
#include "Components/MMOPartyComponent.h"
#include "GameFramework/PlayerState.h"
#include "Player/MMOPlayerController.h"
#include "UI/MMOHUD.h"
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

bool UMMOTradeComponent::RunTradeStartSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks=0; OutError.Reset();
	if(!World || !World->IsGameWorld() || World->GetNetMode()==NM_Client
		|| !FParse::Param(FCommandLine::Get(),TEXT("unattended"))
		|| !FParse::Param(FCommandLine::Get(),TEXT("NullRHI")))
	{ OutError=TEXT("disposable authority headless world required"); return false; }
	TArray<AMMOCharacter*> Pawns; TArray<AMMOPlayerController*> Controllers;
	TArray<AMMOHUD*> HUDs; int32 Failures=0;
	const auto Check=[&](const FString& Case,const TCHAR* Label,bool OK)
	{
		++OutChecks;
		UE_LOG(LogTemp,Display,TEXT("[MMO_TRADE_START_CASE] %s case=%s check=%s"),OK?TEXT("PASS"):TEXT("FAIL"),*Case,Label);
		if(!OK) { ++Failures; if(OutError.IsEmpty()) OutError=Case+TEXT("/")+Label; }
	};
	const auto Snapshot=[](UMMOInventoryComponent* Inv)
	{ FMMOCharacterSaveData D; Inv->FillSaveData(D); return D; };
	const auto Equal=[](const FMMOCharacterSaveData& A,const FMMOCharacterSaveData& B)
	{ TArray<FString> D; FMMOCharacterSaveJson::FindDifferences(A,B,D); return D.IsEmpty(); };
	const auto State=[&](UMMOTradeComponent* T)
	{
		// Integer/payload identity plus formatted clocks; not bit-exact float evidence.
		FString S=FString::Printf(TEXT("%d/%llu/%llu/%d/%s/%s/%s/%g/%g/%d"),T->bTradeActive,
			T->TradeSessionId,T->PendingInviteId,T->OfferRevision,*GetNameSafe(T->Partner.Get()),
			*GetNameSafe(T->PendingInviteFrom.Get()),*T->PartnerName,T->PendingInviteTime,T->LastTradeChangeTime,
			World->GetTimerManager().IsTimerActive(T->ValidateTimerHandle));
		for(const FMMOTradeOffer* O:{&T->MyOffer,&T->TheirOffer})
		{
			S+=FString::Printf(TEXT("|%d/%d"),O->Gold,O->bConfirmed);
			for(const FMMOEquipmentInstance& I:O->Items)
				S+=FString::Printf(TEXT("/%d:%s:%d:%d"),I.InstanceId,*I.ItemId.ToString(),I.Plus,static_cast<int32>(I.Rarity));
		}
		return S;
	};
	const auto Closed=[&](UMMOTradeComponent* T)
	{ return !T->bTradeActive && !T->Partner.IsValid() && T->PartnerName.IsEmpty()
		&& T->MyOffer.Items.IsEmpty() && T->TheirOffer.Items.IsEmpty() && !T->MyOffer.Gold && !T->TheirOffer.Gold
		&& !T->MyOffer.bConfirmed && !T->TheirOffer.bConfirmed && !World->GetTimerManager().IsTimerActive(T->ValidateTimerHandle); };
	const bool Completed=[&]()
	{
		for(int32 Side=0;Side<2;++Side)
		for(int32 Mode=0;Mode<13;++Mode)
		{
			const int32 Peer=1-Side;
			const FString Case=FString::Printf(TEXT("side%d_mode%d"),Side,Mode);
			AMMOCharacter* P[3]={}; AMMOPlayerController* PC[3]={}; AMMOHUD* H[3]={};
			UMMOTradeComponent* T[3]={}; FMMOCharacterSaveData Before[3];
			FString Names[3];
			FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			for(int32 I=0;I<3;++I)
			{
				P[I]=World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(),FVector(1600000+I*100,1600000,5000),FRotator::ZeroRotator,Params);
				PC[I]=World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(),Params);
				if(P[I]) Pawns.Add(P[I]); if(PC[I]) Controllers.Add(PC[I]);
				if(!P[I] || !PC[I] || !P[I]->GetInventory() || !P[I]->GetTradeComponent())
				{ OutError=TEXT("fixture spawn/components"); return false; }
				P[I]->SetTravelSaveCommitted(true); P[I]->SetActorEnableCollision(false); P[I]->SetActorTickEnabled(false);
				PC[I]->SetActorTickEnabled(false); PC[I]->Possess(P[I]);
				APlayerState* PS=P[I]->GetPlayerState();
				if(!PS) { OutError=TEXT("fixture PlayerState"); return false; }
				Names[I]=FString::Printf(TEXT("O54_%d"),I); PS->SetPlayerName(Names[I]);
				PC[I]->ClientSetHUD(AMMOHUD::StaticClass()); H[I]=Cast<AMMOHUD>(PC[I]->GetHUD());
				if(!H[I]) { OutError=TEXT("native HUD fixture"); return false; } HUDs.Add(H[I]);
				T[I]=P[I]->GetTradeComponent();
				FMMOCharacterSaveData Seed; Seed.Gold=1000+I*100; Seed.NextInstanceId=50;
				FMMOEquipmentInstance Item; Item.InstanceId=40; Item.ItemId=TEXT("ShortSword"); Seed.EquipmentBag.Add(Item);
				P[I]->GetInventory()->ApplySaveData(Seed); Before[I]=Snapshot(P[I]->GetInventory());
			}
			const int32 BaseLines[3]={H[0]->GetChatLinesForTests().Num(),H[1]->GetChatLinesForTests().Num(),H[2]->GetChatLinesForTests().Num()};
			const bool Invalid=Mode>=7;
			if(Invalid && Mode<=10)
			{
				const int32 Retired=(Mode==7 || Mode==9)?Side:Peer;
				if(Mode<=8) P[Retired]->Destroy(); else P[Retired]->RouteEndPlay(EEndPlayReason::LevelTransition);
			}
			FString Initial[3]={State(T[0]),State(T[1]),State(T[2])};
			TStrongObjectPtr<UMMOTradeTeardownProbe> First(NewObject<UMMOTradeTeardownProbe>());
			TStrongObjectPtr<UMMOTradeTeardownProbe> Other(NewObject<UMMOTradeTeardownProbe>());
			TStrongObjectPtr<UMMOTradeTeardownProbe> Third(NewObject<UMMOTradeTeardownProbe>());
			bool SawReady=false, SawMessages=false, Seeded=false; FString Fresh[3];
			First->Callback=[&](int32 Call)
			{
				if(Call!=1) return;
				SawReady=T[Side]->GetValidPartnerComp()==T[Peer] && T[Peer]->GetValidPartnerComp()==T[Side]
					&& T[Side]->TradeSessionId==1 && T[Peer]->TradeSessionId==1;
				for(int32 I=0;I<2;++I) SawReady &= T[I]->OfferRevision==0 && T[I]->MyOffer.Items.IsEmpty()
					&& T[I]->TheirOffer.Items.IsEmpty() && !T[I]->MyOffer.Gold && !T[I]->TheirOffer.Gold
					&& !T[I]->MyOffer.bConfirmed && !T[I]->TheirOffer.bConfirmed
					&& World->GetTimerManager().IsTimerActive(T[I]->ValidateTimerHandle);
				SawMessages=H[Side]->GetChatLinesForTests().Num()==BaseLines[Side]+1
					&& H[Peer]->GetChatLinesForTests().Num()==BaseLines[Peer]+1;
				if(Mode>=1 && Mode<=3 || Mode==6) T[Side]->RequestCancelSeen(1);
				if(Mode==2 || Mode==3 || Mode==6)
				{
					const int32 NewPeer=Mode==3?2:Peer;
					StartTradeServer(P[Side],P[NewPeer]);
					if(T[Side]->GetValidPartnerComp()==T[NewPeer])
					{
						T[Side]->MyOffer.Gold=17; T[NewPeer]->TheirOffer=T[Side]->MyOffer;
						T[NewPeer]->MyOffer.Items.Add(P[NewPeer]->GetInventory()->EquipmentBag[0]); T[Side]->TheirOffer=T[NewPeer]->MyOffer;
						T[Side]->MyOffer.bConfirmed=true; T[NewPeer]->TheirOffer.bConfirmed=true;
						T[Side]->OfferRevision=3; T[NewPeer]->OfferRevision=3;
						for(int32 I=0;I<3;++I) Fresh[I]=State(T[I]); Seeded=true;
					}
					if(Mode==6) T[Side]->EndTradeServer(TEXT("fixture second ended"));
				}
				else if(Mode==4 || Mode==5) P[Mode==4?Side:Peer]->Destroy();
			};
			T[Side]->OnTradeChanged.AddDynamic(First.Get(),&UMMOTradeTeardownProbe::OnTrade);
			T[Peer]->OnTradeChanged.AddDynamic(Other.Get(),&UMMOTradeTeardownProbe::OnTrade);
			T[2]->OnTradeChanged.AddDynamic(Third.Get(),&UMMOTradeTeardownProbe::OnTrade);
			bool SawEarlyEnd=false;
			if(Mode>=11)
			{
				// Real character EndPlay -> party leave callback before Super::EndPlay.
				const int32 Retired=Mode==11?Side:Peer;
				UMMOPartyComponent* Party=P[Retired]->GetPartyComponent();
				UMMOPartyComponent* Mate=P[2]->GetPartyComponent();
				if(!Party || !Mate) { OutError=TEXT("fixture party component"); return false; }
				Party->Members.Add(P[2]); Mate->Members.Add(P[Retired]); Party->Leader=P[Retired]; Mate->Leader=P[Retired];
				Party->PartySessionId=Mate->PartySessionId=FGuid::NewGuid();
				Party->PartyMembershipId=FGuid::NewGuid(); Mate->PartyMembershipId=FGuid::NewGuid();
				T[Peer]->SetPendingInviteServer(P[Side]);
				TStrongObjectPtr<UMMOTradeTeardownProbe> PartyProbe(NewObject<UMMOTradeTeardownProbe>());
				PartyProbe->Callback=[&](int32 Call)
				{
					if(Call!=1) return;
					SawEarlyEnd=P[Retired]->HasActorBegunPlay() && P[Retired]->HasStartedEndPlay() && !P[Retired]->IsActorBeingDestroyed();
					T[Peer]->RequestAcceptSeen(T[Peer]->PendingInviteId);
				};
				Party->OnPartyChanged.AddDynamic(PartyProbe.Get(),&UMMOTradeTeardownProbe::OnTrade);
				P[Retired]->RouteEndPlay(EEndPlayReason::LevelTransition);
				SawEarlyEnd &= PartyProbe->Calls==1;
				Party->OnPartyChanged.RemoveDynamic(PartyProbe.Get(),&UMMOTradeTeardownProbe::OnTrade); PartyProbe->Callback=nullptr;
			}
			else if(Invalid) StartTradeServer(P[Side],P[Peer]);
			else { T[Peer]->SetPendingInviteServer(P[Side]); T[Peer]->RequestAcceptSeen(T[Peer]->PendingInviteId); }
			if(IsValid(T[Side])) T[Side]->OnTradeChanged.RemoveDynamic(First.Get(),&UMMOTradeTeardownProbe::OnTrade);
			if(IsValid(T[Peer])) T[Peer]->OnTradeChanged.RemoveDynamic(Other.Get(),&UMMOTradeTeardownProbe::OnTrade);
			T[2]->OnTradeChanged.RemoveDynamic(Third.Get(),&UMMOTradeTeardownProbe::OnTrade); First->Callback=nullptr;
			Check(Case,TEXT("both_ready_before_first_callback"),Invalid?First->Calls==0 && (Mode<11 || SawEarlyEnd):SawReady);
			const int32 ExpectedFirst[]={1,2,3,3,1,2,4,0,0,0,0,0,0};
			const int32 ExpectedOther[]={1,1,2,1,1,0,3,0,0,0,0,0,0};
			Check(Case,TEXT("exact_delegate_counts"),First->Calls==ExpectedFirst[Mode] && Other->Calls==ExpectedOther[Mode] && Third->Calls==(Mode==3?1:0));
			const bool Active=Mode==0 || Mode==2 || Mode==3; const int32 NewPeer=Mode==3?2:Peer;
			bool FinalOK=true;
			for(int32 I=0;I<3;++I)
				FinalOK &= Active && (I==Side || I==NewPeer) ? T[I]->GetValidPartnerComp()==T[I==Side?NewPeer:Side] : Closed(T[I]);
			if(Mode==2 || Mode==3) FinalOK &= Seeded && State(T[Side])==Fresh[Side] && State(T[NewPeer])==Fresh[NewPeer];
			if(Invalid && Mode<11) for(int32 I=0;I<3;++I) FinalOK &= State(T[I])==Initial[I];
			if(Mode>=11) FinalOK &= T[Peer]->PendingInviteId==1 && !T[Peer]->PendingInviteFrom.IsValid() && T[Peer]->PendingInviteTime==-1.f;
			Check(Case,TEXT("final_pair_payload_or_rejected_state"),FinalOK);
			bool Generations=true,Timers=true,Economy=true;
			for(int32 I=0;I<3;++I)
			{
				const uint64 Expected=Invalid?0:(I==2?(Mode==3?2:0):((Mode==2 || Mode==6 || (Mode==3 && I==Side))?2:1));
				Generations &= T[I]->TradeSessionId==Expected;
				Timers &= World->GetTimerManager().IsTimerActive(T[I]->ValidateTimerHandle)==(Active && (I==Side || I==NewPeer));
				Economy &= Equal(Before[I],Snapshot(P[I]->GetInventory()));
			}
			Check(Case,TEXT("generations"),Generations); Check(Case,TEXT("timers"),Timers); Check(Case,TEXT("economy_unchanged"),Economy);
			// Native controller Client RPC implementation and HUD storage; no wire/rendering proof.
			bool Messages=Invalid || SawMessages;
			for(int32 I=0;I<3;++I)
			{
				TArray<FString> Expected;
				if(!Invalid && I!=2) Expected.Add(Names[1-I]+TEXT(" ile ticaret basladi."));
				if(Mode>=1 && Mode<=3 || Mode==6) if(I!=2) Expected.Add(TEXT("Ticaret iptal edildi."));
				if(Mode==4 && I==Peer || Mode==5 && I==Side) Expected.Add(TEXT("Ticaret iptal edildi: karsi taraf ayrildi."));
				if(Mode==2 || Mode==6 || Mode==3 && (I==Side || I==2))
					if(I!=2 || Mode==3) Expected.Add(Names[I==Side?NewPeer:Side]+TEXT(" ile ticaret basladi."));
				if(Mode==6 && I!=2) Expected.Add(TEXT("fixture second ended"));
				const auto& Lines=H[I]->GetChatLinesForTests(); TArray<FString> TradeLines;
				for(int32 N=BaseLines[I];N<Lines.Num();++N)
					if(Lines[N].Text.Contains(TEXT("ticaret")) || Lines[N].Text==TEXT("fixture second ended")) TradeLines.Add(Lines[N].Text);
				Messages &= TradeLines==Expected;
				if(TradeLines!=Expected) UE_LOG(LogTemp,Display,TEXT("[MMO_TRADE_START_CHAT] case=%s endpoint=%d actual=%s expected=%s"),*Case,I,*FString::Join(TradeLines,TEXT("|")),*FString::Join(Expected,TEXT("|")));
			}
			Check(Case,TEXT("native_chat_order_and_no_stale_start"),Messages);
			const FString Final[3]={State(T[0]),State(T[1]),State(T[2])};
			for(int32 I=0;I<3;++I) if(IsValid(P[I]) && P[I]->HasActorBegunPlay() && !P[I]->IsActorBeingDestroyed()) T[I]->RequestCancelSeen(Mode==2 || Mode==3 || Mode==6?1:0);
			Check(Case,TEXT("stale_or_zero_cancel_no_effect"),State(T[0])==Final[0] && State(T[1])==Final[1] && State(T[2])==Final[2]);
			for(int32 I=0;I<3;++I) if(IsValid(T[I])) T[I]->EndTradeServer(TEXT("fixture cleanup"));
		}
		return true;
	}();
	for(AMMOCharacter* P:Pawns) if(IsValid(P)) { P->SetTravelSaveCommitted(true); P->Destroy(); }
	for(AMMOPlayerController* PC:Controllers) if(IsValid(PC)) { PC->UnPossess(); PC->Destroy(); }
	for(AMMOHUD* H:HUDs) if(IsValid(H)) H->Destroy();
	if(!Completed) return false;
	if(OutChecks!=208) { ++Failures; OutError+=TEXT(" check_count_mismatch"); }
	if(Failures) OutError=FString::Printf(TEXT("failures=%d first=%s"),Failures,*OutError);
	return Failures==0;
}

namespace
{
	struct FMMOTradeStartTestExec:public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* World,const TCHAR* Cmd,FOutputDevice& Ar) override
		{
			if(!FParse::Command(&Cmd,TEXT("MMOTestTradeStart"))) return false;
			FString Error; int32 Checks=0;
			const bool Passed=UMMOTradeComponent::RunTradeStartSelfTest(World,Error,Checks);
			Ar.Logf(TEXT("[MMO_TRADE_START_TEST] %s checks=%d reason=%s"),Passed?TEXT("PASS"):TEXT("FAIL"),Checks,*Error); return true;
		}
	};
	FMMOTradeStartTestExec GMMOTradeStartTestExec;
}
#endif

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

bool UMMOTradeComponent::RunTradeTeardownSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client
		|| !FParse::Param(FCommandLine::Get(), TEXT("unattended"))
		|| !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
	{ OutError = TEXT("disposable authority headless game world required"); return false; }
	TArray<AMMOCharacter*> Fixtures;
	TArray<AMMOPlayerController*> Controllers; TArray<AMMOHUD*> HUDs;
	int32 Failures = 0;
	const auto Check = [&](const FString& Case, const TCHAR* Label, bool bOK)
	{
		++OutChecks;
		UE_LOG(LogTemp, Display, TEXT("[MMO_TRADE_TEARDOWN_CASE] %s case=%s check=%s"), bOK ? TEXT("PASS") : TEXT("FAIL"), *Case, Label);
		if (!bOK) { ++Failures; if (OutError.IsEmpty()) OutError = Case + TEXT("/") + Label; }
	};
	const auto Snapshot = [](UMMOInventoryComponent* Inv)
	{ FMMOCharacterSaveData D; Inv->FillSaveData(D); return D; };
	const auto EqualSave = [](const FMMOCharacterSaveData& A, const FMMOCharacterSaveData& B)
	{ TArray<FString> D; FMMOCharacterSaveJson::FindDifferences(A, B, D); return D.IsEmpty(); };
	const auto State = [&](UMMOTradeComponent* T)
	{
		// Exact current trade state, including payloads, invite context, clocks and timer.
		FString S = FString::Printf(TEXT("%d/%s/%llu/%llu/%d/%s/%s/%g/%g/%g/%g/%g/%d"),
			T->bTradeActive, *T->PartnerName, T->TradeSessionId, T->PendingInviteId, T->OfferRevision,
			*GetNameSafe(T->Partner.Get()), *GetNameSafe(T->PendingInviteFrom.Get()), T->PendingInviteTime,
			T->LastInviteTime, T->LastOfferChangeTime, T->LastTradeChangeTime, T->LastConfirmTime,
			World->GetTimerManager().IsTimerActive(T->ValidateTimerHandle));
		for (const FMMOTradeOffer* O : {&T->MyOffer, &T->TheirOffer})
		{
			S += FString::Printf(TEXT("|%d/%d"), O->Gold, O->bConfirmed);
			for (const FMMOEquipmentInstance& I : O->Items)
				S += FString::Printf(TEXT("/%d:%s:%d:%d"), I.InstanceId, *I.ItemId.ToString(), I.Plus, static_cast<int32>(I.Rarity));
		}
		return S;
	};
	const auto Closed = [&](UMMOTradeComponent* T)
	{
		return !T->bTradeActive && !T->Partner.IsValid() && T->PartnerName.IsEmpty()
			&& T->MyOffer.Items.IsEmpty() && T->TheirOffer.Items.IsEmpty()
			&& !T->MyOffer.Gold && !T->TheirOffer.Gold && !T->MyOffer.bConfirmed && !T->TheirOffer.bConfirmed
			&& !World->GetTimerManager().IsTimerActive(T->ValidateTimerHandle);
	};
	const bool bCompleted = [&]()
	{
		for (int32 Side = 0; Side < 2; ++Side)
		for (int32 Mode = 0; Mode < 11; ++Mode)
		{
			AMMOCharacter* P[3] = {};
			UMMOTradeComponent* T[3] = {};
			FMMOCharacterSaveData Before[3];
			FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			for (int32 I = 0; I < 3; ++I)
			{
				P[I] = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(1200000 + I*100,1200000,5000), FRotator::ZeroRotator, Params);
				if (!P[I]) { OutError = TEXT("fixture spawn failed"); return false; }
				Fixtures.Add(P[I]); P[I]->SetActorEnableCollision(false); P[I]->SetTravelSaveCommitted(true);
				T[I] = P[I]->GetTradeComponent();
				if (!T[I] || !P[I]->GetInventory()) { OutError = TEXT("fixture component missing"); return false; }
				FMMOCharacterSaveData Seed; Seed.Gold=1000 + I*100; Seed.NextInstanceId=50;
				FMMOEquipmentInstance Item; Item.InstanceId=40; Item.ItemId=TEXT("ShortSword"); Seed.EquipmentBag.Add(Item);
				P[I]->GetInventory()->ApplySaveData(Seed); Before[I]=Snapshot(P[I]->GetInventory());
			}
			StartTradeServer(P[0], P[1]);
			const int32 Peer = 1-Side;
			const uint64 OldSession = T[Side]->TradeSessionId;
			T[0]->MyOffer.Gold=100; T[1]->TheirOffer=T[0]->MyOffer;
			T[1]->MyOffer.Items.Add(P[1]->GetInventory()->EquipmentBag[0]); T[0]->TheirOffer=T[1]->MyOffer;
			T[0]->MyOffer.bConfirmed=true; T[1]->TheirOffer.bConfirmed=true;
			const FString Case=FString::Printf(TEXT("side%d_mode%d"),Side,Mode);
			const auto EconomyUnchanged = [&]()
			{ for(int32 I=0;I<3;++I) if(!EqualSave(Before[I],Snapshot(P[I]->GetInventory()))) return false; return true; };
			if (Mode == 4 || Mode == 5)
			{
				// A stale local pointer must not clear a peer now associated with somebody else.
				if (Mode == 4) T[Peer]->Partner=P[2]; else ++T[Peer]->TradeSessionId;
				const FString PeerBefore=State(T[Peer]), ThirdBefore=State(T[2]);
				T[Side]->EndTradeServer(TEXT("fixture stale local cleanup"));
				Check(Case,TEXT("local_closed"),Closed(T[Side]));
				Check(Case,TEXT("foreign_peer_preserved"),State(T[Peer])==PeerBefore && State(T[2])==ThirdBefore);
				Check(Case,TEXT("economy_unchanged"),EconomyUnchanged());
				T[Peer]->EndTradeServer(TEXT("fixture broken reciprocal cleanup"));
				Check(Case,TEXT("cleanup_no_third_mutation"),Closed(T[Peer]) && State(T[2])==ThirdBefore);
				continue;
			}
			if (Mode == 10)
			{
				TStrongObjectPtr<UMMOTradeTeardownProbe> First(NewObject<UMMOTradeTeardownProbe>());
				TStrongObjectPtr<UMMOTradeTeardownProbe> Other(NewObject<UMMOTradeTeardownProbe>());
				bool bSawClosed=false;
				First->Callback=[&](int32 Call)
				{
					if(Call!=1) return;
					bSawClosed=Closed(T[Side]) && Closed(T[Peer]);
					StartTradeServer(P[Side],P[Peer]);
					T[Side]->EndTradeServer(TEXT("fixture new session also ended"));
				};
				T[Side]->OnTradeChanged.AddDynamic(First.Get(),&UMMOTradeTeardownProbe::OnTrade);
				T[Peer]->OnTradeChanged.AddDynamic(Other.Get(),&UMMOTradeTeardownProbe::OnTrade);
				T[Side]->RequestCancelSeen(OldSession);
				T[Side]->OnTradeChanged.RemoveDynamic(First.Get(),&UMMOTradeTeardownProbe::OnTrade);
				T[Peer]->OnTradeChanged.RemoveDynamic(Other.Get(),&UMMOTradeTeardownProbe::OnTrade);
				First->Callback=nullptr;
				Check(Case,TEXT("first_callback_both_closed"),bSawClosed);
				Check(Case,TEXT("new_session_ended"),Closed(T[Side]) && Closed(T[Peer]));
				Check(Case,TEXT("ended_generation"),T[Side]->TradeSessionId==OldSession+1 && T[Peer]->TradeSessionId==OldSession+1);
				Check(Case,TEXT("no_old_publication_after_new_end"),First->Calls==3 && Other->Calls==2);
				Check(Case,TEXT("economy_unchanged"),EconomyUnchanged());
				const FString A=State(T[Side]),B=State(T[Peer]);
				T[Side]->RequestCancelSeen(OldSession); T[Peer]->RequestCancelSeen(OldSession);
				Check(Case,TEXT("stale_cancel_no_effect"),State(T[Side])==A && State(T[Peer])==B);
				continue;
			}
			if (Mode >= 6)
			{
				TStrongObjectPtr<UMMOTradeTeardownProbe> First(NewObject<UMMOTradeTeardownProbe>());
				TStrongObjectPtr<UMMOTradeTeardownProbe> Other(NewObject<UMMOTradeTeardownProbe>());
				bool bPeerSawClosed=false;
				First->Callback=[&](int32 Call)
				{ if(Call==1 && (Mode==7 || Mode==8)) P[Mode==7?Side:Peer]->Destroy(); };
				Other->Callback=[&](int32)
				{ bPeerSawClosed=Closed(T[Side]) && Closed(T[Peer]); };
				T[Side]->OnTradeChanged.AddDynamic(First.Get(),&UMMOTradeTeardownProbe::OnTrade);
				T[Peer]->OnTradeChanged.AddDynamic(Other.Get(),&UMMOTradeTeardownProbe::OnTrade);
				if(Mode==9) P[Side]->Destroy(); else T[Side]->RequestCancelSeen(OldSession);
				if(Mode==6)
				{
					T[Side]->PendingInviteFrom=P[2]; T[Side]->PendingInviteTime=World->GetTimeSeconds(); T[Side]->PendingInviteId=7;
					const FString A=State(T[Side]), B=State(T[Peer]);
					const int32 CallsA=First->Calls, CallsB=Other->Calls;
					T[Side]->EndTradeServer(TEXT("fixture duplicate end")); T[Peer]->EndTradeServer(TEXT("fixture duplicate end"));
					Check(Case,TEXT("inactive_state_unchanged"),State(T[Side])==A && State(T[Peer])==B);
					Check(Case,TEXT("inactive_no_publication"),First->Calls==CallsA && Other->Calls==CallsB);
					Check(Case,TEXT("pending_invite_preserved"),T[Side]->PendingInviteFrom.Get()==P[2] && T[Side]->PendingInviteId==7);
					Check(Case,TEXT("economy_unchanged"),EconomyUnchanged());
				}
				else
				{
					const int32 Destroyed=Mode==8?Peer:Side;
					Check(Case,TEXT("destroy_lifecycle_routed"),P[Destroyed]->IsActorBeingDestroyed());
					Check(Case,TEXT("survivor_closed"),Closed(T[1-Destroyed]) && Closed(T[2]));
					Check(Case,TEXT("no_destroyed_endpoint_publication"),First->Calls==(Mode==9?0:1) && Other->Calls==(Mode==8?0:1)
						&& (Mode==8 || bPeerSawClosed));
					Check(Case,TEXT("survivor_economy_unchanged"),EqualSave(Before[1-Destroyed],Snapshot(P[1-Destroyed]->GetInventory()))
						&& EqualSave(Before[2],Snapshot(P[2]->GetInventory())));
				}
				// Destroy defers reclamation; remove bindings only from live components.
				if(IsValid(T[Side])) T[Side]->OnTradeChanged.RemoveDynamic(First.Get(),&UMMOTradeTeardownProbe::OnTrade);
				if(IsValid(T[Peer])) T[Peer]->OnTradeChanged.RemoveDynamic(Other.Get(),&UMMOTradeTeardownProbe::OnTrade);
				First->Callback=nullptr; Other->Callback=nullptr;
				continue;
			}
			TStrongObjectPtr<UMMOTradeTeardownProbe> First(NewObject<UMMOTradeTeardownProbe>());
			TStrongObjectPtr<UMMOTradeTeardownProbe> Other(NewObject<UMMOTradeTeardownProbe>());
			bool bSawClosed=false;
			FString Fresh[3]; bool bSeededFresh=false;
			const auto Restart = [&](int32 NewPeer)
			{
				StartTradeServer(P[Side],P[NewPeer]);
				if(T[Side]->GetValidPartnerComp()!=T[NewPeer]) return;
				T[Side]->MyOffer.Gold=17; T[NewPeer]->TheirOffer=T[Side]->MyOffer;
				T[NewPeer]->MyOffer.Items.Add(P[NewPeer]->GetInventory()->EquipmentBag[0]); T[Side]->TheirOffer=T[NewPeer]->MyOffer;
				T[Side]->MyOffer.bConfirmed=true; T[NewPeer]->TheirOffer.bConfirmed=true;
				T[Side]->OfferRevision=3; T[NewPeer]->OfferRevision=3;
				for(int32 I=0;I<3;++I) Fresh[I]=State(T[I]);
				bSeededFresh=true;
			};
			First->Callback = [&](int32 Call)
			{
				if(Call!=1) return;
				bSawClosed=Closed(T[Side]) && Closed(T[Peer]) && T[Side]->TradeSessionId==OldSession && T[Peer]->TradeSessionId==OldSession;
				if(Mode==1) T[Peer]->RequestCancelSeen(OldSession);
				else if(Mode==2) Restart(Peer);
				else if(Mode==3) Restart(2);
			};
			Other->Callback = [&](int32 Call)
			{ if(Call==1 && Mode==1 && Closed(T[Side]) && Closed(T[Peer])) Restart(Peer); };
			T[Side]->OnTradeChanged.AddDynamic(First.Get(),&UMMOTradeTeardownProbe::OnTrade);
			T[Peer]->OnTradeChanged.AddDynamic(Other.Get(),&UMMOTradeTeardownProbe::OnTrade);
			T[Side]->RequestCancelSeen(OldSession);
			T[Side]->OnTradeChanged.RemoveDynamic(First.Get(),&UMMOTradeTeardownProbe::OnTrade);
			T[Peer]->OnTradeChanged.RemoveDynamic(Other.Get(),&UMMOTradeTeardownProbe::OnTrade);
			First->Callback=nullptr; Other->Callback=nullptr;
			const int32 NewPeer=Mode==3?2:Peer;
			const bool bFresh=Mode!=0;
			Check(Case,TEXT("first_callback_both_closed"),bSawClosed);
			Check(Case,TEXT("exact_delegate_counts"),First->Calls==(Mode==0?1:2) && Other->Calls==(Mode==1?2:1));
			Check(Case,TEXT("final_pair"),bFresh ? T[Side]->GetValidPartnerComp()==T[NewPeer] && T[NewPeer]->GetValidPartnerComp()==T[Side]
				: Closed(T[Side]) && Closed(T[Peer]));
			FString Final[3]={State(T[0]),State(T[1]),State(T[2])};
			for(int32 I=0;I<2;++I) T[I]->RequestCancelSeen(OldSession);
			Check(Case,TEXT("stale_cancel_no_effect"),State(T[0])==Final[0] && State(T[1])==Final[1] && State(T[2])==Final[2]);
			Check(Case,TEXT("economy_unchanged"),EconomyUnchanged());
			Check(Case,TEXT("generation"),bFresh ? T[Side]->TradeSessionId==OldSession+1 && T[NewPeer]->TradeSessionId==OldSession+1
				: T[0]->TradeSessionId==OldSession && T[1]->TradeSessionId==OldSession);
			bool bTimers=true;
			for(int32 I=0;I<3;++I) bTimers &= World->GetTimerManager().IsTimerActive(T[I]->ValidateTimerHandle)==(bFresh && (I==Side || I==NewPeer));
			Check(Case,TEXT("timers"),bTimers);
			Check(Case,TEXT("fresh_payload_preserved"),bFresh ? bSeededFresh && State(T[Side])==Fresh[Side] && State(T[NewPeer])==Fresh[NewPeer]
				: Closed(T[2]));
			T[Side]->EndTradeServer(TEXT("fixture final cleanup"));
		}
		// Native Character EndPlay -> party leave callout before Super::EndPlay.
		// Seed only the public roster/context; this is not invitation/remote RPC proof.
		for(int32 Retired=0;Retired<2;++Retired) for(int32 Caller=0;Caller<2;++Caller) for(int32 Route=0;Route<2;++Route)
		{
			const FString Case=FString::Printf(TEXT("early_retired%d_caller%d_route%d"),Retired,Caller,Route);
			AMMOCharacter* P[2]={}; AMMOPlayerController* PC[2]={}; AMMOHUD* H[2]={};
			UMMOTradeComponent* T[2]={}; UMMOPartyComponent* Party[2]={}; FMMOCharacterSaveData Before[2];
			FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			for(int32 I=0;I<2;++I)
			{
				P[I]=World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(),FVector(2300000+I*100,2300000,5000),FRotator::ZeroRotator,Params);
				PC[I]=World->SpawnActor<AMMOPlayerController>(AMMOPlayerController::StaticClass(),Params);
				if(P[I]) Fixtures.Add(P[I]); if(PC[I]) Controllers.Add(PC[I]);
				if(!P[I] || !PC[I] || !P[I]->GetInventory() || !P[I]->GetTradeComponent() || !P[I]->GetPartyComponent())
				{ OutError=TEXT("early fixture spawn/components"); return false; }
				P[I]->SetTravelSaveCommitted(true); P[I]->SetActorTickEnabled(false); P[I]->SetActorEnableCollision(false);
				PC[I]->SetActorTickEnabled(false); PC[I]->Possess(P[I]);
				if(!P[I]->GetPlayerState()) { OutError=TEXT("early fixture PlayerState"); return false; }
				P[I]->GetPlayerState()->SetPlayerName(FString::Printf(TEXT("O58_%d"),I));
				PC[I]->ClientSetHUD(AMMOHUD::StaticClass()); H[I]=Cast<AMMOHUD>(PC[I]->GetHUD());
				if(!H[I]) { OutError=TEXT("early fixture HUD"); return false; } HUDs.Add(H[I]);
				T[I]=P[I]->GetTradeComponent(); Party[I]=P[I]->GetPartyComponent();
				FMMOCharacterSaveData Seed; Seed.Gold=1000+I*100; Seed.NextInstanceId=50;
				FMMOEquipmentInstance Item; Item.ItemId=TEXT("ShortSword"); Item.InstanceId=40; Seed.EquipmentBag.Add(Item);
				P[I]->GetInventory()->ApplySaveData(Seed); Before[I]=Snapshot(P[I]->GetInventory());
			}
			Party[0]->Members.Add(P[1]); Party[1]->Members.Add(P[0]);
			Party[0]->Leader=Party[1]->Leader=P[0]; Party[0]->PartySessionId=Party[1]->PartySessionId=FGuid::NewGuid();
			Party[0]->PartyMembershipId=FGuid::NewGuid(); Party[1]->PartyMembershipId=FGuid::NewGuid();
			StartTradeServer(P[0],P[1]);
			T[0]->MyOffer.Gold=17; T[1]->TheirOffer=T[0]->MyOffer;
			T[1]->MyOffer.Items.Add(P[1]->GetInventory()->EquipmentBag[0]); T[0]->TheirOffer=T[1]->MyOffer;
			T[1]->MyOffer.bConfirmed=true; T[0]->TheirOffer.bConfirmed=true;
			for(int32 I=0;I<2;++I)
			{
				T[I]->OfferRevision=7; T[I]->PendingInviteId=100+I;
				T[I]->LastInviteTime=-91.f-I; T[I]->LastOfferChangeTime=-81.f-I;
				T[I]->LastTradeChangeTime=-71.f-I; T[I]->LastConfirmTime=-61.f-I;
			}
			const auto ContextUnchanged=[&]()
			{
				for(int32 I=0;I<2;++I) if(T[I]->OfferRevision!=7 || T[I]->PendingInviteId!=100+I
					|| T[I]->PendingInviteFrom.IsValid() || T[I]->PendingInviteTime!=-1.f
					|| T[I]->LastInviteTime!=-91.f-I || T[I]->LastOfferChangeTime!=-81.f-I
					|| T[I]->LastTradeChangeTime!=-71.f-I || T[I]->LastConfirmTime!=-61.f-I) return false;
				return true;
			};
			const int32 Base[2]={H[0]->GetChatLinesForTests().Num(),H[1]->GetChatLinesForTests().Num()};
			TStrongObjectPtr<UMMOTradeTeardownProbe> TradeProbe[2]={TStrongObjectPtr<UMMOTradeTeardownProbe>(NewObject<UMMOTradeTeardownProbe>()),TStrongObjectPtr<UMMOTradeTeardownProbe>(NewObject<UMMOTradeTeardownProbe>())};
			TStrongObjectPtr<UMMOTradeTeardownProbe> PartyProbe(NewObject<UMMOTradeTeardownProbe>());
			bool SawEarly=false, ClosedAtHook=false, LivePublications=true, ClosedAtPublication=true;
			Check(Case,TEXT("nonvacuous_pair_and_party"),T[0]->GetValidPartnerComp()==T[1] && T[1]->GetValidPartnerComp()==T[0]
				&& T[0]->TradeSessionId==1 && T[1]->TradeSessionId==1 && World->GetTimerManager().IsTimerActive(T[0]->ValidateTimerHandle)
				&& World->GetTimerManager().IsTimerActive(T[1]->ValidateTimerHandle) && Party[0]->Members.Num()==1 && Party[1]->Members.Num()==1
				&& Party[0]->PartySessionId.IsValid() && Party[0]->PartySessionId==Party[1]->PartySessionId
				&& Party[0]->PartyMembershipId.IsValid() && Party[1]->PartyMembershipId.IsValid());
			PartyProbe->Callback=[&](int32 Call)
			{
				if(Call!=1) return;
				SawEarly=P[Retired]->HasActorBegunPlay() && P[Retired]->HasStartedEndPlay() && !P[Retired]->IsActorBeingDestroyed()
					&& P[Retired]->GetController()==PC[Retired] && (Route==1 || T[0]->bTradeActive && T[1]->bTradeActive);
				if(Route==0) T[Caller]->RequestCancelSeen(1);
				ClosedAtHook=Closed(T[0]) && Closed(T[1]);
			};
			Party[Retired]->OnPartyChanged.AddDynamic(PartyProbe.Get(),&UMMOTradeTeardownProbe::OnTrade);
			for(int32 I=0;I<2;++I)
			{
				TradeProbe[I]->Callback=[&,I](int32 Call)
				{
					ClosedAtPublication &= Closed(T[0]) && Closed(T[1]) && T[0]->TradeSessionId==1 && T[1]->TradeSessionId==1;
					LivePublications &= !P[I]->HasStartedEndPlay() && P[I]->HasActorBegunPlay() && !P[I]->IsActorBeingDestroyed();
					if(Route==1 && I==Caller && Call==1) P[Retired]->RouteEndPlay(EEndPlayReason::LevelTransition);
				};
				T[I]->OnTradeChanged.AddDynamic(TradeProbe[I].Get(),&UMMOTradeTeardownProbe::OnTrade);
			}
			if(Route==0) P[Retired]->RouteEndPlay(EEndPlayReason::LevelTransition); else T[Caller]->RequestCancelSeen(1);
			Check(Case,TEXT("native_early_window"),SawEarly && PartyProbe->Calls==1);
			Check(Case,TEXT("both_closed_inside_party_hook"),ClosedAtHook);
			Check(Case,TEXT("no_retiring_delegate"),LivePublications);
			Check(Case,TEXT("closed_before_each_trade_observer"),ClosedAtPublication && TradeProbe[1-Retired]->Calls==1);
			Check(Case,TEXT("exact_trade_delegate_counts"),TradeProbe[Retired]->Calls==(Route==1 && Retired==Caller?1:0) && TradeProbe[1-Retired]->Calls==1);
			bool Chat=true;
			for(int32 I=0;I<2;++I)
			{
				TArray<FString> Actual; const auto& Lines=H[I]->GetChatLinesForTests();
				for(int32 N=Base[I];N<Lines.Num();++N) Actual.Add(Lines[N].Text);
				TArray<FString> Expected; if(Route==1 || I!=Retired) Expected.Add(TEXT("Ticaret iptal edildi."));
				Chat &= Actual==Expected;
			}
			Check(Case,TEXT("native_chat_exact"),Chat);
			Check(Case,TEXT("party_cleanup_completed"),!Party[0]->IsInParty() && !Party[1]->IsInParty() && !Party[0]->PartyMembershipId.IsValid() && !Party[1]->PartyMembershipId.IsValid());
			Check(Case,TEXT("trade_states_and_timers_closed"),Closed(T[0]) && Closed(T[1]) && T[0]->TradeSessionId==1 && T[1]->TradeSessionId==1 && ContextUnchanged());
			const auto Economy=[&]() { return EqualSave(Before[0],Snapshot(P[0]->GetInventory())) && EqualSave(Before[1],Snapshot(P[1]->GetInventory())); };
			Check(Case,TEXT("economy_unchanged"),Economy());
			const int32 Calls[2]={TradeProbe[0]->Calls,TradeProbe[1]->Calls}, ChatCounts[2]={H[0]->GetChatLinesForTests().Num(),H[1]->GetChatLinesForTests().Num()};
			const FString Final[2]={State(T[0]),State(T[1])};
			for(int32 I=0;I<2;++I) { T[I]->RequestCancelSeen(1); T[I]->RequestConfirmSeen(1,0); }
			Check(Case,TEXT("stale_requests_no_effect"),State(T[0])==Final[0] && State(T[1])==Final[1] && Economy() && ContextUnchanged()
				&& TradeProbe[0]->Calls==Calls[0] && TradeProbe[1]->Calls==Calls[1] && H[0]->GetChatLinesForTests().Num()==ChatCounts[0] && H[1]->GetChatLinesForTests().Num()==ChatCounts[1]);
			Check(Case,TEXT("native_endplay_completed"),P[Retired]->HasStartedEndPlay() && !P[Retired]->HasActorBegunPlay() && !P[Retired]->IsActorBeingDestroyed());
			Party[Retired]->OnPartyChanged.RemoveDynamic(PartyProbe.Get(),&UMMOTradeTeardownProbe::OnTrade); PartyProbe->Callback=nullptr;
			for(int32 I=0;I<2;++I) { T[I]->OnTradeChanged.RemoveDynamic(TradeProbe[I].Get(),&UMMOTradeTeardownProbe::OnTrade); TradeProbe[I]->Callback=nullptr; }
		}
		return true;
	}();
	for(AMMOCharacter* P:Fixtures) if(IsValid(P)) { if(P->GetTradeComponent()) P->GetTradeComponent()->EndTradeServer(TEXT("fixture cleanup")); P->Destroy(); }
	for(AMMOPlayerController* PC:Controllers) if(IsValid(PC)) { PC->UnPossess(); PC->Destroy(); }
	for(AMMOHUD* H:HUDs) if(IsValid(H)) H->Destroy();
	if(!bCompleted) return false;
	if(OutChecks!=220) { ++Failures; OutError += TEXT(" check_count_mismatch"); }
	if(Failures) OutError=FString::Printf(TEXT("failures=%d first=%s"),Failures,*OutError);
	return Failures==0;
}

namespace
{
	struct FMMOTradeTeardownTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* World,const TCHAR* Cmd,FOutputDevice& Ar) override
		{
			if(!FParse::Command(&Cmd,TEXT("MMOTestTradeTeardown"))) return false;
			FString Error; int32 Checks=0;
			const bool bPassed=UMMOTradeComponent::RunTradeTeardownSelfTest(World,Error,Checks);
			Ar.Logf(TEXT("[MMO_TRADE_TEARDOWN_TEST] %s checks=%d reason=%s"),bPassed?TEXT("PASS"):TEXT("FAIL"),Checks,*Error);
			return true;
		}
	};
	FMMOTradeTeardownTestExec GMMOTradeTeardownTestExec;
}
#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MMOClanComponent.h"
#include "Characters/MMOCharacter.h"
#include "Components/MMOStatsComponent.h"
#include "Components/MMOInventoryComponent.h"
#include "Save/MMOCharacterSaveJson.h"
#include "Player/MMOPlayerController.h"
#include "Save/MMOSaveGame.h"
#include "Save/MMOCharacterStoreSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#if !UE_BUILD_SHIPPING
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMMOClanAudit, Log, All);

#if !UE_BUILD_SHIPPING
bool UMMOClanComponent::RunInviteAuthoritySelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0;
	OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() != NM_Standalone
		|| !FParse::Param(FCommandLine::Get(), TEXT("unattended")) || !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
	{
		OutError = TEXT("disposable standalone headless world required");
		return false;
	}
	TArray<AMMOCharacter*> Actors;
	const bool bPassed = [&]()
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (int32 I = 0; I < 2; ++I)
		{
			AMMOCharacter* Character = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(0, 0, 5000), FRotator::ZeroRotator, Params);
			if (!Character) { OutError = TEXT("fixture spawn failed"); return false; }
			Actors.Add(Character);
		}
		UMMOClanComponent* Inviter = Actors[0]->GetClanComponent();
		UMMOClanComponent* Target = Actors[1]->GetClanComponent();
		// Hicbir gercek cevrimici gruba bildirim/liderlik yan etkisi olmasin.
		const FString FixtureClan = TEXT("Tst") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(9);
		TArray<AMMOCharacter*> Online;
		Inviter->GatherOnlineMembers(FixtureClan, Online, false);
		if (!Online.IsEmpty()) { OutError = TEXT("fixture name collision"); return false; }
		Inviter->ClanName = FixtureClan;
		Inviter->ClanMembershipId = FGuid::NewGuid();
		Inviter->bIsLeader = true;
		auto SetInvite = [&]()
		{
			return Target->SetPendingInviteServer(Actors[0]);
		};
		auto Consumed = [&]()
		{
			return !Target->PendingInviteFrom.IsValid() && Target->PendingInviteClan.IsEmpty() && Target->PendingInviteTime < 0.f && !Target->PendingInviterMembershipId.IsValid();
		};
		auto Check = [&](bool Condition, const TCHAR* Label)
		{
			if (!Condition) { OutError = Label; return false; }
			++OutChecks;
			return true;
		};
		if (!SetInvite()) { OutError = TEXT("authority fixture invite issuance"); return false; }
		Inviter->bIsLeader = false; // Liderligi kaybetti ama ayni klanda uye olarak kaldi.
		Target->CmdAccept();
		if (!Check(!Target->IsInClan() && Consumed(), TEXT("demoted inviter accepted"))) { return false; }
		Inviter->bIsLeader = true;
		if (!SetInvite()) { OutError = TEXT("authority fixture invite issuance"); return false; }
		Target->CmdAccept();
		if (!Check(Target->ClanName == FixtureClan && Target->ClanMembershipId.IsValid() && !Target->bIsLeader && Consumed(), TEXT("valid leader rejected"))) { return false; }
		Target->ClanName.Reset();
		Inviter->bActingLeader = true;
		if (!SetInvite()) { OutError = TEXT("authority fixture invite issuance"); return false; }
		Target->CmdAccept();
		if (!Check(Target->ClanName == FixtureClan && !Target->bIsLeader && Consumed(), TEXT("valid acting leader rejected"))) { return false; }
		Inviter->bActingLeader = false;
		if (!SetInvite()) { OutError = TEXT("authority fixture invite issuance"); return false; }
		Target->CmdAccept();
		if (!Check(Target->ClanName == FixtureClan && Consumed(), TEXT("existing membership changed"))) { return false; }
		Target->ClanName.Reset();
		if (!SetInvite()) { OutError = TEXT("authority fixture invite issuance"); return false; }
		Inviter->ClanName.Reset();
		Target->CmdAccept();
		if (!Check(!Target->IsInClan() && Consumed(), TEXT("departed inviter accepted"))) { return false; }
		Inviter->ClanName = FixtureClan;
		if (!SetInvite()) { OutError = TEXT("authority fixture invite issuance"); return false; }
		Target->PendingInviteTime -= InviteExpirySeconds + 1.f;
		Target->CmdAccept();
		if (!Check(!Target->IsInClan() && Consumed(), TEXT("invalid invite timestamp accepted"))) { return false; }
		if (!SetInvite()) { OutError = TEXT("authority fixture invite issuance"); return false; }
		Target->PendingInviteFrom = Actors[1];
		Target->CmdAccept();
		if (!Check(!Target->IsInClan() && Consumed(), TEXT("self invitation accepted"))) { return false; }
		if (!SetInvite()) { OutError = TEXT("authority fixture invite issuance"); return false; }
		Target->PendingInviteFrom.Reset();
		Target->CmdAccept();
		if (!Check(!Target->IsInClan() && Consumed(), TEXT("missing inviter accepted"))) { return false; }
		if (!SetInvite()) { OutError = TEXT("authority fixture invite issuance"); return false; }
		Inviter->bIsLeader = false; // Fixture'in EndPlay'i gercek oyuncular icin lider secmemeli.
		Inviter->ClanName.Reset();
		Actors[0]->Destroy();
		Target->CmdAccept();
		if (!Check(!Target->IsInClan() && Consumed(), TEXT("destroyed inviter accepted"))) { return false; }
		AMMOCharacter* GenerationActors[3] = {};
		for (int32 I = 0; I < 3; ++I)
		{
			GenerationActors[I] = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(1700000 + I * 100, 1700000, 5000), FRotator::ZeroRotator, Params);
			if (!GenerationActors[I]) { OutError = TEXT("generation fixture spawn"); return false; }
			Actors.Add(GenerationActors[I]);
			GenerationActors[I]->SetActorTickEnabled(false); GenerationActors[I]->SetActorEnableCollision(false);
		}
		UMMOClanComponent* Leaders[2] = {GenerationActors[0]->GetClanComponent(), GenerationActors[1]->GetClanComponent()};
		UMMOClanComponent* Recipient = GenerationActors[2]->GetClanComponent();
		const FString ClanA = TEXT("A") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(11);
		const FString ClanB = TEXT("B") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(11);
		Leaders[0]->ClanName = ClanA; Leaders[1]->ClanName = ClanB;
		Leaders[0]->bIsLeader = true; Leaders[1]->bIsLeader = true;
		Leaders[0]->ClanMembershipId = FGuid::NewGuid(); Leaders[1]->ClanMembershipId = FGuid::NewGuid();
		struct FClanSnapshot
		{
			FString Name, PendingClan; bool Leader, Acting; int64 Seq; uint64 Id; FGuid Membership, PendingMembership;
			TWeakObjectPtr<AMMOCharacter> Pending; float Time, Last;
			FMMOCharacterSaveData Inventory;
		};
		auto Capture = [&]()
		{
			TArray<FClanSnapshot> Result;
			for (AMMOCharacter* C : GenerationActors)
			{
				UMMOClanComponent* CC = C->GetClanComponent(); FClanSnapshot Snap;
				Snap.Name = CC->ClanName; Snap.Leader = CC->bIsLeader; Snap.Acting = CC->bActingLeader; Snap.Seq = CC->JoinSeq;
				Snap.Id = CC->PendingInviteId; Snap.Membership = CC->ClanMembershipId; Snap.Pending = CC->PendingInviteFrom; Snap.PendingClan = CC->PendingInviteClan;
				Snap.PendingMembership = CC->PendingInviterMembershipId; Snap.Time = CC->PendingInviteTime; Snap.Last = CC->LastInviteTime; C->GetInventory()->FillSaveData(Snap.Inventory);
				Result.Add(Snap);
			}
			return Result;
		};
		auto Equal = [&](const TArray<FClanSnapshot>& Before)
		{
			const TArray<FClanSnapshot> After = Capture();
			for (int32 I = 0; I < 3; ++I)
			{
				const FClanSnapshot& A = Before[I]; const FClanSnapshot& B = After[I]; TArray<FString> Differences;
				FMMOCharacterSaveJson::FindDifferences(A.Inventory, B.Inventory, Differences);
				if (A.Name != B.Name || A.Leader != B.Leader || A.Acting != B.Acting || A.Seq != B.Seq || A.Id != B.Id || A.Membership != B.Membership
					|| A.PendingMembership != B.PendingMembership || A.Pending != B.Pending || A.PendingClan != B.PendingClan || A.Time != B.Time || A.Last != B.Last || !Differences.IsEmpty()) return false;
			}
			return true;
		};
		auto Rejected = [&](uint64 Id)
		{
			const auto Before = Capture(); Recipient->CmdAcceptSeen(Id); return Equal(Before);
		};
		for (int32 Round = 0; Round < 2; ++Round)
		{
			Leaders[0]->ClanName = ClanA; Leaders[0]->ClanMembershipId = FGuid::NewGuid();
			if (!Check(Recipient->SetPendingInviteServer(GenerationActors[0]) && Recipient->PendingInviteId != 0
				&& Recipient->PendingInviteFrom.Get() == GenerationActors[0] && Recipient->PendingInviteClan == ClanA, TEXT("old generation issuance"))) return false;
			const uint64 Old = Recipient->PendingInviteId;
			Recipient->CmdAccept();
			if (!Check(Recipient->ClanName == ClanA && Recipient->ClanMembershipId.IsValid() && !Recipient->bIsLeader && Recipient->PendingInviteId == Old
				&& !Recipient->PendingInviteFrom.IsValid() && !Recipient->PendingInviterMembershipId.IsValid() && Recipient->PendingInviteClan.IsEmpty(), TEXT("current accept and generation retention"))) return false;
			Recipient->CmdLeave();
			AMMOCharacter* Sender = GenerationActors[Round];
			if (Round == 0) { Leaders[0]->ClanName = ClanB; Leaders[0]->ClanMembershipId = FGuid::NewGuid(); } // Same inviter, different clan after a consumed invite.
			if (!Check(!Recipient->IsInClan() && Recipient->PendingInviteId == Old && Recipient->SetPendingInviteServer(Sender)
				&& Recipient->PendingInviteId > Old && Recipient->PendingInviteFrom.Get() == Sender
				&& Recipient->PendingInviteClan == ClanB, TEXT("replacement clan invitation"))) return false;
			const uint64 Fresh = Recipient->PendingInviteId;
			if (!Check(Rejected(Old), TEXT("stale clan acceptance mutated replacement"))) return false;
			if (!Check(Rejected(0), TEXT("zero clan acceptance mutated replacement"))) return false;
			if (!Check(Rejected(Fresh + 1), TEXT("future clan acceptance mutated replacement"))) return false;
			Recipient->CmdAcceptSeen(Fresh);
			if (!Check(Recipient->ClanName == ClanB && Recipient->ClanMembershipId.IsValid() && !Recipient->bIsLeader && !Recipient->bActingLeader
				&& Recipient->JoinSeq > 0 && !Recipient->PendingInviteFrom.IsValid() && Recipient->PendingInviteClan.IsEmpty()
				&& Recipient->PendingInviteTime < 0.f && !Recipient->PendingInviterMembershipId.IsValid() && Recipient->PendingInviteId == Fresh, TEXT("fresh clan acceptance rejected"))) return false;
			if (!Check(Rejected(Fresh), TEXT("duplicate clan acceptance changed membership"))) return false;
			Recipient->CmdLeave();
			if (!Check(!Recipient->IsInClan() && !Recipient->ClanMembershipId.IsValid() && Recipient->JoinSeq == 0 && Recipient->PendingInviteId == Fresh, TEXT("leave reset clan generation"))) return false;
		}
		if (!Recipient->SetPendingInviteServer(GenerationActors[1])) { OutError = TEXT("dead fixture issuance"); return false; }
		GenerationActors[2]->GetStats()->bIsDead = true; const uint64 DeadId = Recipient->PendingInviteId;
		Recipient->CmdAcceptSeen(DeadId); GenerationActors[2]->GetStats()->bIsDead = false;
		if (!Check(!Recipient->IsInClan() && Recipient->PendingInviteId == DeadId && !Recipient->PendingInviteFrom.IsValid()
			&& Recipient->PendingInviteClan.IsEmpty() && Recipient->PendingInviteTime < 0.f && !Recipient->PendingInviterMembershipId.IsValid(), TEXT("matching dead acceptance changed rule"))) return false;
		Recipient->PendingInviteId = MAX_uint64 - 1;
		if (!Check(Recipient->SetPendingInviteServer(GenerationActors[1]) && Recipient->PendingInviteId == MAX_uint64, TEXT("last clan generation issuance"))) return false;
		if (!Check(Rejected(MAX_uint64 - 1), TEXT("stale last generation accepted"))) return false;
		Recipient->CmdAcceptSeen(MAX_uint64);
		if (!Check(Recipient->ClanName == ClanB && Recipient->PendingInviteId == MAX_uint64 && !Recipient->PendingInviteFrom.IsValid() && !Recipient->PendingInviterMembershipId.IsValid(), TEXT("last generation fresh acceptance"))) return false;
		const auto MaxBefore = Capture();
		if (!Check(!Recipient->SetPendingInviteServer(GenerationActors[0]) && Equal(MaxBefore), TEXT("clan generation overflow mutated state"))) return false;
		// Source membership is ephemeral: same pawn/name after leave or load is a new authority context.
		AMMOCharacter* SourceActors[2] = {};
		for (int32 I = 0; I < 2; ++I)
		{
			SourceActors[I] = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(2300000 + I * 100, 2300000, 5000), FRotator::ZeroRotator, Params);
			if (!SourceActors[I]) { OutError = TEXT("source membership fixture spawn"); return false; }
			Actors.Add(SourceActors[I]); SourceActors[I]->SetActorTickEnabled(false); SourceActors[I]->SetActorEnableCollision(false);
		}
		UMMOClanComponent* Source = SourceActors[0]->GetClanComponent();
		UMMOClanComponent* Receiver = SourceActors[1]->GetClanComponent();
		FMMOCharacterSaveData SourceData;
		SourceData.ClanName = TEXT("S") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(11);
		SourceData.bClanLeader = true;
		FMMOCharacterSaveData InventoryBefore[2];
		for (int32 I = 0; I < 2; ++I) SourceActors[I]->GetInventory()->FillSaveData(InventoryBefore[I]);
		auto IssueSource = [&]() { return Receiver->SetPendingInviteServer(SourceActors[0]); };
		auto SourceConsumed = [&]()
		{
			return !Receiver->IsInClan() && !Receiver->ClanMembershipId.IsValid() && Receiver->JoinSeq == 0
				&& !Receiver->PendingInviteFrom.IsValid() && Receiver->PendingInviteClan.IsEmpty()
				&& Receiver->PendingInviteTime < 0.f && !Receiver->PendingInviterMembershipId.IsValid();
		};
		auto RejectSource = [&]()
		{
			const uint64 Id = Receiver->PendingInviteId; Receiver->CmdAcceptSeen(Id);
			return SourceConsumed() && Receiver->PendingInviteId == Id;
		};
		auto PreserveSource = [&](uint64 SeenId)
		{
			const uint64 Id = Receiver->PendingInviteId; const FGuid Member = Receiver->ClanMembershipId;
			const FGuid PendingMember = Receiver->PendingInviterMembershipId; const auto Pending = Receiver->PendingInviteFrom;
			const FString Name = Receiver->ClanName, PendingName = Receiver->PendingInviteClan;
			const float Time = Receiver->PendingInviteTime, Last = Receiver->LastInviteTime;
			const int64 Seq = Receiver->JoinSeq; const bool Leader = Receiver->bIsLeader, Acting = Receiver->bActingLeader;
			Receiver->CmdAcceptSeen(SeenId);
			return Id == Receiver->PendingInviteId && Member == Receiver->ClanMembershipId && PendingMember == Receiver->PendingInviterMembershipId
				&& Pending == Receiver->PendingInviteFrom && Name == Receiver->ClanName && PendingName == Receiver->PendingInviteClan
				&& Time == Receiver->PendingInviteTime && Last == Receiver->LastInviteTime && Seq == Receiver->JoinSeq
				&& Leader == Receiver->bIsLeader && Acting == Receiver->bActingLeader;
		};
		Source->ApplySaveData(SourceData);
		if (!Check(Source->bIsLeader && Source->ClanMembershipId.IsValid() && IssueSource()
			&& Receiver->PendingInviterMembershipId == Source->ClanMembershipId, TEXT("source membership capture"))) return false;
		const FGuid OldSource = Source->ClanMembershipId;
		Source->CmdLeave(); const bool bLeftSource = !Source->ClanMembershipId.IsValid() && !Source->IsInClan();
		Source->ApplySaveData(SourceData);
		if (!Check(bLeftSource && Source->bIsLeader && Source->ClanMembershipId.IsValid() && Source->ClanMembershipId != OldSource
			&& Receiver->PendingInviterMembershipId == OldSource, TEXT("source same-name rejoin context"))) return false;
		if (!Check(RejectSource(), TEXT("old invitation accepted after source same-name rejoin"))) return false;
		if (!IssueSource()) { OutError = TEXT("fresh source issuance"); return false; }
		Receiver->CmdAccept();
		if (!Check(Receiver->ClanName == SourceData.ClanName && Receiver->ClanMembershipId.IsValid() && !Receiver->bIsLeader
			&& !Receiver->PendingInviterMembershipId.IsValid(), TEXT("fresh source rejoin invitation rejected"))) return false;
		if (!Check(PreserveSource(Receiver->PendingInviteId), TEXT("duplicate source acceptance changed membership"))) return false;
		Receiver->CmdLeave();
		if (!IssueSource()) { OutError = TEXT("source reload issuance"); return false; }
		const FGuid BeforeReload = Source->ClanMembershipId; Source->ApplySaveData(SourceData);
		if (!Check(Source->ClanMembershipId != BeforeReload && Source->bIsLeader && RejectSource(), TEXT("old invitation accepted after source same-name reload"))) return false;
		if (!IssueSource()) { OutError = TEXT("zero source snapshot issuance"); return false; }
		Receiver->PendingInviterMembershipId.Invalidate();
		if (!Check(RejectSource(), TEXT("zero captured source membership accepted"))) return false;
		if (!IssueSource()) { OutError = TEXT("foreign source snapshot issuance"); return false; }
		Receiver->PendingInviterMembershipId = FGuid::NewGuid();
		if (!Check(RejectSource(), TEXT("foreign captured source membership accepted"))) return false;
		if (!IssueSource()) { OutError = TEXT("invalid current source issuance"); return false; }
		Source->ClanMembershipId.Invalidate();
		if (!Check(RejectSource(), TEXT("invalid current source membership accepted"))) return false;
		const uint64 BeforeInvalidIssue = Receiver->PendingInviteId;
		if (!Check(!IssueSource() && SourceConsumed() && Receiver->PendingInviteId == BeforeInvalidIssue, TEXT("invalid source issuance mutated pending"))) return false;
		Source->ApplySaveData(SourceData);
		if (!IssueSource()) { OutError = TEXT("source stale nonce issuance"); return false; }
		const uint64 SourceNonce = Receiver->PendingInviteId; Source->ApplySaveData(SourceData);
		if (!Check(PreserveSource(SourceNonce - 1), TEXT("stale nonce consumed rebound source snapshot"))) return false;
		if (!Check(PreserveSource(0), TEXT("zero nonce consumed rebound source snapshot"))) return false;
		if (!Check(RejectSource(), TEXT("matching nonce retained rebound source snapshot"))) return false;
		if (!IssueSource()) { OutError = TEXT("source recovery issuance"); return false; }
		Receiver->CmdAcceptSeen(Receiver->PendingInviteId);
		if (!Check(Receiver->ClanName == SourceData.ClanName && Receiver->ClanMembershipId.IsValid() && !Receiver->bIsLeader
			&& !Receiver->PendingInviterMembershipId.IsValid(), TEXT("current source membership recovery rejected"))) return false;
		bool bInventoryEqual = true;
		for (int32 I = 0; I < 2; ++I)
		{
			FMMOCharacterSaveData After; SourceActors[I]->GetInventory()->FillSaveData(After);
			TArray<FString> Differences; FMMOCharacterSaveJson::FindDifferences(InventoryBefore[I], After, Differences);
			bInventoryEqual &= Differences.IsEmpty();
		}
		if (!Check(bInventoryEqual, TEXT("source membership fixture changed inventory"))) return false;
		return AMMOPlayerController::RunClanChatContextSelfTest(World, OutError, OutChecks);
	}();
	for (AMMOCharacter* Character : Actors)
	{
		if (IsValid(Character))
		{
			// EndPlay'deki lider devri ve bildirimleri yalniz fixture icinde tut.
			Character->GetClanComponent()->ClanName.Reset();
			Character->GetClanComponent()->bIsLeader = false;
			Character->Destroy();
		}
	}
	// Controllersiz/ilk kaydi yuklenmemis fixture'lar CmdAccept'te diske kayit yazamaz.
	return bPassed;
}

namespace
{
	struct FMMOClanInviteAuthorityTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestClanInviteAuthority"))) { return false; }
			FString Error;
			int32 Checks = 0;
			const bool bPassed = UMMOClanComponent::RunInviteAuthoritySelfTest(InWorld, Error, Checks);
			Ar.Logf(TEXT("[MMO_CLAN_INVITE_AUTHORITY_TEST] %s checks=%d reason=%s"),
				bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOClanInviteAuthorityTestExec GMMOClanInviteAuthorityTestExec;
}
#endif

namespace
{
	// Katilma sirasi sayaci (sunucu sureci boyunca artar; kucuk = daha eski uye).
	int64 GClanJoinCounter = 0;

	FString ClanPlayerNameOf(const AMMOCharacter* C)
	{
		if (C)
		{
			if (const APlayerState* PS = C->GetPlayerState())
			{
				return PS->GetPlayerName();
			}
		}
		return TEXT("?");
	}

	bool ClanIsDead(const AMMOCharacter* C)
	{
		const UMMOStatsComponent* S = C ? C->GetStats() : nullptr;
		return !S || S->bIsDead;
	}

	UMMOClanComponent* ClanCompOf(const AMMOCharacter* C)
	{
		return C ? C->GetClanComponent() : nullptr;
	}

	// NETWORK_AUDIT O3: kayit yuvasi -> klan adi ozeti kisa sure onbellege alinir. Eskiden her
	// /klan kur denemesi tum MMOSave_*.sav dosyalarini oyun is parcacliginda tam deserialize
	// ediyordu (Kur/Ayril dongusuyle takilma uretilebilirdi). Onbellek yalniz TTL dolunca diskten
	// yeniden kurulur; bu surecte yeni kurulan klan CmdCreate'te onbellege de eklenir, yani
	// ayni sunucuda kurulan klanlar her zaman gorunur. Tek taviz: bir klandan ayrilan uyenin
	// eski adi en fazla TTL kadar "kullanimda" gorunebilir.
	TMap<FString, FString> GSavedClanBySlot;
	double GSavedClanCacheTime = -1.0e9;
	constexpr double SavedClanCacheTtlSeconds = 30.0;

	// Disk kayitlarinda (baska bir karakter yuvasinda) bu klan adi kullaniliyor mu?
	bool SavedClanNameInUse(const UObject* WorldContext, const FString& Name, const FString& ExcludeSlot)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - GSavedClanCacheTime > SavedClanCacheTtlSeconds)
		{
			// Kayit magazasi (IMMOCharacterStore) yoksa cakisma dogrulanamaz: ad "kullanimda" sayilir.
			IMMOCharacterStore* Store = UMMOCharacterStoreSubsystem::Get(WorldContext);
			if (!Store)
			{
				return true;
			}
			GSavedClanBySlot.Reset();
			TArray<FString> Slots;
			Store->ListSlotNames(Slots);
			for (const FString& Slot : Slots)
			{
				FMMOCharacterSaveData SavedData;
				if (Store->Load(Slot, SavedData) && !SavedData.ClanName.IsEmpty())
				{
					GSavedClanBySlot.Add(Slot, SavedData.ClanName);
				}
			}
			GSavedClanCacheTime = Now;
		}
		for (const TPair<FString, FString>& Pair : GSavedClanBySlot)
		{
			if (!Pair.Key.Equals(ExcludeSlot, ESearchCase::IgnoreCase) && Pair.Value.Equals(Name, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}
}

UMMOClanComponent::UMMOClanComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMMOClanComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMMOClanComponent, ClanName);
	DOREPLIFETIME_CONDITION(UMMOClanComponent, bIsLeader, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMMOClanComponent, PendingInviteId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMMOClanComponent, ClanMembershipId, COND_OwnerOnly);
}

void UMMOClanComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Lider baglantisi koparsa / bolge degisirse liderlik cevrimici en eski uyeye gecer.
	// Kendi kaydi degismez (kayit zaten yazildi; geri donunce ApplySaveData liderlik cakismasini cozer).
	const UWorld* World = GetWorld();
	if (HasAuthorityInternal() && bIsLeader && IsInClan() && World && !World->bIsTearingDown)
	{
		EnsureLeader(ClanName, true);
	}
	Super::EndPlay(EndPlayReason);
}

bool UMMOClanComponent::HasAuthorityInternal() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->HasAuthority() : false;
}

AMMOCharacter* UMMOClanComponent::GetOwnerCharacter() const
{
	return Cast<AMMOCharacter>(GetOwner());
}

void UMMOClanComponent::OnRep_Clan()
{
	OnClanChanged.Broadcast();
}

void UMMOClanComponent::MarkChanged()
{
	OnClanChanged.Broadcast();
}

bool UMMOClanComponent::IsValidClanName(const FString& Name)
{
	if (Name.Len() < MinNameLength || Name.Len() > MaxNameLength)
	{
		return false;
	}
	for (const TCHAR Ch : Name)
	{
		if (!FChar::IsAlnum(Ch))
		{
			return false;
		}
	}
	return true;
}

void UMMOClanComponent::NotifyOwner(const FString& Text) const
{
	if (Text.IsEmpty())
	{
		return;
	}
	if (const AMMOCharacter* C = GetOwnerCharacter())
	{
		if (AMMOPlayerController* PC = Cast<AMMOPlayerController>(C->GetController()))
		{
			PC->ClientReceiveChat(FString(), Text, 2);
		}
	}
}

void UMMOClanComponent::GatherOnlineMembers(const FString& InClanName, TArray<AMMOCharacter*>& Out, bool bExcludeSelf) const
{
	Out.Reset();
	const UWorld* World = GetWorld();
	if (!World || InClanName.IsEmpty())
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		AMMOCharacter* C = PC ? Cast<AMMOCharacter>(PC->GetPawn()) : nullptr;
		UMMOClanComponent* CC = ClanCompOf(C);
		if (!CC || (bExcludeSelf && CC == this) || !CC->ClanName.Equals(InClanName, ESearchCase::IgnoreCase))
		{
			continue;
		}
		Out.Add(C);
	}
}

void UMMOClanComponent::NotifyClan(const FString& InClanName, const FString& Text, bool bExcludeSelf) const
{
	TArray<AMMOCharacter*> Members;
	GatherOnlineMembers(InClanName, Members, bExcludeSelf);
	for (const AMMOCharacter* M : Members)
	{
		if (const UMMOClanComponent* CC = ClanCompOf(M))
		{
			CC->NotifyOwner(Text);
		}
	}
}

void UMMOClanComponent::EnsureLeader(const FString& InClanName, bool bExcludeSelf)
{
	TArray<AMMOCharacter*> Members;
	GatherOnlineMembers(InClanName, Members, bExcludeSelf);

	UMMOClanComponent* Best = nullptr;
	for (AMMOCharacter* M : Members)
	{
		UMMOClanComponent* CC = ClanCompOf(M);
		if (!CC)
		{
			continue;
		}
		if (CC->bIsLeader)
		{
			return; // zaten bir lider var
		}
		if (!Best || CC->JoinSeq < Best->JoinSeq)
		{
			Best = CC;
		}
	}
	if (!Best)
	{
		return; // cevrimici uye yok; klan kayitlarda kalir
	}
	Best->bIsLeader = true;
	Best->bActingLeader = true;
	Best->MarkChanged();
	UE_LOG(LogMMOClanAudit, Display, TEXT("event=clan_leader_transfer clan=\"%s\" leader=\"%s\" acting=1"),
		*Best->ClanName, *ClanPlayerNameOf(Best->GetOwnerCharacter()));
	Best->NotifyClan(Best->ClanName, FString::Printf(TEXT("%s artik klan lideri."), *ClanPlayerNameOf(Best->GetOwnerCharacter())), false);
}

// ----------------------------------------------------------------------
// Komutlar
// ----------------------------------------------------------------------

void UMMOClanComponent::CmdCreate(const FString& Name)
{
	AMMOCharacter* Self = GetOwnerCharacter();
	if (!HasAuthorityInternal() || !Self)
	{
		return;
	}
	if (ClanIsDead(Self))
	{
		NotifyOwner(TEXT("Olu iken klan komutu kullanilamaz."));
		return;
	}
	if (IsInClan())
	{
		NotifyOwner(TEXT("Zaten bir klandasin. Once /klan ayril."));
		return;
	}

	const FString N = Name.TrimStartAndEnd();
	if (!IsValidClanName(N))
	{
		NotifyOwner(FString::Printf(TEXT("Klan adi %d-%d karakter olmali; yalniz harf ve rakam. Kullanim: /klan kur ad"), MinNameLength, MaxNameLength));
		return;
	}

	// Sunucuda su an cevrimici baska bir klan bu adi kullaniyorsa reddet (cevrimdisi cakisma kontrol edilmez).
	TArray<AMMOCharacter*> Existing;
	GatherOnlineMembers(N, Existing, true);
	if (Existing.Num() > 0)
	{
		NotifyOwner(TEXT("Bu klan adi kullanimda."));
		return;
	}

	// Kayitlarda (cevrimdisi uyeler) bu adi tasiyan baska bir karakter varsa da reddet:
	// aksi halde ad kapan, geri donen eski uyeleri/lideri devralirdi (QA ORTA).
	if (SavedClanNameInUse(this, N, Self->GetSaveSlotName()))
	{
		NotifyOwner(TEXT("Bu klan adi kullanimda (cevrimdisi uyeleri var)."));
		return;
	}

	ClanName = N;
	ClanMembershipId = FGuid::NewGuid();
	bIsLeader = true;
	bActingLeader = false;
	JoinSeq = ++GClanJoinCounter;
	GSavedClanBySlot.Add(Self->GetSaveSlotName(), N); // onbellek: yeni klan hemen gorunsun
	MarkChanged();
	UE_LOG(LogMMOClanAudit, Display, TEXT("event=clan_create clan=\"%s\" actor=\"%s\" slot=\"%s\""),
		*ClanName, *ClanPlayerNameOf(Self), *Self->GetSaveSlotName());
	NotifyOwner(FString::Printf(TEXT("Klan kuruldu: [%s]. Davet: /klan davet"), *ClanName));
	Self->SaveCharacter();
}

void UMMOClanComponent::CmdInvite()
{
	AMMOCharacter* Self = GetOwnerCharacter();
	const UWorld* World = GetWorld();
	if (!HasAuthorityInternal() || !Self || !World)
	{
		return;
	}
	if (ClanIsDead(Self))
	{
		NotifyOwner(TEXT("Olu iken klan komutu kullanilamaz."));
		return;
	}
	if (!IsInClan())
	{
		NotifyOwner(TEXT("Bir klanda degilsin."));
		return;
	}
	if (!bIsLeader)
	{
		NotifyOwner(TEXT("Yalniz klan lideri davet edebilir."));
		return;
	}

	// Davet spam korumasi (hedefin ekranini doldurmasin).
	const float Now = World->GetTimeSeconds();
	if (Now - LastInviteTime < InviteCooldown)
	{
		NotifyOwner(TEXT("Cok hizli davet gonderiyorsun; biraz bekle."));
		return;
	}
	LastInviteTime = Now;

	AMMOCharacter* Target = nullptr;
	float BestDist = InviteRange;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		AMMOCharacter* C = PC ? Cast<AMMOCharacter>(PC->GetPawn()) : nullptr;
		if (!C || C == Self || ClanIsDead(C))
		{
			continue;
		}
		// Zaten klandaki yakin oyuncular adayligi engellemesin (QA): yalniz klansizlar aday.
		const UMMOClanComponent* CandClan = ClanCompOf(C);
		if (!CandClan || CandClan->IsInClan())
		{
			continue;
		}
		const float D = FVector::Dist(Self->GetActorLocation(), C->GetActorLocation());
		if (D < BestDist)
		{
			BestDist = D;
			Target = C;
		}
	}
	UMMOClanComponent* TC = ClanCompOf(Target);
	if (!TC)
	{
		NotifyOwner(TEXT("Yakinda klansiz davet edilecek oyuncu yok."));
		return;
	}

	// Ayni davet eden ayni hedefe yenilerse sureyi uzatma (hedefi baska liderlere kilitlemesin).
	if (TC->PendingInviteFrom.Get() == Self && TC->PendingInviteTime >= 0.f
		&& Now - TC->PendingInviteTime < InviteExpirySeconds)
	{
		NotifyOwner(TEXT("Bu oyuncuya davet zaten gonderildi; yanitini bekle."));
		return;
	}

	// Davet kapma korumasi: hedefin gecerli baska bir bekleyen daveti varsa ezme.
	if (TC->PendingInviteFrom.IsValid() && TC->PendingInviteFrom.Get() != Self
		&& TC->PendingInviteTime >= 0.f && Now - TC->PendingInviteTime < InviteExpirySeconds)
	{
		NotifyOwner(TEXT("Oyuncunun bekleyen baska bir daveti var; biraz sonra tekrar dene."));
		return;
	}
	if (!TC->SetPendingInviteServer(Self)) return;
	TC->NotifyOwner(FString::Printf(TEXT("%s seni [%s] klanina davet etti. Kabul: /klan kabul"), *ClanPlayerNameOf(Self), *ClanName));
	NotifyOwner(FString::Printf(TEXT("%s adli oyuncuya klan daveti gonderildi."), *ClanPlayerNameOf(Target)));
}

bool UMMOClanComponent::SetPendingInviteServer(AMMOCharacter* Inviter)
{
	const AMMOCharacter* Self = GetOwnerCharacter();
	const UWorld* World = GetWorld();
	const UMMOClanComponent* IC = IsValid(Inviter) ? Inviter->GetClanComponent() : nullptr;
	if (!HasAuthorityInternal() || !Self || !World || !IC || Inviter == Self || IC->ClanName.IsEmpty()
		|| !IC->ClanMembershipId.IsValid() || !Inviter->HasAuthority() || Inviter->GetWorld() != World || PendingInviteId == MAX_uint64) return false;
	++PendingInviteId; // Never reuse a consumed invitation within this pawn's lifetime.
	PendingInviterMembershipId = IC->ClanMembershipId;
	PendingInviteFrom = Inviter; PendingInviteClan = IC->ClanName; PendingInviteTime = World->GetTimeSeconds();
	return true;
}

void UMMOClanComponent::CmdAccept()
{
	CmdAcceptSeen(PendingInviteId);
}

void UMMOClanComponent::CmdAcceptSeen(uint64 SeenInviteId)
{
	if (!HasAuthorityInternal() || SeenInviteId == 0 || SeenInviteId != PendingInviteId) return;
	AMMOCharacter* Self = GetOwnerCharacter();
	const UWorld* World = GetWorld();
	if (!HasAuthorityInternal() || !Self || !World)
	{
		return;
	}

	// Yalniz eslesen davet tuketilir; stale/zero/future kimlik pending durumu korur.
	AMMOCharacter* Inviter = PendingInviteFrom.Get();
	const FString InviteClan = PendingInviteClan;
	const FGuid InviteMembershipId = PendingInviterMembershipId;
	const float InviteTime = PendingInviteTime;
	PendingInviteFrom.Reset();
	PendingInviteClan.Empty();
	PendingInviteTime = -1.f;
	PendingInviterMembershipId.Invalidate();

	if (ClanIsDead(Self))
	{
		NotifyOwner(TEXT("Olu iken klan komutu kullanilamaz."));
		return;
	}
	if (!Inviter || Inviter == Self || InviteClan.IsEmpty())
	{
		NotifyOwner(TEXT("Bekleyen klan daveti yok."));
		return;
	}
	if (InviteTime < 0.f || World->GetTimeSeconds() - InviteTime > InviteExpirySeconds)
	{
		NotifyOwner(TEXT("Klan davetinin suresi doldu."));
		return;
	}
	if (IsInClan())
	{
		NotifyOwner(TEXT("Zaten bir klandasin."));
		return;
	}
	// Davet eden hala o klanda ve lider olmali: vekil lider, asil lider donunce yetkisini
	// kaybedebilir. Bekleyen davet eski liderin uye alma yetkisini devam ettirmemeli.
	// Ayni pawn/klan adi ile yeniden giris veya load eski kaynak uyeligini canlandirmaz.
	const UMMOClanComponent* IC = ClanCompOf(Inviter);
	if (!IC || !IC->bIsLeader || !IC->ClanName.Equals(InviteClan, ESearchCase::IgnoreCase)
		|| !InviteMembershipId.IsValid() || InviteMembershipId != IC->ClanMembershipId)
	{
		NotifyOwner(TEXT("Davet gecersiz: davet edenin klan uyeligi veya liderligi degisti."));
		return;
	}

	ClanName = IC->ClanName;
	ClanMembershipId = FGuid::NewGuid();
	bIsLeader = false;
	bActingLeader = false;
	JoinSeq = ++GClanJoinCounter;
	MarkChanged();
	UE_LOG(LogMMOClanAudit, Display, TEXT("event=clan_join clan=\"%s\" actor=\"%s\" inviter=\"%s\""),
		*ClanName, *ClanPlayerNameOf(Self), *ClanPlayerNameOf(Inviter));
	NotifyClan(ClanName, FString::Printf(TEXT("%s klana katildi."), *ClanPlayerNameOf(Self)), false);
	Self->SaveCharacter();
}

void UMMOClanComponent::CmdLeave()
{
	AMMOCharacter* Self = GetOwnerCharacter();
	if (!HasAuthorityInternal() || !Self)
	{
		return;
	}
	if (ClanIsDead(Self))
	{
		NotifyOwner(TEXT("Olu iken klan komutu kullanilamaz."));
		return;
	}
	if (!IsInClan())
	{
		NotifyOwner(TEXT("Bir klanda degilsin."));
		return;
	}

	const FString OldName = ClanName;
	const bool bWasLeader = bIsLeader;
	ClanName.Empty();
	ClanMembershipId.Invalidate();
	bIsLeader = false;
	bActingLeader = false;
	JoinSeq = 0;
	MarkChanged();
	UE_LOG(LogMMOClanAudit, Display, TEXT("event=clan_leave clan=\"%s\" actor=\"%s\" was_leader=%d"),
		*OldName, *ClanPlayerNameOf(Self), bWasLeader ? 1 : 0);
	NotifyOwner(FString::Printf(TEXT("[%s] klanindan ayrildin."), *OldName));
	NotifyClan(OldName, FString::Printf(TEXT("%s klandan ayrildi."), *ClanPlayerNameOf(Self)), false);
	if (bWasLeader)
	{
		EnsureLeader(OldName, true);
	}
	Self->SaveCharacter();
}

void UMMOClanComponent::CmdKick(const FString& TargetName)
{
	AMMOCharacter* Self = GetOwnerCharacter();
	if (!HasAuthorityInternal() || !Self)
	{
		return;
	}
	if (ClanIsDead(Self))
	{
		NotifyOwner(TEXT("Olu iken klan komutu kullanilamaz."));
		return;
	}
	if (!IsInClan())
	{
		NotifyOwner(TEXT("Bir klanda degilsin."));
		return;
	}
	if (!bIsLeader)
	{
		NotifyOwner(TEXT("Yalniz klan lideri uye atabilir."));
		return;
	}

	const FString Wanted = TargetName.TrimStartAndEnd();
	if (Wanted.IsEmpty())
	{
		NotifyOwner(TEXT("Kullanim: /klan at <isim>"));
		return;
	}

	TArray<AMMOCharacter*> Members;
	GatherOnlineMembers(ClanName, Members, true);
	AMMOCharacter* Victim = nullptr;
	for (AMMOCharacter* M : Members)
	{
		if (ClanPlayerNameOf(M).Equals(Wanted, ESearchCase::IgnoreCase))
		{
			Victim = M;
			break;
		}
	}
	UMMOClanComponent* VC = ClanCompOf(Victim);
	if (!VC)
	{
		NotifyOwner(FString::Printf(TEXT("%s adli cevrimici klan uyesi bulunamadi."), *Wanted));
		return;
	}

	const FString OldName = ClanName;
	const FString VictimName = ClanPlayerNameOf(Victim);
	VC->ClanName.Empty();
	VC->ClanMembershipId.Invalidate();
	VC->bIsLeader = false;
	VC->bActingLeader = false;
	VC->JoinSeq = 0;
	VC->MarkChanged();
	UE_LOG(LogMMOClanAudit, Display, TEXT("event=clan_kick clan=\"%s\" actor=\"%s\" target=\"%s\""),
		*OldName, *ClanPlayerNameOf(Self), *VictimName);
	VC->NotifyOwner(FString::Printf(TEXT("[%s] klanindan atildin."), *OldName));
	NotifyClan(OldName, FString::Printf(TEXT("%s klandan atildi."), *VictimName), false);
	Victim->SaveCharacter();
}

void UMMOClanComponent::CmdList()
{
	if (!HasAuthorityInternal())
	{
		return;
	}
	if (!IsInClan())
	{
		NotifyOwner(TEXT("Bir klanda degilsin."));
		return;
	}

	TArray<AMMOCharacter*> Members;
	GatherOnlineMembers(ClanName, Members, false);
	// TArray<T*>::Sort, ifadeye eleman referanslari verir (isaretci degil).
	Members.Sort([](const AMMOCharacter& A, const AMMOCharacter& B)
	{
		const UMMOClanComponent* CA = ClanCompOf(&A);
		const UMMOClanComponent* CB = ClanCompOf(&B);
		return (CA ? CA->JoinSeq : 0) < (CB ? CB->JoinSeq : 0);
	});

	FString List;
	for (int32 i = 0; i < Members.Num(); ++i)
	{
		const AMMOCharacter* M = Members[i];
		const UMMOClanComponent* CC = ClanCompOf(M);
		const int32 Lv = (M && M->GetStats()) ? M->GetStats()->Level : 0;
		List += FString::Printf(TEXT("%s%s (Lv%d)%s"), i > 0 ? TEXT(", ") : TEXT(""), *ClanPlayerNameOf(M), Lv,
			(CC && CC->bIsLeader) ? TEXT(" [lider]") : TEXT(""));
	}
	NotifyOwner(FString::Printf(TEXT("[%s] cevrimici uyeler (%d): %s"), *ClanName, Members.Num(), *List));
}

void UMMOClanComponent::CmdChat(const FString& Text)
{
	AMMOCharacter* Self = GetOwnerCharacter();
	if (!HasAuthorityInternal() || !Self)
	{
		return;
	}
	if (!IsInClan())
	{
		NotifyOwner(TEXT("Bir klanda degilsin; mesaj gonderilmedi."));
		return;
	}

	const FString Sender = ClanPlayerNameOf(Self);
	TArray<AMMOCharacter*> Members;
	GatherOnlineMembers(ClanName, Members, false);
	for (const AMMOCharacter* M : Members)
	{
		if (AMMOPlayerController* PC = M ? Cast<AMMOPlayerController>(M->GetController()) : nullptr)
		{
			PC->ClientReceiveChat(Sender, Text, 5);
		}
	}
}

// ----------------------------------------------------------------------
// Kalicilik
// ----------------------------------------------------------------------

void UMMOClanComponent::FillSaveData(FMMOCharacterSaveData& Out) const
{
	Out.ClanName = ClanName;
	Out.bClanLeader = IsInClan() && bIsLeader && !bActingLeader;
}

void UMMOClanComponent::ApplySaveData(const FMMOCharacterSaveData& In)
{
	if (!HasAuthorityInternal())
	{
		return;
	}

	// Eski kayitlarda bos gelir; bozuk/gecersiz ad klansiz sayilir.
	const FString N = In.ClanName.TrimStartAndEnd();
	if (!IsValidClanName(N))
	{
		ClanName.Empty();
		ClanMembershipId.Invalidate();
		bIsLeader = false;
		JoinSeq = 0;
		MarkChanged();
		return;
	}

	ClanName = N;
	ClanMembershipId = FGuid::NewGuid();
	JoinSeq = ++GClanJoinCounter;

	// Liderlik tekil olmali. Kayitli lider geri geldiyse liderligi geri alir (vekil lider uyeye doner;
	// bolge gecisinde yukleme sirasi onemsiz kalir). Lider degilse yalniz cevrimici lider yoksa
	// (lider ayrilmis / kimse cevrimici degil) lider olur.
	TArray<AMMOCharacter*> Others;
	GatherOnlineMembers(N, Others, true);
	bool bOtherRealLeader = false;
	bool bOtherAnyLeader = false;
	UMMOClanComponent* OtherActing = nullptr;
	for (const AMMOCharacter* M : Others)
	{
		UMMOClanComponent* CC = ClanCompOf(M);
		if (CC && CC->bIsLeader)
		{
			bOtherAnyLeader = true;
			if (CC->bActingLeader)
			{
				OtherActing = CC;
			}
			else
			{
				bOtherRealLeader = true;
			}
		}
	}

	if (In.bClanLeader && !bOtherRealLeader)
	{
		// Asil lider geri dondu: cevrimici vekil lider (varsa) liderligi birakir ve haberdar edilir.
		if (OtherActing)
		{
			OtherActing->bIsLeader = false;
			OtherActing->bActingLeader = false;
			OtherActing->MarkChanged();
			OtherActing->NotifyOwner(TEXT("Asil klan lideri geri dondu; liderlik ona gecti."));
		}
		bIsLeader = true;
		bActingLeader = false;
	}
	else if (In.bClanLeader)
	{
		// Baska bir asil lider zaten cevrimici (ad cakismasi / bozuk kayit): uye olarak katil,
		// onun liderligini ezme.
		bIsLeader = false;
		bActingLeader = false;
	}
	else
	{
		// Lider degil: yalniz cevrimici hic lider yoksa vekil olarak terfi eder (kayda yazilmaz).
		bIsLeader = !bOtherAnyLeader;
		bActingLeader = bIsLeader;
	}
	MarkChanged();
}

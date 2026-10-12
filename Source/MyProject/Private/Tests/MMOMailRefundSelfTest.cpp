// O69: trusted synchronous callback accounting; no new production API dependency.
#include "Components/MMOMailComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOInventoryComponent.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/Guid.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Save/MMOSaveGame.h"
#include "Tests/MMOGoldMetricProbe.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	enum class ERefundAction { None, Spend, Gift, FillBox, Reenter, DestroyInventory, DestroyOwner, DestroyMail, TravelLock, NameChange, ReloadGenerationSeam };
	struct FRefundCase
	{
		const TCHAR* Name;
		int32 Start, Attachment, Phase, CallbackAmount;
		ERefundAction Action;
		bool SaveOk, SendOk;
		int32 ExpectedGold, ExpectedSaves, ExpectedMails;
	};
	// Wallet oracles are independent: start + actual callback delta - actual mail cost
	// on success; start + actual callback delta on rollback. Partial fee: 500 + 5.
	const FRefundCase RefundCases[] = {
		{TEXT("normal"),1000,500,0,0,ERefundAction::None,true,true,480,1,1},
		{TEXT("savefail"),1000,500,0,0,ERefundAction::None,false,false,1000,1,0},
		{TEXT("clamp_savefail"),1000,500,1,495,ERefundAction::Spend,false,false,505,0,0},
		{TEXT("clamp_saveok"),1000,500,1,495,ERefundAction::Spend,true,false,505,0,0},
		{TEXT("spend_savefail"),10000,300,1,1000,ERefundAction::Spend,false,false,9000,1,0},
		{TEXT("spend_saveok"),10000,300,1,1000,ERefundAction::Spend,true,true,8684,1,1},
		{TEXT("gift_saveok"),10000,300,1,1000,ERefundAction::Gift,true,true,10684,1,1},
		{TEXT("gift_savefail"),10000,300,1,1000,ERefundAction::Gift,false,false,11000,1,0},
		{TEXT("fee_spend_savefail"),1000,500,2,7,ERefundAction::Spend,false,false,993,1,0},
		{TEXT("fee_gift_savefail"),1000,500,2,250,ERefundAction::Gift,false,false,1250,1,0},
		{TEXT("fee_gift_saveok"),1000,500,2,250,ERefundAction::Gift,true,true,730,1,1},
		{TEXT("appendfail"),1000,500,1,0,ERefundAction::FillBox,true,false,1000,2,20},
		{TEXT("refund_spend"),1000,500,3,7,ERefundAction::Spend,false,false,993,1,0},
		{TEXT("reentrant_charge"),1000,500,1,0,ERefundAction::Reenter,true,true,480,1,1},
		{TEXT("reentrant_refund"),1000,500,3,0,ERefundAction::Reenter,false,false,1000,1,0},
		{TEXT("inventory_destroy"),1000,500,1,0,ERefundAction::DestroyInventory,true,false,-1,0,0},
		{TEXT("owner_destroy"),1000,500,1,0,ERefundAction::DestroyOwner,true,false,-1,0,0},
		{TEXT("mail_destroy"),1000,500,1,0,ERefundAction::DestroyMail,true,false,-1,0,0},
		{TEXT("travel_lock"),1000,500,1,0,ERefundAction::TravelLock,true,false,-1,0,0},
		{TEXT("name_change"),1000,500,1,0,ERefundAction::NameChange,true,false,-1,0,0},
		{TEXT("reload_seam_attachment"),1000,500,1,1000,ERefundAction::ReloadGenerationSeam,true,false,1000,0,0},
		{TEXT("reload_seam_fee"),1000,500,2,1000,ERefundAction::ReloadGenerationSeam,true,false,1000,0,0},
	};

	bool RefundNamespaceFree(const FString& Name)
	{
		const FString Path = FMMOMailStore::GetBoxPath(Name);
		for (const TCHAR* Suffix : {TEXT(""),TEXT(".tmp"),TEXT(".bad"),TEXT(".tmp.bad"),TEXT(".bak")})
		{
			if (IFileManager::Get().FileExists(*(Path + Suffix)) || IFileManager::Get().DirectoryExists(*(Path + Suffix))) return false;
		}
		return true;
	}

	bool RunMailRefundTest(UWorld* World, FString& Error, int32& Checks)
	{
		Checks = 0; Error.Reset();
		if (!IsInGameThread() || GIsEditor || !World || !World->IsGameWorld() || World->GetNetMode() == NM_Client
			|| !FParse::Param(FCommandLine::Get(), TEXT("MMOMailRefundTest"))
			|| !FParse::Param(FCommandLine::Get(), TEXT("unattended")) || !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
		{ Error = TEXT("opt-in disposable headless authority game thread required"); return false; }
		static bool Invoked = false;
		if (Invoked) { Error = TEXT("one invocation per process required"); return false; }
		Invoked = true;
		bool Passed = true;
		for (const FRefundCase& Spec : RefundCases)
		{
			const auto Check = [&](bool OK, const TCHAR* Suffix)
			{
				const FString Label = FString(Spec.Name) + TEXT(".") + Suffix;
				++Checks;
				UE_LOG(LogTemp, Display, TEXT("[MMO_MAIL_REFUND_CASE] %s check=%s"), OK ? TEXT("PASS") : TEXT("FAIL"), *Label);
				if (!OK) { Passed = false; if (Error.IsEmpty()) Error = Label; }
			};
			// Full GUID fits the store's 32-character name limit, without truncation.
			const FString Sender = FGuid::NewGuid().ToString(EGuidFormats::Digits);
			const FString Recipient = FGuid::NewGuid().ToString(EGuidFormats::Digits);
			const bool Fresh = RefundNamespaceFree(Sender) && RefundNamespaceFree(Recipient);
			Check(Fresh, TEXT("namespace"));
			if (!Fresh) return false; // Never touch an occupied namespace.
			FActorSpawnParameters Params;
			Params.Name = FName(*(TEXT("O69Refund_") + Sender));
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			TStrongObjectPtr<AMMOCharacter> Actor(World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(),
				FVector(1900000,1900000,5000), FRotator::ZeroRotator, Params));
			UMMOMailComponent* Mail = Actor.IsValid() ? Actor->GetMailComponent() : nullptr;
			UMMOInventoryComponent* Inv = Actor.IsValid() ? Actor->GetInventory() : nullptr;
			Check(Mail && Inv && Actor->HasAuthority() && !Actor->IsPlayerControlled(), TEXT("fixture"));
			if (!Mail || !Inv) { if (Actor.IsValid()) Actor->Destroy(); return false; }
			// No player controller, no real character save; InitForTest counts mocked saves.
			Actor->SetSaveIdentityForTest(false, TEXT("MMOSave_") + Sender, 0);
			Mail->InitForTest(Sender); Mail->SetSaveResultForTest(Spec.SaveOk);
			Inv->Gold = Spec.Start;
			TStrongObjectPtr<UMMOInventoryComponent> KeepInventory(Inv);
			TStrongObjectPtr<UMMOMailComponent> KeepMail(Mail);
			TStrongObjectPtr<UMMOGoldMetricProbe> Probe(NewObject<UMMOGoldMetricProbe>());
			bool Fired = false, ActionOK = true;
			int32 CallbackDelta = 0;
			EMMOMailResult NestedResult = EMMOMailResult::Ok;
			Probe->Callback = [&](int32 Call)
			{
				if (Fired || Call != Spec.Phase || Spec.Action == ERefundAction::None) return;
				Fired = true;
				const int32 Before = Inv->Gold;
				switch (Spec.Action)
				{
				case ERefundAction::Spend: Inv->AddGoldFrom(-Spec.CallbackAmount, TEXT("o69-spend")); break;
				case ERefundAction::Gift: Inv->AddGoldFrom(Spec.CallbackAmount, TEXT("o69-gift")); break;
				case ERefundAction::FillBox:
					for (int32 Index = 0; Index < FMMOMailStore::MaxMailsPerBox; ++Index)
					{ FMMOMail Seed; Seed.From = Sender; Seed.Text = TEXT("callback-fill"); Seed.Gold = 0; ActionOK &= FMMOMailStore::Append(Recipient, Seed); }
					break;
				case ERefundAction::Reenter: NestedResult = Mail->SendMail(Recipient, 1, TEXT("nested"), true); ActionOK = NestedResult == EMMOMailResult::Cooldown; break;
				case ERefundAction::DestroyInventory: Inv->DestroyComponent(); ActionOK = Inv->IsBeingDestroyed(); break;
				case ERefundAction::DestroyOwner: ActionOK = Actor->Destroy(); break;
				case ERefundAction::DestroyMail: Mail->DestroyComponent(); ActionOK = Mail->IsBeingDestroyed(); break;
				case ERefundAction::TravelLock: Mail->SetTravelLockedForTest(true); break;
				case ERefundAction::NameChange: Mail->SetNameChangedForTest(true); break;
				case ERefundAction::ReloadGenerationSeam:
				{
					// Existing test seam + real inventory data application, NOT LoadCharacter
					// or disk/identity/full-reload proof. Same objects/name/slot, new generation.
					const uint32 Serial = Actor->GetLoadSerial();
					const FString Slot = Actor->GetActiveSaveSlot();
					const bool LoadedBefore = Actor->IsInitialLoadComplete();
					Actor->SetSaveIdentityForTest(LoadedBefore, Slot, Serial + 1);
					FMMOCharacterSaveData Restored;
					Restored.Gold = Spec.CallbackAmount;
					Inv->ApplySaveData(Restored);
					ActionOK = Actor->GetLoadSerial() == Serial + 1 && Actor->GetActiveSaveSlot() == Slot
						&& Actor->IsInitialLoadComplete() == LoadedBefore && Actor->GetInventory() == Inv
						&& Actor->GetMailComponent() == Mail && Inv->Gold == Spec.CallbackAmount;
					break;
				}
				default: break;
				}
				// Strong test references prevent GC; no forced-GC/UAF experiment.
				CallbackDelta = Inv->Gold - Before;
			};
			Inv->OnGoldChanged.AddDynamic(Probe.Get(), &UMMOGoldMetricProbe::OnGold);
			const EMMOMailResult Result = Mail->SendMail(Recipient, Spec.Attachment, TEXT("o69-envelope"), true);
			Inv->OnGoldChanged.RemoveDynamic(Probe.Get(), &UMMOGoldMetricProbe::OnGold);
			Probe->Callback = nullptr;
			TArray<FMMOMail> Box;
			const bool Loaded = FMMOMailStore::Load(Recipient, Box);
			bool Shape = Loaded && Box.Num() == Spec.ExpectedMails;
			for (const FMMOMail& Entry : Box)
			{
				Shape &= Entry.From == Sender && (Spec.Action == ERefundAction::FillBox
					? Entry.Text == TEXT("callback-fill") && Entry.Gold == 0
					: Entry.Text == TEXT("o69-envelope") && Entry.Gold == Spec.Attachment);
			}
			// Only the name-change case permits the existing dedicated NameChanged result;
			// every other rejection must be StoreError, never a validation/cooldown fallback.
			const bool ResultOK = Spec.SendOk ? Result == EMMOMailResult::Ok
				: Result == EMMOMailResult::StoreError || (Spec.Action == ERefundAction::NameChange && Result == EMMOMailResult::NameChanged);
			Check(ResultOK, TEXT("result"));
			// Lifecycle/eligibility cases require rejection, not a promise of full refund.
			Check(Spec.ExpectedGold < 0 ? Fired && Result != EMMOMailResult::Ok : Inv->Gold == Spec.ExpectedGold, TEXT("wallet"));
			Check(Shape, TEXT("mailbox"));
			Check(Mail->GetSaveCallsForTest() == Spec.ExpectedSaves, TEXT("saves"));
			Check(ActionOK && (Spec.Action == ERefundAction::None ? !Fired : Fired), TEXT("callback"));
			UE_LOG(LogTemp, Display, TEXT("[MMO_MAIL_REFUND_OBSERVATION] scenario=%s start=%d attachment=%d fee=%d callback_delta=%d final=%d expected=%d result=%d saves=%d mails=%d events=%d sender=%s recipient=%s"),
				Spec.Name, Spec.Start, Spec.Attachment, UMMOMailComponent::FeeFor(Spec.Attachment), CallbackDelta, Inv->Gold, Spec.ExpectedGold,
				static_cast<int32>(Result), Mail->GetSaveCallsForTest(), Box.Num(), Probe->Values.Num(), *Sender, *Recipient);
			FMMOMailStore::DeleteBox(Sender); FMMOMailStore::DeleteBox(Recipient);
			if (!Actor->IsActorBeingDestroyed()) Actor->Destroy();
			Check(RefundNamespaceFree(Sender) && RefundNamespaceFree(Recipient) && Actor->IsActorBeingDestroyed(), TEXT("cleanup"));
		}
		return Passed;
	}
	class FMailRefundExec final : public FSelfRegisteringExec
	{
		bool Exec(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestMailRefund"))) return false;
			FString Error; int32 Checks = 0;
			const bool Passed = RunMailRefundTest(World, Error, Checks);
			UE_LOG(LogTemp, Display, TEXT("[MMO_MAIL_REFUND_TEST] %s checks=%d reason=%s"), Passed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMailRefundExec GMailRefundExec;
}
#endif

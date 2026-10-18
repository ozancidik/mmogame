#include "Components/MMOInventoryComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Components/MMOStatsComponent.h"
#include "Core/MMOMetrics.h"
#include "Engine/World.h"
#include "HAL/CriticalSection.h"
#include "Items/MMOItemDatabase.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"
#include "Tests/MMOUpgradeCallbackProbe.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	// Yalniz bu dosyanin yakalayicisi: GLog'a eklenir, "ev=upgrade" olcum satirlarini toplar.
	class FMMOUpgradeMetricCapture : public FOutputDevice
	{
	public:
		FCriticalSection Lock;
		TArray<FString> Lines;
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type, const FName&) override
		{
			const FString Text(V);
			if (Text.Contains(TEXT("[MMOMETRIC]")) && Text.Contains(TEXT("ev=upgrade")))
			{
				FScopeLock Guard(&Lock);
				Lines.Add(Text);
			}
		}
		TArray<FString> Take()
		{
			GLog->FlushThreadedLogs(); // satirlar gunluk is parcaciginda gecikmeli gelebilir; kilit DISINDA bosalt
			FScopeLock Guard(&Lock);
			TArray<FString> Out = Lines;
			Lines.Reset();
			return Out;
		}
	};

	FString UpgradeMetricField(const FString& Line, const TCHAR* Key)
	{
		TArray<FString> Tokens;
		Line.ParseIntoArrayWS(Tokens);
		const FString Prefix = FString(Key) + TEXT("=");
		for (const FString& Token : Tokens)
		{
			if (Token.StartsWith(Prefix)) return Token.Mid(Prefix.Len());
		}
		return FString(TEXT("<missing>"));
	}
}

bool UMMOInventoryComponent::RunUpgradeMetricSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0;
	OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client
		|| !FParse::Param(FCommandLine::Get(), TEXT("unattended"))
		|| !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
	{
		OutError = TEXT("disposable authority headless game world required (fixture seeds process RNG)");
		return false;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AMMOCharacter* Pawn = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), FVector(1000000, 1000000, 5000), FRotator::ZeroRotator, Params);
	int32 Failures = 0;
	const auto Check = [&](const FString& Case, const TCHAR* Label, bool bOK)
	{
		++OutChecks;
		UE_LOG(LogTemp, Display, TEXT("[MMO_UPGRADE_METRIC_CASE] %s case=%s check=%s"), bOK ? TEXT("PASS") : TEXT("FAIL"), *Case, Label);
		if (!bOK) { ++Failures; if (OutError.IsEmpty()) OutError = Case + TEXT("/") + Label; }
	};
	FMMOUpgradeMetricCapture Capture;
	GLog->AddOutputDevice(&Capture);
	const bool bCompleted = [&]()
	{
		if (!Pawn || !Pawn->GetInventory() || !Pawn->GetStats()) { OutError = TEXT("fixture spawn failed"); return false; }
		Pawn->SetActorEnableCollision(false);
		UMMOInventoryComponent* Inv = Pawn->GetInventory();
		const FName ItemId(TEXT("ShortSword"));
		const FMMOItemDef* Def = FMMOItemDatabase::Find(ItemId);
		if (!Def) { OutError = TEXT("ShortSword missing"); return false; }
		const FString Who = MMOMetrics::WhoName(Pawn);
		const auto Prepare = [&](int32 Plus, int32 PlayerLevel)
		{
			Pawn->GetStats()->bIsDead = false;
			Pawn->GetStats()->Level = PlayerLevel;
			Inv->EquipmentBag.Reset(); Inv->EquippedItems.Reset(); Inv->EquippedItems.SetNum(static_cast<int32>(EMMOEquipSlot::MAX));
			Inv->Items.Reset(); Inv->Warehouse.Reset(); Inv->NextInstanceId = 1;
			Inv->Gold = 10000; Inv->LastUpgradeServerTime = -1000;
			Inv->AddEquipmentInstance(ItemId, Plus);
			Inv->AddEquipmentInstance(ItemId, 0);
		};
		// Gercek platform RNG'si ile belirleyici tohumlar (uretimde RNG sokagi yok); callback testiyle ayni yontem.
		int32 Seeds[3] = { INDEX_NONE, INDEX_NONE, INDEX_NONE };
		for (int32 Index = 0; Index < 4096; ++Index)
		{
			const int32 Seed = Index * 104729 + 17;
			FMath::RandInit(Seed);
			const int32 Roll = FMath::RandRange(1, 100);
			if (Roll <= 90 && Seeds[0] == INDEX_NONE) Seeds[0] = Seed;
			if (Roll > 90) { if (Seeds[1] == INDEX_NONE) Seeds[1] = Seed; if (Seeds[2] == INDEX_NONE) Seeds[2] = Seed; }
		}
		if (Seeds[0] < 0 || Seeds[1] < 0 || Seeds[2] < 0) { OutError = TEXT("RNG seeds missing"); return false; }
		const TCHAR* Outcomes[] = { TEXT("success"), TEXT("failure"), TEXT("burn") };
		const TCHAR* ExpectedResult[] = { TEXT("success"), TEXT("fail"), TEXT("burn") };
		const int32 PlayerLevels[] = { 37, 52, 8 };
		for (int32 Outcome = 0; Outcome < 3; ++Outcome)
		{
			const int32 Plus = Outcome == 2 ? 7 : 0;
			const int32 ToPlus = Outcome == 0 ? 1 : Outcome == 1 ? 0 : -1;
			const int32 Cost = FMMOItemDatabase::GetUpgradeCost(Plus, Def->RequiredLevel);
			for (int32 Mode = 0; Mode < 2; ++Mode) // 0 = duz, 1 = altin delegesi yukseltmeyi yeniden dener
			{
				Prepare(Plus, PlayerLevels[Outcome]);
				const FString Case = FString::Printf(TEXT("%s_%s"), Outcomes[Outcome], Mode == 0 ? TEXT("plain") : TEXT("nested"));
				TStrongObjectPtr<UMMOUpgradeCallbackProbe> Probe(NewObject<UMMOUpgradeCallbackProbe>());
				Probe->Callback = [&]() { if (Mode == 1) { Inv->UpgradeByInstanceId(1, -1); Inv->UpgradeByInstanceId(2, 0); } };
				Inv->OnGoldChanged.AddDynamic(Probe.Get(), &UMMOUpgradeCallbackProbe::OnGold);
				Capture.Take();
				FMath::RandInit(Seeds[Outcome]);
				Inv->UpgradeByInstanceId(1, Plus);
				Inv->OnGoldChanged.RemoveDynamic(Probe.Get(), &UMMOUpgradeCallbackProbe::OnGold);
				Probe->Callback = nullptr;
				const TArray<FString> Lines = Capture.Take();
				Check(Case, TEXT("exactly_one_line"), Lines.Num() == 1);
				const FString Line = Lines.Num() ? Lines[0] : FString();
				Check(Case, TEXT("who"), UpgradeMetricField(Line, TEXT("who")) == Who);
				Check(Case, TEXT("item"), UpgradeMetricField(Line, TEXT("item")) == ItemId.ToString());
				Check(Case, TEXT("player_lv"), UpgradeMetricField(Line, TEXT("lv")) == FString::FromInt(PlayerLevels[Outcome]));
				Check(Case, TEXT("item_lv"), UpgradeMetricField(Line, TEXT("item_lv")) == FString::FromInt(Def->RequiredLevel));
				Check(Case, TEXT("from"), UpgradeMetricField(Line, TEXT("from")) == FString::FromInt(Plus));
				Check(Case, TEXT("to"), UpgradeMetricField(Line, TEXT("to")) == FString::FromInt(ToPlus));
				Check(Case, TEXT("result"), UpgradeMetricField(Line, TEXT("result")) == ExpectedResult[Outcome]);
				Check(Case, TEXT("cost"), UpgradeMetricField(Line, TEXT("cost")) == FString::FromInt(Cost));
				Inv->UpgradeByInstanceId(1, Plus); // Soguma icindeyken tekrar: ikinci satir YOK.
				Check(Case, TEXT("immediate_repeat_no_line"), Capture.Take().Num() == 0);
			}
		}
		for (int32 Gate = 0; Gate < 6; ++Gate)
		{
			Prepare(Gate == 3 ? FMMOItemDatabase::MaxPlus : 0, 20);
			if (Gate == 1) Pawn->GetStats()->bIsDead = true;
			if (Gate == 2) Inv->Gold = 0;
			if (Gate == 5) Inv->LastUpgradeServerTime = World->GetTimeSeconds();
			Capture.Take();
			Inv->UpgradeByInstanceId(Gate == 0 ? 999 : 1, Gate == 4 ? 1 : -1);
			Check(FString::Printf(TEXT("reject_%d"), Gate), TEXT("no_line"), Capture.Take().Num() == 0);
		}
		return true;
	}();
	GLog->RemoveOutputDevice(&Capture);
	if (IsValid(Pawn)) Pawn->Destroy();
	if (!bCompleted) return false;
	if (OutChecks != 66) { ++Failures; OutError += TEXT(" check_count_mismatch"); }
	if (Failures) OutError = FString::Printf(TEXT("failures=%d first=%s"), Failures, *OutError);
	return Failures == 0;
}

namespace
{
	struct FMMOUpgradeMetricTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestUpgradeMetrics"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = UMMOInventoryComponent::RunUpgradeMetricSelfTest(World, Error, Checks);
			Ar.Logf(TEXT("[MMO_UPGRADE_METRIC_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOUpgradeMetricTestExec GMMOUpgradeMetricTestExec;
}
#endif

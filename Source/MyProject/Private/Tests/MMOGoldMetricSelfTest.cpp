#include "Components/MMOInventoryComponent.h"

#if !UE_BUILD_SHIPPING
#include "Characters/MMOCharacter.h"
#include "Core/MMOMetrics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Save/MMOCharacterSaveJson.h"
#include "Save/MMOSaveGame.h"
#include "Tests/MMOGoldMetricProbe.h"
#include "UObject/StrongObjectPtr.h"

bool UMMOInventoryComponent::RunGoldMetricSelfTest(UWorld* World, FString& OutError, int32& OutChecks)
{
	OutChecks = 0; OutError.Reset();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client
		|| !FParse::Param(FCommandLine::Get(), TEXT("unattended"))
		|| !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
	{ OutError = TEXT("disposable authority headless game world required"); return false; }
	TArray<AActor*> Fixtures;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Origin(1000000, 1000000, 5000);
	const auto SpawnPawn = [&]()
	{
		AMMOCharacter* C = World->SpawnActor<AMMOCharacter>(AMMOCharacter::StaticClass(), Origin, FRotator::ZeroRotator, Params);
		if (C) { Fixtures.Add(C); C->SetActorEnableCollision(false); C->SetTravelSaveCommitted(true); }
		return C;
	};
	AMMOCharacter* Pawn = SpawnPawn();
	AMMOCharacter* NPC = SpawnPawn();
	AMMOCharacter* Restored = SpawnPawn();
	APlayerController* PC = World->SpawnActor<APlayerController>(APlayerController::StaticClass(), Origin, FRotator::ZeroRotator, Params);
	if (PC) Fixtures.Add(PC);
	int32 Failures = 0;
	const auto Check = [&](const TCHAR* Case, const TCHAR* Label, bool bOK)
	{
		++OutChecks;
		UE_LOG(LogTemp, Display, TEXT("[MMO_GOLD_METRIC_CASE] %s case=%s check=%s"), bOK ? TEXT("PASS") : TEXT("FAIL"), Case, Label);
		if (!bOK) { ++Failures; if (OutError.IsEmpty()) OutError = FString(Case) + TEXT("/") + Label; }
	};
	struct FExpectedGoldEvent { const TCHAR* Source; int32 Amount; int32 Balance; };
	const bool bCompleted = [&]()
	{
		if (!Pawn || !NPC || !Restored || !PC) { OutError = TEXT("fixture spawn failed"); return false; }
		if (!Pawn->GetInventory() || !NPC->GetInventory() || !Restored->GetInventory())
		{ OutError = TEXT("fixture inventory unavailable"); return false; }
		PC->Possess(Pawn);
		if (!Pawn->IsPlayerControlled() || NPC->IsPlayerControlled()) { OutError = TEXT("possession fixture unavailable"); return false; }
		const auto Run = [&](const TCHAR* Name, int32 SeedGold, int32 Amount, const TCHAR* Source, int32 Mode,
			int32 FinalGold, const TArray<FExpectedGoldEvent>& Events, const TArray<int32>& Published)
		{
			if (PC->GetPawn() != Pawn) PC->Possess(Pawn);
			AMMOCharacter* Target = Mode == 9 ? NPC : Pawn;
			UMMOInventoryComponent* Inv = Target->GetInventory();
			FMMOCharacterSaveData Seed; Seed.Gold = SeedGold; Seed.NextInstanceId = 100;
			Inv->ApplySaveData(Seed);
			const FString Who = MMOMetrics::WhoName(Target);
			UE_LOG(LogTemp, Display, TEXT("[MMO_GOLD_METRIC_BEGIN] case=%s who=%s expected=%d"), Name, *Who, Events.Num());
			for (const FExpectedGoldEvent& E : Events)
				UE_LOG(LogTemp, Display, TEXT("[MMO_GOLD_METRIC_EXPECT] case=%s src=%s amount=%d balance=%d"), Name, E.Source, E.Amount, E.Balance);
			TStrongObjectPtr<UMMOGoldMetricProbe> Probe(NewObject<UMMOGoldMetricProbe>());
			Probe->Callback = [&](int32 Call)
			{
				if (Call == 1)
				{
					if (Mode == 1) Inv->AddGoldFrom(-50, TEXT("yukseltme"));
					else if (Mode == 2) Inv->AddGold(20);
					else if (Mode == 3) { Inv->AddGoldFrom(30, TEXT("gorev")); Inv->AddGold(40); }
					else if (Mode == 4) { Inv->AddGoldFrom(-60, TEXT("yukseltme")); Inv->AddGold(-10); }
					else if (Mode == 5) Inv->AddGoldFrom(-20, TEXT("satin_alma"));
					else if (Mode == 6) Inv->AddGoldFrom(7, TEXT("gorev"));
					else if (Mode == 7) Inv->AddGoldFrom(Amount > 0 ? -1 : 1, TEXT("loot"));
					else if (Mode == 8) { PC->UnPossess(); Inv->AddGoldFrom(7, TEXT("gorev")); }
					else if (Mode == 12) Inv->AddGold(-10);
				}
				else if (Call == 2 && Mode == 4) Inv->AddGoldFrom(5, TEXT("gorev"));
			};
			Inv->OnGoldChanged.AddDynamic(Probe.Get(), &UMMOGoldMetricProbe::OnGold);
			if (Source || Mode == 10) Inv->AddGoldFrom(Amount, Source); else Inv->AddGold(Amount);
			Inv->OnGoldChanged.RemoveDynamic(Probe.Get(), &UMMOGoldMetricProbe::OnGold);
			Probe->Callback = nullptr;
			Check(Name, TEXT("delegate_values"), Probe->Values == Published);
			Check(Name, TEXT("final_balance"), Inv->Gold == FinalGold);
			Check(Name, TEXT("player_gate"), Target->IsPlayerControlled() == (Mode != 8 && Mode != 9));
			FMMOCharacterSaveData Actual, Expected;
			Inv->FillSaveData(Actual); Restored->GetInventory()->ApplySaveData(Actual); Restored->GetInventory()->FillSaveData(Expected);
			TArray<FString> Differences; FMMOCharacterSaveJson::FindDifferences(Actual, Expected, Differences);
			Check(Name, TEXT("memory_roundtrip"), Differences.IsEmpty());
			UE_LOG(LogTemp, Display, TEXT("[MMO_GOLD_METRIC_END] case=%s"), Name);
		};
		Run(TEXT("plain"), 0, 100, nullptr, 0, 100, {{TEXT("diger"),100,100}}, {100});
		Run(TEXT("explicit"), 0, 25, TEXT("loot"), 0, 25, {{TEXT("loot"),25,25}}, {25});
		Run(TEXT("nested_from"), 300, -100, TEXT("satin_alma"), 1, 150,
			{{TEXT("satin_alma"),-100,200},{TEXT("yukseltme"),-50,150}}, {200,150});
		Run(TEXT("nested_plain"), 100, 100, TEXT("gorev"), 2, 220,
			{{TEXT("gorev"),100,200},{TEXT("diger"),20,220}}, {200,220});
		Run(TEXT("nested_then_plain"), 20, 80, TEXT("loot"), 3, 170,
			{{TEXT("loot"),80,100},{TEXT("gorev"),30,130},{TEXT("diger"),40,170}}, {100,130,170});
		Run(TEXT("deep"), 200, 100, TEXT("loot"), 4, 235,
			{{TEXT("loot"),100,300},{TEXT("yukseltme"),-60,240},{TEXT("gorev"),5,245},{TEXT("diger"),-10,235}}, {300,240,245,235});
		Run(TEXT("cap"), MaxGold-5, 50, TEXT("loot"), 5, MaxGold-20,
			{{TEXT("loot"),5,MaxGold},{TEXT("satin_alma"),-20,MaxGold-20}}, {MaxGold,MaxGold-20});
		Run(TEXT("floor"), 5, -50, TEXT("yukseltme"), 6, 7,
			{{TEXT("yukseltme"),-5,0},{TEXT("gorev"),7,7}}, {0,7});
		Run(TEXT("zero"), 100, 0, TEXT("loot"), 0, 100, {}, {});
		Run(TEXT("cap_unchanged"), MaxGold, 1, TEXT("gorev"), 7, MaxGold-1,
			{{TEXT("loot"),-1,MaxGold-1}}, {MaxGold,MaxGold-1});
		Run(TEXT("floor_unchanged"), 0, -1, TEXT("satin_alma"), 7, 1,
			{{TEXT("loot"),1,1}}, {0,1});
		Run(TEXT("detach"), 10, 50, TEXT("loot"), 8, 67, {{TEXT("loot"),50,60}}, {60,67});
		Run(TEXT("npc"), 0, 20, TEXT("loot"), 9, 20, {}, {20});
		Run(TEXT("null_source"), 0, 11, nullptr, 10, 11, {{TEXT("diger"),11,11}}, {11});
		Run(TEXT("returns_seed"), 100, 10, TEXT("loot"), 12, 100,
			{{TEXT("loot"),10,110},{TEXT("diger"),-10,100}}, {110,100});
		return true;
	}();
	for (AActor* Fixture : Fixtures) if (IsValid(Fixture)) Fixture->Destroy();
	// Same frame, travel save suppressed, no fixture LoadCharacter or ticking timers.
	if (!bCompleted) return false;
	if (OutChecks != 60) { ++Failures; OutError += TEXT(" check_count_mismatch"); }
	if (Failures) OutError = FString::Printf(TEXT("failures=%d first=%s"), Failures, *OutError);
	return Failures == 0;
}

namespace
{
	struct FMMOGoldMetricTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestGoldMetrics"))) return false;
			FString Error; int32 Checks=0;
			const bool bPassed = UMMOInventoryComponent::RunGoldMetricSelfTest(World, Error, Checks);
			Ar.Logf(TEXT("[MMO_GOLD_METRIC_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOGoldMetricTestExec GMMOGoldMetricTestExec;
}
#endif

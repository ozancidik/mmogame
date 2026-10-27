// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/MMOShutdownPlan.h"

namespace
{
	// Kronolojik isaret tablosu (azalan esik; esit esikte duyuru faz degisiminden once). AnnounceIdx >= 0 = duyuru,
	// -1 = faz isareti. Duyuru esikleri {3600,1800,600,300,120,60,30,10,5,4,3,2,1} TEK kaynak olarak buradan gelir
	// (her biri bir planda tam bir kez). Buyuk Delta'da bu sira sayesinde ara olaylar yutulmaz ve dogru sirada cikar.
	struct FShutdownPlanMark
	{
		int32 Threshold;
		int32 AnnounceIdx;
		EMMOShutdownPhase Phase;
	};
	constexpr FShutdownPlanMark ShutdownPlanMarks[] =
	{
		{ 3600, 0, EMMOShutdownPhase::Idle },
		{ 1800, 1, EMMOShutdownPhase::Idle },
		{ 600, 2, EMMOShutdownPhase::Idle },
		{ 300, 3, EMMOShutdownPhase::Idle },
		{ 120, 4, EMMOShutdownPhase::Idle },
		{ 60, 5, EMMOShutdownPhase::Idle },
		{ 60, -1, EMMOShutdownPhase::Closing },
		{ 30, 6, EMMOShutdownPhase::Idle },
		{ 20, -1, EMMOShutdownPhase::Flushing },
		{ 10, 7, EMMOShutdownPhase::Idle },
		{ 5, 8, EMMOShutdownPhase::Idle },
		{ 5, -1, EMMOShutdownPhase::Kicking },
		{ 4, 9, EMMOShutdownPhase::Idle },
		{ 3, 10, EMMOShutdownPhase::Idle },
		{ 2, 11, EMMOShutdownPhase::Idle },
		{ 1, 12, EMMOShutdownPhase::Idle },
		{ 0, -1, EMMOShutdownPhase::Exiting },
	};

	constexpr int32 CountAnnounceMarks()
	{
		int32 N = 0;
		for (const FShutdownPlanMark& Mark : ShutdownPlanMarks) { if (Mark.AnnounceIdx >= 0) ++N; }
		return N;
	}
	static_assert(CountAnnounceMarks() == FMMOShutdownPlan::NumAnnounceThresholds, "NumAnnounceThresholds must match the announce marks");
}

void FMMOShutdownPlan::ProcessMarks(TArray<FMMOShutdownEvent>& OutEvents)
{
	for (const FShutdownPlanMark& Mark : ShutdownPlanMarks)
	{
		if (Remaining > static_cast<double>(Mark.Threshold)) continue;
		FMMOShutdownEvent E;
		if (Mark.AnnounceIdx >= 0)
		{
			if (bAnnounced[Mark.AnnounceIdx]) continue;
			bAnnounced[Mark.AnnounceIdx] = true;
			E.Type = EMMOShutdownEventType::Announce;
			E.Phase = Phase;
			E.Seconds = Mark.Threshold;
		}
		else
		{
			if (Phase >= Mark.Phase) continue;
			Phase = Mark.Phase;
			E.Type = EMMOShutdownEventType::PhaseChanged;
			E.Phase = Phase;
		}
		OutEvents.Add(E);
	}
}

EMMOShutdownScheduleResult FMMOShutdownPlan::Schedule(double Seconds, EMMOShutdownReason InReason, TArray<FMMOShutdownEvent>& OutEvents)
{
	if (Phase != EMMOShutdownPhase::Idle) return EMMOShutdownScheduleResult::AlreadyActive;
	if (!FMath::IsFinite(Seconds) || Seconds < MinDelaySeconds || Seconds > MaxDelaySeconds) return EMMOShutdownScheduleResult::OutOfRange;

	Phase = EMMOShutdownPhase::Counting;
	bFlushDone = false;
	Reason = InReason;
	Remaining = Seconds;
	// Gercek kalan sureye esit ya da ondan buyuk esikler zaten gecmis sayilir; ilk duyuru gercek sureyi soyler.
	// Her duyuru bayragi yeniden yazilir (onceki plandan/iptalden bayat bayrak kalmaz; Cancel'in sifirlamasina gerek yok).
	for (const FShutdownPlanMark& Mark : ShutdownPlanMarks)
	{
		if (Mark.AnnounceIdx >= 0) bAnnounced[Mark.AnnounceIdx] = static_cast<double>(Mark.Threshold) >= Seconds;
	}
	FMMOShutdownEvent First;
	First.Type = EMMOShutdownEventType::Announce;
	First.Phase = Phase;
	// FMath::CeilToInt(double) vardir (UE 5.7 FPlatformMath, int64, cift hassasiyet); aralik <= 86400 oldugundan int32 guvenli.
	First.Seconds = static_cast<int32>(FMath::CeilToInt(Seconds));
	OutEvents.Add(First);
	ProcessMarks(OutEvents);
	return EMMOShutdownScheduleResult::Ok;
}

void FMMOShutdownPlan::Advance(double DeltaSeconds, TArray<FMMOShutdownEvent>& OutEvents)
{
	if (Phase == EMMOShutdownPhase::Idle || Phase == EMMOShutdownPhase::Exiting) return;
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0) return;
	Remaining = FMath::Max(0.0, Remaining - DeltaSeconds);
	ProcessMarks(OutEvents);
}

bool FMMOShutdownPlan::NotifyFlushDone()
{
	if (Phase < EMMOShutdownPhase::Flushing || bFlushDone) return false;
	bFlushDone = true;
	return true;
}

bool FMMOShutdownPlan::Cancel(TArray<FMMOShutdownEvent>& OutEvents)
{
	if (Phase != EMMOShutdownPhase::Counting && Phase != EMMOShutdownPhase::Closing) return false;
	Phase = EMMOShutdownPhase::Idle;
	Remaining = 0.0;
	Reason = EMMOShutdownReason::Maintenance; // Idle'da bayat neden kalmaz
	FMMOShutdownEvent E;
	E.Type = EMMOShutdownEventType::Cancelled;
	E.Phase = Phase;
	OutEvents.Add(E);
	return true;
}

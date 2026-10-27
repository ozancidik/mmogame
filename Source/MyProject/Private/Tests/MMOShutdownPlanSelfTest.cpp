// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/MMOShutdownPlan.h"

#if !UE_BUILD_SHIPPING
#include <limits>
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"

// Planli kapatma durum makinesi native testi (docs/DESIGN_SERVER_LIFECYCLE.md 7.1, dilim L0). Dunyasiz: yalniz saf
// FMMOShutdownPlan; olaylar metne cevrilip beklenen dizilerle karsilastirilir. Her Check benzersiz case/check cifti basar.
namespace
{
	constexpr int32 ShutdownPlanTestExpectedChecks = 82;

	using FShutdownTestEvents = TArray<FMMOShutdownEvent>;

	const TCHAR* ShutdownPlanTestPhaseName(EMMOShutdownPhase Phase)
	{
		switch (Phase)
		{
		case EMMOShutdownPhase::Idle: return TEXT("Idle");
		case EMMOShutdownPhase::Counting: return TEXT("Counting");
		case EMMOShutdownPhase::Closing: return TEXT("Closing");
		case EMMOShutdownPhase::Flushing: return TEXT("Flushing");
		case EMMOShutdownPhase::Kicking: return TEXT("Kicking");
		case EMMOShutdownPhase::Exiting: return TEXT("Exiting");
		}
		return TEXT("?");
	}

	// "A125 A120 PClosing C": duyuru = A<saniye>, faz = P<ad>, iptal = C.
	FString ShutdownPlanTestText(const FShutdownTestEvents& Events)
	{
		FString Text;
		for (const FMMOShutdownEvent& E : Events)
		{
			if (!Text.IsEmpty()) Text += TEXT(" ");
			switch (E.Type)
			{
			case EMMOShutdownEventType::Announce: Text += FString::Printf(TEXT("A%d"), E.Seconds); break;
			case EMMOShutdownEventType::PhaseChanged: Text += FString(TEXT("P")) + ShutdownPlanTestPhaseName(E.Phase); break;
			case EMMOShutdownEventType::Cancelled: Text += TEXT("C"); break;
			}
		}
		return Text;
	}

	// Plani Exiting'e kadar Step'lik adimlarla ilerletir; Advance'in urettigi tum olaylari metin olarak dondurur.
	FString ShutdownPlanTestRunTail(FMMOShutdownPlan& Plan, double Step, int32 MaxSteps)
	{
		FShutdownTestEvents All;
		for (int32 i = 0; i < MaxSteps && Plan.GetPhase() != EMMOShutdownPhase::Exiting; ++i) Plan.Advance(Step, All);
		return ShutdownPlanTestText(All);
	}

	bool ShutdownPlanRunSelfTest(FString& OutError, int32& OutChecks)
	{
		OutChecks = 0;
		OutError.Reset();
		int32 Failures = 0;
		const auto Check = [&](const TCHAR* Case, const TCHAR* Label, bool bOK)
		{
			++OutChecks;
			UE_LOG(LogTemp, Display, TEXT("[MMO_SHUTDOWNPLAN_CASE] %s case=%s check=%s"), bOK ? TEXT("PASS") : TEXT("FAIL"), Case, Label);
			if (!bOK) { ++Failures; if (OutError.IsEmpty()) OutError = FString(Case) + TEXT("/") + Label; }
		};
		using P = EMMOShutdownPhase;
		using R = EMMOShutdownReason;
		using S = EMMOShutdownScheduleResult;
		using FEvents = FShutdownTestEvents;
		const R Rs = R::Maintenance;
		const FString FullTail125 = TEXT("A120 A60 PClosing A30 PFlushing A10 A5 PKicking A4 A3 A2 A1 PExiting");

		// 1) Esik dizisi: Schedule(125) ilk duyuru 125, sonra yalniz 120, 60, 30, 10, 5..1; faz olaylari kendi esiklerinde.
		{
			FMMOShutdownPlan Plan; FEvents Ev;
			const S Res = Plan.Schedule(125, Rs, Ev);
			Check(TEXT("threshold_125"), TEXT("schedule_ok"), Res == S::Ok);
			Check(TEXT("threshold_125"), TEXT("first_announce_is_real_remaining"), ShutdownPlanTestText(Ev) == TEXT("A125"));
			Check(TEXT("threshold_125"), TEXT("phase_counting"), Plan.GetPhase() == P::Counting && !Plan.LoginClosed());
			Check(TEXT("threshold_125"), TEXT("tail_sequence"), ShutdownPlanTestRunTail(Plan, 1.0, 400) == FullTail125);
			Check(TEXT("threshold_125"), TEXT("ends_exiting"), Plan.GetPhase() == P::Exiting && Plan.GetRemainingSeconds() == 0.0);
		}
		// 2) Schedule(3600): tum esikler, ilk duyuru 3600.
		{
			FMMOShutdownPlan Plan; FEvents Ev;
			Plan.Schedule(3600, Rs, Ev);
			Check(TEXT("threshold_3600"), TEXT("first_announce"), ShutdownPlanTestText(Ev) == TEXT("A3600"));
			Check(TEXT("threshold_3600"), TEXT("tail_sequence"), ShutdownPlanTestRunTail(Plan, 1.0, 4000)
				== TEXT("A1800 A600 A300 A120 A60 PClosing A30 PFlushing A10 A5 PKicking A4 A3 A2 A1 PExiting"));
		}
		// 3) Schedule(30): en kucuk gecerli sure; ilk duyuru 30 ve kalan zaten Closing esiginin altinda.
		{
			FMMOShutdownPlan Plan; FEvents Ev;
			Plan.Schedule(30, Rs, Ev);
			Check(TEXT("threshold_30"), TEXT("first_events"), ShutdownPlanTestText(Ev) == TEXT("A30 PClosing"));
			Check(TEXT("threshold_30"), TEXT("login_closed_at_schedule"), Plan.GetPhase() == P::Closing && Plan.LoginClosed());
			Check(TEXT("threshold_30"), TEXT("tail_sequence"), ShutdownPlanTestRunTail(Plan, 1.0, 100)
				== TEXT("PFlushing A10 A5 PKicking A4 A3 A2 A1 PExiting"));
		}
		// 4) Kesirli sure: ilk duyuru yukari yuvarlanir.
		{
			FMMOShutdownPlan Plan; FEvents Ev;
			Plan.Schedule(125.5, Rs, Ev);
			Check(TEXT("fractional"), TEXT("first_announce_rounds_up"), ShutdownPlanTestText(Ev) == TEXT("A126"));
			// Esigin hemen ustu: 125.0000001 de yukari yuvarlanir (cift hassasiyet; float'a dusen bir Ceil 125 verirdi).
			FMMOShutdownPlan Tiny; FEvents TinyEv;
			Tiny.Schedule(125.0000001, Rs, TinyEv);
			Check(TEXT("fractional"), TEXT("tiny_fraction_rounds_up_126"), ShutdownPlanTestText(TinyEv) == TEXT("A126"));
		}
		// 5) Esik bir kez: 1000 kucuk adim, tek buyuk adim ve 1 sn'lik adimlar ayni olay kumesini verir.
		{
			FMMOShutdownPlan Small, Big, One; FEvents Ev;
			Small.Schedule(125, Rs, Ev); Big.Schedule(125, Rs, Ev); One.Schedule(125, Rs, Ev);
			Check(TEXT("once_only"), TEXT("small_steps_1000"), ShutdownPlanTestRunTail(Small, 0.125, 1000) == FullTail125);
			Check(TEXT("once_only"), TEXT("single_big_delta"), ShutdownPlanTestRunTail(Big, 1000.0, 1) == FullTail125);
			Check(TEXT("once_only"), TEXT("one_second_steps"), ShutdownPlanTestRunTail(One, 1.0, 400) == FullTail125);
			// Esigin altinda beklerken tekrar tekrar kucuk Advance yeni olay uretmez.
			FMMOShutdownPlan Hold; FEvents HoldEv;
			Hold.Schedule(125, Rs, HoldEv); HoldEv.Reset();
			Hold.Advance(5.0, HoldEv);
			const FString First = ShutdownPlanTestText(HoldEv);
			HoldEv.Reset();
			for (int32 i = 0; i < 20; ++i) Hold.Advance(0.001, HoldEv);
			Check(TEXT("once_only"), TEXT("threshold_fires_first_time"), First == TEXT("A120"));
			Check(TEXT("once_only"), TEXT("no_repeat_below_threshold"), HoldEv.IsEmpty());
		}
		// 6) Buyuk Delta: ara durumlar atlanmaz, sirayla ve bir kez.
		{
			FMMOShutdownPlan Plan; FEvents Ev;
			Plan.Schedule(100, Rs, Ev);
			Check(TEXT("big_delta"), TEXT("schedule_announce"), ShutdownPlanTestText(Ev) == TEXT("A100"));
			Ev.Reset();
			Plan.Advance(100, Ev);
			Check(TEXT("big_delta"), TEXT("all_events_in_order"), ShutdownPlanTestText(Ev)
				== TEXT("A60 PClosing A30 PFlushing A10 A5 PKicking A4 A3 A2 A1 PExiting"));
			int32 PhaseEvents = 0;
			for (const FMMOShutdownEvent& E : Ev) if (E.Type == EMMOShutdownEventType::PhaseChanged) ++PhaseEvents;
			Check(TEXT("big_delta"), TEXT("each_phase_exactly_once"), PhaseEvents == 4);
			Ev.Reset();
			Plan.Advance(100, Ev);
			Check(TEXT("big_delta"), TEXT("terminal_no_more_events"), Ev.IsEmpty() && Plan.GetPhase() == P::Exiting);
		}
		{
			// Tek Advance Closing'i atlayip Flushing'e varir: ikisi de sirayla yayinlanir.
			FMMOShutdownPlan Plan; FEvents Ev;
			Plan.Schedule(125, Rs, Ev); Ev.Reset();
			Plan.Advance(112, Ev); // kalan 13
			Check(TEXT("big_delta_partial"), TEXT("skipped_closing_still_emitted"), ShutdownPlanTestText(Ev) == TEXT("A120 A60 PClosing A30 PFlushing"));
			Check(TEXT("big_delta_partial"), TEXT("phase_flushing"), Plan.GetPhase() == P::Flushing);
		}
		// 7) Limit disi Schedule reddedilir, durum degismez, olay yok.
		{
			struct FBad { const TCHAR* Name; double Value; };
			const FBad Bad[] =
			{
				{ TEXT("bad_schedule_zero"), 0.0 }, { TEXT("bad_schedule_29"), 29.0 }, { TEXT("bad_schedule_29_999"), 29.999 },
				{ TEXT("bad_schedule_86401"), 86401.0 }, { TEXT("bad_schedule_negative"), -5.0 },
				{ TEXT("bad_schedule_nan"), std::numeric_limits<double>::quiet_NaN() },
				{ TEXT("bad_schedule_infinity"), std::numeric_limits<double>::infinity() },
				{ TEXT("bad_schedule_huge"), 1.0e30 },
			};
			for (const FBad& B : Bad)
			{
				FMMOShutdownPlan Plan; FEvents Ev;
				const S Res = Plan.Schedule(B.Value, Rs, Ev);
				Check(B.Name, TEXT("rejected"), Res == S::OutOfRange);
				Check(B.Name, TEXT("state_unchanged"), Plan.GetPhase() == P::Idle && Ev.IsEmpty() && !Plan.LoginClosed());
			}
			FMMOShutdownPlan MinPlan, MaxPlan; FEvents Ev;
			Check(TEXT("bounds"), TEXT("min_30_accepted"), MinPlan.Schedule(30, Rs, Ev) == S::Ok);
			Check(TEXT("bounds"), TEXT("max_86400_accepted"), MaxPlan.Schedule(86400, Rs, Ev) == S::Ok);
		}
		// 8) Ikinci Schedule (Counting iken) ret; Cancel sonrasi kabul.
		{
			FMMOShutdownPlan Plan; FEvents Ev;
			Plan.Schedule(125, Rs, Ev); Ev.Reset();
			Plan.Advance(10, Ev); Ev.Reset();
			const S Second = Plan.Schedule(600, R::Update, Ev);
			Check(TEXT("second_schedule"), TEXT("rejected_while_counting"), Second == S::AlreadyActive);
			Check(TEXT("second_schedule"), TEXT("state_kept"), Ev.IsEmpty() && Plan.GetRemainingSeconds() == 115.0 && Plan.GetReason() == R::Maintenance);
			Check(TEXT("second_schedule"), TEXT("cancel_ok"), Plan.Cancel(Ev) && ShutdownPlanTestText(Ev) == TEXT("C"));
			Ev.Reset();
			Check(TEXT("second_schedule"), TEXT("accepted_after_cancel"), Plan.Schedule(600, R::Update, Ev) == S::Ok);
			Check(TEXT("second_schedule"), TEXT("new_reason_and_announce"), Plan.GetReason() == R::Update && ShutdownPlanTestText(Ev) == TEXT("A600"));
		}
		// 9) Cancel: Counting/Closing'de gecer, Flushing/Kicking/Exiting/Idle'da reddedilir ve durum ayni kalir.
		{
			FMMOShutdownPlan Plan; FEvents Ev;
			Plan.Schedule(125, R::Emergency, Ev); Ev.Reset();
			Check(TEXT("cancel_counting"), TEXT("accepted"), Plan.Cancel(Ev));
			Check(TEXT("cancel_counting"), TEXT("idle_and_event"), Plan.GetPhase() == P::Idle && ShutdownPlanTestText(Ev) == TEXT("C"));
			// Idle'da bayat Reason/Remaining kalmaz (docs/DESIGN_SERVER_LIFECYCLE.md: Idle = "hicbir sey").
			Check(TEXT("cancel_counting"), TEXT("reason_remaining_reset"), Plan.GetReason() == R::Maintenance && Plan.GetRemainingSeconds() == 0.0);
		}
		{
			FMMOShutdownPlan Plan; FEvents Ev;
			Plan.Schedule(125, R::Update, Ev); Ev.Reset();
			Plan.Advance(75, Ev); Ev.Reset(); // kalan 50 = Closing
			Check(TEXT("cancel_closing"), TEXT("in_closing"), Plan.GetPhase() == P::Closing && Plan.LoginClosed());
			Check(TEXT("cancel_closing"), TEXT("accepted"), Plan.Cancel(Ev));
			Check(TEXT("cancel_closing"), TEXT("idle_login_reopened"), Plan.GetPhase() == P::Idle && !Plan.LoginClosed() && ShutdownPlanTestText(Ev) == TEXT("C"));
			Check(TEXT("cancel_closing"), TEXT("reason_remaining_reset"), Plan.GetReason() == R::Maintenance && Plan.GetRemainingSeconds() == 0.0);
			// Yeniden planlama duyuru bayraklarini sifirdan kurar: bunu Schedule'un tum bayraklari yeniden yazmasi saglar
			// (Cancel bayraklara dokunmaz); eski plandan kalan bayrak olsaydi kuyruk eksik duyuru verirdi.
			Ev.Reset();
			Plan.Schedule(125, Rs, Ev);
			Check(TEXT("cancel_closing"), TEXT("reschedule_first_announce"), ShutdownPlanTestText(Ev) == TEXT("A125"));
			Check(TEXT("cancel_closing"), TEXT("reschedule_announces_again"), ShutdownPlanTestRunTail(Plan, 1.0, 400) == FullTail125);
		}
		{
			struct FLate { const TCHAR* Name; double AdvanceBy; EMMOShutdownPhase Expected; };
			const FLate Late[] =
			{
				{ TEXT("cancel_flushing"), 110.0, P::Flushing },  // kalan 15
				{ TEXT("cancel_kicking"), 122.0, P::Kicking },    // kalan 3
				{ TEXT("cancel_exiting"), 125.0, P::Exiting },    // kalan 0
			};
			for (const FLate& L : Late)
			{
				FMMOShutdownPlan Plan; FEvents Ev;
				Plan.Schedule(125, Rs, Ev); Ev.Reset();
				Plan.Advance(L.AdvanceBy, Ev); Ev.Reset();
				const double RemainingBefore = Plan.GetRemainingSeconds();
				const bool bCancelled = Plan.Cancel(Ev);
				Check(L.Name, TEXT("rejected"), !bCancelled && Plan.GetPhase() == L.Expected);
				Check(L.Name, TEXT("state_unchanged"), Ev.IsEmpty() && Plan.GetRemainingSeconds() == RemainingBefore && Plan.LoginClosed());
			}
			FMMOShutdownPlan IdlePlan; FEvents Ev;
			Check(TEXT("cancel_idle"), TEXT("rejected"), !IdlePlan.Cancel(Ev) && Ev.IsEmpty() && IdlePlan.GetPhase() == P::Idle);
		}
		// 10) Negatif / NaN / sonsuz / sifir Delta durum degistirmez.
		{
			FMMOShutdownPlan Plan; FEvents Ev;
			Plan.Schedule(125, Rs, Ev); Ev.Reset();
			Plan.Advance(-10.0, Ev);
			Plan.Advance(std::numeric_limits<double>::quiet_NaN(), Ev);
			Plan.Advance(std::numeric_limits<double>::infinity(), Ev);
			Plan.Advance(0.0, Ev);
			Check(TEXT("bad_delta"), TEXT("no_events"), Ev.IsEmpty());
			Check(TEXT("bad_delta"), TEXT("state_unchanged"), Plan.GetPhase() == P::Counting && Plan.GetRemainingSeconds() == 125.0);
			FMMOShutdownPlan IdlePlan;
			IdlePlan.Advance(100.0, Ev);
			Check(TEXT("bad_delta"), TEXT("idle_advance_noop"), Ev.IsEmpty() && IdlePlan.GetPhase() == P::Idle);
		}
		// 11) LoginClosed yalniz Closing ve sonrasinda true; iptal sonrasi false.
		{
			FMMOShutdownPlan Plan; FEvents Ev;
			Check(TEXT("login_closed"), TEXT("idle_false"), !Plan.LoginClosed());
			Plan.Schedule(125, Rs, Ev);
			Check(TEXT("login_closed"), TEXT("counting_false"), !Plan.LoginClosed());
			Plan.Advance(64, Ev); // kalan 61
			Check(TEXT("login_closed"), TEXT("just_before_threshold_false"), !Plan.LoginClosed() && Plan.GetPhase() == P::Counting);
			Plan.Advance(1, Ev); // kalan 60
			Check(TEXT("login_closed"), TEXT("at_threshold_true"), Plan.LoginClosed() && Plan.GetPhase() == P::Closing);
			Plan.Advance(40, Ev); // kalan 20
			Check(TEXT("login_closed"), TEXT("flushing_true"), Plan.LoginClosed() && Plan.GetPhase() == P::Flushing);
			Plan.Advance(15, Ev); // kalan 5
			Check(TEXT("login_closed"), TEXT("kicking_true"), Plan.LoginClosed() && Plan.GetPhase() == P::Kicking);
			Plan.Advance(5, Ev);
			Check(TEXT("login_closed"), TEXT("exiting_true"), Plan.LoginClosed() && Plan.GetPhase() == P::Exiting);
		}
		// 12) Exiting son durumdur: yeni Schedule reddedilir.
		{
			FMMOShutdownPlan Plan; FEvents Ev;
			Plan.Schedule(30, R::Emergency, Ev);
			Plan.Advance(1000, Ev); Ev.Reset();
			Check(TEXT("terminal"), TEXT("exiting"), Plan.GetPhase() == P::Exiting && Plan.GetReason() == R::Emergency);
			Check(TEXT("terminal"), TEXT("schedule_rejected"), Plan.Schedule(60, Rs, Ev) == S::AlreadyActive && Ev.IsEmpty());
		}
		// 13) L2 kayit turu bitti bilgisi: yalniz Flushing+ kabul, bir kez, atma kapisi yalniz Kicking+ ve flush bitmisse acik.
		{
			FMMOShutdownPlan Idle; FEvents Ev;
			Check(TEXT("flush_done"), TEXT("idle_rejected"), !Idle.NotifyFlushDone() && !Idle.IsFlushDone());
			FMMOShutdownPlan Plan;
			Plan.Schedule(125, Rs, Ev); Ev.Reset();
			Check(TEXT("flush_done"), TEXT("counting_rejected"), !Plan.NotifyFlushDone() && !Plan.IsFlushDone());
			Plan.Advance(70, Ev); // kalan 55 = Closing
			Check(TEXT("flush_done"), TEXT("closing_rejected"), Plan.GetPhase() == P::Closing && !Plan.NotifyFlushDone() && !Plan.IsFlushDone());
			Plan.Advance(40, Ev); // kalan 15 = Flushing
			Check(TEXT("flush_done"), TEXT("flushing_accepted_once"), Plan.GetPhase() == P::Flushing && Plan.NotifyFlushDone() && Plan.IsFlushDone() && !Plan.NotifyFlushDone());
			Check(TEXT("flush_done"), TEXT("kick_not_allowed_in_flushing"), !Plan.KickAllowed());
			Plan.Advance(11, Ev); // kalan 4 = Kicking
			Check(TEXT("flush_done"), TEXT("kick_allowed_when_done_and_kicking"), Plan.GetPhase() == P::Kicking && Plan.KickAllowed());
			FMMOShutdownPlan Late;
			Late.Schedule(125, Rs, Ev); Ev.Reset();
			Late.Advance(122, Ev); // kalan 3 = Kicking, flush bildirilmedi
			Check(TEXT("flush_done"), TEXT("kick_denied_without_flush_done"), Late.GetPhase() == P::Kicking && !Late.KickAllowed());
			Late.Advance(100, Ev);
			Check(TEXT("flush_done"), TEXT("exiting_without_flush_still_denied"), Late.GetPhase() == P::Exiting && !Late.KickAllowed() && Late.NotifyFlushDone() && Late.KickAllowed());
		}

		if (OutChecks != ShutdownPlanTestExpectedChecks) { ++Failures; OutError += TEXT(" check_count_mismatch"); }
		if (Failures) OutError = FString::Printf(TEXT("failures=%d first=%s"), Failures, *OutError);
		return Failures == 0;
	}

	struct FMMOShutdownPlanTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestShutdownPlan"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = ShutdownPlanRunSelfTest(Error, Checks);
			Ar.Logf(TEXT("[MMO_SHUTDOWNPLAN_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOShutdownPlanTestExec GMMOShutdownPlanTestExec;
}
#endif

// Kapsam notu: yalniz saf durum makinesi (kurallar, esikler, olay sirasi). Duyuru metni, PreLogin reddi, kayit turu,
// atma, cikis kodu ve komut satiri/konsol yetkisi sonraki dilimlerdir (L1-L3); burada kanitlanmaz.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Planli kapatma / bakim modu durum makinesi (docs/DESIGN_SERVER_LIFECYCLE.md 4.1, dilim L0).
 * SAF: dunya, aktor, saat, oyuncu yok. Zamani disaridan Advance(DeltaSeconds) ile alir ve olay listesi dondurur;
 * oyunculara duyuru, PreLogin reddi, toplu kayit ve atma sonraki dilimlerin isidir.
 * Idle -> Counting -> Closing -> Flushing -> Kicking -> Exiting (Exiting son durumdur).
 */
enum class EMMOShutdownPhase : uint8
{
	Idle,
	Counting,
	Closing,   // kalan <= LoginCloseSeconds: yeni giris reddi baslar, iptal hala mumkun
	Flushing,  // kalan <= FlushLeadSeconds: toplu kayit turu, iptal artik yok
	Kicking,   // kalan <= KickLeadSeconds: oyuncular atilir
	Exiting    // kalan <= 0: sunucu cikisi
};

/** Kapatma nedeni sabit sozluktur (bakim, guncelleme, acil); serbest metin kabul edilmez. */
enum class EMMOShutdownReason : uint8
{
	Maintenance,
	Update,
	Emergency
};

enum class EMMOShutdownScheduleResult : uint8
{
	Ok,
	AlreadyActive,  // Idle degil (Counting..Exiting): once Cancel (yalniz Counting/Closing'de gecer)
	OutOfRange      // NaN/sonsuz ya da [MinDelaySeconds, MaxDelaySeconds] disi; kirpma yok
};

enum class EMMOShutdownEventType : uint8
{
	Announce,       // Seconds = duyurulacak kalan sure (saniye)
	PhaseChanged,   // Phase = girilen yeni durum
	Cancelled       // Phase = Idle
};

struct FMMOShutdownEvent
{
	EMMOShutdownEventType Type = EMMOShutdownEventType::Announce;
	EMMOShutdownPhase Phase = EMMOShutdownPhase::Idle;
	int32 Seconds = 0;

	bool operator==(const FMMOShutdownEvent& O) const { return Type == O.Type && Phase == O.Phase && Seconds == O.Seconds; }
};

class FMMOShutdownPlan
{
public:
	// [PLACEHOLDER] degerler (docs/DESIGN_SERVER_LIFECYCLE.md 6).
	static constexpr double MinDelaySeconds = 30.0;
	static constexpr double MaxDelaySeconds = 86400.0;
	static constexpr double LoginCloseSeconds = 60.0;
	static constexpr double FlushLeadSeconds = 20.0;
	static constexpr double KickLeadSeconds = 5.0;
	static constexpr int32 NumAnnounceThresholds = 13;

	/**
	 * Geri sayimi baslatir. Gecerliyse OutEvents'e once gercek kalan sureyi soyleyen Announce, sonra (kalan zaten
	 * bir faz esiginin altindaysa) ilgili PhaseChanged olaylari eklenir. Planlama aninda zaten gecmis duyuru esikleri atlanir.
	 */
	EMMOShutdownScheduleResult Schedule(double Seconds, EMMOShutdownReason InReason, TArray<FMMOShutdownEvent>& OutEvents);

	/** Zamani ilerletir; buyuk Delta ara olaylari atlamaz (hepsi sirayla eklenir). Negatif/NaN/sonsuz/0 Delta yok sayilir. */
	void Advance(double DeltaSeconds, TArray<FMMOShutdownEvent>& OutEvents);

	/** Yalniz Counting/Closing'de gecer (Idle'a doner, Cancelled olayi ekler); Flushing/Kicking/Exiting/Idle'da false, durum ayni. */
	bool Cancel(TArray<FMMOShutdownEvent>& OutEvents);

	EMMOShutdownPhase GetPhase() const { return Phase; }
	EMMOShutdownReason GetReason() const { return Reason; }
	double GetRemainingSeconds() const { return Remaining; }
	/** Yeni girisler reddedilir: Closing ve sonrasi (Flushing, Kicking, Exiting). Iptal/Idle'da false. */
	bool LoginClosed() const { return Phase >= EMMOShutdownPhase::Closing; }

private:
	void ProcessMarks(TArray<FMMOShutdownEvent>& OutEvents);

	EMMOShutdownPhase Phase = EMMOShutdownPhase::Idle;
	EMMOShutdownReason Reason = EMMOShutdownReason::Maintenance;
	double Remaining = 0.0;
	bool bAnnounced[NumAnnounceThresholds] = {};
};

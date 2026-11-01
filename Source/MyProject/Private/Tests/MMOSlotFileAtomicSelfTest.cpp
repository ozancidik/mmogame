// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOSlotFileAtomic.h"

#if !UE_BUILD_SHIPPING
#include "Misc/CoreMisc.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"

// Karakter .sav atomik yazim/kurtarma mantigi native testi (docs/DESIGN_CHARACTER_SAVE_ATOMIC.md 6.1, dilim S6.0).
// Dunyasiz ve DISKSIZ: yalniz FMMOSlotFileAtomic + bellek ici sahte dosya sistemi. Sahte FS, motorun IFileManager::Move(Replace)
// davranisini (once hedefi sil, sonra yeniden adlandir: iki ayri adim) ve "her mutasyon adiminden sonra surec olur" ile
// "su adim hata verir" enjeksiyonunu modeller. Gercek disk ve magaza baglantisi S6.1'dedir; burada kanitlanmaz.
// Her Check benzersiz case/check cifti basar; matris sonuclari senaryo basina toplanir (satir sayisi mutasyonlarda sabit kalir).
namespace
{
	constexpr int32 SlotAtomicTestExpectedChecks = 158;

	using FSlotAtomicTestBytes = TArray<uint8>;

	// Test biçimi: 0xA5, 4 bayt LE yuk uzunlugu, yuk (hep ayni etiket), 0x5A. Yarim kesik dosya her zaman gecersiz sayilir.
	FSlotAtomicTestBytes SlotAtomicTestValid(uint8 Tag, int32 PayloadLen = 24)
	{
		FSlotAtomicTestBytes B;
		B.Reserve(PayloadLen + 6);
		B.Add(0xA5);
		for (int32 Shift = 0; Shift < 32; Shift += 8) B.Add(static_cast<uint8>((PayloadLen >> Shift) & 0xFF));
		for (int32 i = 0; i < PayloadLen; ++i) B.Add(Tag);
		B.Add(0x5A);
		return B;
	}

	bool SlotAtomicTestValidate(const TArray<uint8>& B)
	{
		if (B.Num() < 6 || B[0] != 0xA5 || B.Last() != 0x5A) return false;
		const int32 Len = static_cast<int32>(B[1]) | (static_cast<int32>(B[2]) << 8) | (static_cast<int32>(B[3]) << 16) | (static_cast<int32>(B[4]) << 24);
		return Len == B.Num() - 6;
	}

	const auto SlotAtomicTestValidator = [](const TArray<uint8>& B) { return SlotAtomicTestValidate(B); };

	FSlotAtomicTestBytes SlotAtomicTestTorn(const FSlotAtomicTestBytes& Full)
	{
		FSlotAtomicTestBytes B = Full;
		B.SetNum(Full.Num() / 2);
		return B;
	}

	// Bellek ici dosya sistemi. Sayilan "alt adimlar": Delete, WriteNew, Move'un "hedefi sil" yarisi (hedef varsa) ve "yeniden adlandir" yarisi.
	class FSlotAtomicTestFs : public IMMOSlotFileOps
	{
	public:
		TMap<FString, TArray<uint8>> Files;
		TSet<FString> ReadOnly;          // salt-okunur bit: WriteNew / Move-hedef-silme / Delete(bEvenReadOnly=false) basarisiz; Delete(true) calisir; bit yeniden adlandirmayla tasinir
		TSet<FString> Locked;            // paylasim ihlali benzeri: Delete (her durumda) / WriteNew / Move-hedef-silme basarisiz
		TSet<FString> EvenRoDeleted;     // Delete(bEvenReadOnly=true) ile silinmeye CALISILAN yollar (kapsam denetimi)
		TSet<FString> Unreadable;        // ReadAll basarisiz
		TSet<int32> FailOps;             // bu numarali alt adimlar etkisiz hata verir
		TSet<int32> FailTornOps;         // bu numarali alt adim WriteNew ise yarim dosya birakip hata verir
		int32 CrashAt = -1;              // >=0: bu alt adimdan SONRA surec olur (0: hicbir adim calismadan)
		int32 CrashTorn = 0;             // CrashAt adimi WriteNew ise: 0 tam, 1 bos dosya, 2 yarim dosya
		bool bCorruptWrites = false;     // WriteNew "basarili" ama bir bayt farkli saklanir (geri okuma uyusmazligi)
		bool bDead = false;
		int32 OpCount = 0;               // calisan alt adim sayisi
		int32 MutatingCalls = 0;         // Delete/WriteNew/Move cagri sayisi (adim sayilsin sayilmasin)
		TArray<bool> OpIsWrite;

		virtual bool Exists(const FString& Path) override { return Files.Contains(Path); }

		virtual bool ReadAll(const FString& Path, TArray<uint8>& Out) override
		{
			if (Unreadable.Contains(Path)) return false;
			const TArray<uint8>* F = Files.Find(Path);
			if (!F) return false;
			Out = *F;
			return true;
		}

		virtual bool WriteNew(const FString& Path, const TArray<uint8>& Bytes) override
		{
			++MutatingCalls;
			const int32 G = Gate(true);
			if (G == 0) return false;
			if (G == 1)
			{
				if (FailTornOps.Contains(OpCount)) Files.Add(Path, SlotAtomicTestTorn(Bytes));
				After();
				return false;
			}
			if (ReadOnly.Contains(Path) || Locked.Contains(Path)) { After(); return false; }
			TArray<uint8> Stored = Bytes;
			if (bCorruptWrites && Stored.Num() > 2) Stored[Stored.Num() / 2] ^= 0xFF;
			if (OpCount == CrashAt && CrashTorn == 1) Stored.Reset();
			else if (OpCount == CrashAt && CrashTorn == 2) Stored.SetNum(Stored.Num() / 2);
			Files.Add(Path, Stored);
			After();
			return true;
		}

		virtual bool Move(const FString& Dest, const FString& Src) override
		{
			++MutatingCalls;
			if (!Files.Contains(Src)) return false;
			if (Files.Contains(Dest))
			{
				// IFileManager::Move(Replace): hedef ONCE silinir (ayri adim), sonra yeniden adlandirilir.
				if (Gate(false) != 2) return false;
				if (ReadOnly.Contains(Dest) || Locked.Contains(Dest)) { After(); return false; }
				Files.Remove(Dest);
				After();
			}
			if (Gate(false) != 2) return false;
			TArray<uint8> V = Files.FindAndRemoveChecked(Src);
			Files.Add(Dest, MoveTemp(V));
			if (ReadOnly.Remove(Src) > 0) ReadOnly.Add(Dest); // yeniden adlandirma salt-okunur biti tasir
			After();
			return true;
		}

		virtual bool Delete(const FString& Path, bool bEvenReadOnly) override
		{
			++MutatingCalls;
			if (bEvenReadOnly) EvenRoDeleted.Add(Path);
			if (!Files.Contains(Path)) return true;
			if (Gate(false) != 2) return false;
			if (Locked.Contains(Path) || (ReadOnly.Contains(Path) && !bEvenReadOnly)) { After(); return false; }
			Files.Remove(Path);
			ReadOnly.Remove(Path);
			After();
			return true;
		}

		virtual void ClearReadOnly(const FString& Path) override { ReadOnly.Remove(Path); } // sayilan adim degil

	private:
		// 0 = surec olu (etkisiz), 1 = enjekte hata, 2 = calis.
		int32 Gate(bool bIsWrite)
		{
			if (bDead) return 0;
			++OpCount;
			OpIsWrite.Add(bIsWrite);
			if (FailOps.Contains(OpCount) || FailTornOps.Contains(OpCount)) return 1;
			return 2;
		}
		void After() { if (OpCount == CrashAt) bDead = true; }
	};

	bool SlotAtomicTestSameFiles(const TMap<FString, TArray<uint8>>& A, const TMap<FString, TArray<uint8>>& B)
	{
		if (A.Num() != B.Num()) return false;
		for (const TPair<FString, TArray<uint8>>& P : A)
		{
			const TArray<uint8>* O = B.Find(P.Key);
			if (!O || *O != P.Value) return false;
		}
		return true;
	}

	FMMOSlotLoadResult SlotAtomicTestLoad(FSlotAtomicTestFs& Fs, const FMMOSlotAtomicPaths& P)
	{
		return FMMOSlotFileAtomic::Load(Fs, P, SlotAtomicTestValidator);
	}

	FMMOSlotSaveResult SlotAtomicTestSave(FSlotAtomicTestFs& Fs, const FMMOSlotAtomicPaths& P, const TArray<uint8>& Bytes)
	{
		return FMMOSlotFileAtomic::Save(Fs, P, Bytes, SlotAtomicTestValidator);
	}

	// Bir senaryo: baslangic dosya sistemi + yeni veri. Eski veri = baslangicta Load'un verdigi.
	struct FSlotAtomicTestCtx
	{
		FString Name;
		FMMOSlotAtomicPaths Paths;
		FSlotAtomicTestFs Initial;
		FMMOSlotLoadResult OldLoad;
		TArray<TArray<uint8>> InitialCorrupt;  // baslangicta var olan bozuk ana/.bak icerikleri (kanit: hicbiri kaybolmaz)
		FSlotAtomicTestBytes NewData;
	};

	struct FSlotAtomicTestEval
	{
		bool I1 = true, I2 = true, I3 = true, I4 = true, I5 = true;
	};

	void SlotAtomicTestInitCtx(FSlotAtomicTestCtx& C, const TCHAR* Name, const FSlotAtomicTestFs& Initial)
	{
		C.Name = Name;
		C.Paths = FMMOSlotAtomicPaths::From(TEXT("SaveGames/MMOSave_T.sav"));
		C.Initial = Initial;
		C.NewData = SlotAtomicTestValid(3);
		FSlotAtomicTestFs Tmp = Initial;
		C.OldLoad = SlotAtomicTestLoad(Tmp, C.Paths);
		for (const FString& Path : { C.Paths.Main, C.Paths.Bak })
		{
			const TArray<uint8>* F = Initial.Files.Find(Path);
			if (F && !SlotAtomicTestValidate(*F)) C.InitialCorrupt.Add(*F);
		}
	}

	// Surecin (ya da Save'in) sonunda diskteki duruma I1-I5 uygular. Res verilirse Save sonucuna bagli I3/I4 de.
	void SlotAtomicTestEval(const FSlotAtomicTestCtx& C, FSlotAtomicTestFs& After, const FMMOSlotSaveResult* Res, FSlotAtomicTestEval& E)
	{
		const FMMOSlotLoadResult L = SlotAtomicTestLoad(After, C.Paths);
		const bool bIsNew = L.bOk && L.Bytes == C.NewData;
		const bool bIsOld = L.bOk && C.OldLoad.bOk && L.Bytes == C.OldLoad.Bytes;
		// I1: eski ya da yeni, karisim asla.
		if (L.bOk && !bIsNew && !bIsOld) E.I1 = false;
		// I2: eskiden yuklenebilen karakter, hicbir noktada yuklenemez hale gelmez.
		if (C.OldLoad.bOk && !L.bOk) E.I2 = false;
		// I5: baslangictaki her bozuk ana/.bak icerigi diskte bir yerde durur (silinmez, en kotu .bad).
		for (const TArray<uint8>& Corrupt : C.InitialCorrupt)
		{
			bool bFound = false;
			for (const TPair<FString, TArray<uint8>>& P : After.Files) { if (P.Value == Corrupt) { bFound = true; break; } }
			if (!bFound) E.I5 = false;
		}
		if (Res)
		{
			// I3: Save false ise Load eski veriyi verir (tek istisna: dogrulanmis artik .tmp'den gelen yeni veri; ilk kayit senaryosu, .tmp silinemediyse).
			if (!Res->bOk)
			{
				const bool bTmpNew = L.bOk && L.Source == EMMOSlotLoadSource::Tmp && bIsNew;
				if (C.OldLoad.bOk) { if (!(bIsOld || bTmpNew)) E.I3 = false; }
				else if (L.bOk && !bTmpNew) E.I3 = false;
			}
			// I4: Save true ise Load yeni veriyi ANA dosyadan verir.
			if (Res->bOk && !(bIsNew && L.Source == EMMOSlotLoadSource::Main)) E.I4 = false;
		}
	}

	struct FSlotAtomicTestAgg
	{
		bool CleanOk = false;
		int32 Steps = 0;
		TMap<FString, TArray<uint8>> CleanFiles;
		bool CrashI1 = true, CrashI2 = true, CrashI5 = true;
		bool FailI125 = true, FailI3 = true, FailI4 = true;
		bool Fail2All = true;
		int32 CrashPoints = 0, FailRuns = 0, Fail2Runs = 0;
		FString FirstBad;
	};

	void SlotAtomicTestNote(FSlotAtomicTestAgg& A, const TCHAR* What, int32 J, int32 K, int32 V)
	{
		if (A.FirstBad.IsEmpty()) A.FirstBad = FString::Printf(TEXT("%s@%d,%d,v%d"), What, J, K, V);
	}

	void SlotAtomicTestMatrix(const FSlotAtomicTestCtx& C, FSlotAtomicTestAgg& A)
	{
		FSlotAtomicTestFs Probe = C.Initial;
		const FMMOSlotSaveResult Clean = SlotAtomicTestSave(Probe, C.Paths, C.NewData);
		A.CleanOk = Clean.bOk;
		A.Steps = Probe.OpCount;
		A.CleanFiles = Probe.Files;
		const TArray<bool> IsWrite = Probe.OpIsWrite;
		const int32 N = Probe.OpCount;

		// Cokme matrisi: her alt adimdan SONRA surec olur (0 = hic adim calismadan); yazma adiminda ek olarak bos/yarim dosya.
		for (int32 K = 0; K <= N; ++K)
		{
			const int32 MaxV = (K >= 1 && IsWrite[K - 1]) ? 2 : 0;
			for (int32 V = 0; V <= MaxV; ++V)
			{
				FSlotAtomicTestFs F = C.Initial;
				F.CrashAt = K;
				F.CrashTorn = V;
				if (K == 0) F.bDead = true;
				SlotAtomicTestSave(F, C.Paths, C.NewData);
				FSlotAtomicTestEval E;
				SlotAtomicTestEval(C, F, nullptr, E);
				++A.CrashPoints;
				if (!E.I1) { A.CrashI1 = false; SlotAtomicTestNote(A, TEXT("crashI1"), K, 0, V); }
				if (!E.I2) { A.CrashI2 = false; SlotAtomicTestNote(A, TEXT("crashI2"), K, 0, V); }
				if (!E.I5) { A.CrashI5 = false; SlotAtomicTestNote(A, TEXT("crashI5"), K, 0, V); }
			}
		}

		// Tek hata: her alt adim (ve ilk hata yollarinin ek adimlari) etkisiz hata verir; yazma adiminda ek olarak yarim dosya birakan hata.
		for (int32 J = 1; J <= N + 3; ++J)
		{
			const int32 MaxV = (J <= N && IsWrite[J - 1]) ? 1 : 0;
			for (int32 V = 0; V <= MaxV; ++V)
			{
				FSlotAtomicTestFs F = C.Initial;
				if (V == 0) F.FailOps.Add(J); else F.FailTornOps.Add(J);
				const FMMOSlotSaveResult R = SlotAtomicTestSave(F, C.Paths, C.NewData);
				FSlotAtomicTestEval E;
				SlotAtomicTestEval(C, F, &R, E);
				++A.FailRuns;
				if (!(E.I1 && E.I2 && E.I5)) { A.FailI125 = false; SlotAtomicTestNote(A, TEXT("failI125"), J, 0, V); }
				if (!E.I3) { A.FailI3 = false; SlotAtomicTestNote(A, TEXT("failI3"), J, 0, V); }
				if (!E.I4) { A.FailI4 = false; SlotAtomicTestNote(A, TEXT("failI4"), J, 0, V); }
			}
		}

		// Cift hata: iki farkli alt adim birlikte hata verir (ornegin tasima + geri koyma).
		for (int32 J = 1; J <= N + 3; ++J)
		{
			for (int32 K = J + 1; K <= N + 3; ++K)
			{
				FSlotAtomicTestFs F = C.Initial;
				F.FailOps.Add(J);
				F.FailOps.Add(K);
				const FMMOSlotSaveResult R = SlotAtomicTestSave(F, C.Paths, C.NewData);
				FSlotAtomicTestEval E;
				SlotAtomicTestEval(C, F, &R, E);
				++A.Fail2Runs;
				if (!(E.I1 && E.I2 && E.I3 && E.I4 && E.I5)) { A.Fail2All = false; SlotAtomicTestNote(A, TEXT("fail2"), J, K, 0); }
			}
		}
	}

	// Temiz Save'den sonra yalniz ana dosya (ve varsa .bad kanitlari) kalir.
	bool SlotAtomicTestCleanLayout(const FSlotAtomicTestCtx& C, const FSlotAtomicTestAgg& A)
	{
		if (!A.CleanOk) return false;
		const TArray<uint8>* M = A.CleanFiles.Find(C.Paths.Main);
		if (!M || *M != C.NewData) return false;
		for (const TPair<FString, TArray<uint8>>& P : A.CleanFiles)
		{
			if (P.Key == C.Paths.Main) continue;
			if (!P.Key.StartsWith(C.Paths.Main + TEXT(".bad"))) return false;
		}
		return true;
	}

	bool SlotAtomicTestRunSelfTest(FString& OutError, int32& OutChecks)
	{
		OutChecks = 0;
		OutError.Reset();
		int32 Failures = 0;
		const auto Check = [&](const TCHAR* Case, const TCHAR* Label, bool bOK)
		{
			++OutChecks;
			UE_LOG(LogTemp, Display, TEXT("[MMO_SLOTATOMIC_CASE] %s case=%s check=%s"), bOK ? TEXT("PASS") : TEXT("FAIL"), Case, Label);
			if (!bOK) { ++Failures; if (OutError.IsEmpty()) OutError = FString(Case) + TEXT("/") + Label; }
		};

		const FSlotAtomicTestBytes Old = SlotAtomicTestValid(1);
		const FSlotAtomicTestBytes Older = SlotAtomicTestValid(2);
		const FSlotAtomicTestBytes NewD = SlotAtomicTestValid(3);
		const FSlotAtomicTestBytes Newer = SlotAtomicTestValid(4);
		const FSlotAtomicTestBytes TornMain = SlotAtomicTestTorn(SlotAtomicTestValid(5));
		const FSlotAtomicTestBytes TornBak = SlotAtomicTestTorn(SlotAtomicTestValid(6));
		const FSlotAtomicTestBytes TornTmp = SlotAtomicTestTorn(SlotAtomicTestValid(7));
		const FSlotAtomicTestBytes TmpValid = SlotAtomicTestValid(9);
		const FMMOSlotAtomicPaths P = FMMOSlotAtomicPaths::From(TEXT("SaveGames/MMOSave_T.sav"));

		// Senaryolar: ad, beklenen temiz alt adim sayisi (elle hesaplandi, tasarim 4.1 adim 3-7), baslangic dosyalari.
		struct FScenario { const TCHAR* Name; int32 Steps; FSlotAtomicTestFs Fs; };
		TArray<FScenario> Scenarios;
		const auto AddScenario = [&](const TCHAR* Name, int32 Steps, TFunction<void(FSlotAtomicTestFs&)> Setup)
		{
			FScenario S; S.Name = Name; S.Steps = Steps; Setup(S.Fs); Scenarios.Add(MoveTemp(S));
		};
		AddScenario(TEXT("first_ever"), 2, [&](FSlotAtomicTestFs& F) { });
		AddScenario(TEXT("main_only"), 4, [&](FSlotAtomicTestFs& F) { F.Files.Add(P.Main, Old); });
		AddScenario(TEXT("main_and_bak"), 5, [&](FSlotAtomicTestFs& F) { F.Files.Add(P.Main, Old); F.Files.Add(P.Bak, Older); });
		AddScenario(TEXT("main_corrupt_bak_valid"), 4, [&](FSlotAtomicTestFs& F) { F.Files.Add(P.Main, TornMain); F.Files.Add(P.Bak, Old); });
		AddScenario(TEXT("main_corrupt_only"), 3, [&](FSlotAtomicTestFs& F) { F.Files.Add(P.Main, TornMain); });
		AddScenario(TEXT("bak_only"), 3, [&](FSlotAtomicTestFs& F) { F.Files.Add(P.Bak, Old); });
		AddScenario(TEXT("bak_corrupt_main_valid"), 5, [&](FSlotAtomicTestFs& F) { F.Files.Add(P.Main, Old); F.Files.Add(P.Bak, TornBak); });
		AddScenario(TEXT("stale_tmp"), 5, [&](FSlotAtomicTestFs& F) { F.Files.Add(P.Main, Old); F.Files.Add(P.Tmp, TornTmp); });
		AddScenario(TEXT("tmp_valid_first"), 4, [&](FSlotAtomicTestFs& F) { F.Files.Add(P.Tmp, TmpValid); });
		AddScenario(TEXT("both_corrupt"), 4, [&](FSlotAtomicTestFs& F) { F.Files.Add(P.Main, TornMain); F.Files.Add(P.Bak, TornBak); });
		AddScenario(TEXT("bad_slot_taken"), 4, [&](FSlotAtomicTestFs& F) { F.Files.Add(P.Main, TornMain); F.Files.Add(P.Bak, Old); F.Files.Add(P.BadSlot(0), TornTmp); });

		int32 TotalCrash = 0, TotalFail = 0, TotalFail2 = 0;
		bool bLoadPure = true;
		for (const FScenario& S : Scenarios)
		{
			FSlotAtomicTestCtx C;
			SlotAtomicTestInitCtx(C, S.Name, S.Fs);
			FSlotAtomicTestAgg A;
			SlotAtomicTestMatrix(C, A);
			TotalCrash += A.CrashPoints; TotalFail += A.FailRuns; TotalFail2 += A.Fail2Runs;
			UE_LOG(LogTemp, Display, TEXT("[MMO_SLOTATOMIC_MATRIX] scenario=%s steps=%d crash_points=%d fail_runs=%d fail2_runs=%d first_bad=%s"), S.Name, A.Steps, A.CrashPoints, A.FailRuns, A.Fail2Runs, *A.FirstBad);
			Check(S.Name, TEXT("steps_pinned"), A.CleanOk && A.Steps == S.Steps);
			Check(S.Name, TEXT("clean_layout"), SlotAtomicTestCleanLayout(C, A));
			Check(S.Name, TEXT("crash_I1_old_or_new"), A.CrashI1);
			Check(S.Name, TEXT("crash_I2_no_usable_loss"), A.CrashI2);
			Check(S.Name, TEXT("crash_I5_evidence_kept"), A.CrashI5);
			Check(S.Name, TEXT("fail_I1_I2_I5"), A.FailI125);
			Check(S.Name, TEXT("fail_I3_failed_save_keeps_old"), A.FailI3);
			Check(S.Name, TEXT("fail_I4_ok_save_is_new_from_main"), A.FailI4);
			Check(S.Name, TEXT("fail2_all_invariants"), A.Fail2All);
			// Load yan etkisiz: dosyalar ve mutasyon cagri sayisi degismez.
			FSlotAtomicTestFs LF = S.Fs;
			SlotAtomicTestLoad(LF, P);
			if (LF.MutatingCalls != 0 || !SlotAtomicTestSameFiles(LF.Files, S.Fs.Files)) bLoadPure = false;
		}
		Check(TEXT("load"), TEXT("no_side_effects_in_any_scenario"), bLoadPure);

		// --- Load kaynak sirasi ve hata nedenleri ---
		{
			FSlotAtomicTestFs F;
			F.Files.Add(P.Main, Old); F.Files.Add(P.Bak, Older); F.Files.Add(P.Tmp, TmpValid);
			FMMOSlotLoadResult L = SlotAtomicTestLoad(F, P);
			Check(TEXT("load_order"), TEXT("main_wins_over_bak_and_tmp"), L.bOk && L.Source == EMMOSlotLoadSource::Main && L.Bytes == Old);
		}
		{
			FSlotAtomicTestFs F;
			F.Files.Add(P.Main, TornMain); F.Files.Add(P.Bak, Older); F.Files.Add(P.Tmp, TmpValid);
			FMMOSlotLoadResult L = SlotAtomicTestLoad(F, P);
			Check(TEXT("load_order"), TEXT("corrupt_main_uses_bak_not_tmp"), L.bOk && L.Source == EMMOSlotLoadSource::Bak && L.Bytes == Older);
		}
		{
			FSlotAtomicTestFs F;
			F.Files.Add(P.Tmp, TmpValid);
			FMMOSlotLoadResult L = SlotAtomicTestLoad(F, P);
			Check(TEXT("load_order"), TEXT("tmp_only_when_main_and_bak_absent"), L.bOk && L.Source == EMMOSlotLoadSource::Tmp && L.Bytes == TmpValid);
		}
		{
			FSlotAtomicTestFs F;
			F.Files.Add(P.Main, TornMain); F.Files.Add(P.Tmp, TmpValid);
			FMMOSlotLoadResult L = SlotAtomicTestLoad(F, P);
			Check(TEXT("load_order"), TEXT("tmp_not_used_when_main_exists_but_corrupt"), !L.bOk && L.Failure == EMMOSlotLoadFailure::Corrupt && L.Bytes.Num() == 0);
		}
		{
			FSlotAtomicTestFs F;
			F.Files.Add(P.Bak, TornBak); F.Files.Add(P.Tmp, TmpValid);
			FMMOSlotLoadResult L = SlotAtomicTestLoad(F, P);
			Check(TEXT("load_order"), TEXT("tmp_not_used_when_bak_exists_but_corrupt"), !L.bOk && L.Failure == EMMOSlotLoadFailure::Corrupt && L.Bytes.Num() == 0);
		}
		{
			FSlotAtomicTestFs F;
			F.Files.Add(P.Tmp, TornTmp);
			FMMOSlotLoadResult L = SlotAtomicTestLoad(F, P);
			Check(TEXT("load_fail"), TEXT("stray_invalid_tmp_alone_is_missing"), !L.bOk && L.Failure == EMMOSlotLoadFailure::Missing);
		}
		{
			FSlotAtomicTestFs F;
			FMMOSlotLoadResult L = SlotAtomicTestLoad(F, P);
			Check(TEXT("load_fail"), TEXT("nothing_is_missing"), !L.bOk && L.Failure == EMMOSlotLoadFailure::Missing && L.Source == EMMOSlotLoadSource::None);
		}
		{
			FSlotAtomicTestFs F;
			F.Files.Add(P.Main, TornMain);
			FMMOSlotLoadResult L = SlotAtomicTestLoad(F, P);
			Check(TEXT("load_fail"), TEXT("corrupt_main_only_is_corrupt"), !L.bOk && L.Failure == EMMOSlotLoadFailure::Corrupt);
		}
		{
			FSlotAtomicTestFs F;
			F.Files.Add(P.Bak, TornBak);
			FMMOSlotLoadResult L = SlotAtomicTestLoad(F, P);
			Check(TEXT("load_fail"), TEXT("corrupt_bak_only_is_corrupt"), !L.bOk && L.Failure == EMMOSlotLoadFailure::Corrupt);
		}
		{
			FSlotAtomicTestFs F;
			F.Files.Add(P.Main, FSlotAtomicTestBytes());
			F.Files.Add(P.Bak, Older);
			FMMOSlotLoadResult L = SlotAtomicTestLoad(F, P);
			Check(TEXT("load_fail"), TEXT("zero_length_main_is_corrupt_falls_to_bak"), L.bOk && L.Source == EMMOSlotLoadSource::Bak && L.Bytes == Older);
		}
		{
			FSlotAtomicTestFs F;
			F.Files.Add(P.Main, Old); F.Files.Add(P.Bak, Older);
			F.Unreadable.Add(P.Main);
			FMMOSlotLoadResult L = SlotAtomicTestLoad(F, P);
			Check(TEXT("load_fail"), TEXT("unreadable_main_io_error_no_bak_fallback"), !L.bOk && L.Failure == EMMOSlotLoadFailure::IoError && L.Source == EMMOSlotLoadSource::None && L.Bytes.Num() == 0);
		}
		{
			FSlotAtomicTestFs F;
			F.Files.Add(P.Bak, Older); F.Files.Add(P.Tmp, TmpValid);
			F.Unreadable.Add(P.Bak);
			FMMOSlotLoadResult L = SlotAtomicTestLoad(F, P);
			Check(TEXT("load_fail"), TEXT("unreadable_bak_with_main_missing_is_io_error_not_tmp"), !L.bOk && L.Failure == EMMOSlotLoadFailure::IoError && L.Bytes.Num() == 0);
		}
		{
			FSlotAtomicTestFs F;
			F.Files.Add(P.Main, TornMain); F.Files.Add(P.Bak, Older);
			FMMOSlotLoadResult L = SlotAtomicTestLoad(F, P);
			Check(TEXT("load_fail"), TEXT("content_corrupt_main_still_falls_to_bak_not_io_error"), L.bOk && L.Source == EMMOSlotLoadSource::Bak && L.Bytes == Older);
		}

		// --- Exists ---
		{
			FSlotAtomicTestFs F;
			Check(TEXT("exists"), TEXT("nothing_false"), !FMMOSlotFileAtomic::Exists(F, P));
			F.Files.Add(P.Tmp, TmpValid);
			Check(TEXT("exists"), TEXT("tmp_alone_false"), !FMMOSlotFileAtomic::Exists(F, P));
			F.Files.Add(P.Bak, Old);
			Check(TEXT("exists"), TEXT("bak_alone_true"), FMMOSlotFileAtomic::Exists(F, P));
			F.Files.Remove(P.Bak);
			F.Files.Add(P.Main, TornMain);
			Check(TEXT("exists"), TEXT("corrupt_main_true"), FMMOSlotFileAtomic::Exists(F, P));
		}
		{
			// D1: Exists = ana VEYA .bak VEYA GECERLI .tmp (Load ile tutarli). Yarim .tmp yetmez.
			const FMMOSlotValidator Strict(SlotAtomicTestValidator);
			FSlotAtomicTestFs F;
			F.Files.Add(P.Tmp, TmpValid);
			Check(TEXT("exists"), TEXT("valid_tmp_alone_true_with_validator_and_load_agrees"), FMMOSlotFileAtomic::Exists(F, P, &Strict) && SlotAtomicTestLoad(F, P).bOk);
			FSlotAtomicTestFs G;
			G.Files.Add(P.Tmp, TornTmp);
			Check(TEXT("exists"), TEXT("torn_tmp_alone_false_with_validator"), !FMMOSlotFileAtomic::Exists(G, P, &Strict) && !SlotAtomicTestLoad(G, P).bOk);
		}

		// --- Save: erken ret (hicbir dosyaya dokunulmaz) ---
		{
			FSlotAtomicTestFs F; F.Files.Add(P.Main, Old);
			const TMap<FString, TArray<uint8>> Before = F.Files;
			const FMMOSlotSaveResult R = SlotAtomicTestSave(F, P, FSlotAtomicTestBytes());
			Check(TEXT("save_reject"), TEXT("empty_bytes"), !R.bOk && R.Failure == EMMOSlotSaveFailure::BadSize && F.MutatingCalls == 0 && SlotAtomicTestSameFiles(F.Files, Before));
		}
		{
			FSlotAtomicTestFs F; F.Files.Add(P.Main, Old);
			FSlotAtomicTestBytes Big;
			Big.SetNumUninitialized(FMMOSlotFileAtomic::MaxBytes + 1);
			FMemory::Memset(Big.GetData(), 0, Big.Num());
			const FMMOSlotSaveResult R = FMMOSlotFileAtomic::Save(F, P, Big, [](const TArray<uint8>&) { return true; });
			Check(TEXT("save_reject"), TEXT("over_8mb_cap_even_if_validator_accepts"), !R.bOk && R.Failure == EMMOSlotSaveFailure::BadSize && F.MutatingCalls == 0 && F.Files.Num() == 1);
		}
		{
			FSlotAtomicTestFs F;
			const FSlotAtomicTestBytes AtCap = SlotAtomicTestValid(7, FMMOSlotFileAtomic::MaxBytes - 6);
			const FMMOSlotSaveResult R = SlotAtomicTestSave(F, P, AtCap);
			const FMMOSlotLoadResult L = SlotAtomicTestLoad(F, P);
			Check(TEXT("save_reject"), TEXT("exactly_8mb_is_accepted"), AtCap.Num() == FMMOSlotFileAtomic::MaxBytes && R.bOk && L.bOk && L.Source == EMMOSlotLoadSource::Main && L.Bytes == AtCap);
		}
		{
			FSlotAtomicTestFs F; F.Files.Add(P.Main, Old);
			const TMap<FString, TArray<uint8>> Before = F.Files;
			const FMMOSlotSaveResult R = SlotAtomicTestSave(F, P, TornMain);
			Check(TEXT("save_reject"), TEXT("invalid_bytes_rejected_by_validator"), !R.bOk && R.Failure == EMMOSlotSaveFailure::InvalidBytes && F.MutatingCalls == 0 && SlotAtomicTestSameFiles(F.Files, Before));
		}
		{
			// S6.2 QA-2: kodun urettigi baytlar siki (.tmp) dogrulayicidan gecmezse hicbir dosyaya dokunulmaz.
			FSlotAtomicTestFs F; F.Files.Add(P.Main, Old);
			const TMap<FString, TArray<uint8>> Before = F.Files;
			const auto RejectAll = [](const TArray<uint8>&) { return false; };
			const FMMOSlotValidator Strict(RejectAll);
			const FMMOSlotSaveResult R = FMMOSlotFileAtomic::Save(F, P, NewD, SlotAtomicTestValidator, &Strict);
			Check(TEXT("save_reject"), TEXT("noncanonical_bytes_rejected_before_any_write"), !R.bOk && R.Failure == EMMOSlotSaveFailure::NonCanonical && F.MutatingCalls == 0 && SlotAtomicTestSameFiles(F.Files, Before));
		}
		{
			FSlotAtomicTestFs F; F.Files.Add(P.Main, Old);
			const auto AcceptAll = [](const TArray<uint8>&) { return true; };
			const FMMOSlotValidator Strict(AcceptAll);
			const FMMOSlotSaveResult R = FMMOSlotFileAtomic::Save(F, P, NewD, SlotAtomicTestValidator, &Strict);
			Check(TEXT("save_reject"), TEXT("strict_validator_accepting_lets_save_proceed"), R.bOk && F.Files.FindRef(P.Main) == NewD);
		}
		{
			FSlotAtomicTestFs F; F.Files.Add(P.Main, Old); F.Files.Add(P.Bak, Older);
			F.Unreadable.Add(P.Main);
			const TMap<FString, TArray<uint8>> Before = F.Files;
			const FMMOSlotSaveResult R = SlotAtomicTestSave(F, P, NewD);
			Check(TEXT("save_reject"), TEXT("unreadable_main_fails_closed_untouched"), !R.bOk && R.Failure == EMMOSlotSaveFailure::MainUnreadable && F.MutatingCalls == 0 && SlotAtomicTestSameFiles(F.Files, Before));
		}
		{
			FSlotAtomicTestFs F;
			F.Files.Add(P.Main, TornMain); F.Files.Add(P.Bak, Old);
			for (int32 i = 0; i < FMMOSlotFileAtomic::MaxBadFiles; ++i) F.Files.Add(P.BadSlot(i), TornTmp);
			const TMap<FString, TArray<uint8>> Before = F.Files;
			const FMMOSlotSaveResult R = SlotAtomicTestSave(F, P, NewD);
			Check(TEXT("save_reject"), TEXT("bad_cap_full_fails_closed_untouched"), !R.bOk && R.Failure == EMMOSlotSaveFailure::BadCapReached && F.MutatingCalls == 0 && SlotAtomicTestSameFiles(F.Files, Before));
		}

		// --- Save: enjekte hata sonuclari ---
		{
			// G-ORTA-1: kodun kendi salt-okunur .tmp'si SILINIR; Save basarili olur.
			FSlotAtomicTestFs F; F.Files.Add(P.Main, Old); F.Files.Add(P.Tmp, TornTmp);
			F.ReadOnly.Add(P.Tmp);
			const FMMOSlotSaveResult R = SlotAtomicTestSave(F, P, NewD);
			Check(TEXT("inject"), TEXT("readonly_tmp_is_deleted_and_save_succeeds"), R.bOk && F.Files.FindRef(P.Main) == NewD && !F.Files.Contains(P.Tmp) && !F.ReadOnly.Contains(P.Tmp));
		}
		{
			// Bayat (eski) .bak salt-okunursa Move(Replace) hedefi silemezdi; Save once onu kaldirir (kendi dosyamiz).
			FSlotAtomicTestFs F; F.Files.Add(P.Main, Old); F.Files.Add(P.Bak, Older);
			F.ReadOnly.Add(P.Bak);
			const FMMOSlotSaveResult R = SlotAtomicTestSave(F, P, NewD);
			Check(TEXT("inject"), TEXT("readonly_stale_bak_replaced_save_succeeds"), R.bOk && F.Files.FindRef(P.Main) == NewD && !F.Files.Contains(P.Bak));
		}
		{
			// Salt-okunur ANA dosya (eski duman testi enjeksiyonu): iki Save ust uste de basarili, veri kaybi yok, .bak kalmaz.
			FSlotAtomicTestFs F; F.Files.Add(P.Main, Old);
			F.ReadOnly.Add(P.Main);
			const FMMOSlotSaveResult R1 = SlotAtomicTestSave(F, P, NewD);
			const FMMOSlotSaveResult R2 = SlotAtomicTestSave(F, P, Newer);
			Check(TEXT("inject"), TEXT("readonly_main_two_saves_both_succeed_no_stale_bak"), R1.bOk && !R1.bStaleBakLeft && R2.bOk && !R2.bStaleBakLeft && F.Files.FindRef(P.Main) == Newer && F.Files.Num() == 1);
		}
		{
			// Ana->.bak tasimasindan sonra .bak'in salt-okunur biti temizlenir (bayat .bak silinemese bile sonraki Save kilitlenmesin).
			FSlotAtomicTestFs F; F.Files.Add(P.Main, Old);
			F.ReadOnly.Add(P.Main);
			F.FailOps.Add(4); // .bak silme basarisiz -> bayat .bak kalir
			const FMMOSlotSaveResult R = SlotAtomicTestSave(F, P, NewD);
			Check(TEXT("inject"), TEXT("moved_readonly_main_clears_readonly_bit_on_bak"), R.bOk && R.bStaleBakLeft && F.Files.FindRef(P.Bak) == Old && !F.ReadOnly.Contains(P.Bak));
		}
		{
			// Gercekten silinemeyen (kilitli) .tmp: dogrudan yazim YOK, Save false.
			FSlotAtomicTestFs F; F.Files.Add(P.Main, Old); F.Files.Add(P.Tmp, TornTmp);
			F.Locked.Add(P.Tmp);
			const TMap<FString, TArray<uint8>> Before = F.Files;
			const FMMOSlotSaveResult R = SlotAtomicTestSave(F, P, NewD);
			Check(TEXT("inject"), TEXT("locked_tmp_fails_without_direct_write"), !R.bOk && R.Failure == EMMOSlotSaveFailure::TmpDeleteFailed && SlotAtomicTestSameFiles(F.Files, Before));
		}
		{
			// Kanitlar (ana dosya, .bad) HICBIR yolda bEvenReadOnly ile silinmeye calisilmaz; yalniz .tmp ve .bak.
			bool bScoped = true;
			FSlotAtomicTestFs Sets[4];
			Sets[0].Files.Add(P.Main, Old); Sets[0].Files.Add(P.Bak, Older); Sets[0].Files.Add(P.Tmp, TornTmp);
			Sets[1].Files.Add(P.Main, TornMain); Sets[1].Files.Add(P.Bak, Old); Sets[1].Files.Add(P.BadSlot(0), TornTmp);
			Sets[2].Files.Add(P.Main, TornMain); Sets[2].Files.Add(P.Bak, TornBak);
			Sets[3].Files.Add(P.Tmp, TmpValid);
			for (FSlotAtomicTestFs& F : Sets)
			{
				SlotAtomicTestSave(F, P, NewD);
				for (const FString& Deleted : F.EvenRoDeleted) { if (!(Deleted == P.Tmp || Deleted == P.Bak)) bScoped = false; }
			}
			Check(TEXT("inject"), TEXT("even_readonly_delete_scope_is_only_tmp_and_bak"), bScoped);
		}
		{
			FSlotAtomicTestFs F; F.Files.Add(P.Main, Old);
			F.bCorruptWrites = true;
			const FMMOSlotSaveResult R = SlotAtomicTestSave(F, P, NewD);
			const FMMOSlotLoadResult L = SlotAtomicTestLoad(F, P);
			Check(TEXT("inject"), TEXT("readback_mismatch_fails_keeps_old_cleans_tmp"), !R.bOk && R.Failure == EMMOSlotSaveFailure::TmpReadbackMismatch && L.bOk && L.Bytes == Old && !F.Files.Contains(P.Tmp) && !F.Files.Contains(P.Bak));
		}
		{
			FSlotAtomicTestFs F; F.Files.Add(P.Main, Old);
			F.FailTornOps.Add(1);
			const FMMOSlotSaveResult R = SlotAtomicTestSave(F, P, NewD);
			Check(TEXT("inject"), TEXT("torn_tmp_write_failure_cleans_tmp_keeps_main"), !R.bOk && R.Failure == EMMOSlotSaveFailure::TmpWriteFailed && !F.Files.Contains(P.Tmp) && F.Files.FindRef(P.Main) == Old);
		}
		{
			FSlotAtomicTestFs F; F.Files.Add(P.Main, Old);
			F.FailOps.Add(2); // .ana -> .bak
			const FMMOSlotSaveResult R = SlotAtomicTestSave(F, P, NewD);
			Check(TEXT("inject"), TEXT("rotate_failure_keeps_main_in_place"), !R.bOk && R.Failure == EMMOSlotSaveFailure::RotateFailed && F.Files.FindRef(P.Main) == Old && !F.Files.Contains(P.Tmp) && !F.Files.Contains(P.Bak));
		}
		{
			FSlotAtomicTestFs F; F.Files.Add(P.Main, Old);
			F.FailOps.Add(3); // .tmp -> ana
			const FMMOSlotSaveResult R = SlotAtomicTestSave(F, P, NewD);
			Check(TEXT("inject"), TEXT("promote_failure_puts_old_back_as_main"), !R.bOk && R.Failure == EMMOSlotSaveFailure::PromoteFailed && F.Files.FindRef(P.Main) == Old && !F.Files.Contains(P.Tmp) && !F.Files.Contains(P.Bak));
		}
		{
			FSlotAtomicTestFs F; F.Files.Add(P.Main, Old);
			F.FailOps.Add(4); // .bak silme
			const FMMOSlotSaveResult R = SlotAtomicTestSave(F, P, NewD);
			const FMMOSlotLoadResult L = SlotAtomicTestLoad(F, P);
			Check(TEXT("inject"), TEXT("stale_bak_delete_failure_still_ok_and_reported"), R.bOk && R.bStaleBakLeft && L.bOk && L.Source == EMMOSlotLoadSource::Main && L.Bytes == NewD && F.Files.FindRef(P.Bak) == Old);
		}
		{
			FSlotAtomicTestFs F; F.Files.Add(P.Main, Old); F.Files.Add(P.Tmp, TornTmp);
			F.FailOps.Add(1); F.FailOps.Add(2); // yeni .tmp yazilamadi...
			F.FailOps.Reset();
			F.FailOps.Add(2); // 1: eski .tmp silme, 2: yeni .tmp yazma (hata)
			const FMMOSlotSaveResult R = SlotAtomicTestSave(F, P, NewD);
			Check(TEXT("inject"), TEXT("write_failure_after_stale_tmp_cleanup_keeps_main"), !R.bOk && R.Failure == EMMOSlotSaveFailure::TmpWriteFailed && F.Files.FindRef(P.Main) == Old);
		}

		// --- Bozuk dosya .bad'e ayrilir, silinmez ---
		{
			FSlotAtomicTestFs F; F.Files.Add(P.Main, TornMain); F.Files.Add(P.BadSlot(0), TornTmp);
			const FMMOSlotSaveResult R = SlotAtomicTestSave(F, P, NewD);
			Check(TEXT("bad_files"), TEXT("existing_bad_gets_next_slot_and_is_not_overwritten"), R.bOk && F.Files.FindRef(P.BadSlot(0)) == TornTmp && F.Files.FindRef(P.BadSlot(1)) == TornMain && F.Files.FindRef(P.Main) == NewD);
		}
		{
			FSlotAtomicTestFs F; F.Files.Add(P.Main, TornMain); F.Files.Add(P.Bak, TornBak);
			const FMMOSlotSaveResult R = SlotAtomicTestSave(F, P, NewD);
			Check(TEXT("bad_files"), TEXT("both_corrupt_both_kept_in_separate_slots"), R.bOk && F.Files.FindRef(P.BadSlot(0)) == TornBak && F.Files.FindRef(P.BadSlot(1)) == TornMain && F.Files.FindRef(P.Main) == NewD && !F.Files.Contains(P.Bak));
		}
		{
			FSlotAtomicTestFs F; F.Files.Add(P.Main, TornMain); F.Files.Add(P.Bak, Old);
			const FMMOSlotLoadResult Before = SlotAtomicTestLoad(F, P);
			const FMMOSlotSaveResult R = SlotAtomicTestSave(F, P, NewD);
			const FMMOSlotLoadResult After = SlotAtomicTestLoad(F, P);
			Check(TEXT("bad_files"), TEXT("repair_happens_on_next_save_not_on_load"), Before.bOk && Before.Source == EMMOSlotLoadSource::Bak && R.bOk && After.Source == EMMOSlotLoadSource::Main && After.Bytes == NewD && F.Files.FindRef(P.BadSlot(0)) == TornMain && !F.Files.Contains(P.Bak));
		}

		// --- Art arda iki Save ---
		{
			FSlotAtomicTestFs F; F.Files.Add(P.Main, Old);
			const FMMOSlotSaveResult R1 = SlotAtomicTestSave(F, P, NewD);
			const FMMOSlotSaveResult R2 = SlotAtomicTestSave(F, P, Newer);
			const FMMOSlotLoadResult L = SlotAtomicTestLoad(F, P);
			Check(TEXT("two_saves"), TEXT("second_wins_and_only_main_remains"), R1.bOk && R2.bOk && L.bOk && L.Bytes == Newer && F.Files.Num() == 1 && F.Files.Contains(P.Main));
		}

		// --- Sahte FS modeli: Move(Replace) "once hedefi sil" penceresi (motor kaynagi FileManagerGeneric.cpp:299-323) ---
		{
			FSlotAtomicTestFs F; F.Files.Add(TEXT("D"), Old); F.Files.Add(TEXT("S"), NewD);
			F.CrashAt = 1;
			F.Move(TEXT("D"), TEXT("S"));
			Check(TEXT("fake_model"), TEXT("move_replace_deletes_dest_before_rename"), !F.Files.Contains(TEXT("D")) && F.Files.FindRef(TEXT("S")) == NewD && F.OpCount == 1);
		}
		{
			FSlotAtomicTestFs F; F.Files.Add(TEXT("S"), NewD);
			F.Move(TEXT("D"), TEXT("S"));
			Check(TEXT("fake_model"), TEXT("move_to_absent_dest_is_single_rename"), F.OpCount == 1 && F.Files.FindRef(TEXT("D")) == NewD && !F.Files.Contains(TEXT("S")));
		}
		{
			FSlotAtomicTestFs F; F.Files.Add(TEXT("D"), Old); F.Files.Add(TEXT("S"), NewD);
			F.ReadOnly.Add(TEXT("D"));
			const bool bMoved = F.Move(TEXT("D"), TEXT("S"));
			Check(TEXT("fake_model"), TEXT("readonly_dest_makes_replace_fail_both_files_kept"), !bMoved && F.Files.FindRef(TEXT("D")) == Old && F.Files.FindRef(TEXT("S")) == NewD);
		}
		{
			FSlotAtomicTestFs F; F.Files.Add(TEXT("S"), NewD);
			F.ReadOnly.Add(TEXT("S"));
			F.Move(TEXT("D"), TEXT("S"));
			const bool bCarried = F.ReadOnly.Contains(TEXT("D")) && !F.ReadOnly.Contains(TEXT("S"));
			F.ClearReadOnly(TEXT("D"));
			Check(TEXT("fake_model"), TEXT("rename_carries_readonly_bit_and_clear_removes_it"), bCarried && !F.ReadOnly.Contains(TEXT("D")) && F.OpCount == 1);
		}

		// --- Yuva adi savunmasi (yol gecisi) ---
		{
			Check(TEXT("slot_name"), TEXT("valid_names_accepted"),
				FMMOSlotFileAtomic::IsValidSlotName(TEXT("MMOSave_Ali")) && FMMOSlotFileAtomic::IsValidSlotName(TEXT("MMOSave_Ali Veli")) && FMMOSlotFileAtomic::IsValidSlotName(TEXT("MMOSave_a-b_c9")));
			Check(TEXT("slot_name"), TEXT("traversal_rejected"),
				!FMMOSlotFileAtomic::IsValidSlotName(TEXT("MMOSave_..\\x")) && !FMMOSlotFileAtomic::IsValidSlotName(TEXT("MMOSave_../x")) && !FMMOSlotFileAtomic::IsValidSlotName(TEXT("../MMOSave_x")) && !FMMOSlotFileAtomic::IsValidSlotName(TEXT("MMOSave_a/b")));
			Check(TEXT("slot_name"), TEXT("colon_and_dot_rejected"), !FMMOSlotFileAtomic::IsValidSlotName(TEXT("MMOSave_a:b")) && !FMMOSlotFileAtomic::IsValidSlotName(TEXT("MMOSave_a.b")));
			Check(TEXT("slot_name"), TEXT("non_ascii_rejected"), !FMMOSlotFileAtomic::IsValidSlotName(FString(TEXT("MMOSave_")) + FString::Chr(0x00D6) + TEXT("mer")));
			Check(TEXT("slot_name"), TEXT("empty_and_prefix_only_rejected"), !FMMOSlotFileAtomic::IsValidSlotName(TEXT("")) && !FMMOSlotFileAtomic::IsValidSlotName(TEXT("MMOSave_")));
			Check(TEXT("slot_name"), TEXT("prefix_is_case_sensitive"), !FMMOSlotFileAtomic::IsValidSlotName(TEXT("mmosave_Ali")) && !FMMOSlotFileAtomic::IsValidSlotName(TEXT("Ali")));
			Check(TEXT("slot_name"), TEXT("trailing_space_rejected"), !FMMOSlotFileAtomic::IsValidSlotName(TEXT("MMOSave_Ali ")));
			Check(TEXT("slot_name"), TEXT("control_char_rejected"), !FMMOSlotFileAtomic::IsValidSlotName(FString(TEXT("MMOSave_A")) + FString::Chr(0x0A) + TEXT("B")));
			Check(TEXT("slot_name"), TEXT("over_128_chars_rejected"), !FMMOSlotFileAtomic::IsValidSlotName(FString(TEXT("MMOSave_")) + FString::ChrN(121, TEXT('a'))));
			Check(TEXT("slot_name"), TEXT("length_boundary_128_accepted_129_rejected"), FMMOSlotFileAtomic::IsValidSlotName(FString(TEXT("MMOSave_")) + FString::ChrN(120, TEXT('a'))) && !FMMOSlotFileAtomic::IsValidSlotName(FString(TEXT("MMOSave_")) + FString::ChrN(121, TEXT('a'))));
			Check(TEXT("slot_name"), TEXT("char_class_boundaries_and_exact_prefix"),
				FMMOSlotFileAtomic::IsValidSlotName(TEXT("MMOSave_AZaz09")) && !FMMOSlotFileAtomic::IsValidSlotName(TEXT("MMOSave_@")) && !FMMOSlotFileAtomic::IsValidSlotName(TEXT("MMOSave_["))
				&& !FMMOSlotFileAtomic::IsValidSlotName(TEXT("MMOSave_`")) && !FMMOSlotFileAtomic::IsValidSlotName(TEXT("MMOSave_{")) && !FMMOSlotFileAtomic::IsValidSlotName(TEXT("MMOSave_/")) && !FMMOSlotFileAtomic::IsValidSlotName(TEXT("MMOSave_:"))
				&& !FMMOSlotFileAtomic::IsValidSlotName(TEXT("MMOSaveAli")) && !FMMOSlotFileAtomic::IsValidSlotName(TEXT("MMOSav_Ali")));
		}

		UE_LOG(LogTemp, Display, TEXT("[MMO_SLOTATOMIC_MATRIX_TOTAL] scenarios=%d crash_points=%d fail_runs=%d fail2_runs=%d"), Scenarios.Num(), TotalCrash, TotalFail, TotalFail2);

		if (OutChecks != SlotAtomicTestExpectedChecks) { ++Failures; OutError += TEXT(" check_count_mismatch"); }
		if (Failures) OutError = FString::Printf(TEXT("failures=%d first=%s"), Failures, *OutError);
		return Failures == 0;
	}

	struct FMMOSlotAtomicTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestSlotAtomic"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = SlotAtomicTestRunSelfTest(Error, Checks);
			Ar.Logf(TEXT("[MMO_SLOTATOMIC_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOSlotAtomicTestExec GMMOSlotAtomicTestExec;
}
#endif

// Kapsam notu: yalniz saf mantik (sahte dosya sistemi). Gercek IFileManager baglantisi, LoadGameFromMemory dogrulayicisi, loglar,
// magaza entegrasyonu, yedek araci siniflandirmasi ve duman testi enjeksiyonu S6.1-S6.3'tedir; burada kanitlanmaz.

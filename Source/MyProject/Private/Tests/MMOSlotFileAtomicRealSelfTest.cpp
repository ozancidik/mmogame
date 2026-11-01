// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOSlotFileAtomic.h"

#if !UE_BUILD_SHIPPING
#include "Save/MMOLocalFileCharacterStore.h"
#include "Save/MMOSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/CoreMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

// Karakter .sav atomik yazim GERCEK dosya sistemi testi (docs/DESIGN_CHARACTER_SAVE_ATOMIC.md 6.1, dilim S6.1).
// FMMOFileManagerSlotOps (IFileManager) + FMMOLocalFileCharacterStore (gercek UMMOSaveGame serilestirmesi) UCTAN UCA.
// YALNIZ ProjectIntermediateDir/MMOSlotAtomicRealTest/<guid> gecici dizininde calisir; gercek Saved/ klasorune YAZMAZ/OKUMAZ; test sonunda dizin silinir.
// Her Check benzersiz case/check cifti basar. Komut: MMOTestSlotAtomicReal. Kosucu: Scripts/TestSlotFileAtomicReal.ps1.
namespace
{
	constexpr int32 SlotRealTestExpectedChecks = 62;

	// Basilan log satirlarini yakalar (metrik/log satiri testi).
	class FSlotRealLogCapture : public FOutputDevice
	{
	public:
		FString Text;
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type, const FName&) override { Text += V; Text += TEXT("\n"); }
	};

	FMMOCharacterSaveData SlotRealSample(int32 Gold, int32 Level = 7)
	{
		FMMOCharacterSaveData D;
		D.bValid = true;
		D.Level = Level;
		D.Experience = 123456;
		D.Gold = Gold;
		D.Strength = 33;
		FMMOItemStack S; S.ItemId = TEXT("Potion_Small"); S.Count = 4; D.Items.Add(S);
		FMMOEquipmentInstance E; E.InstanceId = 5; E.ItemId = TEXT("Sword_Iron"); E.Plus = 2; D.EquipmentBag.Add(E);
		D.NextInstanceId = 6;
		D.LearnedSkills.Add(TEXT("Skill_A")); D.LearnedSkills.Add(TEXT("Skill_B"));
		D.SkillCooldownExpiryUtcSeconds.Add(TEXT("Skill_A"), 1700000000LL);
		D.SkillCooldownExpiryUtcSeconds.Add(TEXT("Skill_B"), 1700000999LL);
		D.CompletedQuests.Add(TEXT("Q_One"));
		D.ClanName = TEXT("TestClan");
		D.Friends.Add(TEXT("Ali")); D.Friends.Add(TEXT("Veli"));
		D.MapName = TEXT("Summer_scene");
		D.Location = FVector(1.5, -2.25, 100.125);
		D.Rotation = FRotator(0.0, 90.0, 0.0);
		return D;
	}

	bool SlotRealBytes(const FMMOCharacterSaveData& D, TArray<uint8>& Out) { return FMMOLocalFileCharacterStore::SerializeBytes(D, Out); }

	bool SlotRealSameData(const FMMOCharacterSaveData& A, const FMMOCharacterSaveData& B)
	{
		TArray<uint8> BA, BB;
		return SlotRealBytes(A, BA) && SlotRealBytes(B, BB) && BA == BB;
	}

	TMap<FString, TArray<uint8>> SlotRealSnapshot(const FString& Dir)
	{
		TMap<FString, TArray<uint8>> M;
		TArray<FString> Names;
		IFileManager::Get().FindFiles(Names, *FPaths::Combine(Dir, TEXT("*")), true, false);
		for (const FString& N : Names)
		{
			TArray<uint8> B;
			FFileHelper::LoadFileToArray(B, *FPaths::Combine(Dir, N));
			M.Add(N, MoveTemp(B));
		}
		return M;
	}

	bool SlotRealSameSnapshot(const TMap<FString, TArray<uint8>>& A, const TMap<FString, TArray<uint8>>& B)
	{
		if (A.Num() != B.Num()) return false;
		for (const TPair<FString, TArray<uint8>>& P : A)
		{
			const TArray<uint8>* O = B.Find(P.Key);
			if (!O || *O != P.Value) return false;
		}
		return true;
	}

	TArray<FString> SlotRealNames(const FString& Dir)
	{
		TArray<FString> Names;
		IFileManager::Get().FindFiles(Names, *FPaths::Combine(Dir, TEXT("*")), true, false);
		Names.Sort();
		return Names;
	}

	void SlotRealSetReadOnly(const FString& Path, bool bValue) { FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*Path, bValue); }

	void SlotRealClearReadOnlyTree(const FString& Dir)
	{
		for (const FString& N : SlotRealNames(Dir)) SlotRealSetReadOnly(FPaths::Combine(Dir, N), false);
	}

	bool SlotRealWrite(const FString& Path, const TArray<uint8>& B) { return FFileHelper::SaveArrayToFile(B, *Path); }

	TArray<uint8> SlotRealPrefix(const TArray<uint8>& Full, int32 Len)
	{
		TArray<uint8> B;
		B.Append(Full.GetData(), FMath::Clamp(Len, 0, Full.Num()));
		return B;
	}

	bool SlotRealRunSelfTest(FString& OutError, int32& OutChecks)
	{
		OutChecks = 0;
		OutError.Reset();
		int32 Failures = 0;
		const auto Check = [&](const TCHAR* Case, const TCHAR* Label, bool bOK)
		{
			++OutChecks;
			UE_LOG(LogTemp, Display, TEXT("[MMO_SLOTREAL_CASE] %s case=%s check=%s"), bOK ? TEXT("PASS") : TEXT("FAIL"), Case, Label);
			if (!bOK) { ++Failures; if (OutError.IsEmpty()) OutError = FString(Case) + TEXT("/") + Label; }
		};

		// Gecici dizin: YALNIZ Intermediate altinda; Saved'e hic dokunulmaz.
		const FString IntermediateRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir());
		const FString Root = FPaths::Combine(IntermediateRoot, TEXT("MMOSlotAtomicRealTest"));
		const FString Dir = FPaths::Combine(Root, FGuid::NewGuid().ToString(EGuidFormats::Digits));
		const FString SavedRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
		if (!Dir.StartsWith(IntermediateRoot) || Dir.StartsWith(SavedRoot))
		{
			OutError = TEXT("temp_dir_not_under_intermediate");
			return false;
		}
		IFileManager::Get().MakeDirectory(*Dir, true);
		const auto P = [&](const TCHAR* Slot, const TCHAR* Suffix = TEXT("")) { return FPaths::Combine(Dir, FString(Slot) + TEXT(".sav") + Suffix); };

		FMMOFileManagerSlotOps Ops;
		FMMOLocalFileCharacterStore Store(Dir);

		const FMMOCharacterSaveData SampleA = SlotRealSample(1000);
		const FMMOCharacterSaveData SampleB = SlotRealSample(2000);
		TArray<uint8> BytesA, BytesB;
		SlotRealBytes(SampleA, BytesA);
		SlotRealBytes(SampleB, BytesB);

		// --- Serilestirme / dogrulayici (R1) ---
		{
			FMMOCharacterSaveData Back;
			Check(TEXT("codec"), TEXT("roundtrip_fields_equal"), FMMOLocalFileCharacterStore::ParseBytes(BytesA, Back) && SlotRealSameData(Back, SampleA) && Back.Gold == 1000 && Back.SkillCooldownExpiryUtcSeconds.Num() == 2 && Back.Location == SampleA.Location);
			Check(TEXT("codec"), TEXT("full_bytes_canonical_and_main_valid"), FMMOLocalFileCharacterStore::IsCanonical(BytesA) && FMMOLocalFileCharacterStore::IsValidMainBytes(BytesA));
			FMMOCharacterSaveData Empty; // bValid=false, yeni karakter kaydi
			TArray<uint8> EmptyBytes;
			SlotRealBytes(Empty, EmptyBytes);
			Check(TEXT("codec"), TEXT("bvalid_false_full_record_is_valid_not_corrupt"), FMMOLocalFileCharacterStore::IsCanonical(EmptyBytes) && FMMOLocalFileCharacterStore::IsValidMainBytes(EmptyBytes));
			Check(TEXT("codec"), TEXT("empty_and_garbage_rejected"), !FMMOLocalFileCharacterStore::IsCanonical(TArray<uint8>()) && !FMMOLocalFileCharacterStore::IsValidMainBytes(TArray<uint8>()) && !FMMOLocalFileCharacterStore::IsValidMainBytes(TArray<uint8>({1, 2, 3, 4, 5, 6, 7, 8})));
			// Motorun kendisi bozuk basliga COKER (ilk 4 bayt sinif adi uzunlugu sanilir); HasSafeHeader bunu once eler.
			{
				TArray<uint8> Zeros; Zeros.SetNumZeroed(512); FMMOCharacterSaveData DummyOut;
				TArray<uint8> MagicGarbage = SlotRealPrefix(BytesA, 8); // GVAS + surum, sonrasi rastgele
				for (int32 i = 0; i < 64; ++i) MagicGarbage.Add(static_cast<uint8>(0xF0 + (i % 13)));
				TArray<uint8> BadMagic = BytesA; BadMagic[0] = 'X';
				TArray<uint8> WrongClass = BytesA;
				const FString ClassPath = UMMOSaveGame::StaticClass()->GetPathName();
				const FTCHARToUTF8 ClassUtf8(*ClassPath);
				int32 At = INDEX_NONE;
				for (int32 i = 0; i + ClassUtf8.Length() <= WrongClass.Num() && At == INDEX_NONE; ++i)
				{
					if (FMemory::Memcmp(WrongClass.GetData() + i, ClassUtf8.Get(), ClassUtf8.Length()) == 0) At = i;
				}
				if (At != INDEX_NONE) WrongClass[At + ClassUtf8.Length() - 1] = 'X';
				Check(TEXT("codec"), TEXT("all_zero_and_bad_magic_files_rejected_without_crash"), !FMMOLocalFileCharacterStore::HasSafeHeader(Zeros) && !FMMOLocalFileCharacterStore::ParseBytes(Zeros, DummyOut) && !FMMOLocalFileCharacterStore::HasSafeHeader(BadMagic));
				Check(TEXT("codec"), TEXT("magic_ok_garbage_rest_rejected_without_crash"), !FMMOLocalFileCharacterStore::HasSafeHeader(MagicGarbage) && !FMMOLocalFileCharacterStore::IsValidMainBytes(MagicGarbage) && !FMMOLocalFileCharacterStore::IsCanonical(MagicGarbage));
				Check(TEXT("codec"), TEXT("foreign_class_name_rejected_before_loadobject"), At != INDEX_NONE && !FMMOLocalFileCharacterStore::HasSafeHeader(WrongClass) && FMMOLocalFileCharacterStore::HasSafeHeader(BytesA));
			}
			Check(TEXT("codec"), TEXT("matches_engine_savegame_bytes_old_format"), [&]
			{
				UMMOSaveGame* SG = Cast<UMMOSaveGame>(UGameplayStatics::CreateSaveGameObject(UMMOSaveGame::StaticClass()));
				TArray<uint8> Engine;
				if (!SG) return false;
				SG->Data = SampleA;
				return UGameplayStatics::SaveGameToMemory(SG, Engine) && Engine == BytesA;
			}());

			// R1: her kesik onek (0..N-1 bayt) icin siki dogrulayici HICBIRINI kabul etmemeli. Gevsek ayristirici kabul edebilir (olculur, loglanir).
			int32 StrictAccepted = 0, LooseParsed = 0, LooseBValid = 0, MainAccepted = 0, StructAccepted = 0; FString AcceptedAt;
			for (int32 L = 0; L < BytesA.Num(); ++L)
			{
				const TArray<uint8> Prefix = SlotRealPrefix(BytesA, L);
				if (FMMOLocalFileCharacterStore::IsCanonical(Prefix)) ++StrictAccepted;
				if (FMMOLocalFileCharacterStore::HasSafeStructure(Prefix)) { ++StructAccepted; AcceptedAt += FString::Printf(TEXT(" %d"), L); }
				FMMOCharacterSaveData Tmp;
				if (FMMOLocalFileCharacterStore::ParseBytes(Prefix, Tmp)) { ++LooseParsed; if (Tmp.bValid) ++LooseBValid; }
				if (FMMOLocalFileCharacterStore::IsValidMainBytes(Prefix)) ++MainAccepted;
			}
			UE_LOG(LogTemp, Display, TEXT("[MMO_SLOTREAL_R1] bytes=%d prefixes=%d strict_accepted=%d loose_parse_ok=%d loose_bvalid_true=%d main_validator_accepted=%d structure_walk_accepted=%d at=[%s]"), BytesA.Num(), BytesA.Num(), StrictAccepted, LooseParsed, LooseBValid, MainAccepted, StructAccepted, *AcceptedAt);
			Check(TEXT("codec"), TEXT("truncated_prefixes_never_reach_engine_parser_structure_walk"), StructAccepted == 0 && LooseParsed == 0 && MainAccepted == 0 && BytesA.Num() > 400);
			Check(TEXT("codec"), TEXT("truncated_prefixes_never_canonical"), StrictAccepted == 0);
			// Tek bir bayt eksik / tek bir bayt fazla da kanonik degildir (kuyruk yapiskan dosya).
			TArray<uint8> Plus = BytesA; Plus.Add(0);
			Check(TEXT("codec"), TEXT("one_byte_short_and_one_byte_extra_not_canonical"), !FMMOLocalFileCharacterStore::IsCanonical(SlotRealPrefix(BytesA, BytesA.Num() - 1)) && !FMMOLocalFileCharacterStore::IsCanonical(Plus));
			Check(TEXT("codec"), TEXT("legacy_trailing_bytes_still_accepted_by_main_validator"), FMMOLocalFileCharacterStore::IsValidMainBytes(Plus));
		}

		// --- Gercek dosya sistemi islemleri ---
		{
			const FString A = FPaths::Combine(Dir, TEXT("ops_a.bin"));
			const FString B = FPaths::Combine(Dir, TEXT("ops_b.bin"));
			TArray<uint8> Data = BytesA;
			Check(TEXT("ops"), TEXT("write_read_roundtrip_and_exists"), !Ops.Exists(A) && Ops.WriteNew(A, Data) && Ops.Exists(A) && [&] { TArray<uint8> R; return Ops.ReadAll(A, R) && R == Data; }());
			Check(TEXT("ops"), TEXT("write_new_overwrites_existing"), Ops.WriteNew(A, BytesB) && [&] { TArray<uint8> R; return Ops.ReadAll(A, R) && R == BytesB; }());
			Check(TEXT("ops"), TEXT("move_to_absent_dest_single_rename"), Ops.Move(B, A) && !Ops.Exists(A) && Ops.Exists(B));
			Ops.WriteNew(A, BytesA);
			Check(TEXT("ops"), TEXT("move_replace_existing_dest_replaces_content"), Ops.Move(B, A) && !Ops.Exists(A) && [&] { TArray<uint8> R; return Ops.ReadAll(B, R) && R == BytesA; }());
			Check(TEXT("ops"), TEXT("move_missing_src_false"), !Ops.Move(A, FPaths::Combine(Dir, TEXT("ops_nope.bin"))));
			Check(TEXT("ops"), TEXT("delete_missing_true_existing_removes"), Ops.Delete(A) && Ops.Delete(B) && !Ops.Exists(B));
			Check(TEXT("ops"), TEXT("read_missing_false"), [&] { TArray<uint8> R; return !Ops.ReadAll(A, R); }());

			// Salt-okunur enjeksiyon (gercek dosya ozniteligi).
			Ops.WriteNew(A, BytesA);
			SlotRealSetReadOnly(A, true);
			Check(TEXT("ops"), TEXT("readonly_delete_fails_file_kept"), !Ops.Delete(A) && Ops.Exists(A));
			Check(TEXT("ops"), TEXT("readonly_write_fails_content_kept"), !Ops.WriteNew(A, BytesB) && [&] { TArray<uint8> R; return Ops.ReadAll(A, R) && R == BytesA; }());
			Ops.WriteNew(B, BytesB);
			const bool bMoveRo = Ops.Move(A, B);
			Check(TEXT("ops"), TEXT("readonly_dest_makes_replace_fail_both_kept"), !bMoveRo && Ops.Exists(B) && [&] { TArray<uint8> R; return Ops.ReadAll(A, R) && R == BytesA; }());
			SlotRealSetReadOnly(A, false);
			Ops.Delete(A); Ops.Delete(B);

			// Boyut tavani: 8 MB + 1 bayt dosya okunmaz.
			TArray<uint8> Big; Big.SetNumZeroed(FMMOSlotFileAtomic::MaxBytes + 1);
			Ops.WriteNew(A, Big);
			Check(TEXT("ops"), TEXT("read_over_cap_refused"), [&] { TArray<uint8> R; return !Ops.ReadAll(A, R) && R.Num() == 0; }());
			Ops.Delete(A);
		}

		// --- Magaza uctan uca ---
		{
			Check(TEXT("store"), TEXT("missing_slot_load_false_exists_false"), [&] { FMMOCharacterSaveData O; return !Store.Load(TEXT("MMOSave_RealA"), O) && !Store.Exists(TEXT("MMOSave_RealA")); }());
			Check(TEXT("store"), TEXT("save_new_then_load_equal_exists"), [&]
			{
				FMMOCharacterSaveData O;
				return Store.Save(TEXT("MMOSave_RealA"), SampleA) && Store.Exists(TEXT("MMOSave_RealA")) && Store.Load(TEXT("MMOSave_RealA"), O) && SlotRealSameData(O, SampleA);
			}());
			Check(TEXT("store"), TEXT("save_leaves_only_main_file"), SlotRealNames(Dir) == TArray<FString>({TEXT("MMOSave_RealA.sav")}));
			Check(TEXT("store"), TEXT("file_bytes_identical_to_old_engine_format"), [&] { TArray<uint8> F; return FFileHelper::LoadFileToArray(F, *P(TEXT("MMOSave_RealA"))) && F == BytesA; }());
			Check(TEXT("store"), TEXT("file_loads_via_engine_path_format_unchanged"), [&]
			{
				TArray<uint8> F;
				if (!FFileHelper::LoadFileToArray(F, *P(TEXT("MMOSave_RealA")))) return false;
				const UMMOSaveGame* SG = Cast<UMMOSaveGame>(UGameplayStatics::LoadGameFromMemory(F));
				return SG && SlotRealSameData(SG->Data, SampleA);
			}());
			Check(TEXT("store"), TEXT("second_save_replaces_and_leaves_single_file"), [&]
			{
				FMMOCharacterSaveData O;
				return Store.Save(TEXT("MMOSave_RealA"), SampleB) && Store.Load(TEXT("MMOSave_RealA"), O) && SlotRealSameData(O, SampleB) && SlotRealNames(Dir).Num() == 1;
			}());
			Check(TEXT("store"), TEXT("twenty_saves_stable_single_file_last_wins"), [&]
			{
				bool bOk = true;
				for (int32 i = 0; i < 20; ++i) bOk = bOk && Store.Save(TEXT("MMOSave_RealRep"), SlotRealSample(5000 + i));
				FMMOCharacterSaveData O;
				bOk = bOk && Store.Load(TEXT("MMOSave_RealRep"), O) && O.Gold == 5019;
				return bOk && SlotRealNames(Dir).Num() == 2; // RealA + RealRep
			}());
			Ops.Delete(P(TEXT("MMOSave_RealRep")));
		}

		// --- Eski (dogrudan yazilmis, sonek yok) dosyalar okunmaya devam eder ---
		{
			const FString Legacy = P(TEXT("MMOSave_RealLegacy"));
			// Eski magaza: SaveGameToSlot = SaveGameToMemory baytlari dogrudan son yola.
			UMMOSaveGame* SG = Cast<UMMOSaveGame>(UGameplayStatics::CreateSaveGameObject(UMMOSaveGame::StaticClass()));
			SG->Data = SampleA;
			TArray<uint8> LegacyBytes;
			UGameplayStatics::SaveGameToMemory(SG, LegacyBytes);
			SlotRealWrite(Legacy, LegacyBytes);
			FMMOCharacterSaveData O;
			Check(TEXT("legacy"), TEXT("direct_written_sav_loads_via_new_store"), Store.Load(TEXT("MMOSave_RealLegacy"), O) && SlotRealSameData(O, SampleA) && Store.Exists(TEXT("MMOSave_RealLegacy")));
			Check(TEXT("legacy"), TEXT("direct_written_sav_listed"), [&] { TArray<FString> L; Store.ListSlotNames(L); return L.Contains(TEXT("MMOSave_RealLegacy")); }());
			// Eski dosya + sonda fazladan bayt (kanonik degil): bValid=true oldugu icin gecerli kalir.
			TArray<uint8> Trailing = LegacyBytes; Trailing.Add(0); Trailing.Add(0);
			SlotRealWrite(Legacy, Trailing);
			Check(TEXT("legacy"), TEXT("noncanonical_bvalid_true_file_still_loads"), Store.Load(TEXT("MMOSave_RealLegacy"), O) && SlotRealSameData(O, SampleA));
			SlotRealWrite(Legacy, LegacyBytes);
			Check(TEXT("legacy"), TEXT("save_over_legacy_file_replaces_and_cleans_bak"), Store.Save(TEXT("MMOSave_RealLegacy"), SampleB) && Store.Load(TEXT("MMOSave_RealLegacy"), O) && SlotRealSameData(O, SampleB) && !Ops.Exists(P(TEXT("MMOSave_RealLegacy"), TEXT(".bak"))));
			Ops.Delete(Legacy);
		}

		// --- Kurtarma: Load yan etkisiz; ana bozuksa .bak ---
		{
			FSlotRealLogCapture Cap;
			GLog->AddOutputDevice(&Cap);
			const FString Main = P(TEXT("MMOSave_RealRec"));
			SlotRealWrite(P(TEXT("MMOSave_RealRec"), TEXT(".bak")), BytesA);
			SlotRealWrite(Main, SlotRealPrefix(BytesB, BytesB.Num() / 2));
			const TMap<FString, TArray<uint8>> Before = SlotRealSnapshot(Dir);
			FMMOCharacterSaveData O;
			const bool bLoaded = Store.Load(TEXT("MMOSave_RealRec"), O);
			const TMap<FString, TArray<uint8>> After = SlotRealSnapshot(Dir);
			GLog->Flush(); GLog->RemoveOutputDevice(&Cap);
			Check(TEXT("recover"), TEXT("torn_main_bak_valid_loads_bak"), bLoaded && SlotRealSameData(O, SampleA));
			Check(TEXT("recover"), TEXT("load_has_no_disk_side_effects"), SlotRealSameSnapshot(Before, After));
			Check(TEXT("recover"), TEXT("recovered_log_and_metric_emitted"), Cap.Text.Contains(TEXT("[MMOSAVE] event=recovered slot=MMOSave_RealRec source=bak")) && Cap.Text.Contains(TEXT("ev=save_recovered player=RealRec src=bak")));
			Check(TEXT("recover"), TEXT("repair_happens_on_next_save_evidence_kept"), [&]
			{
				const TArray<uint8> Torn = SlotRealPrefix(BytesB, BytesB.Num() / 2);
				FMMOCharacterSaveData R;
				return Store.Save(TEXT("MMOSave_RealRec"), SampleB) && Store.Load(TEXT("MMOSave_RealRec"), R) && SlotRealSameData(R, SampleB)
					&& [&] { TArray<uint8> Bad; return FFileHelper::LoadFileToArray(Bad, *P(TEXT("MMOSave_RealRec"), TEXT(".bad"))) && Bad == Torn; }()
					&& !Ops.Exists(P(TEXT("MMOSave_RealRec"), TEXT(".bak")));
			}());
		}
		{
			FSlotRealLogCapture Cap;
			GLog->AddOutputDevice(&Cap);
			SlotRealWrite(P(TEXT("MMOSave_RealCor")), SlotRealPrefix(BytesA, BytesA.Num() / 3));
			const TMap<FString, TArray<uint8>> Before = SlotRealSnapshot(Dir);
			FMMOCharacterSaveData O;
			const bool bLoaded = Store.Load(TEXT("MMOSave_RealCor"), O);
			GLog->Flush(); GLog->RemoveOutputDevice(&Cap);
			Check(TEXT("recover"), TEXT("torn_main_no_bak_load_false_and_file_untouched"), !bLoaded && SlotRealSameSnapshot(Before, SlotRealSnapshot(Dir)));
			Check(TEXT("recover"), TEXT("load_failed_corrupt_log_emitted"), Cap.Text.Contains(TEXT("[MMOSAVE] event=load_failed slot=MMOSave_RealCor reason=corrupt")));
			Check(TEXT("recover"), TEXT("exists_true_for_corrupt_main_so_caller_does_not_overwrite_blindly"), Store.Exists(TEXT("MMOSave_RealCor")));
		}
		{
			// Ilk-ever kayit yarida kesildi: yalniz .tmp var.
			SlotRealWrite(P(TEXT("MMOSave_RealTmp"), TEXT(".tmp")), SlotRealPrefix(BytesA, BytesA.Num() - 1));
			FMMOCharacterSaveData O;
			Check(TEXT("tmp"), TEXT("one_byte_short_tmp_only_is_not_loaded"), !Store.Load(TEXT("MMOSave_RealTmp"), O));
			SlotRealWrite(P(TEXT("MMOSave_RealTmp"), TEXT(".tmp")), SlotRealPrefix(BytesA, BytesA.Num() / 2));
			Check(TEXT("tmp"), TEXT("half_tmp_only_is_not_loaded"), !Store.Load(TEXT("MMOSave_RealTmp"), O));
			SlotRealWrite(P(TEXT("MMOSave_RealTmp"), TEXT(".tmp")), BytesA);
			Check(TEXT("tmp"), TEXT("complete_tmp_only_is_loaded_when_no_main_no_bak"), Store.Load(TEXT("MMOSave_RealTmp"), O) && SlotRealSameData(O, SampleA));
			Check(TEXT("tmp"), TEXT("tmp_alone_does_not_make_exists_true"), !Store.Exists(TEXT("MMOSave_RealTmp")));
			Check(TEXT("tmp"), TEXT("complete_tmp_promoted_to_bak_by_next_save_not_lost"), [&]
			{
				// Save adim 3a: Load'un verdigi tek kopya silinmez; yeni kayit basarili olunca bak temizlenir.
				FMMOCharacterSaveData R;
				return Store.Save(TEXT("MMOSave_RealTmp"), SampleB) && Store.Load(TEXT("MMOSave_RealTmp"), R) && SlotRealSameData(R, SampleB) && !Ops.Exists(P(TEXT("MMOSave_RealTmp"), TEXT(".tmp")));
			}());
			// Ana gecerliyken kesik .tmp yok sayilir.
			SlotRealWrite(P(TEXT("MMOSave_RealTmp"), TEXT(".tmp")), SlotRealPrefix(BytesA, 40));
			Check(TEXT("tmp"), TEXT("torn_tmp_beside_valid_main_ignored_main_wins"), Store.Load(TEXT("MMOSave_RealTmp"), O) && SlotRealSameData(O, SampleB));
			Check(TEXT("tmp"), TEXT("save_after_torn_tmp_discards_it"), Store.Save(TEXT("MMOSave_RealTmp"), SampleA) && Store.Load(TEXT("MMOSave_RealTmp"), O) && SlotRealSameData(O, SampleA) && !Ops.Exists(P(TEXT("MMOSave_RealTmp"), TEXT(".tmp"))));
		}
		{
			// .tmp SIKI (kanonik) denetlenir; ayni baytlar ANA dosya olarak (eski dosya tolerans kurali) yuklenir.
			TArray<uint8> Trailing = BytesA; Trailing.Add(0); Trailing.Add(0);
			SlotRealWrite(P(TEXT("MMOSave_RealStrict"), TEXT(".tmp")), Trailing);
			SlotRealWrite(P(TEXT("MMOSave_RealLoose")), Trailing);
			FMMOCharacterSaveData O;
			Check(TEXT("tmp"), TEXT("noncanonical_tmp_only_rejected_but_same_bytes_as_main_accepted"), !Store.Load(TEXT("MMOSave_RealStrict"), O) && Store.Load(TEXT("MMOSave_RealLoose"), O) && SlotRealSameData(O, SampleA));
		}
		{
			// Yalniz .bak: Exists true (yeni karakter yaratip .bak'i ezme).
			SlotRealWrite(P(TEXT("MMOSave_RealBak"), TEXT(".bak")), BytesA);
			FMMOCharacterSaveData O;
			Check(TEXT("exists"), TEXT("bak_alone_exists_true_and_loads"), Store.Exists(TEXT("MMOSave_RealBak")) && Store.Load(TEXT("MMOSave_RealBak"), O) && SlotRealSameData(O, SampleA));
		}

		// --- Gercek salt-okunur .tmp enjeksiyonu: Save basarisiz, eski veri korunur ---
		{
			FMMOCharacterSaveData O;
			Store.Save(TEXT("MMOSave_RealRo"), SampleA);
			const FString Tmp = P(TEXT("MMOSave_RealRo"), TEXT(".tmp"));
			SlotRealWrite(Tmp, TArray<uint8>({9, 9, 9}));
			SlotRealSetReadOnly(Tmp, true);
			FSlotRealLogCapture Cap;
			GLog->AddOutputDevice(&Cap);
			const bool bSaved = Store.Save(TEXT("MMOSave_RealRo"), SampleB);
			GLog->Flush(); GLog->RemoveOutputDevice(&Cap);
			Check(TEXT("inject"), TEXT("readonly_tmp_save_false_no_direct_write"), !bSaved && Cap.Text.Contains(TEXT("reason=tmp_delete_failed")));
			Check(TEXT("inject"), TEXT("readonly_tmp_old_data_still_loads_main_untouched"), Store.Load(TEXT("MMOSave_RealRo"), O) && SlotRealSameData(O, SampleA) && Ops.Exists(Tmp));
			SlotRealSetReadOnly(Tmp, false);
			Check(TEXT("inject"), TEXT("after_clearing_readonly_next_save_succeeds"), Store.Save(TEXT("MMOSave_RealRo"), SampleB) && Store.Load(TEXT("MMOSave_RealRo"), O) && SlotRealSameData(O, SampleB) && !Ops.Exists(Tmp));
		}
		{
			// Salt-okunur ANA dosya (eski duman testi enjeksiyonu): ilk Save yeniden adlandirmayla gecer, bayat .bak salt-okunur kalir;
			// ikinci Save .bak'i yerine koyamaz -> fail-closed, veri kaybi yok.
			FMMOCharacterSaveData O;
			Store.Save(TEXT("MMOSave_RealRoMain"), SampleA);
			SlotRealSetReadOnly(P(TEXT("MMOSave_RealRoMain")), true);
			const bool bFirst = Store.Save(TEXT("MMOSave_RealRoMain"), SampleB);
			const bool bLoadedFirst = Store.Load(TEXT("MMOSave_RealRoMain"), O) && SlotRealSameData(O, SampleB);
			const bool bSecond = Store.Save(TEXT("MMOSave_RealRoMain"), SlotRealSample(3000));
			const bool bLoadedSecond = Store.Load(TEXT("MMOSave_RealRoMain"), O) && SlotRealSameData(O, SampleB);
			UE_LOG(LogTemp, Display, TEXT("[MMO_SLOTREAL_OBS] readonly_main first_save=%d second_save=%d bak_exists=%d"), bFirst, bSecond, Ops.Exists(P(TEXT("MMOSave_RealRoMain"), TEXT(".bak"))));
			Check(TEXT("inject"), TEXT("readonly_main_first_save_ok_new_data_loads"), bFirst && bLoadedFirst);
			Check(TEXT("inject"), TEXT("readonly_main_second_save_fails_closed_without_data_loss"), !bSecond && bLoadedSecond);
			SlotRealClearReadOnlyTree(Dir);
		}

		// --- Bozuk dosya kotasi (.bad) ---
		{
			FSlotRealLogCapture Cap;
			GLog->AddOutputDevice(&Cap);
			const FString M = P(TEXT("MMOSave_RealCap"));
			const TArray<uint8> Torn = SlotRealPrefix(BytesA, BytesA.Num() / 2);
			SlotRealWrite(M, Torn);
			SlotRealWrite(M + TEXT(".bad"), TArray<uint8>({1}));
			for (int32 i = 1; i < FMMOSlotFileAtomic::MaxBadFiles; ++i) SlotRealWrite(M + FString::Printf(TEXT(".bad.%d"), i), TArray<uint8>({static_cast<uint8>(i + 1)}));
			const TMap<FString, TArray<uint8>> Before = SlotRealSnapshot(Dir);
			const bool bSaved = Store.Save(TEXT("MMOSave_RealCap"), SampleB);
			GLog->Flush(); GLog->RemoveOutputDevice(&Cap);
			Check(TEXT("bad_cap"), TEXT("quota_full_save_false_nothing_changed"), !bSaved && SlotRealSameSnapshot(Before, SlotRealSnapshot(Dir)));
			Check(TEXT("bad_cap"), TEXT("quota_full_metric_names_player"), Cap.Text.Contains(TEXT("ev=save_bad_cap player=RealCap")) && Cap.Text.Contains(TEXT("reason=bad_cap_reached")));
		}

		// --- Yuva adi savunmasi: yol gecisi / gecersiz ad dosya olusturmaz ---
		{
			const TMap<FString, TArray<uint8>> Before = SlotRealSnapshot(Dir);
			const TMap<FString, TArray<uint8>> ParentBefore = SlotRealSnapshot(Root);
			FMMOCharacterSaveData O;
			const TCHAR* Bad[] = {TEXT("MMOSave_..\\x"), TEXT("MMOSave_../x"), TEXT("../MMOSave_x"), TEXT("MMOSave_a:b"), TEXT("MMOSave_a.b"), TEXT("MMOSave_"), TEXT(""), TEXT("Other_Ali")};
			bool bAllRefused = true;
			for (const TCHAR* Name : Bad)
			{
				bAllRefused = bAllRefused && !Store.Save(Name, SampleA) && !Store.Load(Name, O) && !Store.Exists(Name);
			}
			Check(TEXT("slot_name"), TEXT("invalid_names_refused_by_all_three_calls"), bAllRefused);
			Check(TEXT("slot_name"), TEXT("invalid_names_create_no_files_anywhere"), SlotRealSameSnapshot(Before, SlotRealSnapshot(Dir)) && SlotRealSameSnapshot(ParentBefore, SlotRealSnapshot(Root)));
		}

		// --- Boyut tavani (8 MB): veri tavandan buyukse Save false, dosya olusmaz ---
		{
			FMMOCharacterSaveData Huge = SampleA;
			Huge.ClanName = FString::ChrN(5 * 1024 * 1024, static_cast<TCHAR>(0x0100)); // UTF-16: ~10 MB
			const bool bSaved = Store.Save(TEXT("MMOSave_RealHuge"), Huge);
			Check(TEXT("size_cap"), TEXT("over_8mb_save_false_no_files"), !bSaved && !Ops.Exists(P(TEXT("MMOSave_RealHuge"))) && !Ops.Exists(P(TEXT("MMOSave_RealHuge"), TEXT(".tmp"))));
		}

		// --- Listeleme: .tmp/.bak/.bad yuva sayilmaz ---
		{
			TArray<FString> L;
			Store.ListSlotNames(L);
			bool bOnlySav = true;
			for (const FString& N : L) bOnlySav = bOnlySav && N.StartsWith(TEXT("MMOSave_")) && !N.Contains(TEXT("."));
			Check(TEXT("list"), TEXT("sidecar_files_not_listed_as_slots"), bOnlySav && L.Contains(TEXT("MMOSave_RealA")) && !L.Contains(TEXT("MMOSave_RealBak")));
		}

		UE_LOG(LogTemp, Display, TEXT("[MMO_SLOTREAL_INFO] temp_dir_files_before_cleanup=%d"), SlotRealNames(Dir).Num());

		// Temizlik: yalniz kendi gecici dizini (salt-okunur kalmis olabilecekleri once ac).
		SlotRealClearReadOnlyTree(Dir);
		IFileManager::Get().DeleteDirectory(*Dir, false, true);
		Check(TEXT("cleanup"), TEXT("temp_dir_removed"), !IFileManager::Get().DirectoryExists(*Dir));
		TArray<FString> Left;
		IFileManager::Get().FindFiles(Left, *FPaths::Combine(Root, TEXT("*")), false, true);
		if (Left.Num() == 0) IFileManager::Get().DeleteDirectory(*Root, false, false); // bos kok dizin (baska kosu yoksa)

		if (OutChecks != SlotRealTestExpectedChecks) { ++Failures; OutError += TEXT(" check_count_mismatch"); }
		if (Failures) OutError = FString::Printf(TEXT("failures=%d first=%s"), Failures, *OutError);
		return Failures == 0;
	}

	struct FMMOSlotRealTestExec : public FSelfRegisteringExec
	{
		virtual bool Exec_Dev(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestSlotAtomicReal"))) return false;
			FString Error; int32 Checks = 0;
			const bool bPassed = SlotRealRunSelfTest(Error, Checks);
			Ar.Logf(TEXT("[MMO_SLOTREAL_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOSlotRealTestExec GMMOSlotRealTestExec;
}
#endif

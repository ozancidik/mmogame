// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOSlotFileAtomic.h"

#if !UE_BUILD_SHIPPING
#include "Save/MMOLocalFileCharacterStore.h"
#include "Save/MMOSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
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
	constexpr int32 SlotRealTestExpectedChecks = 100;

	// Basilan log satirlarini yakalar (metrik/log satiri testi). bSawError: Error seviyesinde en az bir satir geldi mi.
	class FSlotRealLogCapture : public FOutputDevice
	{
	public:
		FString Text;
		bool bSawError = false;
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName&) override
		{
			Text += V; Text += TEXT("\n");
			if (Verbosity <= ELogVerbosity::Error) bSawError = true;
		}
	};

	// WriteNew Flush/Tell hata benzetimi: gercek tutamagi sarar. Cikis isaretleri disaridan verilir (tutamak WriteNew icinde silinir).
	class FSlotRealFaultHandle : public IFileHandle
	{
	public:
		FSlotRealFaultHandle(IFileHandle* InInner, bool bInFailFlush, int64 InTellDelta, bool* InOutLastFlushWasFull)
			: Inner(InInner), bFailFlush(bInFailFlush), TellDelta(InTellDelta), OutLastFlushWasFull(InOutLastFlushWasFull) {}
		virtual ~FSlotRealFaultHandle() override { delete Inner; }
		virtual int64 Tell() override { return Inner->Tell() + TellDelta; }
		virtual bool Seek(int64 NewPosition) override { return Inner->Seek(NewPosition); }
		virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd = 0) override { return Inner->SeekFromEnd(NewPositionRelativeToEnd); }
		virtual bool Read(uint8* Destination, int64 BytesToRead) override { return Inner->Read(Destination, BytesToRead); }
		virtual bool ReadAt(uint8* Destination, int64 BytesToRead, int64 Offset) override { return Inner->ReadAt(Destination, BytesToRead, Offset); }
		virtual bool Write(const uint8* Source, int64 BytesToWrite) override { return Inner->Write(Source, BytesToWrite); }
		virtual bool Flush(const bool bFullFlush = false) override
		{
			if (OutLastFlushWasFull) *OutLastFlushWasFull = bFullFlush;
			return !bFailFlush && Inner->Flush(bFullFlush);
		}
		virtual bool Truncate(int64 NewSize) override { return Inner->Truncate(NewSize); }
	private:
		IFileHandle* Inner;
		bool bFailFlush;
		int64 TellDelta;
		bool* OutLastFlushWasFull;
	};

	// Gercek eski .sav baslik/etiket yerlesimi (5.7): dosyadaki sabit noktalarin ofsetleri. Test bunlari kasitli bozar.
	struct FSlotRealOffsets
	{
		int32 FormatOff = -1, CountOff = -1, CountVal = 0, ClassOff = -1, CtrlOff = -1, SizeOff = -1, FlagsOff = -1;
	};

	bool SlotRealFindOffsets(const TArray<uint8>& B, FSlotRealOffsets& O)
	{
		const auto I32 = [&B](int32 At, int32& V) { if (At < 0 || At + 4 > B.Num()) return false; FMemory::Memcpy(&V, B.GetData() + At, 4); return true; };
		const auto SkipString = [&](int32& At) { int32 Len = 0; if (!I32(At, Len)) return false; At += 4 + (Len < 0 ? -Len * 2 : Len); return At <= B.Num(); };
		int32 At = 26; // GVAS(4) surum(4) UE4(4) UE5(4) motor major/minor/patch(2x3) changelist(4)
		if (!SkipString(At)) return false; // motor branch
		O.FormatOff = At; At += 4;
		O.CountOff = At;
		if (!I32(At, O.CountVal)) return false;
		At += 4 + O.CountVal * 20;
		O.ClassOff = At;
		if (!SkipString(At)) return false;
		O.CtrlOff = At; At += 1;
		if (!SkipString(At)) return false; // ilk property adi
		int32 Remaining = 1;
		for (int32 Nodes = 0; Remaining > 0 && Nodes < 64; ++Nodes)
		{
			int32 Inner = 0;
			if (!SkipString(At) || !I32(At, Inner)) return false;
			At += 4;
			Remaining += Inner - 1;
		}
		O.SizeOff = At;
		O.FlagsOff = At + 4;
		return O.FlagsOff < B.Num();
	}

	TArray<uint8> SlotRealWithI32(TArray<uint8> B, int32 At, int32 V)
	{
		if (At >= 0 && At + 4 <= B.Num()) FMemory::Memcpy(B.GetData() + At, &V, 4);
		return B;
	}


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

	// Gercekci kayit (QA-2): 30 girdili TMap'te silinmis girisler (delikli) + bosluklari dolduran yeniler, cok sayida dizi ogesi.
	FMMOCharacterSaveData SlotRealBigSample(int32 Gold)
	{
		FMMOCharacterSaveData D = SlotRealSample(Gold, 42);
		for (int32 i = 0; i < 30; ++i)
		{
			D.SkillCooldownExpiryUtcSeconds.Add(FName(*FString::Printf(TEXT("Skill_%02d"), i)), 1700000000LL + i * 37LL);
		}
		for (int32 i = 0; i < 30; i += 3)
		{
			D.SkillCooldownExpiryUtcSeconds.Remove(FName(*FString::Printf(TEXT("Skill_%02d"), i))); // delikli sparse dizi
		}
		for (int32 i = 30; i < 34; ++i)
		{
			D.SkillCooldownExpiryUtcSeconds.Add(FName(*FString::Printf(TEXT("Skill_%02d"), i)), 1700001000LL + i); // bosluklari doldur
		}
		for (int32 i = 0; i < 20; ++i)
		{
			FMMOItemStack S; S.ItemId = FName(*FString::Printf(TEXT("Item_%02d"), i)); S.Count = 1 + i; D.Items.Add(S);
			D.LearnedSkills.Add(FName(*FString::Printf(TEXT("Learned_%02d"), i)));
		}
		for (int32 i = 0; i < 12; ++i)
		{
			FMMOEquipmentInstance E; E.InstanceId = 100 + i; E.ItemId = FName(*FString::Printf(TEXT("Gear_%02d"), i)); E.Plus = i % 9; D.EquipmentBag.Add(E);
		}
		for (int32 i = 0; i < 24; ++i)
		{
			FMMOEquipmentInstance E; E.InstanceId = 300 + i; E.ItemId = FName(*FString::Printf(TEXT("Stored_%02d"), i)); D.Warehouse.Add(E);
		}
		for (int32 i = 0; i < 30; ++i)
		{
			D.CompletedQuests.Add(FName(*FString::Printf(TEXT("Q_Done_%02d"), i)));
			D.Friends.Add(FString::Printf(TEXT("Friend%02d"), i));
			D.MasteryRanks.Add(i % 5);
		}
		FMMOQuestProgress Q; Q.QuestId = TEXT("Q_Active_1"); Q.KillProgress = 7; D.ActiveQuests.Add(Q);
		D.ClassChangeCooldownExpiryUtcSeconds = 1700005555LL;
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

		// --- QA-2: gercekci kayit (20+ girdili, silinmis girisli TMap) kanonik; VerifyRoundTrip bValid'i karsilastirir ---
		{
			const FMMOCharacterSaveData Big = SlotRealBigSample(5555);
			TArray<uint8> BigBytes;
			SlotRealBytes(Big, BigBytes);
			Check(TEXT("codec"), TEXT("realistic_record_big_tmap_with_holes_is_canonical_and_valid"), Big.SkillCooldownExpiryUtcSeconds.Num() >= 20 && BigBytes.Num() > 2000 && FMMOLocalFileCharacterStore::IsCanonical(BigBytes) && FMMOLocalFileCharacterStore::IsValidMainBytes(BigBytes) && FMMOLocalFileCharacterStore::HasSafeStructure(BigBytes));
			FMMOCharacterSaveData BigBack;
			Check(TEXT("codec"), TEXT("realistic_record_roundtrip_fields_equal"), FMMOLocalFileCharacterStore::ParseBytes(BigBytes, BigBack) && SlotRealSameData(BigBack, Big) && BigBack.SkillCooldownExpiryUtcSeconds.Num() == Big.SkillCooldownExpiryUtcSeconds.Num() && BigBack.Warehouse.Num() == 24 && BigBack.ActiveQuests.Num() == 1);
			FMMOCharacterSaveData NotValid; // bValid=false
			Check(TEXT("codec"), TEXT("verify_round_trip_compares_bvalid_and_rejects_garbage"), FMMOLocalFileCharacterStore::VerifyRoundTrip(SampleA, BytesA) && !FMMOLocalFileCharacterStore::VerifyRoundTrip(NotValid, BytesA) && !FMMOLocalFileCharacterStore::VerifyRoundTrip(SampleA, TArray<uint8>({1, 2, 3})));
		}

		// --- QA-9: yapi yuruyusu sinirlari (kasitli bozulmus kopyalar) ---
		{
			FSlotRealOffsets O;
			const bool bOffsets = SlotRealFindOffsets(BytesA, O) && O.CountVal >= 0 && O.SizeOff > O.CtrlOff;
			UE_LOG(LogTemp, Display, TEXT("[MMO_SLOTREAL_INFO] offsets format=%d count=%d(n=%d) class=%d ctrl=%d size=%d flags=%d"), O.FormatOff, O.CountOff, O.CountVal, O.ClassOff, O.CtrlOff, O.SizeOff, O.FlagsOff);
			Check(TEXT("walk"), TEXT("offsets_found_and_pristine_file_accepted"), bOffsets && FMMOLocalFileCharacterStore::HasSafeHeader(BytesA) && FMMOLocalFileCharacterStore::HasSafeStructure(BytesA));
			Check(TEXT("walk"), TEXT("version_out_of_range_rejected"), !FMMOLocalFileCharacterStore::HasSafeHeader(SlotRealWithI32(BytesA, 4, 1)) && !FMMOLocalFileCharacterStore::HasSafeHeader(SlotRealWithI32(BytesA, 4, 4)) && !FMMOLocalFileCharacterStore::HasSafeHeader(SlotRealWithI32(BytesA, 4, 99)));
			Check(TEXT("walk"), TEXT("legacy_custom_version_format_rejected"), bOffsets && !FMMOLocalFileCharacterStore::HasSafeHeader(SlotRealWithI32(BytesA, O.FormatOff, 0)));
			{
				// Ozel surum tablosu: 4096 kayit sinirda kabul, 4097 ret (CustomCount ust siniri).
				const auto WithCount = [&](int32 Count)
				{
					TArray<uint8> S;
					S.Append(BytesA.GetData(), O.CountOff);
					S.Append(reinterpret_cast<const uint8*>(&Count), 4);
					S.AddZeroed(Count * 20);
					S.Append(BytesA.GetData() + O.ClassOff, BytesA.Num() - O.ClassOff);
					return S;
				};
				Check(TEXT("walk"), TEXT("custom_count_4096_accepted_4097_rejected"), bOffsets && FMMOLocalFileCharacterStore::HasSafeStructure(WithCount(4096)) && !FMMOLocalFileCharacterStore::HasSafeHeader(WithCount(4097)));
			}
			{
				TArray<uint8> Ctrl = BytesA;
				if (bOffsets) Ctrl[O.CtrlOff] = 1;
				Check(TEXT("walk"), TEXT("nonzero_control_extension_rejected_by_structure_walk"), bOffsets && FMMOLocalFileCharacterStore::HasSafeHeader(Ctrl) && !FMMOLocalFileCharacterStore::HasSafeStructure(Ctrl));
			}
			{
				TArray<uint8> Ext = BytesA;
				if (bOffsets) Ext[O.FlagsOff] |= 0x04;
				Check(TEXT("walk"), TEXT("property_extension_flag_rejected"), bOffsets && !FMMOLocalFileCharacterStore::HasSafeStructure(Ext));
			}
			Check(TEXT("walk"), TEXT("property_size_beyond_file_rejected"), bOffsets && !FMMOLocalFileCharacterStore::HasSafeStructure(SlotRealWithI32(BytesA, O.SizeOff, 0x7FFFFFF0)));
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
			Check(TEXT("ops"), TEXT("delete_missing_true_existing_removes"), Ops.Delete(A, false) && Ops.Delete(B, false) && !Ops.Exists(B));
			Check(TEXT("ops"), TEXT("read_missing_false"), [&] { TArray<uint8> R; return !Ops.ReadAll(A, R); }());

			// Salt-okunur enjeksiyon (gercek dosya ozniteligi).
			Ops.WriteNew(A, BytesA);
			SlotRealSetReadOnly(A, true);
			Check(TEXT("ops"), TEXT("readonly_delete_fails_file_kept"), !Ops.Delete(A, false) && Ops.Exists(A));
			Check(TEXT("ops"), TEXT("readonly_write_fails_content_kept"), !Ops.WriteNew(A, BytesB) && [&] { TArray<uint8> R; return Ops.ReadAll(A, R) && R == BytesA; }());
			Ops.WriteNew(B, BytesB);
			const bool bMoveRo = Ops.Move(A, B);
			Check(TEXT("ops"), TEXT("readonly_dest_makes_replace_fail_both_kept"), !bMoveRo && Ops.Exists(B) && [&] { TArray<uint8> R; return Ops.ReadAll(A, R) && R == BytesA; }());
			SlotRealSetReadOnly(A, false);
			Ops.Delete(A, true); Ops.Delete(B, true);

			// Boyut tavani: 8 MB + 1 bayt dosya okunmaz.
			TArray<uint8> Big; Big.SetNumZeroed(FMMOSlotFileAtomic::MaxBytes + 1);
			Ops.WriteNew(A, Big);
			Check(TEXT("ops"), TEXT("read_over_cap_refused"), [&] { TArray<uint8> R; return !Ops.ReadAll(A, R) && R.Num() == 0; }());
			Ops.Delete(A, true);
		}

		// --- S6.2 ops: Delete(EvenReadOnly), ClearReadOnly, Move kaynak korumasi, yeniden deneme, WriteNew Flush/Tell ---
		{
			const FString A = FPaths::Combine(Dir, TEXT("ops2_a.bin"));
			const FString B = FPaths::Combine(Dir, TEXT("ops2_b.bin"));
			Ops.WriteNew(A, BytesA);
			SlotRealSetReadOnly(A, true);
			Check(TEXT("ops2"), TEXT("delete_even_readonly_removes_readonly_file"), Ops.Delete(A, true) && !Ops.Exists(A));
			Ops.WriteNew(A, BytesA);
			SlotRealSetReadOnly(A, true);
			Ops.ClearReadOnly(A);
			Check(TEXT("ops2"), TEXT("clear_readonly_makes_file_deletable_without_flag"), Ops.Delete(A, false) && !Ops.Exists(A));
			// QA-12: IFileManager::Move(Replace) kaynak yokken de ONCE hedefi siler; Ops katmani kaynak yoksa hic denemez.
			Ops.WriteNew(B, BytesB);
			const bool bMovedMissing = Ops.Move(B, FPaths::Combine(Dir, TEXT("ops2_nope.bin")));
			Check(TEXT("ops2"), TEXT("move_with_missing_source_keeps_existing_dest"), !bMovedMissing && [&] { TArray<uint8> R; return Ops.ReadAll(B, R) && R == BytesB; }());
			Ops.Delete(B, true);
		}
		{
			IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
			const FString A = FPaths::Combine(Dir, TEXT("ops3_a.bin"));
			const FString B = FPaths::Combine(Dir, TEXT("ops3_b.bin"));
			{
				FMMOFileManagerSlotOps R;
				Ops.WriteNew(A, BytesA);
				R.TestInjectFailure = [](const TCHAR* Op, const FString&, int32 Attempt) { return FCString::Strcmp(Op, TEXT("delete")) == 0 && Attempt < 2; };
				const bool bDel = R.Delete(A, true);
				Check(TEXT("ops3"), TEXT("delete_transient_failure_is_retried_until_success"), bDel && R.RetriesPerformed == 2 && !Ops.Exists(A));
			}
			{
				FMMOFileManagerSlotOps R;
				int32 Calls = 0;
				Ops.WriteNew(A, BytesA);
				R.TestInjectFailure = [&Calls](const TCHAR* Op, const FString&, int32) { if (FCString::Strcmp(Op, TEXT("delete")) == 0) { ++Calls; return true; } return false; };
				const bool bDel = R.Delete(A, true);
				Check(TEXT("ops3"), TEXT("delete_gives_up_after_exactly_three_attempts_file_kept"), !bDel && Calls == FMMOFileManagerSlotOps::RetryAttempts && R.RetriesPerformed == 2 && Ops.Exists(A));
			}
			{
				FMMOFileManagerSlotOps R;
				TArray<uint8> Out;
				R.TestInjectFailure = [](const TCHAR* Op, const FString&, int32 Attempt) { return FCString::Strcmp(Op, TEXT("read")) == 0 && Attempt < 1; };
				const bool bRead = R.ReadAll(A, Out);
				Check(TEXT("ops3"), TEXT("read_transient_failure_is_retried_content_intact"), bRead && R.RetriesPerformed == 1 && Out == BytesA);
			}
			{
				FMMOFileManagerSlotOps R;
				R.TestInjectFailure = [](const TCHAR* Op, const FString&, int32 Attempt) { return FCString::Strcmp(Op, TEXT("move")) == 0 && Attempt < 1; };
				const bool bMoved = R.Move(B, A);
				Check(TEXT("ops3"), TEXT("move_transient_failure_is_retried_then_renamed"), bMoved && R.RetriesPerformed == 1 && Ops.Exists(B) && !Ops.Exists(A));
			}
			{
				// Kalici durumlar yeniden denenmez: yok dosya, tavandan buyuk dosya, kaynagi olmayan Move.
				FMMOFileManagerSlotOps R;
				TArray<uint8> Out;
				const bool bMissing = R.ReadAll(FPaths::Combine(Dir, TEXT("ops3_missing.bin")), Out);
				TArray<uint8> Big; Big.SetNumZeroed(FMMOSlotFileAtomic::MaxBytes + 1);
				Ops.WriteNew(A, Big);
				const bool bOver = R.ReadAll(A, Out);
				const bool bMoveNoSrc = R.Move(B, FPaths::Combine(Dir, TEXT("ops3_missing.bin")));
				Check(TEXT("ops3"), TEXT("permanent_conditions_are_not_retried"), !bMissing && !bOver && !bMoveNoSrc && R.RetriesPerformed == 0);
				Ops.Delete(A, true);
			}
			{
				// Toplam bekleme butcesi: 6 kalici-hatali islem (12 deneme beklemesi) tek nesnede 200 ms'yi asmaz.
				FMMOFileManagerSlotOps R;
				Ops.WriteNew(A, BytesA);
				Ops.WriteNew(B, BytesB);
				R.TestInjectFailure = [](const TCHAR*, const FString&, int32) { return true; };
				TArray<uint8> Out;
				const double T0 = FPlatformTime::Seconds();
				for (int32 i = 0; i < 2; ++i) { R.ReadAll(A, Out); R.Move(B, A); R.Delete(A, true); }
				const double Elapsed = FPlatformTime::Seconds() - T0;
				UE_LOG(LogTemp, Display, TEXT("[MMO_SLOTREAL_INFO] retry_budget elapsed_ms=%.1f retries=%d"), Elapsed * 1000.0, R.RetriesPerformed);
				Check(TEXT("ops3"), TEXT("total_retry_wait_stays_under_200ms_across_many_failing_ops"), Elapsed < 0.2 && R.RetriesPerformed >= 1 && R.RetriesPerformed <= 6 && Ops.Exists(A) && Ops.Exists(B));
				Ops.Delete(A, true); Ops.Delete(B, true);
			}
			{
				// WriteNew: tam flush, Flush hatasi ve Tell uyusmazligi basarisizlik sayilir.
				bool bFull = false;
				FMMOFileManagerSlotOps W;
				W.TestOpenWrite = [&](const FString& Path) -> IFileHandle* { IFileHandle* H = PF.OpenWrite(*Path, false, false); return H ? new FSlotRealFaultHandle(H, false, 0, &bFull) : nullptr; };
				const bool bOk = W.WriteNew(A, BytesA);
				Check(TEXT("ops3"), TEXT("write_new_requests_full_flush"), bOk && bFull);
				FMMOFileManagerSlotOps F;
				F.TestOpenWrite = [&](const FString& Path) -> IFileHandle* { IFileHandle* H = PF.OpenWrite(*Path, false, false); return H ? new FSlotRealFaultHandle(H, true, 0, &bFull) : nullptr; };
				Check(TEXT("ops3"), TEXT("write_new_fails_when_flush_fails"), !F.WriteNew(A, BytesA));
				FMMOFileManagerSlotOps T;
				T.TestOpenWrite = [&](const FString& Path) -> IFileHandle* { IFileHandle* H = PF.OpenWrite(*Path, false, false); return H ? new FSlotRealFaultHandle(H, false, 1, &bFull) : nullptr; };
				Check(TEXT("ops3"), TEXT("write_new_fails_when_tell_does_not_match_length"), !T.WriteNew(A, BytesA));
				Ops.Delete(A, true);
			}
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
			Ops.Delete(P(TEXT("MMOSave_RealRep")), false);
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
			Ops.Delete(Legacy, false);
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
			Check(TEXT("tmp"), TEXT("complete_tmp_alone_makes_exists_true_so_caller_reaches_load"), Store.Exists(TEXT("MMOSave_RealTmp")));
			SlotRealWrite(P(TEXT("MMOSave_RealTmpTorn"), TEXT(".tmp")), SlotRealPrefix(BytesA, BytesA.Num() / 2));
			Check(TEXT("tmp"), TEXT("torn_tmp_alone_exists_false_and_load_false"), !Store.Exists(TEXT("MMOSave_RealTmpTorn")) && !Store.Load(TEXT("MMOSave_RealTmpTorn"), O));
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

		// --- Gercek enjeksiyonlar (S6.2): kilitli .tmp fail-closed; salt-okunur .tmp/.bak/ana kendi kendine iyilesir; okunamayan ana .bak'a dusmez ---
		{
			// Kilitli .tmp (acik yazma tutamagi = paylasim ihlali): silinemez -> Save false, dogrudan yazim YOK, eski veri korunur.
			FMMOCharacterSaveData O;
			Store.Save(TEXT("MMOSave_RealRo"), SampleA);
			const FString Tmp = P(TEXT("MMOSave_RealRo"), TEXT(".tmp"));
			SlotRealWrite(Tmp, TArray<uint8>({9, 9, 9}));
			IFileHandle* Lock = FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*Tmp, /*bAppend*/ true, false);
			FSlotRealLogCapture Cap;
			GLog->AddOutputDevice(&Cap);
			const bool bSaved = Store.Save(TEXT("MMOSave_RealRo"), SampleB);
			GLog->Flush(); GLog->RemoveOutputDevice(&Cap);
			Check(TEXT("inject"), TEXT("locked_tmp_save_false_no_direct_write"), Lock != nullptr && !bSaved && Cap.Text.Contains(TEXT("reason=tmp_delete_failed")));
			Check(TEXT("inject"), TEXT("save_failure_emits_metric_with_reason_and_player"), Cap.Text.Contains(TEXT("ev=save_failed reason=tmp_delete_failed player=RealRo")));
			Check(TEXT("inject"), TEXT("locked_tmp_old_data_still_loads_main_untouched"), Store.Load(TEXT("MMOSave_RealRo"), O) && SlotRealSameData(O, SampleA) && Ops.Exists(Tmp));
			delete Lock;
			Check(TEXT("inject"), TEXT("after_unlock_next_save_succeeds"), Store.Save(TEXT("MMOSave_RealRo"), SampleB) && Store.Load(TEXT("MMOSave_RealRo"), O) && SlotRealSameData(O, SampleB) && !Ops.Exists(Tmp));
		}
		{
			// G-ORTA-1: salt-okunur .tmp (kodun kendi dosyasi) silinir, Save basarili olur.
			FMMOCharacterSaveData O;
			Store.Save(TEXT("MMOSave_RealRoTmp"), SampleA);
			const FString Tmp = P(TEXT("MMOSave_RealRoTmp"), TEXT(".tmp"));
			SlotRealWrite(Tmp, TArray<uint8>({9, 9, 9}));
			SlotRealSetReadOnly(Tmp, true);
			const bool bSaved = Store.Save(TEXT("MMOSave_RealRoTmp"), SampleB);
			Check(TEXT("inject"), TEXT("readonly_tmp_is_deleted_and_save_succeeds"), bSaved && Store.Load(TEXT("MMOSave_RealRoTmp"), O) && SlotRealSameData(O, SampleB) && !Ops.Exists(Tmp));
		}
		{
			// Salt-okunur bayat .bak (kodun kendi dosyasi) bir sonraki Save'i kilitlemez.
			FMMOCharacterSaveData O;
			Store.Save(TEXT("MMOSave_RealRoBak"), SampleA);
			const FString Bak = P(TEXT("MMOSave_RealRoBak"), TEXT(".bak"));
			SlotRealWrite(Bak, BytesB);
			SlotRealSetReadOnly(Bak, true);
			const bool bSaved = Store.Save(TEXT("MMOSave_RealRoBak"), SlotRealSample(3500));
			Check(TEXT("inject"), TEXT("readonly_stale_bak_replaced_and_cleaned"), bSaved && Store.Load(TEXT("MMOSave_RealRoBak"), O) && O.Gold == 3500 && !Ops.Exists(Bak));
		}
		{
			// Salt-okunur ANA dosya (eski duman testi enjeksiyonu): ilk ve ikinci Save basarili, .bak temizlenir, veri kaybi yok.
			FMMOCharacterSaveData O;
			FSlotRealLogCapture Cap;
			GLog->AddOutputDevice(&Cap);
			Store.Save(TEXT("MMOSave_RealRoMain"), SampleA);
			SlotRealSetReadOnly(P(TEXT("MMOSave_RealRoMain")), true);
			const bool bFirst = Store.Save(TEXT("MMOSave_RealRoMain"), SampleB);
			const bool bLoadedFirst = Store.Load(TEXT("MMOSave_RealRoMain"), O) && SlotRealSameData(O, SampleB);
			const bool bBakGoneAfterFirst = !Ops.Exists(P(TEXT("MMOSave_RealRoMain"), TEXT(".bak")));
			const bool bSecond = Store.Save(TEXT("MMOSave_RealRoMain"), SlotRealSample(3000));
			const bool bLoadedSecond = Store.Load(TEXT("MMOSave_RealRoMain"), O) && O.Gold == 3000;
			GLog->Flush(); GLog->RemoveOutputDevice(&Cap);
			UE_LOG(LogTemp, Display, TEXT("[MMO_SLOTREAL_OBS] readonly_main first_save=%d second_save=%d bak_exists=%d"), bFirst, bSecond, Ops.Exists(P(TEXT("MMOSave_RealRoMain"), TEXT(".bak"))));
			Check(TEXT("inject"), TEXT("readonly_main_first_save_ok_new_data_loads_bak_cleaned"), bFirst && bLoadedFirst && bBakGoneAfterFirst);
			Check(TEXT("inject"), TEXT("readonly_main_second_save_succeeds_bak_cleaned_no_data_loss"), bSecond && bLoadedSecond && !Ops.Exists(P(TEXT("MMOSave_RealRoMain"), TEXT(".bak"))) && !Cap.Text.Contains(TEXT("stale_bak_left")));
			SlotRealClearReadOnlyTree(Dir);
		}
		{
			// G-ORTA-1: bayat .bak silinemezse ayri metrik (ev=save_stale_bak); yeni ana dosya kazanir, sonraki Save bayat .bak'i temizler.
			FMMOCharacterSaveData O;
			Store.Save(TEXT("MMOSave_RealStale"), SampleA);
			FMMOLocalFileCharacterStore::TestOpsConfigurator = [](FMMOFileManagerSlotOps& Cfg)
			{
				Cfg.TestInjectFailure = [](const TCHAR* Op, const FString& Path, int32) { return FCString::Strcmp(Op, TEXT("delete")) == 0 && Path.EndsWith(TEXT(".bak")); };
			};
			FSlotRealLogCapture Cap;
			GLog->AddOutputDevice(&Cap);
			const bool bSaved = Store.Save(TEXT("MMOSave_RealStale"), SampleB);
			GLog->Flush(); GLog->RemoveOutputDevice(&Cap);
			FMMOLocalFileCharacterStore::TestOpsConfigurator = nullptr;
			const bool bBakLeft = Ops.Exists(P(TEXT("MMOSave_RealStale"), TEXT(".bak")));
			Check(TEXT("stale"), TEXT("undeletable_bak_save_ok_logged_with_separate_metric"), bSaved && bBakLeft && Cap.Text.Contains(TEXT("event=stale_bak_left slot=MMOSave_RealStale")) && Cap.Text.Contains(TEXT("ev=save_stale_bak player=RealStale")));
			Check(TEXT("stale"), TEXT("new_main_wins_and_next_save_cleans_stale_bak"), Store.Load(TEXT("MMOSave_RealStale"), O) && SlotRealSameData(O, SampleB) && Store.Save(TEXT("MMOSave_RealStale"), SlotRealSample(4100)) && !Ops.Exists(P(TEXT("MMOSave_RealStale"), TEXT(".bak"))) && Store.Load(TEXT("MMOSave_RealStale"), O) && O.Gold == 4100);
		}
		{
			// G-ORTA-3: ana dosya VAR ama I/O hatasiyla okunamiyor (acik yazma tutamagi): Load .bak'a DUSMEZ, Save fail-closed.
			FMMOCharacterSaveData O;
			SlotRealWrite(P(TEXT("MMOSave_RealIo"), TEXT(".bak")), BytesA);
			SlotRealWrite(P(TEXT("MMOSave_RealIo")), BytesB);
			const TMap<FString, TArray<uint8>> Before = SlotRealSnapshot(Dir);
			IFileHandle* Lock = FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*P(TEXT("MMOSave_RealIo")), /*bAppend*/ true, false);
			FSlotRealLogCapture Cap;
			GLog->AddOutputDevice(&Cap);
			const bool bLocked = Lock != nullptr;
			const bool bLoaded = Store.Load(TEXT("MMOSave_RealIo"), O);
			const bool bExists = Store.Exists(TEXT("MMOSave_RealIo"));
			const bool bSaved = Store.Save(TEXT("MMOSave_RealIo"), SampleA);
			GLog->Flush(); GLog->RemoveOutputDevice(&Cap);
			delete Lock;
			Check(TEXT("io"), TEXT("unreadable_main_load_false_no_bak_fallback_metric_io"), bLocked && !bLoaded && !Cap.Text.Contains(TEXT("event=recovered")) && Cap.Text.Contains(TEXT("event=load_failed slot=MMOSave_RealIo reason=io")) && Cap.Text.Contains(TEXT("ev=load_failed reason=io player=RealIo")));
			Check(TEXT("io"), TEXT("unreadable_main_exists_true_and_save_fails_closed_with_metric"), bExists && !bSaved && Cap.Text.Contains(TEXT("ev=save_failed reason=main_unreadable player=RealIo")));
			Check(TEXT("io"), TEXT("after_unlock_files_untouched_and_main_loads_not_bak"), SlotRealSameSnapshot(Before, SlotRealSnapshot(Dir)) && Store.Load(TEXT("MMOSave_RealIo"), O) && SlotRealSameData(O, SampleB));
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
			Check(TEXT("bad_cap"), TEXT("quota_full_metric_names_player"), Cap.Text.Contains(TEXT("ev=save_bad_cap player=RealCap")) && Cap.Text.Contains(TEXT("ev=save_failed reason=bad_cap_reached player=RealCap")));
		}

		// --- Yuva adi savunmasi: yol gecisi / gecersiz ad dosya olusturmaz ---
		{
			const TMap<FString, TArray<uint8>> Before = SlotRealSnapshot(Dir);
			const TMap<FString, TArray<uint8>> ParentBefore = SlotRealSnapshot(Root);
			FMMOCharacterSaveData O;
			const TCHAR* Bad[] = {TEXT("MMOSave_..\\x"), TEXT("MMOSave_../x"), TEXT("../MMOSave_x"), TEXT("MMOSave_a:b"), TEXT("MMOSave_a.b"), TEXT("MMOSave_"), TEXT(""), TEXT("Other_Ali")};
			bool bAllRefused = true;
			FSlotRealLogCapture Cap;
			GLog->AddOutputDevice(&Cap);
			for (const TCHAR* Name : Bad)
			{
				bAllRefused = bAllRefused && !Store.Save(Name, SampleA) && !Store.Load(Name, O) && !Store.Exists(Name);
			}
			GLog->Flush(); GLog->RemoveOutputDevice(&Cap);
			Check(TEXT("slot_name"), TEXT("invalid_names_refused_by_all_three_calls"), bAllRefused);
			Check(TEXT("slot_name"), TEXT("invalid_name_save_failure_metric_has_no_player_name"), Cap.Text.Contains(TEXT("ev=save_failed reason=bad_slot_name player=-")) && !Cap.Text.Contains(TEXT("player=..")));
			Check(TEXT("slot_name"), TEXT("invalid_names_create_no_files_anywhere"), SlotRealSameSnapshot(Before, SlotRealSnapshot(Dir)) && SlotRealSameSnapshot(ParentBefore, SlotRealSnapshot(Root)));
		}

		// --- Boyut tavani (8 MB): veri tavandan buyukse Save false, dosya olusmaz ---
		{
			FMMOCharacterSaveData Huge = SampleA;
			Huge.ClanName = FString::ChrN(5 * 1024 * 1024, static_cast<TCHAR>(0x0100)); // UTF-16: ~10 MB
			const bool bSaved = Store.Save(TEXT("MMOSave_RealHuge"), Huge);
			Check(TEXT("size_cap"), TEXT("over_8mb_save_false_no_files"), !bSaved && !Ops.Exists(P(TEXT("MMOSave_RealHuge"))) && !Ops.Exists(P(TEXT("MMOSave_RealHuge"), TEXT(".tmp"))));
		}

		// --- D2: oyun is parcacigi disindan Save reddedilir (cokmeden: log + metrik, dosya olusmaz) ---
		{
			FSlotRealLogCapture Cap;
			GLog->AddOutputDevice(&Cap);
			const FMMOCharacterSaveData Copy = SampleA;
			const bool bOffThread = Async(EAsyncExecution::Thread, [&Store, &Copy]() { return Store.Save(TEXT("MMOSave_RealThr"), Copy); }).Get();
			GLog->Flush(); GLog->RemoveOutputDevice(&Cap);
			Check(TEXT("thread"), TEXT("save_from_non_game_thread_refused_no_files_with_metric"), !bOffThread && !Ops.Exists(P(TEXT("MMOSave_RealThr"))) && !Ops.Exists(P(TEXT("MMOSave_RealThr"), TEXT(".tmp"))) && Cap.Text.Contains(TEXT("ev=save_failed reason=not_game_thread player=RealThr")));
			Check(TEXT("thread"), TEXT("save_on_game_thread_still_works"), Store.Save(TEXT("MMOSave_RealThr"), SampleA));
		}

		// --- QA-3: acilis oz-sinamasi surec basina bir kez kosar, gecer; hata raporu Error + metrik basar ---
		{
			FString Why;
			const bool bRoundTrip = FMMOLocalFileCharacterStore::CheckFormatRoundTrip(Why);
			Check(TEXT("selftest"), TEXT("format_round_trip_passes_and_ran_exactly_once_in_process"), bRoundTrip && Why.IsEmpty() && FMMOLocalFileCharacterStore::GetFormatSelfTestRunCount() == 1);
			FSlotRealLogCapture OkCap;
			GLog->AddOutputDevice(&OkCap);
			FMMOLocalFileCharacterStore::ReportFormatSelfTest(true, TEXT(""));
			GLog->Flush(); GLog->RemoveOutputDevice(&OkCap);
			FSlotRealLogCapture BadCap;
			GLog->AddOutputDevice(&BadCap);
			FMMOLocalFileCharacterStore::ReportFormatSelfTest(false, TEXT("test_injected=1"));
			GLog->Flush(); GLog->RemoveOutputDevice(&BadCap);
			Check(TEXT("selftest"), TEXT("ok_report_is_silent"), !OkCap.Text.Contains(TEXT("format_selftest_failed")) && !OkCap.Text.Contains(TEXT("save_selftest_failed")) && !OkCap.bSawError);
			Check(TEXT("selftest"), TEXT("failure_report_is_error_level_with_event_and_metric"), BadCap.bSawError && BadCap.Text.Contains(TEXT("event=format_selftest_failed why=test_injected=1")) && BadCap.Text.Contains(TEXT("ev=save_selftest_failed why=test_injected=1")));
		}

		// --- Listeleme (D1): .sav + yalniz-.bak + GECERLI yalniz-.tmp yuvalari; yarim .tmp, .bad, joker 8.3 eslesmesi yuva degil ---
		{
			SlotRealWrite(P(TEXT("MMOSave_RealListBak"), TEXT(".bak")), BytesA);
			SlotRealWrite(P(TEXT("MMOSave_RealListTmp"), TEXT(".tmp")), BytesA);
			SlotRealWrite(P(TEXT("MMOSave_RealListTorn"), TEXT(".tmp")), SlotRealPrefix(BytesA, BytesA.Num() / 2));
			SlotRealWrite(P(TEXT("MMOSave_RealListBoth")), BytesA);
			SlotRealWrite(P(TEXT("MMOSave_RealListBoth"), TEXT(".bak")), BytesB);
			SlotRealWrite(P(TEXT("MMOSave_RealListBoth"), TEXT(".tmp")), BytesB);
			SlotRealWrite(P(TEXT("MMOSave_RealListBad"), TEXT(".bad")), BytesA);
			// "MMOSave_.savx": 8.3 kisa adi "MMOSAVE_.SAV" -> Windows "MMOSave_*.sav" joker aramasi bunu da dondurur; ".sav" ile bitmedigi icin yuva DEGIL.
			SlotRealWrite(FPaths::Combine(Dir, TEXT("MMOSave_.savx")), BytesA);
			TArray<FString> Raw;
			IFileManager::Get().FindFiles(Raw, *FPaths::Combine(Dir, TEXT("MMOSave_*.sav")), true, false);
			UE_LOG(LogTemp, Display, TEXT("[MMO_SLOTREAL_INFO] wildcard_quirk_active=%d"), Raw.Contains(TEXT("MMOSave_.savx")) ? 1 : 0);
			TArray<FString> L;
			Store.ListSlotNames(L);
			bool bNoDots = true;
			for (const FString& N : L) bNoDots = bNoDots && N.StartsWith(TEXT("MMOSave_")) && !N.Contains(TEXT("."));
			int32 BothCount = 0;
			for (const FString& N : L) { if (N == TEXT("MMOSave_RealListBoth")) ++BothCount; }
			Check(TEXT("list"), TEXT("main_bak_only_and_valid_tmp_only_slots_listed"), bNoDots && L.Contains(TEXT("MMOSave_RealA")) && L.Contains(TEXT("MMOSave_RealListBak")) && L.Contains(TEXT("MMOSave_RealListTmp")) && L.Contains(TEXT("MMOSave_RealBak")));
			Check(TEXT("list"), TEXT("torn_tmp_bad_and_wildcard_quirk_files_not_listed_and_no_duplicates"), !L.Contains(TEXT("MMOSave_RealListTorn")) && !L.Contains(TEXT("MMOSave_RealListBad")) && !L.Contains(TEXT("MMOSave_")) && !L.Contains(TEXT("MMOSave_.savx")) && BothCount == 1);
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

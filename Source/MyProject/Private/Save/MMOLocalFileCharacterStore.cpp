// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOLocalFileCharacterStore.h"
#include "Save/MMOSlotFileAtomic.h"
#include "Core/MMOMetrics.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/CustomVersion.h"
#include "UObject/ObjectVersion.h"

DEFINE_LOG_CATEGORY_STATIC(LogMMOSaveFile, Log, All);

namespace
{
	// Birim-dosyaya ozel onek (unity build): MMOLocalFileStore*.
	constexpr double MMOLocalFileStoreSlowSaveSeconds = 0.050; // [PLACEHOLDER] bilgi logu esigi (karar degil)
	constexpr int32 MMOLocalFileStoreSaveTag = 0x53415647;     // motor UE_SAVEGAME_FILE_TYPE_TAG ("GVAS" dosyada)

	// Sinirli FString okuyucu (motorun FString<< 'i dosya boyutuyla sinirlanmaz ve hata Error loglar). Ad/sinif adi gibi kisa dizgiler icin.
	bool MMOLocalFileStoreReadString(FMemoryReader& Ar, FString& Out, int32 MaxChars)
	{
		Out.Reset();
		int32 Len = 0;
		Ar << Len;
		if (Ar.IsError() || Len == MIN_int32) return false;
		if (Len == 0) return true;
		const bool bWide = Len < 0;
		const int32 N = bWide ? -Len : Len; // sonlandirici NUL dahil
		if (N > MaxChars + 1) return false;
		const int64 NeedBytes = static_cast<int64>(N) * (bWide ? 2 : 1);
		if (Ar.Tell() + NeedBytes > Ar.TotalSize()) return false;
		TArray<uint8> Raw;
		Raw.SetNumUninitialized(static_cast<int32>(NeedBytes));
		Ar.Serialize(Raw.GetData(), NeedBytes);
		if (Ar.IsError()) return false;
		if (bWide)
		{
			if (Raw[Raw.Num() - 1] != 0 || Raw[Raw.Num() - 2] != 0) return false;
			Out = FString(N - 1, reinterpret_cast<const TCHAR*>(Raw.GetData()));
		}
		else
		{
			if (Raw[Raw.Num() - 1] != 0) return false;
			Out = FString(N - 1, reinterpret_cast<const ANSICHAR*>(Raw.GetData()));
		}
		return true;
	}
	const TCHAR* MMOLocalFileStoreSaveFailureName(EMMOSlotSaveFailure F)
	{
		switch (F)
		{
		case EMMOSlotSaveFailure::BadSize: return TEXT("bad_size");
		case EMMOSlotSaveFailure::InvalidBytes: return TEXT("invalid_bytes");
		case EMMOSlotSaveFailure::NonCanonical: return TEXT("noncanonical");
		case EMMOSlotSaveFailure::MainUnreadable: return TEXT("main_unreadable");
		case EMMOSlotSaveFailure::BadCapReached: return TEXT("bad_cap_reached");
		case EMMOSlotSaveFailure::TmpDeleteFailed: return TEXT("tmp_delete_failed");
		case EMMOSlotSaveFailure::TmpWriteFailed: return TEXT("tmp_write_failed");
		case EMMOSlotSaveFailure::TmpReadbackMismatch: return TEXT("tmp_readback_mismatch");
		case EMMOSlotSaveFailure::RotateFailed: return TEXT("rotate_failed");
		case EMMOSlotSaveFailure::PromoteFailed: return TEXT("promote_failed");
		default: return TEXT("none");
		}
	}

	// Yuva adi dogrulandiktan sonra (ASCII izin listesi) log'a yazilabilir ad kismi.
	FString MMOLocalFileStorePlayerOf(const FString& SlotName)
	{
		return SlotName.Mid(8).Replace(TEXT(" "), TEXT("_")); // "MMOSave_" sonrasi; bosluk '_' (anahtar=deger satiri ayrisabilsin)
	}

	// Her Save basarisizligi icin tek tip satir (S6.2 G-ORTA-2): operatore gorunur; arayan (AMMOCharacter) zaten false'u yakalar.
	// Gecersiz yuva adinda oyuncu adi YAZILMAZ (player=-): izin listesi disi baytlar loga girmesin.
	void MMOLocalFileStoreReportSaveFailed(const FString& SlotName, const TCHAR* Reason, int32 Bytes)
	{
		const bool bNameOk = FMMOSlotFileAtomic::IsValidSlotName(SlotName);
		if (bNameOk)
		{
			UE_LOG(LogMMOSaveFile, Warning, TEXT("[MMOSAVE] event=save_failed slot=%s reason=%s bytes=%d"), *SlotName, Reason, Bytes);
		}
		else
		{
			UE_LOG(LogMMOSaveFile, Warning, TEXT("[MMOSAVE] event=save_failed slot=- reason=%s len=%d"), Reason, SlotName.Len());
		}
		UE_LOG(LogMMOSaveFile, Warning, TEXT("[MMOMETRIC] t=%.1f ev=save_failed reason=%s player=%s"), MMOMetrics::Seconds(), Reason, bNameOk ? *MMOLocalFileStorePlayerOf(SlotName) : TEXT("-"));
	}

	int32 GMMOLocalFileStoreFormatSelfTestRuns = 0;
}

TFunction<void(FMMOFileManagerSlotOps&)> FMMOLocalFileCharacterStore::TestOpsConfigurator;

FMMOLocalFileCharacterStore::FMMOLocalFileCharacterStore(const FString& InDirectoryOverride)
	: Directory(InDirectoryOverride.IsEmpty() ? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames")) : InDirectoryOverride)
{
}

FString FMMOLocalFileCharacterStore::SlotMainPath(const FString& SlotName) const
{
	return FPaths::Combine(Directory, SlotName + TEXT(".sav"));
}

bool FMMOLocalFileCharacterStore::SerializeBytes(const FMMOCharacterSaveData& In, TArray<uint8>& OutBytes)
{
	OutBytes.Reset();
	UMMOSaveGame* SG = Cast<UMMOSaveGame>(UGameplayStatics::CreateSaveGameObject(UMMOSaveGame::StaticClass()));
	if (!SG)
	{
		return false;
	}
	SG->Data = In;
	return UGameplayStatics::SaveGameToMemory(SG, OutBytes) && OutBytes.Num() > 0;
}

bool FMMOLocalFileCharacterStore::WalkStructure(const TArray<uint8>& Bytes, bool bWalkBody)
{
	// Motorun KENDISI bozuk/kesik baytlarda COKER (Development'ta check): GVAS etiketi yoksa dosyanin ilk 4 baytini sinif adi FString
	// uzunlugu sanir (-> FName 1023 assert); kesik dosyada UStruct::SerializeVersionedTaggedProperties 'Tag.Size == Loaded' check'i patlar.
	// Bu yuzden LoadGameFromMemory'ye girmeden once FSaveGameHeader::Read'in sinirli kopyasi + ust duzey etiket (FPropertyTag) boyut yuruyusu yapilir.
	if (Bytes.Num() < 32 || Bytes.Num() > FMMOSlotFileAtomic::MaxBytes)
	{
		return false;
	}
	FMemoryReader Ar(Bytes, true);
	int32 Tag = 0;
	int32 Version = 0;
	Ar << Tag;
	if (Tag != MMOLocalFileStoreSaveTag) return false;
	Ar << Version;
	if (Version < 2 || Version > 3) return false; // motor: AddedCustomVersions=2, PackageFileSummaryVersionChange=3 (=LatestVersion, 5.7)
	int32 Ue4 = 0, Ue5 = 0;
	Ar << Ue4;
	if (Version >= 3) Ar << Ue5;
	// FEngineVersion: Major/Minor/Patch (uint16), Changelist (uint32), Branch (FString).
	uint16 EngMajor = 0, EngMinor = 0, EngPatch = 0;
	uint32 EngChangelist = 0;
	FString EngBranch;
	Ar << EngMajor << EngMinor << EngPatch << EngChangelist;
	if (Ar.IsError() || !MMOLocalFileStoreReadString(Ar, EngBranch, 256)) return false;
	int32 Format = 0;
	Ar << Format;
	if (Ar.IsError()) return false;
	if (Format != static_cast<int32>(ECustomVersionSerializationFormat::Latest)) return false; // Optimized: kayit basina GUID(16)+int32
	int32 CustomCount = 0;
	Ar << CustomCount;
	if (Ar.IsError() || CustomCount < 0 || CustomCount > 4096) return false;
	const int64 AfterCustom = Ar.Tell() + static_cast<int64>(CustomCount) * 20;
	if (AfterCustom >= Ar.TotalSize()) return false;
	Ar.Seek(AfterCustom);
	FString ClassName;
	if (!MMOLocalFileStoreReadString(Ar, ClassName, 256)) return false;
	// Yalniz kendi sinifimiz: yabanci sinif yolunu LoadObject'e vermeyiz.
	if (ClassName != UMMOSaveGame::StaticClass()->GetPathName()) return false;
	if (!bWalkBody) return true;

	// Ust duzey property etiketleri (motorun FPropertyTag'i disa acik DEGIL; 5.7 bicimi elle yurunur): her etiketin Size'i dosyaya sigmali
	// ve akis "None" ile bitmeli. Kesik dosya burada elenir. Eski (tam tip adi oncesi) bicim dogrulanamaz: yalniz baslik denetimi.
	if (Version < 3 || Ue5 < static_cast<int32>(EUnrealEngineObjectUE5Version::PROPERTY_TAG_COMPLETE_TYPE_NAME))
	{
		return true;
	}
	// Sinif adindan sonra nesne seviyesi 1 bayt "serializasyon kontrol uzantisi" (0 = uzanti yok; gozlem: gercek .sav dosyalari) gelir.
	uint8 ControlExtension = 0;
	Ar << ControlExtension;
	if (Ar.IsError() || Ar.IsCriticalError() || ControlExtension != 0) return false;
	for (int32 Guard = 0; Guard < 64; ++Guard)
	{
		FString PropName;
		if (!MMOLocalFileStoreReadString(Ar, PropName, 256)) return false;
		if (PropName == TEXT("None")) return Ar.Tell() + 4 <= Ar.TotalSize(); // bitis isareti + nesne sonu int32 (gozlem: 0); sondaki FAZLA bayt eski dosyalarda hosgorulur
		// FPropertyTypeName: dugumler (ad + ic sayisi), on-siralama.
		int32 Remaining = 1;
		for (int32 Nodes = 0; Remaining > 0; ++Nodes)
		{
			if (Nodes >= 64) return false;
			FString NodeName;
			int32 InnerCount = 0;
			if (!MMOLocalFileStoreReadString(Ar, NodeName, 256)) return false;
			Ar << InnerCount;
			if (Ar.IsError() || InnerCount < 0 || InnerCount > 64) return false;
			Remaining += InnerCount - 1;
		}
		int32 PropSize = 0;
		uint8 TagFlags = 0;
		Ar << PropSize;
		Ar << TagFlags;
		if (Ar.IsError() || Ar.IsCriticalError() || PropSize < 0) return false;
		if (TagFlags & 0x04) return false; // HasPropertyExtensions: bu sinifin etiketlerinde beklenmez; bicim dogrulanamaz
		if (TagFlags & 0x01) { int32 ArrayIndex = 0; Ar << ArrayIndex; }
		if (TagFlags & 0x02) { FGuid PropGuid; Ar << PropGuid; }
		if (Ar.IsError() || Ar.IsCriticalError()) return false;
		const int64 Start = Ar.Tell();
		if (Start + PropSize > Ar.TotalSize()) return false;
		Ar.Seek(Start + PropSize);
	}
	return false;
}

bool FMMOLocalFileCharacterStore::HasSafeHeader(const TArray<uint8>& Bytes)
{
	return WalkStructure(Bytes, false);
}

bool FMMOLocalFileCharacterStore::HasSafeStructure(const TArray<uint8>& Bytes)
{
	return WalkStructure(Bytes, true);
}

bool FMMOLocalFileCharacterStore::ParseBytes(const TArray<uint8>& Bytes, FMMOCharacterSaveData& Out)
{
	if (Bytes.Num() <= 0 || !HasSafeStructure(Bytes))
	{
		return false;
	}
	const UMMOSaveGame* SG = Cast<UMMOSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!SG)
	{
		return false;
	}
	Out = SG->Data;
	return true;
}

bool FMMOLocalFileCharacterStore::IsCanonical(const TArray<uint8>& Bytes)
{
	FMMOCharacterSaveData Parsed;
	TArray<uint8> Again;
	return ParseBytes(Bytes, Parsed) && SerializeBytes(Parsed, Again) && Again == Bytes;
}

bool FMMOLocalFileCharacterStore::IsValidMainBytes(const TArray<uint8>& Bytes)
{
	FMMOCharacterSaveData Parsed;
	if (!ParseBytes(Bytes, Parsed))
	{
		return false;
	}
	if (Parsed.bValid)
	{
		return true; // eski dosyalar kanonik olmayabilir (motor/struct surumu); kesik olmadigi bu katmanda kanitlanamaz (R1)
	}
	// bValid=false: gercek bos kayit (kanonik) ile bValid alanindan once kesilmis dosya ayirt edilir.
	TArray<uint8> Again;
	return SerializeBytes(Parsed, Again) && Again == Bytes;
}

bool FMMOLocalFileCharacterStore::VerifyRoundTrip(const FMMOCharacterSaveData& In, const TArray<uint8>& Bytes)
{
	FMMOCharacterSaveData Check;
	return ParseBytes(Bytes, Check) && Check.bValid == In.bValid;
}

bool FMMOLocalFileCharacterStore::CheckFormatRoundTrip(FString& OutWhy)
{
	OutWhy.Reset();
	// Temsili ornek: her alan tipinden (dizi, struct dizisi, TMap<FName,int64>, FString, FVector/FRotator) en az bir deger.
	FMMOCharacterSaveData Sample;
	Sample.bValid = true;
	Sample.Level = 17;
	Sample.Experience = 345678;
	Sample.Gold = 91234;
	FMMOItemStack Stack; Stack.ItemId = TEXT("SelfTest_Potion"); Stack.Count = 3; Sample.Items.Add(Stack);
	FMMOEquipmentInstance Equip; Equip.InstanceId = 7; Equip.ItemId = TEXT("SelfTest_Sword"); Equip.Plus = 3; Sample.EquipmentBag.Add(Equip);
	Sample.NextInstanceId = 8;
	Sample.LearnedSkills.Add(TEXT("SelfTest_SkillA"));
	Sample.SkillCooldownExpiryUtcSeconds.Add(TEXT("SelfTest_SkillA"), 1700000000LL);
	Sample.SkillCooldownExpiryUtcSeconds.Add(TEXT("SelfTest_SkillB"), 1700000999LL);
	Sample.CompletedQuests.Add(TEXT("SelfTest_Quest"));
	Sample.ClanName = TEXT("SelfTestClan");
	Sample.Friends.Add(TEXT("SelfTestFriend"));
	Sample.MapName = TEXT("SelfTest_scene");
	Sample.Location = FVector(1.5, -2.25, 100.125);
	Sample.Rotation = FRotator(0.0, 90.0, 0.0);
	TArray<uint8> Bytes;
	if (!SerializeBytes(Sample, Bytes)) { OutWhy = TEXT("serialize_failed"); return false; }
	if (!HasSafeStructure(Bytes)) { OutWhy = TEXT("structure_walk_rejected_engine_output"); return false; }
	FMMOCharacterSaveData Parsed;
	if (!ParseBytes(Bytes, Parsed)) { OutWhy = TEXT("parse_failed"); return false; }
	if (Parsed.Gold != Sample.Gold || Parsed.Level != Sample.Level || Parsed.SkillCooldownExpiryUtcSeconds.Num() != 2 || Parsed.ClanName != Sample.ClanName) { OutWhy = TEXT("fields_differ"); return false; }
	if (!IsCanonical(Bytes)) { OutWhy = TEXT("not_canonical"); return false; }
	return true;
}

void FMMOLocalFileCharacterStore::ReportFormatSelfTest(bool bOk, const FString& Why)
{
	if (bOk)
	{
		return;
	}
	UE_LOG(LogMMOSaveFile, Error, TEXT("[MMOSAVE] event=format_selftest_failed why=%s (motor surumu yukseltmesi sonrasi kayit bicimi/yapi yuruyusu uyumsuz olabilir: tum kayitlar bozuk gorunur, Save reddeder; TestSlotFileAtomicReal/TestCharacterStore kosun)"), *Why);
	UE_LOG(LogMMOSaveFile, Warning, TEXT("[MMOMETRIC] t=%.1f ev=save_selftest_failed why=%s"), MMOMetrics::Seconds(), *Why);
}

int32 FMMOLocalFileCharacterStore::GetFormatSelfTestRunCount()
{
	return GMMOLocalFileStoreFormatSelfTestRuns;
}

void FMMOLocalFileCharacterStore::RunFormatSelfTestOnce()
{
	// Surec basina bir kez (C++ yerel statik baslatma); davranisi DEGISTIRMEZ.
	static const bool bRan = []()
	{
		++GMMOLocalFileStoreFormatSelfTestRuns;
		FString Why;
		const bool bOk = FMMOLocalFileCharacterStore::CheckFormatRoundTrip(Why);
		FMMOLocalFileCharacterStore::ReportFormatSelfTest(bOk, Why);
		return true;
	}();
	(void)bRan;
}

bool FMMOLocalFileCharacterStore::Load(const FString& SlotName, FMMOCharacterSaveData& Out)
{
	RunFormatSelfTestOnce();
	if (!FMMOSlotFileAtomic::IsValidSlotName(SlotName))
	{
		UE_LOG(LogMMOSaveFile, Warning, TEXT("[MMOSAVE] event=load_refused reason=bad_slot_name len=%d"), SlotName.Len());
		return false;
	}
	FMMOFileManagerSlotOps Ops;
	if (TestOpsConfigurator) { TestOpsConfigurator(Ops); }
	const FMMOSlotAtomicPaths Paths = FMMOSlotAtomicPaths::From(SlotMainPath(SlotName));
	const auto MainValid = [](const TArray<uint8>& B) { return FMMOLocalFileCharacterStore::IsValidMainBytes(B); };
	const auto TmpValid = [](const TArray<uint8>& B) { return FMMOLocalFileCharacterStore::IsCanonical(B); };
	const FMMOSlotValidator TmpValidator(TmpValid);
	const FMMOSlotLoadResult R = FMMOSlotFileAtomic::Load(Ops, Paths, MainValid, &TmpValidator);
	if (!R.bOk)
	{
		if (R.Failure == EMMOSlotLoadFailure::Corrupt || R.Failure == EMMOSlotLoadFailure::IoError)
		{
			const TCHAR* Reason = R.Failure == EMMOSlotLoadFailure::IoError ? TEXT("io") : TEXT("corrupt");
			UE_LOG(LogMMOSaveFile, Warning, TEXT("[MMOSAVE] event=load_failed slot=%s reason=%s"), *SlotName, Reason);
			UE_LOG(LogMMOSaveFile, Warning, TEXT("[MMOMETRIC] t=%.1f ev=load_failed reason=%s player=%s"), MMOMetrics::Seconds(), Reason, *MMOLocalFileStorePlayerOf(SlotName));
		}
		return false;
	}
	FMMOCharacterSaveData Parsed;
	if (!ParseBytes(R.Bytes, Parsed))
	{
		return false;
	}
	if (R.Source != EMMOSlotLoadSource::Main)
	{
		const TCHAR* Src = R.Source == EMMOSlotLoadSource::Bak ? TEXT("bak") : TEXT("tmp");
		UE_LOG(LogMMOSaveFile, Warning, TEXT("[MMOSAVE] event=recovered slot=%s source=%s"), *SlotName, Src);
		UE_LOG(LogMMOSaveFile, Warning, TEXT("[MMOMETRIC] t=%.1f ev=save_recovered player=%s src=%s"), MMOMetrics::Seconds(), *MMOLocalFileStorePlayerOf(SlotName), Src);
	}
	Out = MoveTemp(Parsed);
	return true;
}

bool FMMOLocalFileCharacterStore::Save(const FString& SlotName, const FMMOCharacterSaveData& In)
{
	RunFormatSelfTestOnce();
	if (!FMMOSlotFileAtomic::IsValidSlotName(SlotName))
	{
		UE_LOG(LogMMOSaveFile, Warning, TEXT("[MMOSAVE] event=save_refused reason=bad_slot_name len=%d"), SlotName.Len());
		MMOLocalFileStoreReportSaveFailed(SlotName, TEXT("bad_slot_name"), 0);
		return false;
	}
	// D2: .tmp yolu surec ici tek yazar varsayar (kilitsiz). Oyun is parcacigi disindan cagri reddedilir (cokertmeden: log + metrik).
	if (!IsInGameThread())
	{
		MMOLocalFileStoreReportSaveFailed(SlotName, TEXT("not_game_thread"), 0);
		return false;
	}
	const double Start = FPlatformTime::Seconds();
	TArray<uint8> Bytes;
	if (!SerializeBytes(In, Bytes))
	{
		MMOLocalFileStoreReportSaveFailed(SlotName, TEXT("serialize"), Bytes.Num());
		return false;
	}
	if (!VerifyRoundTrip(In, Bytes))
	{
		MMOLocalFileStoreReportSaveFailed(SlotName, TEXT("roundtrip"), Bytes.Num());
		return false;
	}
	FMMOFileManagerSlotOps Ops;
	if (TestOpsConfigurator) { TestOpsConfigurator(Ops); }
	const FMMOSlotAtomicPaths Paths = FMMOSlotAtomicPaths::From(SlotMainPath(SlotName));
	const auto MainValid = [](const TArray<uint8>& B) { return FMMOLocalFileCharacterStore::IsValidMainBytes(B); };
	const auto TmpValid = [](const TArray<uint8>& B) { return FMMOLocalFileCharacterStore::IsCanonical(B); };
	const FMMOSlotValidator TmpValidator(TmpValid);
	const FMMOSlotSaveResult R = FMMOSlotFileAtomic::Save(Ops, Paths, Bytes, MainValid, &TmpValidator);
	if (!R.bOk)
	{
		MMOLocalFileStoreReportSaveFailed(SlotName, MMOLocalFileStoreSaveFailureName(R.Failure), Bytes.Num());
		if (R.Failure == EMMOSlotSaveFailure::BadCapReached)
		{
			UE_LOG(LogMMOSaveFile, Warning, TEXT("[MMOMETRIC] t=%.1f ev=save_bad_cap player=%s"), MMOMetrics::Seconds(), *MMOLocalFileStorePlayerOf(SlotName));
		}
		return false;
	}
	if (R.bStaleBakLeft)
	{
		UE_LOG(LogMMOSaveFile, Warning, TEXT("[MMOSAVE] event=stale_bak_left slot=%s"), *SlotName);
		UE_LOG(LogMMOSaveFile, Warning, TEXT("[MMOMETRIC] t=%.1f ev=save_stale_bak player=%s"), MMOMetrics::Seconds(), *MMOLocalFileStorePlayerOf(SlotName));
	}
	const double Elapsed = FPlatformTime::Seconds() - Start;
	if (Elapsed > MMOLocalFileStoreSlowSaveSeconds)
	{
		UE_LOG(LogMMOSaveFile, Warning, TEXT("[MMOSAVE] event=slow_save slot=%s ms=%.1f bytes=%d"), *SlotName, Elapsed * 1000.0, Bytes.Num());
	}
	return true;
}

bool FMMOLocalFileCharacterStore::Exists(const FString& SlotName)
{
	RunFormatSelfTestOnce();
	if (!FMMOSlotFileAtomic::IsValidSlotName(SlotName))
	{
		return false;
	}
	FMMOFileManagerSlotOps Ops;
	const auto TmpValid = [](const TArray<uint8>& B) { return FMMOLocalFileCharacterStore::IsCanonical(B); };
	const FMMOSlotValidator TmpValidator(TmpValid);
	return FMMOSlotFileAtomic::Exists(Ops, FMMOSlotAtomicPaths::From(SlotMainPath(SlotName)), &TmpValidator);
}

void FMMOLocalFileCharacterStore::ListSlotNames(TArray<FString>& Out)
{
	RunFormatSelfTestOnce();
	// Yalniz karakter yuvalari (MMOSave_*): ayni klasorde baska .sav'lar olabilir.
	// D1: yalniz .bak'i (ana kayip) ya da GECERLI .tmp'si (ilk kayit yarida kesildi) olan yuvalar da listelenir ki klan adi cakisma taramasi
	// onlari kacirmasin (Exists ile ayni kural). .sav.bad / .sav.bad.N yuva sayilmaz.
	TSet<FString> Seen;
	const auto AddUnique = [&](const FString& Name)
	{
		bool bAlready = false;
		Seen.Add(Name.ToLower(), &bAlready);
		if (!bAlready)
		{
			Out.Add(Name);
		}
	};
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Directory, TEXT("MMOSave_*.sav")), true, false);
	for (const FString& File : Files)
	{
		if (!File.EndsWith(TEXT(".sav"), ESearchCase::IgnoreCase))
		{
			continue; // Windows joker 8.3 kisa ad eslesmesi (ornegin .savx): yalniz tam ".sav" sonlu dosyalar
		}
		AddUnique(FPaths::GetBaseFilename(File));
	}
	Files.Reset();
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Directory, TEXT("MMOSave_*.sav.bak")), true, false);
	for (const FString& File : Files)
	{
		if (File.EndsWith(TEXT(".sav.bak"), ESearchCase::IgnoreCase))
		{
			AddUnique(File.LeftChop(8));
		}
	}
	Files.Reset();
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Directory, TEXT("MMOSave_*.sav.tmp")), true, false);
	FMMOFileManagerSlotOps Ops;
	for (const FString& File : Files)
	{
		if (!File.EndsWith(TEXT(".sav.tmp"), ESearchCase::IgnoreCase))
		{
			continue;
		}
		const FString Name = File.LeftChop(8);
		if (Seen.Contains(Name.ToLower()))
		{
			continue;
		}
		TArray<uint8> Bytes;
		if (Ops.ReadAll(FPaths::Combine(Directory, File), Bytes) && IsCanonical(Bytes))
		{
			AddUnique(Name);
		}
	}
}

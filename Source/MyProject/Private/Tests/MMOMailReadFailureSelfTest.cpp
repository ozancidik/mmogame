// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mail/MMOMailStore.h"

#if !UE_BUILD_SHIPPING
#include "Async/AsyncFileHandle.h"
#include "Async/MappedFileHandle.h"
#include "CoreGlobals.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/OutputDevice.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
	FString MMOMailReadFailureCanonicalPath(const TCHAR* Path)
	{
		FString Full = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeFilename(Full);
		FPaths::CollapseRelativeDirectories(Full);
		return Full;
	}

	// One opt-in disposable process, one immutable GUID target. Deliberately retained
	// until process exit: restoring the manager does not retire pointers held by IO threads.
	class FMMOMailReadFailurePlatformFile final : public IPlatformFile
	{
	public:
		FMMOMailReadFailurePlatformFile(IPlatformFile& InLower, const FString& InPath)
			: Lower(&InLower), Path(MMOMailReadFailureCanonicalPath(*InPath)), Tmp(Path + TEXT(".tmp")), Bad(Path + TEXT(".bad")) {}

		FThreadSafeCounter DeniedReads;
		FThreadSafeCounter Writes;
		FThreadSafeCounter Moves;
		FThreadSafeCounter Deletes;

		bool Initialize(IPlatformFile* Inner, const TCHAR* CmdLine) override { return Inner == Lower; }
		IPlatformFile* GetLowerLevel() override { return Lower; }
		void SetLowerLevel(IPlatformFile* NewLower) override { check(NewLower == Lower); }
		const TCHAR* GetName() const override { return TEXT("MMOMailReadFailureTest"); }
		// Lower is already initialized; manager lifecycle/tick traverses the chain itself.
		void MakeUniquePakFilesForTheseFiles(const TArray<TArray<FString>>& Files) override { Lower->MakeUniquePakFilesForTheseFiles(Files); }
		void SetSandboxEnabled(bool bEnabled) override { Lower->SetSandboxEnabled(bEnabled); }
		bool IsSandboxEnabled() const override { return Lower->IsSandboxEnabled(); }
		bool FileExists(const TCHAR* Filename) override { return Lower->FileExists(Filename); }
		int64 FileSize(const TCHAR* Filename) override { return Lower->FileSize(Filename); }
		bool DeleteFile(const TCHAR* Filename) override
		{
			if (IsFixture(Filename)) Deletes.Increment();
			return Lower->DeleteFile(Filename);
		}
		bool DeleteFiles(const TArrayView<const TCHAR*>& Filenames) override
		{
			for (const TCHAR* Filename : Filenames) { if (IsFixture(Filename)) Deletes.Increment(); }
			return Lower->DeleteFiles(Filenames);
		}
		bool IsReadOnly(const TCHAR* Filename) override { return Lower->IsReadOnly(Filename); }
		bool MoveFile(const TCHAR* To, const TCHAR* From) override
		{
			if (IsFixture(To) || IsFixture(From)) Moves.Increment();
			return Lower->MoveFile(To, From);
		}
		bool SetReadOnly(const TCHAR* Filename, bool bValue) override { return Lower->SetReadOnly(Filename, bValue); }
		FDateTime GetTimeStamp(const TCHAR* Filename) override { return Lower->GetTimeStamp(Filename); }
		void SetTimeStamp(const TCHAR* Filename, FDateTime Time) override { Lower->SetTimeStamp(Filename, Time); }
		FDateTime GetAccessTimeStamp(const TCHAR* Filename) override { return Lower->GetAccessTimeStamp(Filename); }
		FString GetFilenameOnDisk(const TCHAR* Filename) override { return Lower->GetFilenameOnDisk(Filename); }
		ESymlinkResult IsSymlink(const TCHAR* Filename) override { return Lower->IsSymlink(Filename); }
		bool HasMarkOfTheWeb(FStringView Filename, FString* URL = nullptr) override { return Lower->HasMarkOfTheWeb(Filename, URL); }
		bool SetMarkOfTheWeb(FStringView Filename, bool bStatus, const FString* URL = nullptr) override { return Lower->SetMarkOfTheWeb(Filename, bStatus, URL); }
		IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite = false) override
		{
			if (IsTarget(Filename)) { DeniedReads.Increment(); return nullptr; }
			return Lower->OpenRead(Filename, bAllowWrite);
		}
		FFileOpenResult OpenRead(const TCHAR* Filename, EOpenReadFlags Flags) override
		{
			if (IsTarget(Filename))
			{
				DeniedReads.Increment();
				return MakeError(FFileSystemError(FString(TEXT("MMOMailReadFailureTest: fixture read denied"))));
			}
			return Lower->OpenRead(Filename, Flags);
		}
		IFileHandle* OpenReadNoBuffering(const TCHAR* Filename, bool bAllowWrite = false) override
		{
			if (IsTarget(Filename)) { DeniedReads.Increment(); return nullptr; }
			return Lower->OpenReadNoBuffering(Filename, bAllowWrite);
		}
		FFileOpenResult OpenReadNoBuffering(const TCHAR* Filename, EOpenReadFlags Flags) override
		{
			if (IsTarget(Filename))
			{
				DeniedReads.Increment();
				return MakeError(FFileSystemError(FString(TEXT("MMOMailReadFailureTest: fixture read denied"))));
			}
			return Lower->OpenReadNoBuffering(Filename, Flags);
		}
		IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) override
		{
			if (IsFixture(Filename)) Writes.Increment();
			return Lower->OpenWrite(Filename, bAppend, bAllowRead);
		}
		FFileOpenResult OpenWrite(const TCHAR* Filename, EOpenWriteFlags Flags) override
		{
			if (IsFixture(Filename)) Writes.Increment();
			return Lower->OpenWrite(Filename, Flags);
		}
		bool DirectoryExists(const TCHAR* Directory) override { return Lower->DirectoryExists(Directory); }
		bool CreateDirectory(const TCHAR* Directory) override { return Lower->CreateDirectory(Directory); }
		bool DeleteDirectory(const TCHAR* Directory) override { return Lower->DeleteDirectory(Directory); }
		FFileStatData GetStatData(const TCHAR* Filename) override { return Lower->GetStatData(Filename); }
		bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) override { return Lower->IterateDirectory(Directory, Visitor); }
		bool IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor) override { return Lower->IterateDirectoryStat(Directory, Visitor); }
		bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitorFunc Visitor) override { return Lower->IterateDirectory(Directory, Visitor); }
		bool IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitorFunc Visitor) override { return Lower->IterateDirectoryStat(Directory, Visitor); }
		bool IterateDirectoryRecursively(const TCHAR* Directory, FDirectoryVisitor& Visitor) override { return Lower->IterateDirectoryRecursively(Directory, Visitor); }
		bool IterateDirectoryStatRecursively(const TCHAR* Directory, FDirectoryStatVisitor& Visitor) override { return Lower->IterateDirectoryStatRecursively(Directory, Visitor); }
		bool IterateDirectoryRecursively(const TCHAR* Directory, FDirectoryVisitorFunc Visitor) override { return Lower->IterateDirectoryRecursively(Directory, Visitor); }
		bool IterateDirectoryStatRecursively(const TCHAR* Directory, FDirectoryStatVisitorFunc Visitor) override { return Lower->IterateDirectoryStatRecursively(Directory, Visitor); }
		IAsyncReadFileHandle* OpenAsyncRead(const TCHAR* Filename, bool bAllowWrite = false) override { return Lower->OpenAsyncRead(Filename, bAllowWrite); }
		FFileOpenAsyncResult OpenAsyncRead(const TCHAR* Filename, EOpenReadFlags Flags) override { return Lower->OpenAsyncRead(Filename, Flags); }
		void SetAsyncMinimumPriority(EAsyncIOPriorityAndFlags Priority) override { Lower->SetAsyncMinimumPriority(Priority); }
		FOpenMappedResult OpenMappedEx(const TCHAR* Filename, EOpenReadFlags Flags = EOpenReadFlags::None, int64 Size = 0) override { return Lower->OpenMappedEx(Filename, Flags, Size); }
		void GetTimeStampPair(const TCHAR* A, const TCHAR* B, FDateTime& TimeA, FDateTime& TimeB) override { Lower->GetTimeStampPair(A, B, TimeA, TimeB); }
		FDateTime GetTimeStampLocal(const TCHAR* Filename) override { return Lower->GetTimeStampLocal(Filename); }
		void FindFiles(TArray<FString>& Files, const TCHAR* Directory, const TCHAR* Extension) override { Lower->FindFiles(Files, Directory, Extension); }
		void FindFilesRecursively(TArray<FString>& Files, const TCHAR* Directory, const TCHAR* Extension) override { Lower->FindFilesRecursively(Files, Directory, Extension); }
		bool DeleteDirectoryRecursively(const TCHAR* Directory) override { return Lower->DeleteDirectoryRecursively(Directory); }
		bool CreateDirectoryTree(const TCHAR* Directory) override { return Lower->CreateDirectoryTree(Directory); }
		bool CopyFile(const TCHAR* To, const TCHAR* From, EPlatformFileRead ReadFlags = EPlatformFileRead::None, EPlatformFileWrite WriteFlags = EPlatformFileWrite::None) override { return Lower->CopyFile(To, From, ReadFlags, WriteFlags); }
		bool CopyDirectoryTree(const TCHAR* To, const TCHAR* From, bool bOverwrite) override { return Lower->CopyDirectoryTree(To, From, bOverwrite); }
		FString ConvertToAbsolutePathForExternalAppForRead(const TCHAR* Filename) override { return Lower->ConvertToAbsolutePathForExternalAppForRead(Filename); }
		FString ConvertToAbsolutePathForExternalAppForWrite(const TCHAR* Filename) override { return Lower->ConvertToAbsolutePathForExternalAppForWrite(Filename); }
		bool SendMessageToServer(const TCHAR* Message, IFileServerMessageHandler* Handler) override { return Lower->SendMessageToServer(Message, Handler); }
		bool DoesCreatePublicFiles() override { return Lower->DoesCreatePublicFiles(); }
		void SetCreatePublicFiles(bool bValue) override { Lower->SetCreatePublicFiles(bValue); }
		int64 GetAllowedBytesToWriteThrottledStorage(const TCHAR* Destination = nullptr) override { return Lower->GetAllowedBytesToWriteThrottledStorage(Destination); }
		bool FileJournalIsAvailable(const TCHAR* Volume = nullptr, ELogVerbosity::Type* Level = nullptr, FString* Error = nullptr) override { return Lower->FileJournalIsAvailable(Volume, Level, Error); }
		uint64 FileJournalGetMaximumSize(const TCHAR* Volume = nullptr, ELogVerbosity::Type* Level = nullptr, FString* Error = nullptr) const override { return Lower->FileJournalGetMaximumSize(Volume, Level, Error); }
		EFileJournalResult FileJournalGetLatestEntry(const TCHAR* Volume, FFileJournalId& Id, FFileJournalEntryHandle& Entry, FString* Error = nullptr) override { return Lower->FileJournalGetLatestEntry(Volume, Id, Entry, Error); }
		bool FileJournalIterateDirectory(const TCHAR* Directory, FDirectoryJournalVisitorFunc Visitor, FString* Error = nullptr) override { return Lower->FileJournalIterateDirectory(Directory, Visitor, Error); }
		FFileJournalData FileJournalGetFileData(const TCHAR* Filename, FString* Error = nullptr) override { return Lower->FileJournalGetFileData(Filename, Error); }
		EFileJournalResult FileJournalReadModified(const TCHAR* Volume, const FFileJournalId& Id, const FFileJournalEntryHandle& Start, TMap<FFileJournalFileHandle, FString>& Known, TSet<FString>& Modified, FFileJournalEntryHandle& Next, FString* Error = nullptr) override { return Lower->FileJournalReadModified(Volume, Id, Start, Known, Modified, Next, Error); }
		FString FileJournalGetVolumeName(FStringView InPath) override { return Lower->FileJournalGetVolumeName(InPath); }

	private:
		static bool SamePath(const FString& A, const FString& B)
		{
#if PLATFORM_WINDOWS
			return A.Equals(B, ESearchCase::IgnoreCase);
#else
			return A.Equals(B, ESearchCase::CaseSensitive);
#endif
		}
		bool IsTarget(const TCHAR* Filename) const { return SamePath(MMOMailReadFailureCanonicalPath(Filename), Path); }
		bool IsFixture(const TCHAR* Filename) const
		{
			const FString Full = MMOMailReadFailureCanonicalPath(Filename);
			return SamePath(Full, Path) || SamePath(Full, Tmp) || SamePath(Full, Bad);
		}
		IPlatformFile* const Lower;
		const FString Path;
		const FString Tmp;
		const FString Bad;
	};

	class FMMOMailReadFailureScope final
	{
	public:
		explicit FMMOMailReadFailureScope(IPlatformFile& Wrapper)
			: Previous(FPlatformFileManager::Get().GetPlatformFile())
		{
			FPlatformFileManager::Get().SetPlatformFile(Wrapper);
		}
		~FMMOMailReadFailureScope() { FPlatformFileManager::Get().SetPlatformFile(Previous); }
		FMMOMailReadFailureScope(const FMMOMailReadFailureScope&) = delete;
		FMMOMailReadFailureScope& operator=(const FMMOMailReadFailureScope&) = delete;
	private:
		IPlatformFile& Previous;
	};

	bool MMOMailReadFailureReadBytes(IPlatformFile& Platform, const FString& Path, TArray<uint8>& Out)
	{
		Out.Reset();
		TUniquePtr<IFileHandle> Handle(Platform.OpenRead(*Path));
		if (!Handle) return false;
		const int64 Size = Handle->Size();
		if (Size < 0 || Size > MAX_int32) return false;
		Out.SetNumUninitialized(static_cast<int32>(Size));
		return Size == 0 || Handle->Read(Out.GetData(), Size);
	}

	bool MMOMailReadFailureSameMail(const FMMOMail& A, const FMMOMail& B)
	{
		return A.From == B.From && A.Text == B.Text && A.Gold == B.Gold && A.SentUnix == B.SentUnix;
	}

	bool RunMMOMailReadFailureSelfTest(UWorld* World, FString& Error, int32& Checks)
	{
		Checks = 0;
		Error.Reset();
		if (!USE_ATOMIC_PLATFORM_FILE || !IsInGameThread() || GIsEditor || !World || !World->IsGameWorld() || World->GetNetMode() == NM_Client
			|| !FParse::Param(FCommandLine::Get(), TEXT("MMOMailReadFailureTest"))
			|| !FParse::Param(FCommandLine::Get(), TEXT("unattended"))
			|| !FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
		{
			Error = TEXT("atomic-platform-file build, opt-in disposable headless authority game process on game thread required");
			return false;
		}
		// Game-thread only; the retained wrapper's immutable lower pointer must never be rebound.
		static bool bInvoked = false;
		if (bInvoked) { Error = TEXT("one invocation per disposable process required"); return false; }
		bInvoked = true;
		bool bPassed = true;
		const auto Check = [&](bool bOK, const TCHAR* Label)
		{
			++Checks;
			UE_LOG(LogTemp, Display, TEXT("[MMO_MAIL_READ_FAILURE_CASE] %s check=%s"), bOK ? TEXT("PASS") : TEXT("FAIL"), Label);
			if (!bOK) { bPassed = false; if (Error.IsEmpty()) Error = Label; }
		};
		// All 32 GUID digits fit the store limit; no truncation or shared fixed mailbox name.
		const FString Name = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString Path = FMMOMailStore::GetBoxPath(Name);
		IPlatformFile& Previous = FPlatformFileManager::Get().GetPlatformFile();
		const bool bFresh = !Previous.FileExists(*Path) && !Previous.DirectoryExists(*Path)
			&& !Previous.FileExists(*(Path + TEXT(".tmp"))) && !Previous.DirectoryExists(*(Path + TEXT(".tmp")))
			&& !Previous.FileExists(*(Path + TEXT(".bad"))) && !Previous.DirectoryExists(*(Path + TEXT(".bad")));
		Check(bFresh, TEXT("guid_namespace_unoccupied"));
		if (!bFresh) return false; // Never delete an occupied namespace, even on a GUID collision.
		struct FFixtureCleanup
		{
			FString Name;
			~FFixtureCleanup() { FMMOMailStore::DeleteBox(Name); }
		} Cleanup{Name};
		UE_LOG(LogTemp, Display, TEXT("[MMO_MAIL_READ_FAILURE_FIXTURE] path=%s"), *FPaths::ConvertRelativePathToFull(Path));
		FMMOMail OldMail;
		OldMail.From = TEXT("ReadFailureSender"); OldMail.Text = TEXT("original mailbox mail");
		OldMail.Gold = 137; OldMail.SentUnix = 1700000001;
		FMMOMail NewMail = OldMail;
		NewMail.Text = TEXT("new mail after read restoration"); NewMail.Gold = 29; NewMail.SentUnix = 1700000002;
		TArray<FMMOMail> Loaded{OldMail};
		Check(FMMOMailStore::Load(Name, Loaded) && Loaded.IsEmpty(), TEXT("missing_load_success_resets_output"));
		const bool bSeeded = FMMOMailStore::Append(Name, OldMail);
		Check(bSeeded, TEXT("seed_real_store_append"));
		if (!bSeeded) return false;
		Check(FMMOMailStore::Load(Name, Loaded) && Loaded.Num() == 1 && MMOMailReadFailureSameMail(Loaded[0], OldMail), TEXT("normal_load_original_mail"));
		TArray<uint8> Before;
		const bool bSnapshot = MMOMailReadFailureReadBytes(Previous, Path, Before) && !Before.IsEmpty();
		Check(bSnapshot, TEXT("snapshot_real_mailbox_bytes"));
		if (!bSnapshot) return false;

		// Intentionally no delete: unrelated IO threads may retain this pointer after RAII restoration.
		FMMOMailReadFailurePlatformFile* const Fault = new FMMOMailReadFailurePlatformFile(Previous, Path);
		{
			FMMOMailReadFailureScope Scope(*Fault);
			Check(IFileManager::Get().FileExists(*Path), TEXT("unreadable_mailbox_still_exists"));
			const auto BytesPreserved = [&]()
			{
				TArray<uint8> After;
				return MMOMailReadFailureReadBytes(Previous, Path, After) && Before == After;
			};
			int32 Reads = Fault->DeniedReads.GetValue();
			Loaded = {NewMail};
			const bool bLoaded = FMMOMailStore::Load(Name, Loaded);
			Check(!bLoaded && Loaded.IsEmpty(), TEXT("existing_read_failure_false_resets_output"));
			Check(Fault->DeniedReads.GetValue() == Reads + 1, TEXT("load_reached_exact_read_fault"));
			Check(BytesPreserved(), TEXT("load_preserves_bytes"));
			Reads = Fault->DeniedReads.GetValue();
			Check(!FMMOMailStore::Append(Name, NewMail), TEXT("append_rejects_read_failure"));
			Check(Fault->DeniedReads.GetValue() == Reads + 1, TEXT("append_reached_exact_read_fault"));
			Check(BytesPreserved(), TEXT("append_preserves_original_bytes"));
			Reads = Fault->DeniedReads.GetValue();
			Loaded = {NewMail};
			const bool bTaken = FMMOMailStore::TakeAll(Name, Loaded);
			Check(!bTaken && Loaded.IsEmpty(), TEXT("take_all_read_failure_false_resets_output"));
			Check(Fault->DeniedReads.GetValue() == Reads + 1, TEXT("take_all_reached_exact_read_fault"));
			Check(BytesPreserved(), TEXT("take_all_preserves_bytes"));
			Reads = Fault->DeniedReads.GetValue();
			Check(FMMOMailStore::Count(Name) == 0, TEXT("count_read_failure_returns_zero"));
			Check(Fault->DeniedReads.GetValue() == Reads + 1, TEXT("count_reached_exact_read_fault"));
			Check(BytesPreserved(), TEXT("count_preserves_bytes"));
			Check(Fault->Writes.GetValue() == 0 && Fault->Moves.GetValue() == 0 && Fault->Deletes.GetValue() == 0, TEXT("read_failure_no_write_move_delete_attempts"));
			Check(!Previous.FileExists(*(Path + TEXT(".tmp"))) && !Previous.FileExists(*(Path + TEXT(".bad"))), TEXT("read_failure_no_tmp_or_quarantine"));
		}
		UE_LOG(LogTemp, Display, TEXT("[MMO_MAIL_READ_FAILURE_IO] atomic=%d denied_reads=%d writes=%d moves=%d deletes=%d"),
			USE_ATOMIC_PLATFORM_FILE, Fault->DeniedReads.GetValue(), Fault->Writes.GetValue(), Fault->Moves.GetValue(), Fault->Deletes.GetValue());
		Check(&FPlatformFileManager::Get().GetPlatformFile() == &Previous, TEXT("platform_file_restored"));
		Check(FMMOMailStore::Load(Name, Loaded) && Loaded.Num() == 1 && MMOMailReadFailureSameMail(Loaded[0], OldMail), TEXT("restored_read_retains_original_mail"));
		Check(FMMOMailStore::Append(Name, NewMail), TEXT("restored_normal_append_succeeds"));
		Check(FMMOMailStore::Load(Name, Loaded) && Loaded.Num() == 2
			&& MMOMailReadFailureSameMail(Loaded[0], OldMail) && MMOMailReadFailureSameMail(Loaded[1], NewMail), TEXT("restored_append_retains_old_and_new_mail_in_order"));
		return bPassed;
	}

	class FMMOMailReadFailureTestExec final : public FSelfRegisteringExec
	{
		bool Exec(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override
		{
			if (!FParse::Command(&Cmd, TEXT("MMOTestMailReadFailure"))) return false;
			FString Error;
			int32 Checks = 0;
			const bool bPassed = RunMMOMailReadFailureSelfTest(World, Error, Checks);
			UE_LOG(LogTemp, Display, TEXT("[MMO_MAIL_READ_FAILURE_TEST] %s checks=%d reason=%s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Checks, *Error);
			return true;
		}
	};
	FMMOMailReadFailureTestExec GMMOMailReadFailureTestExec;
}
#endif

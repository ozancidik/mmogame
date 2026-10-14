// Copyright Epic Games, Inc. All Rights Reserved.

#include "Database/DatabaseMigrator.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "GenericPlatform/GenericPlatformFile.h"

// NOT USED (QA/security round 6, docs/SIX_MAP_PLAN.md section 1.5): there is no database backend in this module.
// Every DB operation below fails closed with LastError = DbMigratorNotImplemented; migrations are applied with
// psql/Flyway. Keep or delete this class = owner decision S15 (do not delete without it).
namespace
{
	const TCHAR* const DbMigratorNotImplemented = TEXT("not implemented: no database backend");

	// Forward migrations only: V{n}__*_rollback.sql must never run in the forward direction.
	bool DbMigratorIsRollbackFile(const FString& FileName)
	{
		return FileName.EndsWith(TEXT("_rollback.sql"), ESearchCase::IgnoreCase);
	}
}

UDatabaseMigrator::UDatabaseMigrator()
	: CurrentVersion(0)
	, MigrationDirectory(FPaths::ProjectDir() / TEXT("Migrations"))
{
}

bool UDatabaseMigrator::Initialize(const FString& InConnectionString)
{
	ConnectionString = InConnectionString;
	LastError.Empty();
	CurrentVersion = 0;

	if (ConnectionString.IsEmpty())
	{
		LastError = TEXT("Connection string is empty");
		return false;
	}

	// Test connection and initialize schema_version table
	if (!CreateSchemaVersionTable())
	{
		return false;
	}

	CurrentVersion = GetAppliedVersion();
	return true;
}

bool UDatabaseMigrator::CreateSchemaVersionTable()
{
	const FString SQL = TEXT(
		"CREATE TABLE IF NOT EXISTS schema_version ("
		"    version INTEGER PRIMARY KEY,"
		"    name VARCHAR(255) NOT NULL,"
		"    applied_at TIMESTAMPTZ NOT NULL DEFAULT NOW()"
		");"
	);

	return ExecuteSQL(SQL);
}

int32 UDatabaseMigrator::GetAppliedVersion()
{
	// No backend: the applied version is unknown. 0 = "not initialized" (header contract) + LastError.
	LastError = DbMigratorNotImplemented;
	UE_LOG(LogTemp, Warning, TEXT("[DatabaseMigrator] GetAppliedVersion: %s"), *LastError);
	return 0;
}

bool UDatabaseMigrator::RunMigrations()
{
	if (!IsDatabaseInitialized())
	{
		LastError = FString::Printf(TEXT("Database not initialized: %s"), *LastError);
		return false;
	}

	const TArray<int32> Migrations = GetAvailableMigrations();
	
	for (int32 Version : Migrations)
	{
		if (Version > CurrentVersion)
		{
			if (!RunMigration(Version))
			{
				return false;
			}
			CurrentVersion = Version;
		}
	}

	return true;
}

bool UDatabaseMigrator::RunMigration(int32 Version)
{
	const FString MigrationFile = FString::Printf(TEXT("V%d__*.sql"), Version);
	
	// Find the actual migration file
	IFileManager& FileManager = IFileManager::Get();
	TArray<FString> Files;
	FileManager.FindFiles(Files, *MigrationDirectory, *MigrationFile);
	Files.RemoveAll([](const FString& File) { return DbMigratorIsRollbackFile(File); });

	if (Files.Num() == 0)
	{
		LastError = FString::Printf(TEXT("Migration file not found for version %d"), Version);
		return false;
	}

	const FString FilePath = MigrationDirectory / Files[0];
	
	if (!ExecuteSQLFile(FilePath))
	{
		LastError = FString::Printf(TEXT("Failed to execute migration %d: %s"), Version, *LastError);
		return false;
	}

	// Record migration
	if (!RecordMigration(Version, Files[0]))
	{
		LastError = FString::Printf(TEXT("Failed to record migration %d"), Version);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[DatabaseMigrator] Applied migration %d: %s"), Version, *Files[0]);
	return true;
}

bool UDatabaseMigrator::RollbackMigration(int32 Version)
{
	// Find rollback file (V{Version}__*_rollback.sql)
	const FString RollbackFile = FString::Printf(TEXT("V%d__*_rollback.sql"), Version);
	
	IFileManager& FileManager = IFileManager::Get();
	TArray<FString> Files;
	FileManager.FindFiles(Files, *MigrationDirectory, *RollbackFile);
	
	if (Files.Num() == 0)
	{
		LastError = FString::Printf(TEXT("Rollback file not found for version %d"), Version);
		return false;
	}

	const FString FilePath = MigrationDirectory / Files[0];
	
	if (!ExecuteSQLFile(FilePath))
	{
		LastError = FString::Printf(TEXT("Failed to execute rollback %d: %s"), Version, *LastError);
		return false;
	}

	// Remove migration record
	if (!RemoveMigrationRecord(Version))
	{
		LastError = FString::Printf(TEXT("Failed to remove migration record %d"), Version);
		return false;
	}

	CurrentVersion = Version - 1;
	UE_LOG(LogTemp, Log, TEXT("[DatabaseMigrator] Rolled back migration %d"), Version);
	return true;
}

bool UDatabaseMigrator::RecordMigration(int32 Version, const FString& Name)
{
	// Would be: INSERT INTO schema_version (version, name) VALUES (?, ?). No backend: fail closed.
	LastError = DbMigratorNotImplemented;
	UE_LOG(LogTemp, Warning, TEXT("[DatabaseMigrator] RecordMigration(%d, %s): %s"), Version, *Name, *LastError);
	return false;
}

bool UDatabaseMigrator::RemoveMigrationRecord(int32 Version)
{
	// Would be: DELETE FROM schema_version WHERE version = ?. No backend: fail closed.
	LastError = DbMigratorNotImplemented;
	UE_LOG(LogTemp, Warning, TEXT("[DatabaseMigrator] RemoveMigrationRecord(%d): %s"), Version, *LastError);
	return false;
}

bool UDatabaseMigrator::ExecuteSQLFile(const FString& FilePath)
{
	FString SQLContent;
	if (!FFileHelper::LoadFileToString(SQLContent, *FilePath))
	{
		LastError = FString::Printf(TEXT("Failed to read migration file: %s"), *FilePath);
		return false;
	}

	return ExecuteSQL(SQLContent);
}

bool UDatabaseMigrator::ExecuteSQL(const FString& SQL)
{
	// No PostgreSQL client (libpq) is linked. Previously this returned true without running anything, so
	// Initialize/RunMigrations reported success for SQL that never executed. Fail closed instead.
	UE_LOG(LogTemp, Verbose, TEXT("[DatabaseMigrator] Not executing SQL: %s"), *SQL.Left(500));
	LastError = DbMigratorNotImplemented;
	UE_LOG(LogTemp, Warning, TEXT("[DatabaseMigrator] ExecuteSQL: %s"), *LastError);
	return false;
}

bool UDatabaseMigrator::IsDatabaseInitialized() const
{
	// Would check that the schema_version table exists. No backend: fail closed.
	LastError = DbMigratorNotImplemented;
	UE_LOG(LogTemp, Warning, TEXT("[DatabaseMigrator] IsDatabaseInitialized: %s"), *LastError);
	return false;
}

TArray<int32> UDatabaseMigrator::GetAvailableMigrations() const
{
	TArray<int32> Migrations;
	
	IFileManager& FileManager = IFileManager::Get();
	TArray<FString> Files;
	FileManager.FindFiles(Files, *MigrationDirectory, TEXT("V*__*.sql"));
	
	for (const FString& File : Files)
	{
		if (DbMigratorIsRollbackFile(File))
		{
			continue; // V{n}__*_rollback.sql is not a forward migration
		}
		// Extract version from filename (V{Version}__name.sql)
		FString VersionStr;
		if (File.Split(TEXT("__"), &VersionStr, nullptr))
		{
			VersionStr.RemoveFromStart(TEXT("V"));
			int32 Version = FCString::Atoi(*VersionStr);
			if (Version > 0)
			{
				Migrations.AddUnique(Version);
			}
		}
	}
	
	Migrations.Sort();
	return Migrations;
}

bool UDatabaseMigrator::ValidateSchema() const
{
	// Would check that all tables/columns/indexes exist. No backend: fail closed.
	LastError = DbMigratorNotImplemented;
	UE_LOG(LogTemp, Warning, TEXT("[DatabaseMigrator] ValidateSchema: %s"), *LastError);
	return false;
}

bool UDatabaseMigrator::BP_Initialize(const FString& InConnectionString)
{
	return Initialize(InConnectionString);
}

bool UDatabaseMigrator::BP_RunMigrations()
{
	return RunMigrations();
}

bool UDatabaseMigrator::BP_RollbackMigration(int32 Version)
{
	return RollbackMigration(Version);
}

int32 UDatabaseMigrator::BP_GetCurrentVersion()
{
	return CurrentVersion;
}

FString UDatabaseMigrator::BP_GetLastError()
{
	return LastError;
}

int32 UDatabaseMigrator::GetCurrentVersion() const
{
	return CurrentVersion;
}

FString UDatabaseMigrator::GetLastError() const
{
	return LastError;
}
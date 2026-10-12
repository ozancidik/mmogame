// Copyright Epic Games, Inc. All Rights Reserved.

#include "Database/DatabaseMigrator.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "GenericPlatform/GenericPlatformFile.h"

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
	// TODO: Implement actual database query
	// For now, return 0 to indicate no migrations applied
	return 0;
}

bool UDatabaseMigrator::RunMigrations()
{
	if (!IsDatabaseInitialized())
	{
		LastError = TEXT("Database not initialized");
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
	// TODO: Implement actual database insert
	// INSERT INTO schema_version (version, name) VALUES (?, ?)
	return true;
}

bool UDatabaseMigrator::RemoveMigrationRecord(int32 Version)
{
	// TODO: Implement actual database delete
	// DELETE FROM schema_version WHERE version = ?
	return true;
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
	// TODO: Implement actual PostgreSQL execution using libpq or similar
	// For now, log the SQL that would be executed
	UE_LOG(LogTemp, Verbose, TEXT("[DatabaseMigrator] Executing SQL: %s"), *SQL.Left(500));
	
	// TODO: Actual implementation would use libpq:
	// PGconn* conn = PQconnectdb(TCHAR_TO_UTF8(*ConnectionString));
	// PGresult* res = PQexec(conn, TCHAR_TO_UTF8(*SQL));
	// Check PQresultStatus(res) == PGRES_COMMAND_OK
	// PQclear(res);
	// PQfinish(conn);
	
	return true;
}

bool UDatabaseMigrator::IsDatabaseInitialized() const
{
	// TODO: Check if schema_version table exists
	return true;
}

TArray<int32> UDatabaseMigrator::GetAvailableMigrations() const
{
	TArray<int32> Migrations;
	
	IFileManager& FileManager = IFileManager::Get();
	TArray<FString> Files;
	FileManager.FindFiles(Files, *MigrationDirectory, TEXT("V*__*.sql"));
	
	for (const FString& File : Files)
	{
		// Extract version from filename (V{Version}__name.sql)
		FString VersionStr;
		if (File.Split(TEXT("__"), &VersionStr, nullptr))
		{
			VersionStr.RemoveFromStart(TEXT("V"));
			int32 Version = FCString::Atoi(*VersionStr);
			if (Version > 0)
			{
				Migrations.Add(Version);
			}
		}
	}
	
	Migrations.Sort();
	return Migrations;
}

bool UDatabaseMigrator::ValidateSchema() const
{
	// TODO: Implement schema validation
	// Check all tables exist, columns match, indexes exist, etc.
	return true;
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
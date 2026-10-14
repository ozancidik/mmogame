// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DatabaseMigrator.generated.h"

/**
 * PostgreSQL database migration system (STUB, NOT USED).
 *
 * There is no database backend (no libpq) in this module: every operation that would touch the
 * database (ExecuteSQL, RecordMigration, RemoveMigrationRecord, IsDatabaseInitialized, ValidateSchema,
 * GetAppliedVersion) fails closed and sets LastError = "not implemented: no database backend", so
 * Initialize / RunMigrations / RollbackMigration always return false. Nothing in the game calls it.
 * Migrations under Migrations/ are applied with psql or Flyway (docs/SIX_MAP_PLAN.md section 1.5).
 * Keeping or deleting this class is owner decision S15.
 */
UCLASS()
class MYPROJECT_API UDatabaseMigrator : public UObject
{
	GENERATED_BODY()

public:
	UDatabaseMigrator();

	/**
	 * Initializes the migration system
	 * @param ConnectionString PostgreSQL connection string
	 * @return true if initialization successful
	 */
	bool Initialize(const FString& ConnectionString);

	/**
	 * Runs all pending migrations
	 * @return true if all migrations successful
	 */
	bool RunMigrations();

	/**
	 * Runs a specific migration by version
	 * @param Version Migration version to run
	 * @return true if successful
	 */
	bool RunMigration(int32 Version);

	/**
	 * Rolls back a specific migration
	 * @param Version Migration version to rollback
	 * @return true if successful
	 */
	bool RollbackMigration(int32 Version);

	/**
	 * Gets the current database schema version
	 * @return Current version, 0 if not initialized
	 */
	int32 GetCurrentVersion() const;

	/**
	 * Gets list of available migrations
	 * @return Array of migration versions
	 */
	TArray<int32> GetAvailableMigrations() const;

	/**
	 * Checks if database is initialized
	 * @return true if schema_version table exists
	 */
	bool IsDatabaseInitialized() const;

	/**
	 * Gets the last migration error
	 * @return Last error message
	 */
	FString GetLastError() const;

	/**
	 * Validates database schema integrity
	 * @return true if schema is valid
	 */
	bool ValidateSchema() const;

	// Blueprint callable wrappers
	UFUNCTION(BlueprintCallable, Category = "Database|Migration")
	bool BP_Initialize(const FString& ConnectionString);

	UFUNCTION(BlueprintCallable, Category = "Database|Migration")
	bool BP_RunMigrations();

	UFUNCTION(BlueprintCallable, Category = "Database|Migration")
	bool BP_RollbackMigration(int32 Version);

	UFUNCTION(BlueprintCallable, Category = "Database|Migration")
	int32 BP_GetCurrentVersion();

	UFUNCTION(BlueprintCallable, Category = "Database|Migration")
	FString BP_GetLastError();

protected:
	/** PostgreSQL connection string */
	FString ConnectionString;

	/** Current schema version */
	int32 CurrentVersion;

	/** Last error message (mutable: const checks also report "not implemented") */
	mutable FString LastError;

	/** Migration directory path */
	FString MigrationDirectory;

	/** Creates the schema_version table if it doesn't exist */
	bool CreateSchemaVersionTable();

	/** Gets the applied version from database */
	int32 GetAppliedVersion();

	/** Records a migration as applied */
	bool RecordMigration(int32 Version, const FString& Name);

	/** Removes a migration record */
	bool RemoveMigrationRecord(int32 Version);

	/** Executes SQL from file */
	bool ExecuteSQLFile(const FString& FilePath);

	/** Executes raw SQL */
	bool ExecuteSQL(const FString& SQL);
};
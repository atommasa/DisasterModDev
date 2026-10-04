// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RPGIdMigration.h"

class URPGSaveGame;
struct FRPGReleaseMigrationCatalog;

enum class ERPGSaveGameMigrationResult : uint8
{
	Success,
	InvalidReleaseCatalog,
	InvalidMigrationChain,
	ContainerKeyCollision
};

struct SAVESYSTEM_API FRPGSaveGameMigrationResult
{
	ERPGSaveGameMigrationResult Code = ERPGSaveGameMigrationResult::Success;
	int32 MigratedIdCount = 0;
	FString Diagnostic;

	bool IsSuccess() const { return Code == ERPGSaveGameMigrationResult::Success; }
};

class SAVESYSTEM_API FRPGSaveGameMigrator
{
public:
	static FRPGSaveGameMigrationResult MigrateToCurrent(URPGSaveGame& SaveGame);
	static FRPGSaveGameMigrationResult Migrate(URPGSaveGame& SaveGame, const FRPGReleaseMigrationCatalog& Catalog);

	static FRPGSaveGameMigrationResult Migrate(URPGSaveGame& SaveGame, uint16 TargetVersion,
		TConstArrayView<FRPGIdMigrationStep> Steps);
};

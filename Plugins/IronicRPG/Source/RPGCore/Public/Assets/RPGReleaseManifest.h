// Copyright Ironic Studio. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RPGIdMigration.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "RPGReleaseManifest.generated.h"

USTRUCT()
struct RPGCORE_API FRPGReleaseClaim
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Release")
	FName Id;
	UPROPERTY(VisibleAnywhere, Category = "Release")
	FSoftObjectPath OwnerPath;
	UPROPERTY(VisibleAnywhere, Category = "Release")
	FSoftClassPath OwnerClass;
};

/** Immutable authored identity snapshot; paths describe the release, not current ownership. */
USTRUCT()
struct RPGCORE_API FRPGReleaseSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Release")
	int32 Schema = 2;
	UPROPERTY(VisibleAnywhere, Category = "Release")
	FString ReleaseId;
	UPROPERTY(VisibleAnywhere, Category = "Release")
	uint16 SaveDataVersion = 1;
	UPROPERTY(VisibleAnywhere, Category = "Release")
	FString PreviousHash;
	UPROPERTY(VisibleAnywhere, Category = "Release")
	TArray<FRPGIdMigrationStep> SaveGameMigrations;
	UPROPERTY(VisibleAnywhere, Category = "Release")
	TArray<FRPGReleaseClaim> Claims;

	bool Validate(FText& OutError) const;
	FString ComputeHash() const;
	static bool IsValidReleaseId(const FString& Value);
};

struct RPGCORE_API FRPGReleaseMigrationCatalog
{
	uint16 CurrentVersion = 1;
	TArray<FRPGIdMigrationStep> Steps;
};

enum class ERPGReleaseMigrationCatalogResult : uint8
{
	Success,
	IncompleteHead,
	ManifestUnavailable,
	HashMismatch,
	InvalidSnapshot
};

struct RPGCORE_API FRPGReleaseMigrationCatalogResult
{
	ERPGReleaseMigrationCatalogResult Code = ERPGReleaseMigrationCatalogResult::Success;
	FString Diagnostic;

	bool IsSuccess() const { return Code == ERPGReleaseMigrationCatalogResult::Success; }
};

class RPGCORE_API FRPGReleaseMigrationCatalogReader
{
public:
	static FRPGReleaseMigrationCatalogResult ReadCurrent(FRPGReleaseMigrationCatalog& OutCatalog);
	static FRPGReleaseMigrationCatalogResult ReadSnapshot(const FRPGReleaseSnapshot& Snapshot, const FString& ExpectedHash,
		FRPGReleaseMigrationCatalog& OutCatalog);
};

UCLASS(HideDropdown, NotBlueprintable)
class RPGCORE_API URPGReleaseManifest final : public UDataAsset
{
	GENERATED_BODY()
	friend class FRPGReleaseSealService;

public:
	const FRPGReleaseSnapshot& GetSnapshot() const { return Snapshot; }
	const FString& GetSemanticHash() const { return SemanticHash; }
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* Property) const override { return false; }
#endif

private:
	UPROPERTY(VisibleAnywhere, Category = "Release")
	FRPGReleaseSnapshot Snapshot;
	UPROPERTY(VisibleAnywhere, Category = "Release")
	FString SemanticHash;
	UPROPERTY(VisibleAnywhere, Category = "Metadata")
	FDateTime CreatedAt;
};

/** Source-controlled head. Only the Seal service commits these fields. */
UCLASS(Config = RPGRelease, DefaultConfig, meta=(DisplayName = "RPG Release History"))
class RPGCORE_API URPGReleaseSettings final : public UDeveloperSettings
{
	GENERATED_BODY()
	friend class FRPGReleaseSealService;

public:
	virtual FName GetCategoryName() const override { return TEXT("IronicRPG"); }
	const FString& GetHeadManifest() const { return HeadManifest; }
	const FString& GetHeadHash() const { return HeadHash; }
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* Property) const override { return false; }
#endif

private:
	UPROPERTY(Config, VisibleAnywhere, Category = "History")
	FString HeadManifest;
	UPROPERTY(Config, VisibleAnywhere, Category = "History")
	FString HeadHash;
};

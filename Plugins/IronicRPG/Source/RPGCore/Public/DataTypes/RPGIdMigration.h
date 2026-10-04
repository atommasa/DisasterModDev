// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RPGId.h"
#include "RPGIdMigration.generated.h"

USTRUCT()
struct RPGCORE_API FRPGIdRedirect
{
	GENERATED_BODY()

	UPROPERTY()
	FRPGId OldId;

	UPROPERTY()
	FRPGId NewId;

	FRPGIdRedirect() = default;
	FRPGIdRedirect(const FRPGId& InOldId, const FRPGId& InNewId) : OldId(InOldId), NewId(InNewId) {}
};

USTRUCT()
struct RPGCORE_API FRPGIdMigrationStep
{
	GENERATED_BODY()

	UPROPERTY()
	uint16 FromVersion = 0;

	UPROPERTY()
	uint16 ToVersion = 0;

	UPROPERTY()
	TArray<FRPGIdRedirect> Redirects;

	FRPGIdMigrationStep() = default;
	FRPGIdMigrationStep(uint16 InFromVersion, uint16 InToVersion, TArray<FRPGIdRedirect> InRedirects)
		: FromVersion(InFromVersion), ToVersion(InToVersion), Redirects(MoveTemp(InRedirects)) {}
};

enum class ERPGIdMigrationResult : uint8
{
	Success,
	SourceVersionIsNewer,
	DuplicateVersionStep,
	MissingVersionStep,
	NonSequentialVersionStep,
	InvalidRedirectId,
	RedirectTypeMismatch,
	DuplicateRedirectSource,
	DuplicateRedirectTarget
};

struct RPGCORE_API FRPGIdMigrationResult
{
	ERPGIdMigrationResult Code = ERPGIdMigrationResult::Success;
	FString Diagnostic;

	bool IsSuccess() const { return Code == ERPGIdMigrationResult::Success; }
};

class RPGCORE_API FRPGIdMigrationChain
{
public:
	static FRPGIdMigrationResult Build(uint16 SourceVersion, uint16 TargetVersion, TConstArrayView<FRPGIdMigrationStep> Steps,
		FRPGIdMigrationChain& OutChain);

	bool TryResolve(const FRPGId& SourceId, FRPGId& OutId) const;

private:
	TArray<TMap<FName, FName>> OrderedRedirects;
	bool bIsValid = false;
};

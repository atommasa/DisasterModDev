// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "RPGPrimaryAssetDataValidator.generated.h"

struct FRPGPrimaryAssetValidationSubject
{
	FSoftObjectPath AssetPath;
	FName ClaimedId = NAME_None;
	FName ExpectedPrefix = NAME_None;
	int32 NumericLength = 0;
	bool bSupportedOwner = false;
	bool bConfigurationValid = false;
	bool bFormatValid = false;
};

struct FRPGPrimaryAssetValidationResult
{
	bool IsValid() const { return Errors.IsEmpty(); }

	TArray<FText> Errors;
	TArray<FText> Warnings;
};

/** Release validation for RPGPrimaryAsset ownership. Interactive Save permits an unclaimed draft with a warning;
 * manual, commandlet, pre-submit and Cook validation require a valid unique Claim.
 */
UCLASS()
class URPGPrimaryAssetDataValidator final : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	URPGPrimaryAssetDataValidator();

	static FRPGPrimaryAssetValidationResult BuildValidationResult(const FRPGPrimaryAssetValidationSubject& Subject,
		const TMap<FSoftObjectPath, FName>& WorkspaceOwners, const FText& SnapshotFailure, bool bRequireClaim);
	static bool ShouldRequireClaim(EDataValidationUsecase ValidationUsecase, bool bIsCookCommandlet);

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject,
		FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset,
		FDataValidationContext& Context) override;
};

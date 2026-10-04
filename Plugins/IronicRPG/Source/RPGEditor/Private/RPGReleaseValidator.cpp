// Copyright Ironic Studio. All Rights Reserved.
#include "RPGReleaseValidator.h"
#include "RPGReleaseSealService.h"
#include "Assets/RPGPrimaryAsset.h"
#include "Engine/World.h"

bool URPGReleaseValidator::CanValidateAsset_Implementation(const FAssetData&, UObject* Object, FDataValidationContext&) const
{
	return Object && (Object->IsA<URPGReleaseManifest>() || Object->IsA<URPGPrimaryAsset>() || Object->IsA<UWorld>());
}

EDataValidationResult URPGReleaseValidator::ValidateLoadedAsset_Implementation(const FAssetData&, UObject* Object, FDataValidationContext&)
{
	FText Error;
	if (!FRPGReleaseSealService::ValidateHistory(Error))
	{
		AssetFails(Object, Error);
		return EDataValidationResult::Invalid;
	}
	AssetPasses(Object);
	return EDataValidationResult::Valid;
}

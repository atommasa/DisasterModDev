// Copyright Ironic Studio. All Rights Reserved.
#pragma once
#include "EditorValidatorBase.h"
#include "RPGReleaseValidator.generated.h"

UCLASS()
class URPGReleaseValidator final : public UEditorValidatorBase
{
	GENERATED_BODY()

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& Data, UObject* Object, FDataValidationContext& Context) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& Data, UObject* Object, FDataValidationContext& Context) override;
};

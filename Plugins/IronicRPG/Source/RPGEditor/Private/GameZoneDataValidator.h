// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "GameZoneBindingCoordinator.h"
#include "Levels/GameZoneAsset.h"
#include "GameZoneDataValidator.generated.h"

struct FGameZoneValidationSubject
{
    FSoftObjectPath AssetPath;
    FName LevelPackage = NAME_None;
};

struct FGameZoneReleaseValidationResult
{
    bool IsValid() const { return Errors.IsEmpty(); }

    TArray<FText> Errors;
};

UCLASS()
class UGameZoneDataValidator final : public UEditorValidatorBase
{
    GENERATED_BODY()

public:
    UGameZoneDataValidator();

    static FGameZoneReleaseValidationResult BuildValidationResult(
        const FGameZoneValidationSubject& Subject,
        const FGameZoneGlobalBindingAuditResult& BindingAudit,
        const FGameZoneMapBakeAuditResult* BakeAudit = nullptr);
    static bool ShouldAuditWorldBake(EDataValidationUsecase ValidationUsecase, bool bIsCookCommandlet);

protected:
    virtual bool CanValidateAsset_Implementation(
        const FAssetData& InAssetData,
        UObject* InObject,
        FDataValidationContext& InContext) const override;

    virtual EDataValidationResult ValidateLoadedAsset_Implementation(
        const FAssetData& InAssetData,
        UObject* InAsset,
        FDataValidationContext& Context) override;
};

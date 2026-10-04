// Copyright Ironic Studio. All Rights Reserved.

#include "GameZoneDataValidator.h"

#include "Levels/RPGWorldSettings.h"

#include "Engine/World.h"
#include "Misc/DataValidation.h"

namespace
{
    void AddUniqueError(TArray<FText>& Errors, TSet<FString>& SeenErrors, const FText& Error)
    {
        const FString ErrorString = Error.ToString();
        if (!ErrorString.IsEmpty() && !SeenErrors.Contains(ErrorString))
        {
            SeenErrors.Add(ErrorString);
            Errors.Add(Error);
        }
    }

    bool IsBindingIssueRelevant(
        const FGameZoneGlobalBindingIssue& Issue,
        const FGameZoneValidationSubject& Subject)
    {
        return (!Subject.AssetPath.IsNull()
                && (Issue.AssetPath == Subject.AssetPath || Issue.RelatedAssetPath == Subject.AssetPath))
            || (!Subject.LevelPackage.IsNone() && Issue.LevelPackage == Subject.LevelPackage);
    }

    bool HasGameZoneClaim(const UWorld& World)
    {
        const ARPGWorldSettings* Settings = Cast<ARPGWorldSettings>(World.GetWorldSettings());
        return Settings
            && (Settings->GetGameZoneId().IsValid() || Settings->GetGameZoneBindingId().IsValid());
    }
}

UGameZoneDataValidator::UGameZoneDataValidator()
{
    bOnlyPrintCustomMessage = true;
}

FGameZoneReleaseValidationResult UGameZoneDataValidator::BuildValidationResult(
    const FGameZoneValidationSubject& Subject,
    const FGameZoneGlobalBindingAuditResult& BindingAudit,
    const FGameZoneMapBakeAuditResult* BakeAudit)
{
    FGameZoneReleaseValidationResult Result;
    TSet<FString> SeenErrors;

    for (const FGameZoneGlobalBindingIssue& Issue : BindingAudit.Issues)
    {
        if (IsBindingIssueRelevant(Issue, Subject))
        {
            AddUniqueError(Result.Errors, SeenErrors, Issue.Message);
        }
    }

    if (BakeAudit && !BakeAudit->IsVerifiedForRelease())
    {
        for (const FText& Issue : BakeAudit->Issues)
        {
            AddUniqueError(Result.Errors, SeenErrors, Issue);
        }

        if (BakeAudit->Issues.IsEmpty())
        {
            AddUniqueError(
                Result.Errors,
                SeenErrors,
                FText::FromString(TEXT("Game Zone Bake data is stale or cannot be verified.")));
        }
    }

    return Result;
}

bool UGameZoneDataValidator::ShouldAuditWorldBake(
    const EDataValidationUsecase ValidationUsecase,
    const bool bIsCookCommandlet)
{
    return ValidationUsecase != EDataValidationUsecase::Save || bIsCookCommandlet;
}

bool UGameZoneDataValidator::CanValidateAsset_Implementation(
    const FAssetData&,
    UObject* InObject,
    FDataValidationContext&) const
{
    if (InObject && InObject->IsA<UGameZoneAsset>())
    {
        return true;
    }

    const UWorld* World = Cast<UWorld>(InObject);
    return World && HasGameZoneClaim(*World);
}

EDataValidationResult UGameZoneDataValidator::ValidateLoadedAsset_Implementation(
    const FAssetData&,
    UObject* InAsset,
    FDataValidationContext& Context)
{
    FGameZoneValidationSubject Subject;
    TOptional<FGameZoneMapBakeAuditResult> BakeAudit;
    FText BoundAssetFailure;

    if (UGameZoneAsset* ZoneAsset = Cast<UGameZoneAsset>(InAsset))
    {
        Subject.AssetPath = FSoftObjectPath(ZoneAsset);
        Subject.LevelPackage = FName(*ZoneAsset->GetLevelToLoad().ToSoftObjectPath().GetLongPackageName());
        BakeAudit.Emplace(ZoneAsset->AuditMapDataForRelease());
    }
    else if (UWorld* World = Cast<UWorld>(InAsset))
    {
        Subject.LevelPackage = World->GetOutermost()->GetFName();
        if (UGameZoneAsset* BoundZoneAsset = FGameZoneBindingCoordinator::FindExactBoundAsset(*World, &BoundAssetFailure))
        {
            Subject.AssetPath = FSoftObjectPath(BoundZoneAsset);
            if (ShouldAuditWorldBake(Context.GetValidationUsecase(), IsRunningCookCommandlet()))
            {
                BakeAudit.Emplace(BoundZoneAsset->AuditMapDataForRelease());
            }
        }
    }

    FGameZoneReleaseValidationResult Result = BuildValidationResult(
        Subject,
        FGameZoneBindingCoordinator::AuditProjectBindings(),
        BakeAudit.IsSet() ? &BakeAudit.GetValue() : nullptr);
    if (!BoundAssetFailure.IsEmpty())
    {
        Result.Errors.Add(BoundAssetFailure);
    }

    if (Result.IsValid())
    {
        AssetPasses(InAsset);
        return EDataValidationResult::Valid;
    }

    for (const FText& Error : Result.Errors)
    {
        AssetFails(InAsset, Error);
    }
    return EDataValidationResult::Invalid;
}

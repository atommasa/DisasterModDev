// Copyright Ironic Studio. All Rights Reserved.

#include "Levels/GameZoneBinding.h"

#if WITH_EDITOR

#include "Levels/GameZoneAsset.h"
#include "Levels/RPGWorldSettings.h"

#include "Engine/World.h"

namespace
{
    FApplyGameZoneBindingPropertyChange ApplyGameZoneBindingPropertyChange;

    FGameZoneBindingAuditResult MakeBindingIssue(
        const EGameZoneBindingIssue Issue,
        const FString& Message,
        const FName ExpectedLevelPackage = NAME_None,
        const FName ActualLevelPackage = NAME_None)
    {
        FGameZoneBindingAuditResult Result;
        Result.Issue = Issue;
        Result.Message = FText::FromString(Message);
        Result.ExpectedLevelPackage = ExpectedLevelPackage;
        Result.ActualLevelPackage = ActualLevelPackage;
        return Result;
    }

    FName ResolveExpectedLevelPackage(const UGameZoneAsset& ZoneAsset)
    {
        const TSoftObjectPtr<UWorld> Level = ZoneAsset.GetLevelToLoad();
        if (const UWorld* ResolvedWorld = Level.Get())
        {
            return ResolvedWorld->GetOutermost()->GetFName();
        }

        const FString LongPackageName = Level.ToSoftObjectPath().GetLongPackageName();
        return LongPackageName.IsEmpty() ? NAME_None : FName(*LongPackageName);
    }
}

FApplyGameZoneBindingPropertyChange& GetApplyGameZoneBindingPropertyChangeDelegate()
{
    return ApplyGameZoneBindingPropertyChange;
}

FGameZoneBindingAuditResult AuditGameZoneBinding(const UGameZoneAsset& ZoneAsset)
{
    if (ZoneAsset.GetLevelToLoad().IsNull())
    {
        return MakeBindingIssue(
            EGameZoneBindingIssue::MissingLevel,
            FString::Printf(TEXT("Zone Asset '%s' has no LevelToLoad."), *ZoneAsset.GetPathName()));
    }

    UWorld* World = ZoneAsset.GetLevelToLoad().LoadSynchronous();
    if (!IsValid(World))
    {
        return MakeBindingIssue(
            EGameZoneBindingIssue::LevelUnavailable,
            FString::Printf(
                TEXT("Zone Asset '%s' could not load Level '%s'."),
                *ZoneAsset.GetPathName(),
                *ZoneAsset.GetLevelToLoad().ToSoftObjectPath().ToString()),
            ResolveExpectedLevelPackage(ZoneAsset));
    }

    return AuditGameZoneBindingAgainstWorld(ZoneAsset, *World);
}

FGameZoneBindingAuditResult AuditGameZoneBindingAgainstWorld(const UGameZoneAsset& ZoneAsset, const UWorld& World)
{
    if (!ZoneAsset.GetId().IsValid())
    {
        return MakeBindingIssue(
            EGameZoneBindingIssue::InvalidAssetId,
            FString::Printf(TEXT("Zone Asset '%s' has no valid Id."), *ZoneAsset.GetPathName()));
    }

    if (ZoneAsset.GetLevelToLoad().IsNull())
    {
        return MakeBindingIssue(
            EGameZoneBindingIssue::MissingLevel,
            FString::Printf(TEXT("Zone Asset '%s' has no LevelToLoad."), *ZoneAsset.GetPathName()));
    }

    const FName ExpectedLevelPackage = ResolveExpectedLevelPackage(ZoneAsset);
    const FName ActualLevelPackage = World.GetOutermost()->GetFName();
    if (ExpectedLevelPackage.IsNone() || ExpectedLevelPackage != ActualLevelPackage)
    {
        return MakeBindingIssue(
            EGameZoneBindingIssue::LevelPathMismatch,
            FString::Printf(
                TEXT("Zone Asset '%s' expects Level '%s', but the audited World is '%s'."),
                *ZoneAsset.GetPathName(),
                *ExpectedLevelPackage.ToString(),
                *ActualLevelPackage.ToString()),
            ExpectedLevelPackage,
            ActualLevelPackage);
    }

    const ARPGWorldSettings* WorldSettings = Cast<ARPGWorldSettings>(World.GetWorldSettings());
    if (!WorldSettings)
    {
        return MakeBindingIssue(
            EGameZoneBindingIssue::WrongWorldSettingsClass,
            FString::Printf(
                TEXT("Level '%s' does not use RPGWorldSettings."),
                *ActualLevelPackage.ToString()),
            ExpectedLevelPackage,
            ActualLevelPackage);
    }

    if (WorldSettings->GetGameZoneId() != ZoneAsset.GetId())
    {
        return MakeBindingIssue(
            EGameZoneBindingIssue::ZoneIdMismatch,
            FString::Printf(
                TEXT("Zone Asset '%s' uses Id '%s', but Level '%s' claims '%s'."),
                *ZoneAsset.GetPathName(),
                *ZoneAsset.GetId().ToString(),
                *ActualLevelPackage.ToString(),
                *WorldSettings->GetGameZoneId().ToString()),
            ExpectedLevelPackage,
            ActualLevelPackage);
    }

    const FGuid AssetBindingId = ZoneAsset.GetGameZoneBindingId();
    const FGuid LevelBindingId = WorldSettings->GetGameZoneBindingId();
    if (!AssetBindingId.IsValid() || !LevelBindingId.IsValid())
    {
        return MakeBindingIssue(
            EGameZoneBindingIssue::MissingBindingId,
            FString::Printf(
                TEXT("Zone Asset '%s' and Level '%s' require binding migration."),
                *ZoneAsset.GetPathName(),
                *ActualLevelPackage.ToString()),
            ExpectedLevelPackage,
            ActualLevelPackage);
    }

    if (AssetBindingId != LevelBindingId)
    {
        return MakeBindingIssue(
            EGameZoneBindingIssue::BindingIdMismatch,
            FString::Printf(
                TEXT("Zone Asset '%s' and Level '%s' belong to different binding generations."),
                *ZoneAsset.GetPathName(),
                *ActualLevelPackage.ToString()),
            ExpectedLevelPackage,
            ActualLevelPackage);
    }

    FGameZoneBindingAuditResult Result;
    Result.ExpectedLevelPackage = ExpectedLevelPackage;
    Result.ActualLevelPackage = ActualLevelPackage;
    return Result;
}

#endif // WITH_EDITOR

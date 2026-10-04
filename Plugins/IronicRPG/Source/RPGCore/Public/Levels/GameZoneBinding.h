// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RPGId.h"
#include "GameZoneBinding.generated.h"

class UGameZoneAsset;
class UWorld;

UENUM()
enum class EGameZoneBindingVerificationStatus : uint8
{
    LegacyUnverified,
    Verified,
    BindingError,
    BindingConflict,
    SourceStale,
    BakeFailed,
};

enum class EGameZoneBindingIssue : uint8
{
    None,
    InvalidAssetId,
    MissingLevel,
    LevelUnavailable,
    LevelPathMismatch,
    WrongWorldSettingsClass,
    ZoneIdMismatch,
    MissingBindingId,
    BindingIdMismatch,
    BindingConflict,
    DuplicateAssetId,
    DuplicateLevelTarget,
    OrphanLevelClaim,
    BoundZoneIdEdit,
    CoordinatorUnavailable,
};

struct RPGCORE_API FGameZoneBindingAuditResult
{
    bool IsVerified() const { return Issue == EGameZoneBindingIssue::None; }

    EGameZoneBindingIssue Issue = EGameZoneBindingIssue::None;
    FText Message;
    FName ExpectedLevelPackage = NAME_None;
    FName ActualLevelPackage = NAME_None;
};

#if WITH_EDITOR
enum class EGameZoneBindingPropertyChange : uint8
{
    LevelToLoad,
    ZoneId,
    ChangeZoneIdCommand,
};

struct RPGCORE_API FGameZoneBindingPropertyChangeRequest
{
    UGameZoneAsset* ZoneAsset = nullptr;
    EGameZoneBindingPropertyChange Change = EGameZoneBindingPropertyChange::LevelToLoad;
    FSoftObjectPath PreviousLevel;
    FRPGId PreviousZoneId;
    FGuid PreviousBindingId;
    FRPGId ProposedZoneId;
#if WITH_EDITORONLY_DATA
    EGameZoneBindingVerificationStatus PreviousVerificationStatus =
        EGameZoneBindingVerificationStatus::LegacyUnverified;
#endif
};

DECLARE_DELEGATE_RetVal_OneParam(
    bool,
    FApplyGameZoneBindingPropertyChange,
    const FGameZoneBindingPropertyChangeRequest&);

RPGCORE_API FGameZoneBindingAuditResult AuditGameZoneBinding(const UGameZoneAsset& ZoneAsset);
RPGCORE_API FGameZoneBindingAuditResult AuditGameZoneBindingAgainstWorld(const UGameZoneAsset& ZoneAsset, const UWorld& World);
RPGCORE_API FApplyGameZoneBindingPropertyChange& GetApplyGameZoneBindingPropertyChangeDelegate();
#endif

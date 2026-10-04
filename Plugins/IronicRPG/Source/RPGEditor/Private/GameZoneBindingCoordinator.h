// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Levels/GameZoneBinding.h"

class UGameZoneAsset;
class ARPGWorldSettings;
class UObject;
class UWorld;

struct FGameZoneAssetBindingRecord
{
    TWeakObjectPtr<UGameZoneAsset> Asset;
    FSoftObjectPath AssetPath;
    FRPGId ZoneId;
    FName LevelPackage = NAME_None;
    FGuid BindingId;
};

struct FGameZoneLevelClaimRecord
{
    TWeakObjectPtr<UWorld> World;
    FName LevelPackage = NAME_None;
    FRPGId ZoneId;
    FGuid BindingId;
};

struct FGameZoneGlobalBindingIssue
{
    EGameZoneBindingIssue Issue = EGameZoneBindingIssue::None;
    FSoftObjectPath AssetPath;
    FSoftObjectPath RelatedAssetPath;
    FName LevelPackage = NAME_None;
    FText Message;
};

struct FGameZoneGlobalBindingAuditResult
{
    bool IsVerified() const { return Issues.IsEmpty(); }

    TArray<FGameZoneGlobalBindingIssue> Issues;
};

struct FGameZoneBindingMutationResult
{
    bool IsSuccess() const { return Issue == EGameZoneBindingIssue::None; }

    EGameZoneBindingIssue Issue = EGameZoneBindingIssue::None;
    FText Message;
};

class FGameZoneBindingCoordinator
{
public:
    static void Register();
    static void Unregister();

    static FGameZoneBindingMutationResult ApplyPropertyChange(
        const FGameZoneBindingPropertyChangeRequest& Request);
    static FGameZoneBindingMutationResult PreviewZoneIdChange(
        const UGameZoneAsset& ZoneAsset,
        const FRPGId& ProposedZoneId);

    static FGameZoneGlobalBindingAuditResult AuditRecords(
        const TArray<FGameZoneAssetBindingRecord>& Assets,
        const TArray<FGameZoneLevelClaimRecord>& LevelClaims);
    static FGameZoneGlobalBindingAuditResult AuditProjectBindings();
    static UGameZoneAsset* FindExactBoundAsset(const UWorld& World, FText* OutFailure = nullptr);
    static bool CanDeleteObjects(const TArray<UObject*>& Objects, FText* OutReason = nullptr);
    static void PrepareForceDeleteObjects(const TArray<UObject*>& Objects);
    static bool CanonicalizeRenamedWorld(UWorld& World, FName OldLevelPackage);

private:
    static void RestoreSnapshot(const FGameZoneBindingPropertyChangeRequest& Request);
    static FGameZoneBindingMutationResult ApplyLevelChange(
        const FGameZoneBindingPropertyChangeRequest& Request);
    static FGameZoneBindingMutationResult ApplyZoneIdCommand(
        const FGameZoneBindingPropertyChangeRequest& Request);
    static FGameZoneBindingMutationResult ValidateZoneIdCommand(
        const UGameZoneAsset& ZoneAsset,
        const FRPGId& ProposedZoneId,
        UWorld*& OutWorld,
        ARPGWorldSettings*& OutWorldSettings,
        FName& OutLevelPackage);
    static FGameZoneBindingMutationResult ValidateGlobalUniqueness(
        const UGameZoneAsset& ZoneAsset,
        const FRPGId& ProposedZoneId,
        const FName ProposedLevelPackage);
    static void HandleAssetsCanDelete(const TArray<UObject*>& Objects, struct FCanDeleteAssetResult& Result);
    static void HandlePreForceDeleteObjects(const TArray<UObject*>& Objects);
    static void HandleAssetRenamed(const struct FAssetData& AssetData, const FString& OldObjectPath);
    static void AuditAndLogProjectBindings();
};

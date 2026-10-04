// Copyright Ironic Studio. All Rights Reserved.

#include "GameZoneBindingCoordinator.h"
#include "RPGReleaseSealService.h"

#include "Assets/RPGAssetManager.h"
#include "Levels/GameZoneAsset.h"
#include "Levels/RPGWorldSettings.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/MessageDialog.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameZoneBinding, Log, All);

namespace
{
    FDelegateHandle AssetsCanDeleteHandle;
    FDelegateHandle PreForceDeleteHandle;
    FDelegateHandle AssetRenamedHandle;
    TUniquePtr<FAutoConsoleCommand> AuditBindingsCommand;

    enum class ELevelClaim : uint8
    {
        Empty,
        Exact,
        Legacy,
        Foreign,
    };

    FGameZoneBindingMutationResult MakeFailure(
        const EGameZoneBindingIssue Issue,
        const FString& Message)
    {
        FGameZoneBindingMutationResult Result;
        Result.Issue = Issue;
        Result.Message = FText::FromString(Message);
        return Result;
    }

    void ReportFailure(const FGameZoneBindingMutationResult& Result)
    {
        UE_LOG(LogGameZoneBinding, Error, TEXT("%s"), *Result.Message.ToString());
        if (!FApp::IsUnattended())
        {
            FMessageDialog::Open(EAppMsgType::Ok, Result.Message);
        }
    }

    UWorld* ResolveWorld(const FSoftObjectPath& WorldPath)
    {
        if (WorldPath.IsNull())
        {
            return nullptr;
        }

        UObject* WorldObject = WorldPath.ResolveObject();
        if (!WorldObject)
        {
            WorldObject = WorldPath.TryLoad();
        }
        return Cast<UWorld>(WorldObject);
    }

    FName GetLevelPackage(const FSoftObjectPath& WorldPath)
    {
        if (const UWorld* World = Cast<UWorld>(WorldPath.ResolveObject()))
        {
            return World->GetOutermost()->GetFName();
        }

        const FString PackageName = WorldPath.GetLongPackageName();
        return PackageName.IsEmpty() ? NAME_None : FName(*PackageName);
    }

    TArray<UGameZoneAsset*> LoadAllZoneAssets(TArray<FSoftObjectPath>* OutLoadFailures = nullptr)
    {
        IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
        if (AssetRegistry.IsLoadingAssets())
        {
            AssetRegistry.WaitForCompletion();
        }

        FARFilter Filter;
        Filter.ClassPaths.Add(UGameZoneAsset::StaticClass()->GetClassPathName());
        Filter.bRecursiveClasses = true;

        TArray<FAssetData> AssetData;
        AssetRegistry.GetAssets(Filter, AssetData);

        TArray<UGameZoneAsset*> Assets;
        Assets.Reserve(AssetData.Num());
        for (const FAssetData& Data : AssetData)
        {
            if (UGameZoneAsset* Asset = Cast<UGameZoneAsset>(Data.GetAsset()))
            {
                Assets.Add(Asset);
            }
            else if (OutLoadFailures)
            {
                OutLoadFailures->Add(Data.GetSoftObjectPath());
            }
        }

        for (TObjectIterator<UGameZoneAsset> It; It; ++It)
        {
            UGameZoneAsset* Asset = *It;
            if (IsValid(Asset)
                && Asset->IsAsset()
                && !Asset->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
                && Asset->GetOutermost() != GetTransientPackage())
            {
                Assets.AddUnique(Asset);
            }
        }
        return Assets;
    }

    TArray<FGameZoneAssetBindingRecord> MakeAssetRecords(TArray<FSoftObjectPath>* OutLoadFailures = nullptr)
    {
        TArray<FGameZoneAssetBindingRecord> Records;
        for (UGameZoneAsset* Asset : LoadAllZoneAssets(OutLoadFailures))
        {
            FGameZoneAssetBindingRecord& Record = Records.AddDefaulted_GetRef();
            Record.Asset = Asset;
            Record.AssetPath = FSoftObjectPath(Asset);
            Record.ZoneId = Asset->GetId();
            Record.LevelPackage = GetLevelPackage(Asset->GetLevelToLoad().ToSoftObjectPath());
            Record.BindingId = Asset->GetGameZoneBindingId();
        }
        return Records;
    }

    void AddGlobalIssue(
        FGameZoneGlobalBindingAuditResult& Result,
        const EGameZoneBindingIssue Issue,
        const FSoftObjectPath& AssetPath,
        const FName LevelPackage,
        const FString& Message,
        const FSoftObjectPath& RelatedAssetPath = {})
    {
        FGameZoneGlobalBindingIssue& Entry = Result.Issues.AddDefaulted_GetRef();
        Entry.Issue = Issue;
        Entry.AssetPath = AssetPath;
        Entry.RelatedAssetPath = RelatedAssetPath;
        Entry.LevelPackage = LevelPackage;
        Entry.Message = FText::FromString(Message);
    }

    ELevelClaim ClassifyClaim(
        const ARPGWorldSettings& WorldSettings,
        const FRPGId& ExpectedZoneId,
        const FGuid& ExpectedBindingId)
    {
        const FRPGId& ClaimedZoneId = WorldSettings.GetGameZoneId();
        const FGuid& ClaimedBindingId = WorldSettings.GetGameZoneBindingId();
        if (!ClaimedZoneId.IsValid() && !ClaimedBindingId.IsValid())
        {
            return ELevelClaim::Empty;
        }
        if (ClaimedZoneId == ExpectedZoneId
            && ExpectedBindingId.IsValid()
            && ClaimedBindingId == ExpectedBindingId)
        {
            return ELevelClaim::Exact;
        }
        if (ClaimedZoneId == ExpectedZoneId
            && !ExpectedBindingId.IsValid()
            && !ClaimedBindingId.IsValid())
        {
            return ELevelClaim::Legacy;
        }
        return ELevelClaim::Foreign;
    }

    void MarkWorldBindingDirty(UWorld& World, ARPGWorldSettings& WorldSettings)
    {
        WorldSettings.MarkPackageDirty();
        World.MarkPackageDirty();
        if (World.PersistentLevel)
        {
            World.PersistentLevel->MarkPackageDirty();
        }
    }
}

void FGameZoneBindingCoordinator::RestoreSnapshot(const FGameZoneBindingPropertyChangeRequest& Request)
{
    UGameZoneAsset& ZoneAsset = *Request.ZoneAsset;
    ZoneAsset.LevelToLoad = TSoftObjectPtr<UWorld>(Request.PreviousLevel);
    ZoneAsset.Id = Request.PreviousZoneId;
    ZoneAsset.GameZoneBindingId = Request.PreviousBindingId;
#if WITH_EDITORONLY_DATA
    ZoneAsset.BindingVerificationStatus = Request.PreviousVerificationStatus;
#endif
}

FGameZoneBindingMutationResult FGameZoneBindingCoordinator::ValidateGlobalUniqueness(
    const UGameZoneAsset& ZoneAsset,
    const FRPGId& ProposedZoneId,
    const FName ProposedLevelPackage)
{
    TArray<FSoftObjectPath> LoadFailures;
    const TArray<UGameZoneAsset*> Assets = LoadAllZoneAssets(&LoadFailures);
    if (!LoadFailures.IsEmpty())
    {
        return MakeFailure(
            EGameZoneBindingIssue::CoordinatorUnavailable,
            FString::Printf(
                TEXT("Global uniqueness could not be proven because Zone Asset '%s' failed to load."),
                *LoadFailures[0].ToString()));
    }

    for (UGameZoneAsset* OtherAsset : Assets)
    {
        if (!IsValid(OtherAsset) || OtherAsset == &ZoneAsset)
        {
            continue;
        }

        if (ProposedZoneId.IsValid() && OtherAsset->GetId() == ProposedZoneId)
        {
            return MakeFailure(
                EGameZoneBindingIssue::DuplicateAssetId,
                FString::Printf(
                    TEXT("Zone Id '%s' is already owned by '%s'."),
                    *ProposedZoneId.ToString(),
                    *OtherAsset->GetPathName()));
        }

        const FName OtherLevelPackage = GetLevelPackage(OtherAsset->GetLevelToLoad().ToSoftObjectPath());
        if (!ProposedLevelPackage.IsNone() && OtherLevelPackage == ProposedLevelPackage)
        {
            return MakeFailure(
                EGameZoneBindingIssue::DuplicateLevelTarget,
                FString::Printf(
                    TEXT("Level '%s' is already targeted by Zone Asset '%s'."),
                    *ProposedLevelPackage.ToString(),
                    *OtherAsset->GetPathName()));
        }
    }

    return {};
}

FGameZoneGlobalBindingAuditResult FGameZoneBindingCoordinator::AuditRecords(
    const TArray<FGameZoneAssetBindingRecord>& Assets,
    const TArray<FGameZoneLevelClaimRecord>& LevelClaims)
{
    FGameZoneGlobalBindingAuditResult Result;

    for (int32 Index = 0; Index < Assets.Num(); ++Index)
    {
        const FGameZoneAssetBindingRecord& Asset = Assets[Index];
        for (int32 OtherIndex = Index + 1; OtherIndex < Assets.Num(); ++OtherIndex)
        {
            const FGameZoneAssetBindingRecord& Other = Assets[OtherIndex];
            if (Asset.ZoneId.IsValid() && Asset.ZoneId == Other.ZoneId)
            {
                AddGlobalIssue(
                    Result,
                    EGameZoneBindingIssue::DuplicateAssetId,
                    Asset.AssetPath,
                    Asset.LevelPackage,
                    FString::Printf(
                        TEXT("Zone Id '%s' is duplicated by '%s' and '%s'."),
                        *Asset.ZoneId.ToString(),
                        *Asset.AssetPath.ToString(),
                        *Other.AssetPath.ToString()),
                    Other.AssetPath);
            }
            if (!Asset.LevelPackage.IsNone() && Asset.LevelPackage == Other.LevelPackage)
            {
                AddGlobalIssue(
                    Result,
                    EGameZoneBindingIssue::DuplicateLevelTarget,
                    Asset.AssetPath,
                    Asset.LevelPackage,
                    FString::Printf(
                        TEXT("Level '%s' is targeted by both '%s' and '%s'."),
                        *Asset.LevelPackage.ToString(),
                        *Asset.AssetPath.ToString(),
                        *Other.AssetPath.ToString()),
                    Other.AssetPath);
            }
        }
    }

    for (const FGameZoneLevelClaimRecord& Claim : LevelClaims)
    {
        if (!Claim.ZoneId.IsValid() && !Claim.BindingId.IsValid())
        {
            continue;
        }

        int32 ExactOwnerCount = 0;
        FSoftObjectPath ExactOwnerPath;
        for (const FGameZoneAssetBindingRecord& Asset : Assets)
        {
            if (Asset.LevelPackage == Claim.LevelPackage
                && Asset.ZoneId == Claim.ZoneId
                && Asset.BindingId.IsValid()
                && Asset.BindingId == Claim.BindingId)
            {
                ++ExactOwnerCount;
                ExactOwnerPath = Asset.AssetPath;
            }
        }

        if (ExactOwnerCount != 1)
        {
            AddGlobalIssue(
                Result,
                EGameZoneBindingIssue::OrphanLevelClaim,
                ExactOwnerPath,
                Claim.LevelPackage,
                FString::Printf(
                    TEXT("Level '%s' claim for Zone Id '%s' resolves to %d exact Zone Assets."),
                    *Claim.LevelPackage.ToString(),
                    *Claim.ZoneId.ToString(),
                    ExactOwnerCount));
        }
    }

    for (const FGameZoneAssetBindingRecord& Asset : Assets)
    {
        if (Asset.LevelPackage.IsNone() && !Asset.BindingId.IsValid())
        {
            continue;
        }

        const int32 ExactClaimCount = LevelClaims.FilterByPredicate(
            [&Asset](const FGameZoneLevelClaimRecord& Claim)
            {
                return Claim.LevelPackage == Asset.LevelPackage
                    && Claim.ZoneId == Asset.ZoneId
                    && Asset.BindingId.IsValid()
                    && Claim.BindingId == Asset.BindingId;
            }).Num();
        if (ExactClaimCount != 1)
        {
            AddGlobalIssue(
                Result,
                EGameZoneBindingIssue::BindingConflict,
                Asset.AssetPath,
                Asset.LevelPackage,
                FString::Printf(
                    TEXT("Zone Asset '%s' binding resolves to %d exact Level claims."),
                    *Asset.AssetPath.ToString(),
                    ExactClaimCount));
        }
    }

    return Result;
}

FGameZoneGlobalBindingAuditResult FGameZoneBindingCoordinator::AuditProjectBindings()
{
    TArray<FSoftObjectPath> AssetLoadFailures;
    const TArray<FGameZoneAssetBindingRecord> Assets = MakeAssetRecords(&AssetLoadFailures);
    TArray<FGameZoneLevelClaimRecord> Claims;
    FGameZoneGlobalBindingAuditResult ScanIssues;

    for (const FSoftObjectPath& FailedPath : AssetLoadFailures)
    {
        AddGlobalIssue(
            ScanIssues,
            EGameZoneBindingIssue::CoordinatorUnavailable,
            FailedPath,
            NAME_None,
            FString::Printf(TEXT("Zone Asset '%s' failed to load during global audit."), *FailedPath.ToString()));
    }

    FARFilter Filter;
    Filter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
    Filter.PackagePaths.Add(FName(TEXT("/Game")));
    Filter.PackagePaths.Add(FName(TEXT("/IronicRPG")));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> WorldData;
    IAssetRegistry::GetChecked().GetAssets(Filter, WorldData);
    for (const FAssetData& Data : WorldData)
    {
        UWorld* World = Cast<UWorld>(Data.GetAsset());
        ARPGWorldSettings* Settings = World ? Cast<ARPGWorldSettings>(World->GetWorldSettings()) : nullptr;
        if (!World)
        {
            AddGlobalIssue(
                ScanIssues,
                EGameZoneBindingIssue::LevelUnavailable,
                {},
                Data.PackageName,
                FString::Printf(TEXT("Level '%s' failed to load during global audit."), *Data.PackageName.ToString()));
            continue;
        }
        if (!Settings || (!Settings->GetGameZoneId().IsValid() && !Settings->GetGameZoneBindingId().IsValid()))
        {
            continue;
        }

        FGameZoneLevelClaimRecord& Claim = Claims.AddDefaulted_GetRef();
        Claim.World = World;
        Claim.LevelPackage = World->GetOutermost()->GetFName();
        Claim.ZoneId = Settings->GetGameZoneId();
        Claim.BindingId = Settings->GetGameZoneBindingId();
    }

    FGameZoneGlobalBindingAuditResult Result = AuditRecords(Assets, Claims);
    Result.Issues.Append(MoveTemp(ScanIssues.Issues));
    return Result;
}

UGameZoneAsset* FGameZoneBindingCoordinator::FindExactBoundAsset(
    const UWorld& World,
    FText* OutFailure)
{
    const ARPGWorldSettings* Settings = Cast<ARPGWorldSettings>(World.GetWorldSettings());
    if (!Settings
        || !Settings->GetGameZoneId().IsValid()
        || !Settings->GetGameZoneBindingId().IsValid())
    {
        return nullptr;
    }

    TArray<UGameZoneAsset*> ExactAssets;
    for (UGameZoneAsset* Asset : LoadAllZoneAssets())
    {
        if (!Asset
            || GetLevelPackage(Asset->GetLevelToLoad().ToSoftObjectPath()) != World.GetOutermost()->GetFName())
        {
            continue;
        }

        const FGameZoneBindingAuditResult Audit = AuditGameZoneBindingAgainstWorld(*Asset, World);
        if (Audit.IsVerified())
        {
            ExactAssets.Add(Asset);
        }
    }

    if (ExactAssets.Num() == 1)
    {
        return ExactAssets[0];
    }

    if (OutFailure)
    {
        *OutFailure = FText::FromString(FString::Printf(
            TEXT("Level '%s' has a Game Zone claim but resolves to %d exact Zone Assets."),
            *World.GetOutermost()->GetName(),
            ExactAssets.Num()));
    }
    return nullptr;
}

FGameZoneBindingMutationResult FGameZoneBindingCoordinator::ApplyLevelChange(
    const FGameZoneBindingPropertyChangeRequest& Request)
{
    UGameZoneAsset& ZoneAsset = *Request.ZoneAsset;
    const FSoftObjectPath NewLevelPath = ZoneAsset.LevelToLoad.ToSoftObjectPath();
    const bool bSameLevel = NewLevelPath == Request.PreviousLevel;

    UWorld* NewWorld = nullptr;
    ARPGWorldSettings* NewWorldSettings = nullptr;
    ELevelClaim NewClaim = ELevelClaim::Empty;
    if (!NewLevelPath.IsNull())
    {
        if (!ZoneAsset.Id.IsValid())
        {
            return MakeFailure(
                EGameZoneBindingIssue::InvalidAssetId,
                FString::Printf(TEXT("Zone Asset '%s' requires a valid Id before binding a Level."), *ZoneAsset.GetPathName()));
        }

        NewWorld = ResolveWorld(NewLevelPath);
        if (!NewWorld)
        {
            return MakeFailure(
                EGameZoneBindingIssue::LevelUnavailable,
                FString::Printf(TEXT("Could not load target Level '%s'."), *NewLevelPath.ToString()));
        }
        NewWorldSettings = Cast<ARPGWorldSettings>(NewWorld->GetWorldSettings());
        if (!NewWorldSettings)
        {
            return MakeFailure(
                EGameZoneBindingIssue::WrongWorldSettingsClass,
                FString::Printf(TEXT("Target Level '%s' does not use RPGWorldSettings."), *NewWorld->GetPathName()));
        }

        const FGameZoneBindingMutationResult UniquenessResult = ValidateGlobalUniqueness(
            ZoneAsset,
            ZoneAsset.Id,
            NewWorld->GetOutermost()->GetFName());
        if (!UniquenessResult.IsSuccess())
        {
            return UniquenessResult;
        }

        const FGuid ExpectedNewBindingId = bSameLevel ? Request.PreviousBindingId : FGuid();
        NewClaim = ClassifyClaim(*NewWorldSettings, ZoneAsset.Id, ExpectedNewBindingId);
        if (NewClaim == ELevelClaim::Foreign || (NewClaim == ELevelClaim::Legacy && !bSameLevel))
        {
            return MakeFailure(
                EGameZoneBindingIssue::BindingConflict,
                FString::Printf(
                    TEXT("Target Level '%s' is already claimed by Zone Id '%s' and was not modified."),
                    *NewWorld->GetPathName(),
                    *NewWorldSettings->GetGameZoneId().ToString()));
        }
        if (bSameLevel && NewClaim == ELevelClaim::Exact)
        {
            return {};
        }
    }

    UWorld* PreviousWorld = nullptr;
    ARPGWorldSettings* PreviousWorldSettings = nullptr;
    ELevelClaim PreviousClaim = ELevelClaim::Empty;
    if (!Request.PreviousLevel.IsNull() && !bSameLevel)
    {
        PreviousWorld = ResolveWorld(Request.PreviousLevel);
        if (!PreviousWorld)
        {
            return MakeFailure(
                EGameZoneBindingIssue::LevelUnavailable,
                FString::Printf(
                    TEXT("Previous Level '%s' could not be loaded, so its claim cannot be cleared safely."),
                    *Request.PreviousLevel.ToString()));
        }
        PreviousWorldSettings = Cast<ARPGWorldSettings>(PreviousWorld->GetWorldSettings());
        if (!PreviousWorldSettings)
        {
            return MakeFailure(
                EGameZoneBindingIssue::WrongWorldSettingsClass,
                FString::Printf(
                    TEXT("Previous Level '%s' does not use RPGWorldSettings and cannot be cleared safely."),
                    *PreviousWorld->GetPathName()));
        }

        PreviousClaim = ClassifyClaim(
            *PreviousWorldSettings,
            Request.PreviousZoneId,
            Request.PreviousBindingId);
        if (PreviousClaim == ELevelClaim::Foreign)
        {
            return MakeFailure(
                EGameZoneBindingIssue::BindingConflict,
                FString::Printf(
                    TEXT("Previous Level '%s' no longer has this Asset's exact claim and was not cleared."),
                    *PreviousWorld->GetPathName()));
        }
    }

    ZoneAsset.Modify();
    if (PreviousWorldSettings
        && (PreviousClaim == ELevelClaim::Exact || PreviousClaim == ELevelClaim::Legacy))
    {
        PreviousWorldSettings->Modify();
        PreviousWorldSettings->GameZoneId = {};
        PreviousWorldSettings->GameZoneBindingId.Invalidate();
        MarkWorldBindingDirty(*PreviousWorld, *PreviousWorldSettings);
    }

    if (NewWorldSettings)
    {
        const FGuid NewBindingId = NewClaim == ELevelClaim::Exact
            ? Request.PreviousBindingId
            : FGuid::NewGuid();
        NewWorldSettings->Modify();
        NewWorldSettings->GameZoneId = ZoneAsset.Id;
        NewWorldSettings->GameZoneBindingId = NewBindingId;
        ZoneAsset.LevelToLoad = TSoftObjectPtr<UWorld>(NewWorld);
        ZoneAsset.GameZoneBindingId = NewBindingId;
#if WITH_EDITORONLY_DATA
        ZoneAsset.BindingVerificationStatus = EGameZoneBindingVerificationStatus::SourceStale;
#endif
        MarkWorldBindingDirty(*NewWorld, *NewWorldSettings);
    }
    else
    {
        ZoneAsset.GameZoneBindingId.Invalidate();
#if WITH_EDITORONLY_DATA
        ZoneAsset.BindingVerificationStatus = EGameZoneBindingVerificationStatus::LegacyUnverified;
#endif
    }
    ZoneAsset.MarkPackageDirty();
    return {};
}

FGameZoneBindingMutationResult FGameZoneBindingCoordinator::ValidateZoneIdCommand(
    const UGameZoneAsset& ZoneAsset,
    const FRPGId& ProposedZoneId,
    UWorld*& OutWorld,
    ARPGWorldSettings*& OutWorldSettings,
    FName& OutLevelPackage)
{
    OutWorld = nullptr;
    OutWorldSettings = nullptr;
    OutLevelPackage = NAME_None;
    FText ReleaseError;
    if (!FRPGReleaseSealService::CheckMutation(ZoneAsset.GetId(), ProposedZoneId, ReleaseError))
    {
        return MakeFailure(EGameZoneBindingIssue::BindingConflict, ReleaseError.ToString());
    }
    if (!ProposedZoneId.IsValid())
    {
        return MakeFailure(
            EGameZoneBindingIssue::InvalidAssetId,
            FString::Printf(TEXT("Zone Asset '%s' requires a valid new Zone Id."), *ZoneAsset.GetPathName()));
    }

    if (!ZoneAsset.GetLevelToLoad().IsNull())
    {
        OutWorld = ResolveWorld(ZoneAsset.GetLevelToLoad().ToSoftObjectPath());
        if (!OutWorld)
        {
            return MakeFailure(
                EGameZoneBindingIssue::LevelUnavailable,
                FString::Printf(TEXT("Could not load bound Level for '%s'."), *ZoneAsset.GetPathName()));
        }

        const FGameZoneBindingAuditResult PairAudit = AuditGameZoneBindingAgainstWorld(ZoneAsset, *OutWorld);
        if (!PairAudit.IsVerified())
        {
            return MakeFailure(
                PairAudit.Issue,
                FString::Printf(
                    TEXT("Zone Id change requires an exact current binding: %s"),
                    *PairAudit.Message.ToString()));
        }
        OutWorldSettings = CastChecked<ARPGWorldSettings>(OutWorld->GetWorldSettings());
        OutLevelPackage = OutWorld->GetOutermost()->GetFName();
    }

    return ValidateGlobalUniqueness(ZoneAsset, ProposedZoneId, OutLevelPackage);
}

FGameZoneBindingMutationResult FGameZoneBindingCoordinator::PreviewZoneIdChange(
    const UGameZoneAsset& ZoneAsset,
    const FRPGId& ProposedZoneId)
{
    UWorld* World = nullptr;
    ARPGWorldSettings* Settings = nullptr;
    FName LevelPackage = NAME_None;
    return ValidateZoneIdCommand(ZoneAsset, ProposedZoneId, World, Settings, LevelPackage);
}

FGameZoneBindingMutationResult FGameZoneBindingCoordinator::ApplyZoneIdCommand(
    const FGameZoneBindingPropertyChangeRequest& Request)
{
    UGameZoneAsset& ZoneAsset = *Request.ZoneAsset;
    if (ZoneAsset.Id != Request.PreviousZoneId
        || ZoneAsset.LevelToLoad.ToSoftObjectPath() != Request.PreviousLevel
        || ZoneAsset.GameZoneBindingId != Request.PreviousBindingId)
    {
        return MakeFailure(
            EGameZoneBindingIssue::BindingConflict,
            FString::Printf(TEXT("Zone Asset '%s' changed after the request was prepared; no data was modified."), *ZoneAsset.GetPathName()));
    }

    UWorld* World = nullptr;
    ARPGWorldSettings* Settings = nullptr;
    FName LevelPackage = NAME_None;
    const FGameZoneBindingMutationResult Validation = ValidateZoneIdCommand(
        ZoneAsset,
        Request.ProposedZoneId,
        World,
        Settings,
        LevelPackage);
    if (!Validation.IsSuccess())
    {
        return Validation;
    }
    if (Request.ProposedZoneId == ZoneAsset.Id)
    {
        return {};
    }

    const bool bAssetWasDirty = ZoneAsset.GetOutermost()->IsDirty();
    const bool bWorldWasDirty = World && World->GetOutermost()->IsDirty();
    const FRPGId PreviousWorldId = Settings ? Settings->GameZoneId : FRPGId();
    FScopedTransaction Transaction(FText::FromString(TEXT("Change Game Zone Id")));
    ZoneAsset.Modify();
    if (Settings)
    {
        Settings->Modify();
    }

    ZoneAsset.Id = Request.ProposedZoneId;
    if (Settings)
    {
        Settings->GameZoneId = Request.ProposedZoneId;
        MarkWorldBindingDirty(*World, *Settings);
    }
#if WITH_EDITORONLY_DATA
    ZoneAsset.BindingVerificationStatus = EGameZoneBindingVerificationStatus::SourceStale;
#endif
    ZoneAsset.MarkPackageDirty();

    URPGAssetManager& Manager = URPGAssetManager::Get();
    if (ZoneAsset.IsAsset())
    {
        Manager.RefreshAssetData(&ZoneAsset);
        if (Manager.GetPrimaryAssetPath(ZoneAsset.GetPrimaryAssetId()) != FSoftObjectPath(&ZoneAsset))
        {
            RestoreSnapshot(Request);
            if (Settings)
            {
                Settings->GameZoneId = PreviousWorldId;
            }
            Manager.RefreshAssetData(&ZoneAsset);
            ZoneAsset.GetOutermost()->SetDirtyFlag(bAssetWasDirty);
            if (World)
            {
                World->GetOutermost()->SetDirtyFlag(bWorldWasDirty);
            }
            Transaction.Cancel();
            return MakeFailure(
                EGameZoneBindingIssue::CoordinatorUnavailable,
                FString::Printf(TEXT("AssetManager could not register the new Id for '%s'; both sides were rolled back."), *ZoneAsset.GetPathName()));
        }
    }

    FPropertyChangedEvent Event(FindFProperty<FProperty>(URPGPrimaryAsset::StaticClass(), TEXT("Id")), EPropertyChangeType::ValueSet);
    ZoneAsset.URPGPrimaryAsset::PostEditChangeProperty(Event);
    return {};
}

bool FGameZoneBindingCoordinator::CanDeleteObjects(const TArray<UObject*>& Objects, FText* OutReason)
{
    for (UObject* Object : Objects)
    {
        if (const UGameZoneAsset* Asset = Cast<UGameZoneAsset>(Object))
        {
            if (!Asset->GetLevelToLoad().IsNull() || Asset->GetGameZoneBindingId().IsValid())
            {
                if (OutReason)
                {
                    *OutReason = FText::FromString(FString::Printf(
                        TEXT("Zone Asset '%s' is bound; unbind it before deleting."),
                        *Asset->GetPathName()));
                }
                return false;
            }
        }
        else if (const UWorld* World = Cast<UWorld>(Object))
        {
            const ARPGWorldSettings* Settings = Cast<ARPGWorldSettings>(World->GetWorldSettings());
            if (Settings && (Settings->GetGameZoneId().IsValid() || Settings->GetGameZoneBindingId().IsValid()))
            {
                if (OutReason)
                {
                    *OutReason = FText::FromString(FString::Printf(
                        TEXT("Level '%s' has a Game Zone claim; unbind its Zone Asset before deleting."),
                        *World->GetOutermost()->GetName()));
                }
                return false;
            }
        }
    }
    return true;
}

void FGameZoneBindingCoordinator::HandleAssetsCanDelete(
    const TArray<UObject*>& Objects,
    FCanDeleteAssetResult& Result)
{
    FText Reason;
    const bool bCanDelete = CanDeleteObjects(Objects, &Reason);
    Result.Set(bCanDelete);
    if (!bCanDelete)
    {
        UE_LOG(LogGameZoneBinding, Warning, TEXT("%s"), *Reason.ToString());
    }
}

void FGameZoneBindingCoordinator::HandlePreForceDeleteObjects(const TArray<UObject*>& Objects)
{
    PrepareForceDeleteObjects(Objects);
}

void FGameZoneBindingCoordinator::PrepareForceDeleteObjects(const TArray<UObject*>& Objects)
{
    TSet<const UObject*> DeletingObjects;
    for (const UObject* Object : Objects)
    {
        DeletingObjects.Add(Object);
    }

    for (UObject* Object : Objects)
    {
        if (UGameZoneAsset* Asset = Cast<UGameZoneAsset>(Object))
        {
            UWorld* World = ResolveWorld(Asset->GetLevelToLoad().ToSoftObjectPath());
            ARPGWorldSettings* Settings = World ? Cast<ARPGWorldSettings>(World->GetWorldSettings()) : nullptr;
            if (Settings
                && Settings->GetGameZoneId() == Asset->GetId()
                && Asset->GetGameZoneBindingId().IsValid()
                && Settings->GetGameZoneBindingId() == Asset->GetGameZoneBindingId())
            {
                Settings->Modify();
                Settings->GameZoneId = {};
                Settings->GameZoneBindingId.Invalidate();
                MarkWorldBindingDirty(*World, *Settings);
            }
            continue;
        }

        UWorld* World = Cast<UWorld>(Object);
        ARPGWorldSettings* Settings = World ? Cast<ARPGWorldSettings>(World->GetWorldSettings()) : nullptr;
        if (!Settings)
        {
            continue;
        }

        const FName WorldPackage = World->GetOutermost()->GetFName();
        for (UGameZoneAsset* Asset : LoadAllZoneAssets())
        {
            if (!Asset || DeletingObjects.Contains(Asset))
            {
                continue;
            }
            if (GetLevelPackage(Asset->GetLevelToLoad().ToSoftObjectPath()) == WorldPackage
                && Asset->GetId() == Settings->GetGameZoneId()
                && Asset->GetGameZoneBindingId().IsValid()
                && Asset->GetGameZoneBindingId() == Settings->GetGameZoneBindingId())
            {
                Asset->Modify();
                Asset->LevelToLoad.Reset();
                Asset->GameZoneBindingId.Invalidate();
#if WITH_EDITORONLY_DATA
                Asset->BindingVerificationStatus = EGameZoneBindingVerificationStatus::LegacyUnverified;
#endif
                Asset->MarkPackageDirty();
            }
        }
    }
}

void FGameZoneBindingCoordinator::HandleAssetRenamed(
    const FAssetData& AssetData,
    const FString& OldObjectPath)
{
    if (AssetData.AssetClassPath != UWorld::StaticClass()->GetClassPathName())
    {
        return;
    }

    UWorld* World = Cast<UWorld>(AssetData.GetAsset());
    ARPGWorldSettings* Settings = World ? Cast<ARPGWorldSettings>(World->GetWorldSettings()) : nullptr;
    if (!Settings)
    {
        UE_LOG(LogGameZoneBinding, Error, TEXT("Renamed Level '%s' could not be audited."), *AssetData.GetSoftObjectPath().ToString());
        return;
    }

    const FName OldPackage(*FSoftObjectPath(OldObjectPath).GetLongPackageName());
    CanonicalizeRenamedWorld(*World, OldPackage);
}

bool FGameZoneBindingCoordinator::CanonicalizeRenamedWorld(
    UWorld& World,
    const FName OldLevelPackage)
{
    ARPGWorldSettings* Settings = Cast<ARPGWorldSettings>(World.GetWorldSettings());
    if (!Settings)
    {
        return false;
    }

    const FName NewPackage = World.GetOutermost()->GetFName();
    TArray<UGameZoneAsset*> Candidates;
    for (UGameZoneAsset* Asset : LoadAllZoneAssets())
    {
        if (Asset && GetLevelPackage(Asset->GetLevelToLoad().ToSoftObjectPath()) == OldLevelPackage)
        {
            Candidates.Add(Asset);
        }
    }

    if (Candidates.Num() != 1)
    {
        UE_LOG(
            LogGameZoneBinding,
            Error,
            TEXT("Level rename '%s' -> '%s' resolves to %d Zone Assets; no Asset was changed."),
            *OldLevelPackage.ToString(),
            *NewPackage.ToString(),
            Candidates.Num());
        return false;
    }

    UGameZoneAsset* Asset = Candidates[0];
    if (Asset->GetId() != Settings->GetGameZoneId()
        || !Asset->GetGameZoneBindingId().IsValid()
        || Asset->GetGameZoneBindingId() != Settings->GetGameZoneBindingId())
    {
        UE_LOG(
            LogGameZoneBinding,
            Error,
            TEXT("Level rename '%s' -> '%s' found a non-exact Zone binding; no Asset was changed."),
            *OldLevelPackage.ToString(),
            *NewPackage.ToString());
        return false;
    }

    const FScopedTransaction Transaction(FText::FromString(TEXT("Canonicalize Game Zone Level Path")));
    Asset->Modify();
    Asset->LevelToLoad = TSoftObjectPtr<UWorld>(&World);
#if WITH_EDITORONLY_DATA
    Asset->BindingVerificationStatus = EGameZoneBindingVerificationStatus::SourceStale;
#endif
    Asset->MarkPackageDirty();
    return true;
}

void FGameZoneBindingCoordinator::AuditAndLogProjectBindings()
{
    const FGameZoneGlobalBindingAuditResult Audit = AuditProjectBindings();
    if (Audit.IsVerified())
    {
        UE_LOG(LogGameZoneBinding, Display, TEXT("Global Game Zone binding audit passed."));
        return;
    }

    for (const FGameZoneGlobalBindingIssue& Issue : Audit.Issues)
    {
        UE_LOG(LogGameZoneBinding, Error, TEXT("%s"), *Issue.Message.ToString());
    }
    UE_LOG(LogGameZoneBinding, Error, TEXT("Global Game Zone binding audit found %d issue(s)."), Audit.Issues.Num());
}

void FGameZoneBindingCoordinator::Register()
{
    GetApplyGameZoneBindingPropertyChangeDelegate().BindLambda(
        [](const FGameZoneBindingPropertyChangeRequest& Request)
        {
            const FGameZoneBindingMutationResult Result = ApplyPropertyChange(Request);
            if (!Result.IsSuccess())
            {
                ReportFailure(Result);
            }
            return Result.IsSuccess();
        });

    AssetsCanDeleteHandle = FEditorDelegates::OnAssetsCanDelete.AddStatic(&HandleAssetsCanDelete);
    PreForceDeleteHandle = FEditorDelegates::OnPreForceDeleteObjects.AddStatic(&HandlePreForceDeleteObjects);
    AssetRenamedHandle = IAssetRegistry::GetChecked().OnAssetRenamed().AddStatic(&HandleAssetRenamed);
    AuditBindingsCommand = MakeUnique<FAutoConsoleCommand>(
        TEXT("IronicRPG.GameZone.AuditBindings"),
        TEXT("Loads project and IronicRPG Zone Assets/Levels and reports global binding conflicts."),
        FConsoleCommandDelegate::CreateStatic(&AuditAndLogProjectBindings));
}

void FGameZoneBindingCoordinator::Unregister()
{
    AuditBindingsCommand.Reset();
    if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
    {
        AssetRegistry->OnAssetRenamed().Remove(AssetRenamedHandle);
    }
    FEditorDelegates::OnPreForceDeleteObjects.Remove(PreForceDeleteHandle);
    FEditorDelegates::OnAssetsCanDelete.Remove(AssetsCanDeleteHandle);
    GetApplyGameZoneBindingPropertyChangeDelegate().Unbind();
}

FGameZoneBindingMutationResult FGameZoneBindingCoordinator::ApplyPropertyChange(
    const FGameZoneBindingPropertyChangeRequest& Request)
{
    if (!IsValid(Request.ZoneAsset))
    {
        return MakeFailure(EGameZoneBindingIssue::CoordinatorUnavailable, TEXT("GameZone binding request has no valid Asset."));
    }

    if (Request.Change == EGameZoneBindingPropertyChange::ZoneId)
    {
        if (!Request.PreviousLevel.IsNull() || Request.PreviousBindingId.IsValid())
        {
            RestoreSnapshot(Request);
            return MakeFailure(
                EGameZoneBindingIssue::BoundZoneIdEdit,
                FString::Printf(
                    TEXT("Zone Asset '%s' is bound to a Level; direct Id edits are not allowed."),
                    *Request.ZoneAsset->GetPathName()));
        }

        const FGameZoneBindingMutationResult UniquenessResult = ValidateGlobalUniqueness(
            *Request.ZoneAsset,
            Request.ZoneAsset->GetId(),
            NAME_None);
        if (!UniquenessResult.IsSuccess())
        {
            RestoreSnapshot(Request);
            return UniquenessResult;
        }
        return {};
    }

    if (Request.Change == EGameZoneBindingPropertyChange::ChangeZoneIdCommand)
    {
        return ApplyZoneIdCommand(Request);
    }

    const FGameZoneBindingMutationResult Result = ApplyLevelChange(Request);
    if (!Result.IsSuccess())
    {
        RestoreSnapshot(Request);
    }
    return Result;
}


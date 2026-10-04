// Copyright Ironic Studio. All Rights Reserved.

#include "GameZoneBakeCoordinator.h"

#include "GameZoneBindingCoordinator.h"
#include "Levels/GameZoneAsset.h"
#include "Levels/RPGWorldSettings.h"

#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameZoneBake, Log, All);

namespace
{
    TSet<TWeakObjectPtr<UWorld>> PendingWorlds;
    FTSTicker::FDelegateHandle DeferredBakeHandle;
    FDelegateHandle PostSaveWorldHandle;
    FDelegateHandle PackageSavedHandle;
    bool bProcessingPendingBakes = false;

    UWorld* FindOwningWorldForSavedPackage(UPackage& SavedPackage, UWorld* EditorWorld)
    {
        if (UWorld* SavedWorld = UWorld::FindWorldInPackage(&SavedPackage))
        {
            return SavedWorld;
        }

        if (AActor* SavedActor = AActor::FindActorInPackage(&SavedPackage, false))
        {
            return SavedActor->GetWorld();
        }

        if (!EditorWorld || !EditorWorld->PersistentLevel)
        {
            return nullptr;
        }

        const FString SavedPackageName = SavedPackage.GetName();
        for (const FString& ExternalActorsPath : ULevel::GetExternalActorsPaths(EditorWorld->GetOutermost()->GetName()))
        {
            if (SavedPackageName.StartsWith(ExternalActorsPath + TEXT("/")))
            {
                return EditorWorld;
            }
        }
        return nullptr;
    }
}

bool FGameZoneBakeCoordinator::ShouldScheduleWorldSave(
    const UWorld& World,
    const bool bSaveSucceeded,
    const bool bFromAutoSave,
    const bool bProceduralSave,
    const bool bCooking,
    const UWorld* EditorWorld)
{
    const UPackage* Package = World.GetOutermost();
    return bSaveSucceeded
        && !bFromAutoSave
        && !bProceduralSave
        && !bCooking
        && World.WorldType == EWorldType::Editor
        && EditorWorld == &World
        && Package
        && FPackageName::IsValidLongPackageName(Package->GetName())
        && !Package->GetName().StartsWith(TEXT("/Temp/"));
}

void FGameZoneBakeCoordinator::HandlePostSaveWorld(
    UWorld* World,
    const FObjectPostSaveContext SaveContext)
{
    const UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World
        || !ShouldScheduleWorldSave(
            *World,
            SaveContext.SaveSucceeded(),
            SaveContext.IsFromAutoSave(),
            SaveContext.IsProceduralSave(),
            SaveContext.IsCooking(),
            EditorWorld))
    {
        return;
    }

    QueueWorldForBake(*World);
}

void FGameZoneBakeCoordinator::QueueWorldForBake(UWorld& World)
{
    PendingWorlds.Add(&World);
    if (!DeferredBakeHandle.IsValid())
    {
        DeferredBakeHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateStatic(&HandleDeferredBake));
    }
}

bool FGameZoneBakeCoordinator::QueueSavedPackageForBake(
    UPackage& SavedPackage,
    const bool bSaveSucceeded,
    const bool bFromAutoSave,
    const bool bProceduralSave,
    const bool bCooking,
    UWorld* EditorWorld)
{
    UWorld* SavedWorld = FindOwningWorldForSavedPackage(SavedPackage, EditorWorld);
    if (!SavedWorld
        || !ShouldScheduleWorldSave(
            *SavedWorld,
            bSaveSucceeded,
            bFromAutoSave,
            bProceduralSave,
            bCooking,
            EditorWorld))
    {
        return false;
    }

    QueueWorldForBake(*SavedWorld);
    return true;
}

void FGameZoneBakeCoordinator::HandlePackageSaved(
    const FString&,
    UPackage* Package,
    const FObjectPostSaveContext SaveContext)
{
    UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (Package)
    {
        QueueSavedPackageForBake(
            *Package,
            SaveContext.SaveSucceeded(),
            SaveContext.IsFromAutoSave(),
            SaveContext.IsProceduralSave(),
            SaveContext.IsCooking(),
            EditorWorld);
    }
}

bool FGameZoneBakeCoordinator::HandleDeferredBake(const float)
{
    DeferredBakeHandle.Reset();
    ProcessPendingBakes();
    return false;
}

void FGameZoneBakeCoordinator::ProcessPendingBakes()
{
    if (bProcessingPendingBakes)
    {
        return;
    }

    TGuardValue<bool> ProcessingGuard(bProcessingPendingBakes, true);
    TArray<TWeakObjectPtr<UWorld>> Worlds = PendingWorlds.Array();
    PendingWorlds.Reset();

    for (const TWeakObjectPtr<UWorld>& WorldPtr : Worlds)
    {
        UWorld* World = WorldPtr.Get();
        if (!World)
        {
            continue;
        }

        FText BindingFailure;
        UGameZoneAsset* ZoneAsset = FGameZoneBindingCoordinator::FindExactBoundAsset(*World, &BindingFailure);
        if (!ZoneAsset)
        {
            const ARPGWorldSettings* Settings = Cast<ARPGWorldSettings>(World->GetWorldSettings());
            if (Settings
                && (Settings->GetGameZoneId().IsValid() || Settings->GetGameZoneBindingId().IsValid()))
            {
                UE_LOG(LogGameZoneBake, Error, TEXT("%s"), *BindingFailure.ToString());
            }
            continue;
        }

        const EGameZoneMapBakeResult BakeResult = ZoneAsset->BakeMapDataWithResult(true);
        if (BakeResult == EGameZoneMapBakeResult::Failed)
        {
            UE_LOG(
                LogGameZoneBake,
                Error,
                TEXT("Level '%s' saved, but Zone Asset '%s' kept its previous Bake snapshot."),
                *World->GetOutermost()->GetName(),
                *ZoneAsset->GetPathName());
            continue;
        }

        UPackage* AssetPackage = ZoneAsset->GetOutermost();
        if (AssetPackage && AssetPackage->IsDirty())
        {
            const TArray<UPackage*> PackagesToSave{AssetPackage};
            if (!UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true))
            {
                UE_LOG(
                    LogGameZoneBake,
                    Error,
                    TEXT("Zone Asset '%s' baked but could not be saved."),
                    *ZoneAsset->GetPathName());
            }
        }
    }
}

int32 FGameZoneBakeCoordinator::GetPendingWorldCount()
{
    return PendingWorlds.Num();
}

void FGameZoneBakeCoordinator::Register()
{
    PostSaveWorldHandle = FEditorDelegates::PostSaveWorldWithContext.AddStatic(&HandlePostSaveWorld);
    PackageSavedHandle = UPackage::PackageSavedWithContextEvent.AddStatic(&HandlePackageSaved);
}

void FGameZoneBakeCoordinator::Unregister()
{
    FEditorDelegates::PostSaveWorldWithContext.Remove(PostSaveWorldHandle);
    UPackage::PackageSavedWithContextEvent.Remove(PackageSavedHandle);
    if (DeferredBakeHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(DeferredBakeHandle);
        DeferredBakeHandle.Reset();
    }
    PendingWorlds.Reset();
}

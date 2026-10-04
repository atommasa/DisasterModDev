// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FObjectPostSaveContext;
class UPackage;
class UWorld;

class FGameZoneBakeCoordinator
{
public:
    static void Register();
    static void Unregister();

    static bool ShouldScheduleWorldSave(
        const UWorld& World,
        bool bSaveSucceeded,
        bool bFromAutoSave,
        bool bProceduralSave,
        bool bCooking,
        const UWorld* EditorWorld);
    static void QueueWorldForBake(UWorld& World);
    static bool QueueSavedPackageForBake(
        UPackage& SavedPackage,
        bool bSaveSucceeded,
        bool bFromAutoSave,
        bool bProceduralSave,
        bool bCooking,
        UWorld* EditorWorld);
    static void ProcessPendingBakes();
    static int32 GetPendingWorldCount();

private:
    static void HandlePostSaveWorld(UWorld* World, FObjectPostSaveContext SaveContext);
    static void HandlePackageSaved(const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext SaveContext);
    static bool HandleDeferredBake(float DeltaTime);
};

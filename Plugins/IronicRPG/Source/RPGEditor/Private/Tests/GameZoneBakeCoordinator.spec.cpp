// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "GameZoneBakeCoordinator.h"

#include "EditorWorldUtils.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneBakeSaveSchedulingTest,
    "IronicRPG.GameZone.Map.Bake.SaveSchedulingFiltersAndDeduplicates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneBakeSaveSchedulingTest::RunTest(const FString& Parameters)
{
    static int32 WorldIndex = 0;
    const FString WorldName = FString::Printf(TEXT("Phase4SaveScheduling_%d"), ++WorldIndex);
    UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Game/IronicRPGTests/%s"), *WorldName));
    const UWorld::InitializationValues InitializationValues = UWorld::InitializationValues()
        .AllowAudioPlayback(false)
        .CreatePhysicsScene(false)
        .CreateNavigation(false)
        .CreateAISystem(false)
        .ShouldSimulatePhysics(false)
        .CreateFXSystem(false);
    UWorld* World = UWorld::CreateWorld(
        EWorldType::Editor,
        false,
        FName(*WorldName),
        Package,
        false,
        ERHIFeatureLevel::Num,
        &InitializationValues,
        true);
    TUniquePtr<FScopedEditorWorld> WorldScope = MakeUnique<FScopedEditorWorld>(World, InitializationValues);

    TestNotNull(TEXT("The scheduling fixture creates an Editor World"), World);
    if (!World)
    {
        return false;
    }

    TestTrue(
        TEXT("A successful explicit persistent-Level save is scheduled"),
        FGameZoneBakeCoordinator::ShouldScheduleWorldSave(*World, true, false, false, false, World));
    TestFalse(
        TEXT("Autosave is ignored"),
        FGameZoneBakeCoordinator::ShouldScheduleWorldSave(*World, true, true, false, false, World));
    TestFalse(
        TEXT("Procedural save is ignored"),
        FGameZoneBakeCoordinator::ShouldScheduleWorldSave(*World, true, false, true, false, World));
    TestFalse(
        TEXT("Cooking is ignored"),
        FGameZoneBakeCoordinator::ShouldScheduleWorldSave(*World, true, false, false, true, World));
    TestFalse(
        TEXT("A streaming or non-current World is ignored"),
        FGameZoneBakeCoordinator::ShouldScheduleWorldSave(*World, true, false, false, false, nullptr));
    TestFalse(
        TEXT("A failed save is ignored"),
        FGameZoneBakeCoordinator::ShouldScheduleWorldSave(*World, false, false, false, false, World));

    FGameZoneBakeCoordinator::QueueWorldForBake(*World);
    FGameZoneBakeCoordinator::QueueWorldForBake(*World);
    TestEqual(TEXT("Repeated save callbacks queue one World"), FGameZoneBakeCoordinator::GetPendingWorldCount(), 1);
    FGameZoneBakeCoordinator::ProcessPendingBakes();
    TestEqual(TEXT("Processing drains the queue"), FGameZoneBakeCoordinator::GetPendingWorldCount(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneBakeExternalActorSaveSchedulingTest,
    "IronicRPG.GameZone.Map.Bake.ExternalActorSaveSchedulesOwningWorld",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneBakeExternalActorSaveSchedulingTest::RunTest(const FString& Parameters)
{
    static int32 WorldIndex = 0;
    const FString WorldName = FString::Printf(TEXT("Phase4ExternalActorSave_%d"), ++WorldIndex);
    UPackage* WorldPackage = CreatePackage(*FString::Printf(TEXT("/Game/IronicRPGTests/%s"), *WorldName));
    const UWorld::InitializationValues InitializationValues = UWorld::InitializationValues()
        .AllowAudioPlayback(false)
        .CreatePhysicsScene(false)
        .CreateNavigation(false)
        .CreateAISystem(false)
        .ShouldSimulatePhysics(false)
        .CreateFXSystem(false);
    UWorld* World = UWorld::CreateWorld(
        EWorldType::Editor,
        false,
        FName(*WorldName),
        WorldPackage,
        false,
        ERHIFeatureLevel::Num,
        &InitializationValues,
        true);
    TUniquePtr<FScopedEditorWorld> WorldScope = MakeUnique<FScopedEditorWorld>(World, InitializationValues);

    TestNotNull(TEXT("The external-actor save fixture creates an Editor World"), World);
    if (!World)
    {
        return false;
    }

    AActor* Actor = World->SpawnActor<AActor>();
    UPackage* ExternalActorPackage = CreatePackage(
        *(ULevel::GetExternalActorsPath(WorldPackage->GetName()) + TEXT("/Actor")));
    TestNotNull(TEXT("The fixture creates an Actor"), Actor);
    TestNotNull(TEXT("The fixture creates an external Actor package"), ExternalActorPackage);
    if (!Actor || !ExternalActorPackage)
    {
        return false;
    }

    Actor->SetPackageExternal(true, false, ExternalActorPackage);
    TestEqual(TEXT("The Actor is owned by the external package"), Actor->GetExternalPackage(), ExternalActorPackage);
    TestTrue(
        TEXT("Saving a current World external Actor package schedules its owning World"),
        FGameZoneBakeCoordinator::QueueSavedPackageForBake(
            *ExternalActorPackage,
            true,
            false,
            false,
            false,
            World));
    TestEqual(TEXT("The external Actor package queues one owning World"), FGameZoneBakeCoordinator::GetPendingWorldCount(), 1);

    FGameZoneBakeCoordinator::ProcessPendingBakes();
    TestEqual(TEXT("Processing drains the external Actor queue"), FGameZoneBakeCoordinator::GetPendingWorldCount(), 0);

    Actor->SetPackageExternal(false, false);
    TestNull(
        TEXT("The fallback fixture no longer has a loaded Actor in the external package"),
        AActor::FindActorInPackage(ExternalActorPackage, false));
    TestTrue(
        TEXT("An external Actor path still resolves the owning World after its Actor is removed"),
        FGameZoneBakeCoordinator::QueueSavedPackageForBake(
            *ExternalActorPackage,
            true,
            false,
            false,
            false,
            World));
    TestEqual(TEXT("The external path fallback queues one owning World"), FGameZoneBakeCoordinator::GetPendingWorldCount(), 1);
    FGameZoneBakeCoordinator::ProcessPendingBakes();
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

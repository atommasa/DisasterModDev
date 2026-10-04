// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "GameZoneSubsystem.h"
#include "GameZoneSaveModule.h"
#include "Tests/GameZoneSubsystemTestTypes.h"

#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Levels/GameZonePointComponent.h"
#include "Levels/GameZonePointRegistryProvider.h"
#include "Levels/GameZoneAsset.h"
#include "Maps/GameZoneMapTypes.h"
#include "MapMarkers/GameZoneMarkerTypes.h"
#include "SaveGame/RPGSaveGame.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace
{
    enum class EMapTexturePresentationTestPhase : uint8
    {
        WaitingForPresentation,
        WaitingForRetainingCompletion,
        WaitingForPausedCompletion,
    };

    template <typename ValueType>
    ValueType& GetPropertyValue(UObject& Object, const FName PropertyName)
    {
        FProperty* Property = FindFProperty<FProperty>(Object.GetClass(), PropertyName);
        check(Property);
        return *Property->ContainerPtrToValuePtr<ValueType>(&Object);
    }

    struct FMapTexturePresentationTestState
    {
        TStrongObjectPtr<UGameInstance> GameInstance;
        TStrongObjectPtr<UGameZoneSubsystem> Subsystem;
        TStrongObjectPtr<UGameZoneAsset> ZoneAsset;
        TStrongObjectPtr<UTexture2D> Texture;
        TStrongObjectPtr<UGameZoneMapTextureTestReceiver> Receiver;
        FGameZonePresentationHandle PresentationHandle;
        FGameZonePresentationHandle PausedPresentationHandle;
        TWeakObjectPtr<APlayerState> PauserPlayerState;
        FGuid PausedRequestId;
        TArray<FGameZoneMapLayer> OriginalLayers;
        TArray<FGameZoneMapSheet> OriginalSheets;
        FGameZoneMapSheetId OriginalDefaultSheetId;
        TArray<FGameZoneMapSheetMapping> OriginalMappings;
        TArray<FGameZoneMapRegion> OriginalRegions;
        TMap<FGuid, FGameZonePointData> OriginalPoints;
        FGuid OriginalMapBakeRevision;
        EMapTexturePresentationTestPhase Phase = EMapTexturePresentationTestPhase::WaitingForPresentation;
        double StartTime = FPlatformTime::Seconds();

        void RestoreAndShutdown()
        {
            if (ZoneAsset.IsValid())
            {
                GetPropertyValue<TArray<FGameZoneMapLayer>>(*ZoneAsset, TEXT("MapLayers")) = OriginalLayers;
                GetPropertyValue<TArray<FGameZoneMapSheet>>(*ZoneAsset, TEXT("MapSheets")) = OriginalSheets;
                GetPropertyValue<FGameZoneMapSheetId>(*ZoneAsset, TEXT("DefaultSheetId")) = OriginalDefaultSheetId;
                GetPropertyValue<TArray<FGameZoneMapSheetMapping>>(*ZoneAsset, TEXT("BakedSheetMappings")) = OriginalMappings;
                GetPropertyValue<TArray<FGameZoneMapRegion>>(*ZoneAsset, TEXT("BakedMapRegions")) = OriginalRegions;
                GetPropertyValue<TMap<FGuid, FGameZonePointData>>(*ZoneAsset, TEXT("BakedPoints")) = OriginalPoints;
                GetPropertyValue<FGuid>(*ZoneAsset, TEXT("MapBakeRevision")) = OriginalMapBakeRevision;
            }

            UWorld* World = GameInstance.IsValid() ? GameInstance->GetWorld() : nullptr;
            if (World && World->GetWorldSettings())
            {
                World->GetWorldSettings()->SetPauserPlayerState(nullptr);
            }
            if (PauserPlayerState.IsValid())
            {
                PauserPlayerState->Destroy();
            }
            if (Subsystem.IsValid())
            {
                Subsystem->EndMapPresentation(PausedPresentationHandle);
                Subsystem->EndMapPresentation(PresentationHandle);
            }
            if (GameInstance.IsValid())
            {
                GameInstance->Shutdown();
            }
            if (World)
            {
                World->DestroyWorld(false);
                GEngine->DestroyWorldContext(World);
            }
        }
    };
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
    FWaitForMapTexturePresentationCommand,
    TSharedPtr<FMapTexturePresentationTestState>,
    State,
    FAutomationTestBase*,
    Test);

bool FWaitForMapTexturePresentationCommand::Update()
{
    UWorld* World = State->GameInstance->GetWorld();
    if (World)
    {
        World->Tick(LEVELTICK_All, 1.0f / 60.0f);
    }

    if (State->Phase == EMapTexturePresentationTestPhase::WaitingForPresentation)
    {
        FResolvedGameZoneMapSheet ResolvedSheet;
        if (!State->Subsystem->ResolveMapSheetAtLocation(State->ZoneAsset->GetId(), FVector::ZeroVector, ResolvedSheet))
        {
            if (FPlatformTime::Seconds() - State->StartTime <= 5.0)
            {
                return false;
            }

            Test->AddError(TEXT("Timed out waiting for the test Zone presentation to load."));
            State->RestoreAndShutdown();
            return true;
        }

        Test->TestEqual(
            TEXT("A resolved Sheet exposes its Mapping WorldSize"),
            ResolvedSheet.WorldSize,
            FVector2D(1000.0, 1000.0));
        Test->TestEqual(
            TEXT("A resolved Sheet exposes its Mapping WorldYaw"),
            ResolvedSheet.WorldYaw,
            37.5f);
        Test->TestEqual(
            TEXT("The presentation publishes one Zone bounds entry"),
            State->Receiver->LastSnapshot.MapWorldBounds.Num(),
            1);
        if (State->Receiver->LastSnapshot.MapWorldBounds.Num() == 1)
        {
            const FGameZoneMapWorldBounds& Bounds = State->Receiver->LastSnapshot.MapWorldBounds[0];
            Test->TestTrue(TEXT("The presentation Zone bounds are valid"), Bounds.bIsValid);
            Test->TestEqual(TEXT("The presentation Zone bounds identify the loaded Zone"), Bounds.ZoneId, State->ZoneAsset->GetId());
            Test->TestEqual(
                TEXT("The presentation Zone bounds retain the bake revision"),
                Bounds.MapBakeRevision,
                State->ZoneAsset->GetMapBakeRevision());
        }

        FOnGameZoneMapTextureReady Completion;
        Completion.BindDynamic(State->Receiver.Get(), &UGameZoneMapTextureTestReceiver::HandleTextureReady);
        const FGuid RequestId = State->Subsystem->RequestMapSheetTexture(
            State->PresentationHandle,
            State->ZoneAsset->GetId(),
            ResolvedSheet.SheetId,
            Completion);
        Test->TestTrue(
            TEXT("A loaded active presentation accepts a Sheet Texture request"),
            RequestId.IsValid());
        State->Phase = EMapTexturePresentationTestPhase::WaitingForRetainingCompletion;
        State->StartTime = FPlatformTime::Seconds();
        return false;
    }

    if (State->Phase == EMapTexturePresentationTestPhase::WaitingForRetainingCompletion)
    {
        if (State->Receiver->CompletionCount == 0)
        {
            if (FPlatformTime::Seconds() - State->StartTime <= 5.0)
            {
                return false;
            }

            Test->AddError(TEXT("Timed out waiting for the retaining Texture request to complete."));
            State->RestoreAndShutdown();
            return true;
        }

        State->PausedPresentationHandle = State->Subsystem->BeginMapPresentation(
            {State->ZoneAsset->GetId()},
            EMapMarkerDisplayMode::WorldMap,
            FOnGameZonePresentationReady());
        Test->TestTrue(TEXT("A second presentation starts for the cached Texture request"), State->PausedPresentationHandle.IsValid());

        APlayerState* Pauser = World ? World->SpawnActor<APlayerState>() : nullptr;
        State->PauserPlayerState = Pauser;
        if (World && World->GetWorldSettings())
        {
            World->GetWorldSettings()->SetPauserPlayerState(Pauser);
        }
        Test->TestTrue(TEXT("The regression World is paused"), World && World->IsPaused());

        FOnGameZoneMapTextureReady Completion;
        Completion.BindDynamic(State->Receiver.Get(), &UGameZoneMapTextureTestReceiver::HandleTextureReady);
        State->PausedRequestId = State->Subsystem->RequestMapSheetTexture(
            State->PausedPresentationHandle,
            State->ZoneAsset->GetId(),
            State->ZoneAsset->GetDefaultSheetId(),
            Completion);
        Test->TestTrue(TEXT("The paused presentation accepts a cached Texture request"), State->PausedRequestId.IsValid());
        Test->TestEqual(TEXT("Cached Texture completion remains asynchronous"), State->Receiver->CompletionCount, 1);
        State->Phase = EMapTexturePresentationTestPhase::WaitingForPausedCompletion;
        State->StartTime = FPlatformTime::Seconds();
        return false;
    }

    if (State->Receiver->CompletionCount >= 2)
    {
        Test->TestEqual(TEXT("The paused cached request produces the matching completion"), State->Receiver->LastResult.RequestId, State->PausedRequestId);
        Test->TestTrue(TEXT("The paused cached request resolves its Texture"), State->Receiver->LastResult.bSucceeded);
        State->RestoreAndShutdown();
        return true;
    }

    if (FPlatformTime::Seconds() - State->StartTime > 5.0)
    {
        Test->AddError(TEXT("Timed out waiting for a cached Texture request while the World was paused."));
        State->RestoreAndShutdown();
        return true;
    }

    return false;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneSubsystemRegistersLiveMarkerTest,
    "IronicRPG.GameZone.Subsystem.RegisteredLiveMarkerCanBeResolved",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneSubsystemRegistersLiveMarkerTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    UGameZoneSubsystem* Subsystem = NewObject<UGameZoneSubsystem>(GameInstance);
    UGameZonePointComponent* Component = NewObject<UGameZonePointComponent>();
    Component->SetMarkerState(EGameZonePointState::Deactivated);

    const EGameZonePointRegistrationResult RegistrationResult =
        Subsystem->RegisterPoint(*Component);

    TestEqual(
        TEXT("Subsystem accepts the live marker"),
        RegistrationResult,
        EGameZonePointRegistrationResult::Registered);

    FResolvedGameZonePoint ResolvedPoint;
    TestTrue(
        TEXT("Registered marker resolves through subsystem"),
        Subsystem->TryResolveMapMarker(Component->GetPointId(), ResolvedPoint));
    TestEqual(
        TEXT("Resolved marker uses live data"),
        ResolvedPoint.Source,
        EGameZonePointResolvedSource::Live);
    TestEqual(
        TEXT("Resolved marker keeps runtime state"),
        ResolvedPoint.Data.MarkerState,
        EGameZonePointState::Deactivated);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneSubsystemSanitizesInvalidLiveAfterGarbageCollectionTest,
    "IronicRPG.GameZone.Subsystem.InvalidLiveIsSanitizedAfterGarbageCollection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneSubsystemSanitizesInvalidLiveAfterGarbageCollectionTest::RunTest(
    const FString& Parameters)
{
    TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>(GEngine));
    GameInstance->InitializeStandalone(TEXT("GameZoneMarkerSanitationAutomationWorld"));

    UWorld* World = GameInstance->GetWorld();
    UGameZoneSubsystem* Subsystem = GameInstance->GetSubsystem<UGameZoneSubsystem>();
    TestNotNull(TEXT("Standalone GameInstance creates the GameZone subsystem adapter"), Subsystem);
    TestNotNull(TEXT("Standalone GameInstance creates a World"), World);
    if (!Subsystem || !World)
    {
        GameInstance->Shutdown();
        return false;
    }

    AActor* Owner = World->SpawnActor<AActor>();
    TestNotNull(TEXT("Test World creates a marker owner"), Owner);
    if (!Owner)
    {
        GameInstance->Shutdown();
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
        return false;
    }

    TStrongObjectPtr<UGameZonePointComponent> StaleComponent(
        NewObject<UGameZonePointComponent>(Owner));
    TestEqual(
        TEXT("The first live marker registers"),
        Subsystem->RegisterPoint(*StaleComponent),
        EGameZonePointRegistrationResult::Registered);

    TWeakObjectPtr<UGameZonePointComponent> StaleProbe(StaleComponent.Get());
    StaleComponent.Reset();
    Owner->Destroy();
    Owner = nullptr;
    CollectGarbage(RF_NoFlags);
    TestTrue(TEXT("The abandoned live source becomes stale"), StaleProbe.IsStale());

    Owner = World->SpawnActor<AActor>();
    TStrongObjectPtr<UGameZonePointComponent> CurrentComponent(
        NewObject<UGameZonePointComponent>(Owner));
    TestEqual(
        TEXT("A marker can register after post-GC sanitation"),
        Subsystem->RegisterPoint(*CurrentComponent),
        EGameZonePointRegistrationResult::Registered);

    FResolvedGameZonePoint ResolvedPoint;
    TestTrue(
        TEXT("The current marker resolves"),
        Subsystem->TryResolveMapMarker(CurrentComponent->GetPointId(), ResolvedPoint));
    TestEqual(
        TEXT("Post-GC sanitation advances the shared registry revision"),
        ResolvedPoint.Revision,
        int64(3));

    CurrentComponent.Reset();
    GameInstance->Shutdown();
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneSubsystemRejectsEmptyPresentationTest,
    "IronicRPG.GameZone.Subsystem.EmptyPresentationIsRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneSubsystemRejectsEmptyPresentationTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    UGameZoneSubsystem* Subsystem = NewObject<UGameZoneSubsystem>(GameInstance);

    const FGameZonePresentationHandle Handle = Subsystem->BeginMapPresentation(
        {},
        EMapMarkerDisplayMode::WorldMap,
        FOnGameZonePresentationReady());

    TestFalse(TEXT("Empty presentation returns an invalid handle"), Handle.IsValid());
    Subsystem->EndMapPresentation(Handle);
    Subsystem->EndMapPresentation(Handle);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneSubsystemRejectsUnloadedMapSheetQueriesTest,
    "IronicRPG.GameZone.Subsystem.UnloadedMapSheetQueriesAreRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneSubsystemRejectsUnloadedMapSheetQueriesTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    UGameZoneSubsystem* Subsystem = NewObject<UGameZoneSubsystem>(GameInstance);
    const FRPGId ZoneId(TEXT("z.UnloadedMapQuery"));
    const FGameZoneMapSheetId SheetId(TEXT("Outdoor_Main"));

    FResolvedGameZoneMapSheet ResolvedSheet;
    ResolvedSheet.SheetId = SheetId;
    TestFalse(
        TEXT("A location query cannot resolve a Zone outside an active presentation"),
        Subsystem->ResolveMapSheetAtLocation(
            ZoneId,
            FVector::ZeroVector,
            ResolvedSheet));
    TestFalse(
        TEXT("A failed location query clears stale resolved data"),
        ResolvedSheet.SheetId.IsValid());

    ResolvedSheet.SheetId = SheetId;
    TestFalse(
        TEXT("A Layer query cannot resolve a Zone outside an active presentation"),
        Subsystem->ResolveMapSheetInLayerAtLocation(
            ZoneId,
            FGameZoneMapLayerId(TEXT("Outdoor")),
            FVector::ZeroVector,
            ResolvedSheet));
    TestFalse(
        TEXT("A failed Layer query clears stale resolved data"),
        ResolvedSheet.SheetId.IsValid());

    ResolvedSheet.SheetId = SheetId;
    TestFalse(
        TEXT("An identity query cannot resolve a Zone outside an active presentation"),
        Subsystem->ResolveMapSheetById(ZoneId, SheetId, ResolvedSheet));
    TestFalse(
        TEXT("A failed identity query clears stale resolved data"),
        ResolvedSheet.SheetId.IsValid());

    FGameZoneMapProjection Projection;
    Projection.UV = FVector2D(0.25, 0.75);
    Projection.bIsInsideSheet = true;
    TestFalse(
        TEXT("A projection query cannot resolve a Zone outside an active presentation"),
        Subsystem->ProjectWorldLocationToMapSheet(
            ZoneId,
            SheetId,
            FVector::ZeroVector,
            Projection));
    TestEqual(
        TEXT("A failed projection query clears stale UV data"),
        Projection.UV,
        FVector2D::ZeroVector);
    TestFalse(
        TEXT("A failed projection query clears stale inside state"),
        Projection.bIsInsideSheet);

    const FGuid TextureRequestId = Subsystem->RequestMapSheetTexture(
        FGameZonePresentationHandle(),
        ZoneId,
        SheetId,
        FOnGameZoneMapTextureReady());
    TestFalse(
        TEXT("A texture request without an active presentation is rejected"),
        TextureRequestId.IsValid());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneSubsystemCompletesCachedTextureRequestWhilePausedTest,
    "IronicRPG.GameZone.Subsystem.CachedTextureRequestCompletesWhilePaused",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneSubsystemCompletesCachedTextureRequestWhilePausedTest::RunTest(
    const FString& Parameters)
{
    UGameZoneAsset* ZoneAsset = LoadObject<UGameZoneAsset>(
        nullptr,
        TEXT("/Game/DataAssets/Zone/DA_NewWorld.DA_NewWorld"));
    TestNotNull(TEXT("The test host provides DA_NewWorld"), ZoneAsset);
    if (!ZoneAsset)
    {
        return false;
    }

    TSharedPtr<FMapTexturePresentationTestState> State =
        MakeShared<FMapTexturePresentationTestState>();
    State->ZoneAsset.Reset(ZoneAsset);
    UTexture2D* ResidentTexture = LoadObject<UTexture2D>(
        nullptr,
        TEXT("/Game/Characters/Portraits/T_AtomPortrait.T_AtomPortrait"));
    TestNotNull(TEXT("The test host provides a resident map Texture"), ResidentTexture);
    if (!ResidentTexture)
    {
        return false;
    }
    State->Texture.Reset(ResidentTexture);
    State->Receiver.Reset(NewObject<UGameZoneMapTextureTestReceiver>());

    State->OriginalLayers = ZoneAsset->GetMapLayers();
    State->OriginalSheets = ZoneAsset->GetMapSheets();
    State->OriginalDefaultSheetId = ZoneAsset->GetDefaultSheetId();
    State->OriginalMappings = ZoneAsset->GetBakedSheetMappings();
    State->OriginalRegions = ZoneAsset->GetBakedMapRegions();
    State->OriginalPoints = ZoneAsset->GetBakedPoints();
    State->OriginalMapBakeRevision = ZoneAsset->GetMapBakeRevision();

    FGameZoneMapLayer Layer;
    Layer.LayerId = FGameZoneMapLayerId(TEXT("Automation"));
    FGameZoneMapSheet Sheet;
    Sheet.SheetId = FGameZoneMapSheetId(TEXT("Automation_Main"));
    Sheet.LayerId = Layer.LayerId;
    Sheet.MapTexture = State->Texture.Get();
    FGameZoneMapSheetMapping Mapping;
    Mapping.SheetId = Sheet.SheetId;
    Mapping.WorldSize = FVector2D(1000.0, 1000.0);
    Mapping.WorldYaw = 37.5f;

    GetPropertyValue<TArray<FGameZoneMapLayer>>(*ZoneAsset, TEXT("MapLayers")) = {Layer};
    GetPropertyValue<TArray<FGameZoneMapSheet>>(*ZoneAsset, TEXT("MapSheets")) = {Sheet};
    GetPropertyValue<FGameZoneMapSheetId>(*ZoneAsset, TEXT("DefaultSheetId")) = Sheet.SheetId;
    GetPropertyValue<TArray<FGameZoneMapSheetMapping>>(*ZoneAsset, TEXT("BakedSheetMappings")) = {Mapping};
    GetPropertyValue<TArray<FGameZoneMapRegion>>(*ZoneAsset, TEXT("BakedMapRegions")).Reset();
    GetPropertyValue<TMap<FGuid, FGameZonePointData>>(*ZoneAsset, TEXT("BakedPoints")).Reset();
    GetPropertyValue<FGuid>(*ZoneAsset, TEXT("MapBakeRevision")) = FGuid::NewGuid();

    State->GameInstance.Reset(NewObject<UGameInstance>(GEngine));
    State->GameInstance->InitializeStandalone(TEXT("GameZoneMapTextureAutomationWorld"));
    State->Subsystem.Reset(NewObject<UGameZoneSubsystem>(State->GameInstance.Get()));
    FOnGameZonePresentationReady PresentationCompletion;
    PresentationCompletion.BindDynamic(State->Receiver.Get(), &UGameZoneMapTextureTestReceiver::HandlePresentationReady);
    State->PresentationHandle = State->Subsystem->BeginMapPresentation(
        {ZoneAsset->GetId()},
        EMapMarkerDisplayMode::MiniMap,
        PresentationCompletion);
    TestTrue(
        TEXT("The configured Zone starts a presentation"),
        State->PresentationHandle.IsValid());
    if (!State->PresentationHandle.IsValid())
    {
        State->RestoreAndShutdown();
        return false;
    }

    ADD_LATENT_AUTOMATION_COMMAND(FWaitForMapTexturePresentationCommand(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerSaveModuleRoundTripTest,
    "IronicRPG.GameZone.Save.MarkerOverrideRoundTripsThroughSaveGame",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerSaveModuleRoundTripTest::RunTest(const FString& Parameters)
{
    const FGuid PointId(111, 222, 333, 444);
    FGameZonePointData PointData;
    PointData.PointId = PointId;
    PointData.ZoneId = FRPGId(TEXT("z.SaveRoundTrip"));
    PointData.MarkerTypeId = FRPGId(TEXT("m.Teleport"));
    PointData.MarkerState = EGameZonePointState::Deactivated;
    PointData.DisplayMode = static_cast<int32>(EMapMarkerDisplayMode::WorldMap | EMapMarkerDisplayMode::MiniMap);
    PointData.SavePolicy = EGameZonePointSavePolicy::Saveable;
    PointData.DisplayName = FText::FromString(TEXT("Saved Marker"));
    PointData.Description = FText::FromString(TEXT("Round-trip payload"));
    PointData.ReferenceAssetId = FRPGId(TEXT("a.TeleportDestination"));
    PointData.WorldTransform = FTransform(FRotator(0.0, 37.0, 0.0), FVector(100.0, 200.0, 300.0));

    FGameZoneSaveModule Module;
    Module.CurrentContext.ZoneId = PointData.ZoneId;
    Module.PointOverrides.Add(PointId, PointData);

    URPGSaveGame* SaveGame = NewObject<URPGSaveGame>();
    SaveGame->SaveModules.Add(
        FGameZoneSaveModule::StaticStruct()->GetFName(),
        FInstancedStruct::Make<FGameZoneSaveModule>(Module));

    TArray<uint8> Bytes;
    TestTrue(TEXT("SaveGame serializes the marker override"), UGameplayStatics::SaveGameToMemory(SaveGame, Bytes));
    USaveGame* LoadedBase = UGameplayStatics::LoadGameFromMemory(Bytes);
    URPGSaveGame* LoadedGame = Cast<URPGSaveGame>(LoadedBase);
    TestNotNull(TEXT("The serialized SaveGame loads"), LoadedGame);
    if (!LoadedGame)
    {
        return false;
    }

    const FInstancedStruct* LoadedStruct = LoadedGame->SaveModules.Find(FGameZoneSaveModule::StaticStruct()->GetFName());
    const FGameZoneSaveModule* LoadedModule = LoadedStruct ? LoadedStruct->GetPtr<FGameZoneSaveModule>() : nullptr;
    TestNotNull(TEXT("The GameZone save module survives the round trip"), LoadedModule);
    if (!LoadedModule)
    {
        return false;
    }

    const FGameZonePointData* LoadedPoint = LoadedModule->PointOverrides.Find(PointId);
    TestNotNull(TEXT("The PointId-indexed override survives the round trip"), LoadedPoint);
    if (!LoadedPoint)
    {
        return false;
    }

    TestEqual(TEXT("PointId survives"), LoadedPoint->PointId, PointData.PointId);
    TestEqual(TEXT("ZoneId survives"), LoadedPoint->ZoneId, PointData.ZoneId);
    TestEqual(TEXT("MarkerTypeId survives"), LoadedPoint->MarkerTypeId, PointData.MarkerTypeId);
    TestEqual(TEXT("Marker state survives"), LoadedPoint->MarkerState, PointData.MarkerState);
    TestEqual(TEXT("Display mode survives"), LoadedPoint->DisplayMode, PointData.DisplayMode);
    TestEqual(TEXT("Save policy survives"), LoadedPoint->SavePolicy, PointData.SavePolicy);
    TestTrue(TEXT("Display name survives"), LoadedPoint->DisplayName.EqualTo(PointData.DisplayName));
    TestTrue(TEXT("Description survives"), LoadedPoint->Description.EqualTo(PointData.Description));
    TestEqual(TEXT("Reference asset survives"), LoadedPoint->ReferenceAssetId, PointData.ReferenceAssetId);
    TestTrue(TEXT("World transform survives"), LoadedPoint->WorldTransform.Equals(PointData.WorldTransform));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneSubsystemCapturesMarkerSavePoliciesTest,
    "IronicRPG.GameZone.Save.SubsystemCapturesMarkerSavePolicies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneSubsystemCapturesMarkerSavePoliciesTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    UGameZoneSubsystem* Subsystem = NewObject<UGameZoneSubsystem>(GameInstance);

    UGameZonePointComponent* SaveableComponent = NewObject<UGameZonePointComponent>();
    FGameZonePointData SaveableData = SaveableComponent->MakePointSnapshot();
    SaveableData.SavePolicy = EGameZonePointSavePolicy::Saveable;
    SaveableData.MarkerState = EGameZonePointState::Deactivated;
    SaveableComponent->ApplyPointDataInitialization(SaveableData);
    TestEqual(
        TEXT("The Saveable marker registers through the subsystem seam"),
        Subsystem->RegisterPoint(*SaveableComponent),
        EGameZonePointRegistrationResult::Registered);

    UGameZonePointComponent* NotSaveableComponent = NewObject<UGameZonePointComponent>();
    FGameZonePointData NotSaveableData = NotSaveableComponent->MakePointSnapshot();
    NotSaveableData.SavePolicy = EGameZonePointSavePolicy::NotSaveable;
    NotSaveableComponent->ApplyPointDataInitialization(NotSaveableData);
    TestEqual(
        TEXT("The NotSaveable marker registers through the subsystem seam"),
        Subsystem->RegisterPoint(*NotSaveableComponent),
        EGameZonePointRegistrationResult::Registered);

    FGameZonePointData CustomData;
    CustomData.PointId = FGuid(444, 333, 222, 111);
    CustomData.SavePolicy = EGameZonePointSavePolicy::Custom;
    CustomData.MarkerState = EGameZonePointState::Deactivated;
    TestTrue(TEXT("The subsystem accepts an explicit Custom override"), Subsystem->SetMapMarkerSaveOverride(CustomData));

    FInstancedStruct SaveData;
    Subsystem->SaveDataTo(SaveData);
    const FGameZoneSaveModule* Module = SaveData.GetPtr<FGameZoneSaveModule>();
    TestNotNull(TEXT("The subsystem emits a GameZone save module"), Module);
    if (!Module)
    {
        return false;
    }

    TestTrue(TEXT("The module contains the automatic Saveable snapshot"), Module->PointOverrides.Contains(SaveableComponent->GetPointId()));
    TestFalse(TEXT("The module omits NotSaveable"), Module->PointOverrides.Contains(NotSaveableComponent->GetPointId()));
    TestTrue(TEXT("The module contains the explicit Custom override"), Module->PointOverrides.Contains(CustomData.PointId));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

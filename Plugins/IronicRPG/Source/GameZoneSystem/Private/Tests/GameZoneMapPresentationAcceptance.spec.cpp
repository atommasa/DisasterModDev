// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "GameZoneSubsystem.h"
#include "Tests/GameZoneSubsystemTestTypes.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Levels/GameZoneAsset.h"
#include "Maps/GameZoneMapTypes.h"
#include "Misc/AutomationTest.h"
#include "Settings/GameZoneSystemSettings.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace
{
    template <typename ValueType>
    ValueType& GetAcceptancePropertyValue(UObject& Object, const FName PropertyName)
    {
        FProperty* Property = FindFProperty<FProperty>(Object.GetClass(), PropertyName);
        check(Property);
        return *Property->ContainerPtrToValuePtr<ValueType>(&Object);
    }

    struct FMapPresentationAcceptanceState
    {
        TStrongObjectPtr<UGameInstance> GameInstance;
        TStrongObjectPtr<UGameZoneSubsystem> Subsystem;
        TStrongObjectPtr<UGameZoneAsset> ZoneAsset;
        TStrongObjectPtr<UTexture2D> Texture;
        TStrongObjectPtr<UTexture2D> SecondTexture;
        TStrongObjectPtr<UGameZoneMapTextureTestReceiver> Receiver;
        TStrongObjectPtr<UGameZoneMapTextureTestReceiver> SecondReceiver;
        FGameZonePresentationHandle PresentationHandle;
        FGameZonePresentationHandle SecondPresentationHandle;
        FGuid TextureRequestId;
        FGuid SecondTextureRequestId;
        TArray<FGameZoneMapLayer> OriginalLayers;
        TArray<FGameZoneMapSheet> OriginalSheets;
        FGameZoneMapSheetId OriginalDefaultSheetId;
        TArray<FGameZoneMapSheetMapping> OriginalMappings;
        TArray<FGameZoneMapRegion> OriginalRegions;
        TMap<FGuid, FGameZonePointData> OriginalPoints;
        FGuid OriginalMapBakeRevision;
        TSoftObjectPtr<UTexture2D> OriginalDefaultMapTexture;
        bool bTextureRequestStarted = false;
        int32 VerificationTicksRemaining = 2;
        double StartTime = FPlatformTime::Seconds();

        void RestoreAndShutdown()
        {
            if (ZoneAsset.IsValid())
            {
                GetAcceptancePropertyValue<TArray<FGameZoneMapLayer>>(*ZoneAsset, TEXT("MapLayers")) = OriginalLayers;
                GetAcceptancePropertyValue<TArray<FGameZoneMapSheet>>(*ZoneAsset, TEXT("MapSheets")) = OriginalSheets;
                GetAcceptancePropertyValue<FGameZoneMapSheetId>(*ZoneAsset, TEXT("DefaultSheetId")) = OriginalDefaultSheetId;
                GetAcceptancePropertyValue<TArray<FGameZoneMapSheetMapping>>(*ZoneAsset, TEXT("BakedSheetMappings")) = OriginalMappings;
                GetAcceptancePropertyValue<TArray<FGameZoneMapRegion>>(*ZoneAsset, TEXT("BakedMapRegions")) = OriginalRegions;
                GetAcceptancePropertyValue<TMap<FGuid, FGameZonePointData>>(*ZoneAsset, TEXT("BakedPoints")) = OriginalPoints;
                GetAcceptancePropertyValue<FGuid>(*ZoneAsset, TEXT("MapBakeRevision")) = OriginalMapBakeRevision;
            }

            GetMutableDefault<UGameZoneSystemSettings>()->DefaultMapTexture = OriginalDefaultMapTexture;

            if (Subsystem.IsValid())
            {
                Subsystem->EndMapPresentation(SecondPresentationHandle);
                Subsystem->EndMapPresentation(PresentationHandle);
            }

            UWorld* World = GameInstance.IsValid() ? GameInstance->GetWorld() : nullptr;
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

    void SaveAcceptanceAssetState(FMapPresentationAcceptanceState& State, UGameZoneAsset& ZoneAsset)
    {
        State.ZoneAsset.Reset(&ZoneAsset);
        State.OriginalLayers = ZoneAsset.GetMapLayers();
        State.OriginalSheets = ZoneAsset.GetMapSheets();
        State.OriginalDefaultSheetId = ZoneAsset.GetDefaultSheetId();
        State.OriginalMappings = ZoneAsset.GetBakedSheetMappings();
        State.OriginalRegions = ZoneAsset.GetBakedMapRegions();
        State.OriginalPoints = ZoneAsset.GetBakedPoints();
        State.OriginalMapBakeRevision = ZoneAsset.GetMapBakeRevision();
        State.OriginalDefaultMapTexture = GetDefault<UGameZoneSystemSettings>()->DefaultMapTexture;
    }
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
    FWaitForPublicRegionSwitchingCommand,
    TSharedPtr<FMapPresentationAcceptanceState>,
    State,
    FAutomationTestBase*,
    Test);

bool FWaitForPublicRegionSwitchingCommand::Update()
{
    UWorld* World = State->GameInstance->GetWorld();
    if (World)
    {
        World->Tick(LEVELTICK_All, 1.0f / 60.0f);
    }

    if (State->Receiver->PresentationCompletionCount == 0)
    {
        if (FPlatformTime::Seconds() - State->StartTime <= 5.0)
        {
            return false;
        }

        Test->AddError(TEXT("Timed out waiting for the Region switching presentation."));
        State->RestoreAndShutdown();
        return true;
    }

    FResolvedGameZoneMapSheet ResolvedSheet;
    Test->TestTrue(
        TEXT("A location inside the Region resolves through the public subsystem seam"),
        State->Subsystem->ResolveMapSheetAtLocation(State->ZoneAsset->GetId(), FVector::ZeroVector, ResolvedSheet));
    Test->TestEqual(
        TEXT("The Region selects its authored Sheet"),
        ResolvedSheet.SheetId,
        FGameZoneMapSheetId(TEXT("Acceptance_Interior")));

    Test->TestTrue(
        TEXT("A location outside the Region still resolves through the public subsystem seam"),
        State->Subsystem->ResolveMapSheetAtLocation(State->ZoneAsset->GetId(), FVector(400.0, 0.0, 0.0), ResolvedSheet));
    Test->TestEqual(
        TEXT("Outside every Region falls back to the default Sheet"),
        ResolvedSheet.SheetId,
        FGameZoneMapSheetId(TEXT("Acceptance_Default")));

    State->RestoreAndShutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapPresentationPublicRegionSwitchingTest,
    "IronicRPG.GameZone.Presentation.PublicRegionSwitchingUsesRegionThenDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapPresentationPublicRegionSwitchingTest::RunTest(const FString& Parameters)
{
    UGameZoneAsset* ZoneAsset = LoadObject<UGameZoneAsset>(nullptr, TEXT("/Game/DataAssets/Zone/DA_NewWorld.DA_NewWorld"));
    TestNotNull(TEXT("The test host provides DA_NewWorld"), ZoneAsset);
    if (!ZoneAsset)
    {
        return false;
    }

    TSharedPtr<FMapPresentationAcceptanceState> State = MakeShared<FMapPresentationAcceptanceState>();
    SaveAcceptanceAssetState(*State, *ZoneAsset);
    State->Receiver.Reset(NewObject<UGameZoneMapTextureTestReceiver>());

    FGameZoneMapLayer DefaultLayer;
    DefaultLayer.LayerId = FGameZoneMapLayerId(TEXT("Acceptance_Base"));
    FGameZoneMapLayer InteriorLayer;
    InteriorLayer.LayerId = FGameZoneMapLayerId(TEXT("Acceptance_Interior"));

    FGameZoneMapSheet DefaultSheet;
    DefaultSheet.SheetId = FGameZoneMapSheetId(TEXT("Acceptance_Default"));
    DefaultSheet.LayerId = DefaultLayer.LayerId;
    FGameZoneMapSheet InteriorSheet;
    InteriorSheet.SheetId = FGameZoneMapSheetId(TEXT("Acceptance_Interior"));
    InteriorSheet.LayerId = InteriorLayer.LayerId;

    FGameZoneMapSheetMapping DefaultMapping;
    DefaultMapping.SheetId = DefaultSheet.SheetId;
    DefaultMapping.WorldSize = FVector2D(1000.0, 1000.0);
    FGameZoneMapSheetMapping InteriorMapping = DefaultMapping;
    InteriorMapping.SheetId = InteriorSheet.SheetId;

    FGameZoneMapRegion InteriorRegion;
    InteriorRegion.RegionId = FGameZoneMapRegionId(TEXT("Acceptance_InteriorRegion"));
    InteriorRegion.SheetId = InteriorSheet.SheetId;
    InteriorRegion.Priority = 10;
    InteriorRegion.Bounds = FBox(FVector(-100.0), FVector(100.0));

    GetAcceptancePropertyValue<TArray<FGameZoneMapLayer>>(*ZoneAsset, TEXT("MapLayers")) = {DefaultLayer, InteriorLayer};
    GetAcceptancePropertyValue<TArray<FGameZoneMapSheet>>(*ZoneAsset, TEXT("MapSheets")) = {DefaultSheet, InteriorSheet};
    GetAcceptancePropertyValue<FGameZoneMapSheetId>(*ZoneAsset, TEXT("DefaultSheetId")) = DefaultSheet.SheetId;
    GetAcceptancePropertyValue<TArray<FGameZoneMapSheetMapping>>(*ZoneAsset, TEXT("BakedSheetMappings")) = {DefaultMapping, InteriorMapping};
    GetAcceptancePropertyValue<TArray<FGameZoneMapRegion>>(*ZoneAsset, TEXT("BakedMapRegions")) = {InteriorRegion};
    GetAcceptancePropertyValue<TMap<FGuid, FGameZonePointData>>(*ZoneAsset, TEXT("BakedPoints")).Reset();
    GetAcceptancePropertyValue<FGuid>(*ZoneAsset, TEXT("MapBakeRevision")) = FGuid::NewGuid();

    State->GameInstance.Reset(NewObject<UGameInstance>(GEngine));
    State->GameInstance->InitializeStandalone(TEXT("GameZoneRegionSwitchingAcceptanceWorld"));
    State->Subsystem.Reset(NewObject<UGameZoneSubsystem>(State->GameInstance.Get()));

    FOnGameZonePresentationReady Completion;
    Completion.BindDynamic(State->Receiver.Get(), &UGameZoneMapTextureTestReceiver::HandlePresentationReady);
    State->PresentationHandle = State->Subsystem->BeginMapPresentation(
        {ZoneAsset->GetId()},
        EMapMarkerDisplayMode::WorldMap,
        Completion);
    TestTrue(TEXT("The Region switching presentation starts"), State->PresentationHandle.IsValid());
    if (!State->PresentationHandle.IsValid())
    {
        State->RestoreAndShutdown();
        return false;
    }

    ADD_LATENT_AUTOMATION_COMMAND(FWaitForPublicRegionSwitchingCommand(State, this));
    return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
    FWaitForDefaultMapTextureFallbackCommand,
    TSharedPtr<FMapPresentationAcceptanceState>,
    State,
    FAutomationTestBase*,
    Test);

bool FWaitForDefaultMapTextureFallbackCommand::Update()
{
    UWorld* World = State->GameInstance->GetWorld();
    if (World)
    {
        World->Tick(LEVELTICK_All, 1.0f / 60.0f);
    }

    if (State->Receiver->PresentationCompletionCount == 0)
    {
        if (FPlatformTime::Seconds() - State->StartTime <= 5.0)
        {
            return false;
        }

        Test->AddError(TEXT("Timed out waiting for the default Texture fallback presentation."));
        State->RestoreAndShutdown();
        return true;
    }

    if (!State->bTextureRequestStarted)
    {
        FOnGameZoneMapTextureReady Completion;
        Completion.BindDynamic(State->Receiver.Get(), &UGameZoneMapTextureTestReceiver::HandleTextureReady);
        State->TextureRequestId = State->Subsystem->RequestMapSheetTexture(
            State->PresentationHandle,
            State->ZoneAsset->GetId(),
            FGameZoneMapSheetId(TEXT("Acceptance_NoTexture")),
            Completion);
        Test->TestTrue(TEXT("A Sheet without a dedicated Texture accepts a fallback request"), State->TextureRequestId.IsValid());
        State->bTextureRequestStarted = true;
        State->StartTime = FPlatformTime::Seconds();
        return false;
    }

    if (State->Receiver->CompletionCount == 0)
    {
        if (FPlatformTime::Seconds() - State->StartTime <= 5.0)
        {
            return false;
        }

        Test->AddError(TEXT("Timed out waiting for the default Texture fallback result."));
        State->RestoreAndShutdown();
        return true;
    }

    Test->TestEqual(TEXT("The fallback result matches the accepted request"), State->Receiver->LastResult.RequestId, State->TextureRequestId);
    Test->TestTrue(TEXT("The fallback request succeeds"), State->Receiver->LastResult.bSucceeded);
    Test->TestTrue(TEXT("The result identifies use of DefaultMapTexture"), State->Receiver->LastResult.bUsedDefaultTexture);
    Test->TestEqual(TEXT("The resolved Texture is the configured default"), State->Receiver->LastResult.Texture.Get(), State->Texture.Get());

    State->RestoreAndShutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapPresentationDefaultTextureFallbackTest,
    "IronicRPG.GameZone.Presentation.MissingSheetTextureUsesConfiguredDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapPresentationDefaultTextureFallbackTest::RunTest(const FString& Parameters)
{
    UGameZoneAsset* ZoneAsset = LoadObject<UGameZoneAsset>(nullptr, TEXT("/Game/DataAssets/Zone/DA_NewWorld.DA_NewWorld"));
    UTexture2D* DefaultTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Characters/Portraits/T_AtomPortrait.T_AtomPortrait"));
    TestNotNull(TEXT("The test host provides DA_NewWorld"), ZoneAsset);
    TestNotNull(TEXT("The test host provides a resident default Texture"), DefaultTexture);
    if (!ZoneAsset || !DefaultTexture)
    {
        return false;
    }

    TSharedPtr<FMapPresentationAcceptanceState> State = MakeShared<FMapPresentationAcceptanceState>();
    SaveAcceptanceAssetState(*State, *ZoneAsset);
    State->Texture.Reset(DefaultTexture);
    State->Receiver.Reset(NewObject<UGameZoneMapTextureTestReceiver>());
    GetMutableDefault<UGameZoneSystemSettings>()->DefaultMapTexture = DefaultTexture;

    FGameZoneMapLayer Layer;
    Layer.LayerId = FGameZoneMapLayerId(TEXT("Acceptance_Texture"));
    FGameZoneMapSheet Sheet;
    Sheet.SheetId = FGameZoneMapSheetId(TEXT("Acceptance_NoTexture"));
    Sheet.LayerId = Layer.LayerId;
    FGameZoneMapSheetMapping Mapping;
    Mapping.SheetId = Sheet.SheetId;
    Mapping.WorldSize = FVector2D(1000.0, 1000.0);

    GetAcceptancePropertyValue<TArray<FGameZoneMapLayer>>(*ZoneAsset, TEXT("MapLayers")) = {Layer};
    GetAcceptancePropertyValue<TArray<FGameZoneMapSheet>>(*ZoneAsset, TEXT("MapSheets")) = {Sheet};
    GetAcceptancePropertyValue<FGameZoneMapSheetId>(*ZoneAsset, TEXT("DefaultSheetId")) = Sheet.SheetId;
    GetAcceptancePropertyValue<TArray<FGameZoneMapSheetMapping>>(*ZoneAsset, TEXT("BakedSheetMappings")) = {Mapping};
    GetAcceptancePropertyValue<TArray<FGameZoneMapRegion>>(*ZoneAsset, TEXT("BakedMapRegions")).Reset();
    GetAcceptancePropertyValue<TMap<FGuid, FGameZonePointData>>(*ZoneAsset, TEXT("BakedPoints")).Reset();
    GetAcceptancePropertyValue<FGuid>(*ZoneAsset, TEXT("MapBakeRevision")) = FGuid::NewGuid();

    State->GameInstance.Reset(NewObject<UGameInstance>(GEngine));
    State->GameInstance->InitializeStandalone(TEXT("GameZoneDefaultTextureAcceptanceWorld"));
    State->Subsystem.Reset(NewObject<UGameZoneSubsystem>(State->GameInstance.Get()));

    FOnGameZonePresentationReady Completion;
    Completion.BindDynamic(State->Receiver.Get(), &UGameZoneMapTextureTestReceiver::HandlePresentationReady);
    State->PresentationHandle = State->Subsystem->BeginMapPresentation(
        {ZoneAsset->GetId()},
        EMapMarkerDisplayMode::WorldMap,
        Completion);
    TestTrue(TEXT("The default Texture fallback presentation starts"), State->PresentationHandle.IsValid());
    if (!State->PresentationHandle.IsValid())
    {
        State->RestoreAndShutdown();
        return false;
    }

    ADD_LATENT_AUTOMATION_COMMAND(FWaitForDefaultMapTextureFallbackCommand(State, this));
    return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
    FWaitForMissingMapTextureFailureCommand,
    TSharedPtr<FMapPresentationAcceptanceState>,
    State,
    FAutomationTestBase*,
    Test);

bool FWaitForMissingMapTextureFailureCommand::Update()
{
    UWorld* World = State->GameInstance->GetWorld();
    if (World)
    {
        World->Tick(LEVELTICK_All, 1.0f / 60.0f);
    }

    if (State->Receiver->PresentationCompletionCount == 0)
    {
        if (FPlatformTime::Seconds() - State->StartTime <= 5.0)
        {
            return false;
        }

        Test->AddError(TEXT("Timed out waiting for the missing Texture presentation."));
        State->RestoreAndShutdown();
        return true;
    }

    if (!State->bTextureRequestStarted)
    {
        FOnGameZoneMapTextureReady Completion;
        Completion.BindDynamic(State->Receiver.Get(), &UGameZoneMapTextureTestReceiver::HandleTextureReady);
        State->TextureRequestId = State->Subsystem->RequestMapSheetTexture(
            State->PresentationHandle,
            State->ZoneAsset->GetId(),
            FGameZoneMapSheetId(TEXT("Acceptance_MissingTexture")),
            Completion);
        Test->TestTrue(TEXT("A valid Sheet accepts a request even when no Texture source exists"), State->TextureRequestId.IsValid());
        Test->TestEqual(TEXT("The missing Texture result remains asynchronous"), State->Receiver->CompletionCount, 0);
        State->bTextureRequestStarted = true;
        State->StartTime = FPlatformTime::Seconds();
        return false;
    }

    if (State->Receiver->CompletionCount == 0)
    {
        if (FPlatformTime::Seconds() - State->StartTime <= 5.0)
        {
            return false;
        }

        Test->AddError(TEXT("Timed out waiting for the missing Texture failure result."));
        State->RestoreAndShutdown();
        return true;
    }

    Test->TestEqual(TEXT("The failure result matches the accepted request"), State->Receiver->LastResult.RequestId, State->TextureRequestId);
    Test->TestFalse(TEXT("The request reports failure when no Texture source exists"), State->Receiver->LastResult.bSucceeded);
    Test->TestFalse(TEXT("A failed request does not report successful default use"), State->Receiver->LastResult.bUsedDefaultTexture);
    Test->TestNull(TEXT("A failed request returns no Texture"), State->Receiver->LastResult.Texture.Get());

    State->RestoreAndShutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapPresentationMissingTextureFailureTest,
    "IronicRPG.GameZone.Presentation.MissingAllTextureSourcesCompletesWithFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapPresentationMissingTextureFailureTest::RunTest(const FString& Parameters)
{
    UGameZoneAsset* ZoneAsset = LoadObject<UGameZoneAsset>(nullptr, TEXT("/Game/DataAssets/Zone/DA_NewWorld.DA_NewWorld"));
    TestNotNull(TEXT("The test host provides DA_NewWorld"), ZoneAsset);
    if (!ZoneAsset)
    {
        return false;
    }

    TSharedPtr<FMapPresentationAcceptanceState> State = MakeShared<FMapPresentationAcceptanceState>();
    SaveAcceptanceAssetState(*State, *ZoneAsset);
    State->Receiver.Reset(NewObject<UGameZoneMapTextureTestReceiver>());
    GetMutableDefault<UGameZoneSystemSettings>()->DefaultMapTexture.Reset();

    FGameZoneMapLayer Layer;
    Layer.LayerId = FGameZoneMapLayerId(TEXT("Acceptance_Texture"));
    FGameZoneMapSheet Sheet;
    Sheet.SheetId = FGameZoneMapSheetId(TEXT("Acceptance_MissingTexture"));
    Sheet.LayerId = Layer.LayerId;
    FGameZoneMapSheetMapping Mapping;
    Mapping.SheetId = Sheet.SheetId;
    Mapping.WorldSize = FVector2D(1000.0, 1000.0);

    GetAcceptancePropertyValue<TArray<FGameZoneMapLayer>>(*ZoneAsset, TEXT("MapLayers")) = {Layer};
    GetAcceptancePropertyValue<TArray<FGameZoneMapSheet>>(*ZoneAsset, TEXT("MapSheets")) = {Sheet};
    GetAcceptancePropertyValue<FGameZoneMapSheetId>(*ZoneAsset, TEXT("DefaultSheetId")) = Sheet.SheetId;
    GetAcceptancePropertyValue<TArray<FGameZoneMapSheetMapping>>(*ZoneAsset, TEXT("BakedSheetMappings")) = {Mapping};
    GetAcceptancePropertyValue<TArray<FGameZoneMapRegion>>(*ZoneAsset, TEXT("BakedMapRegions")).Reset();
    GetAcceptancePropertyValue<TMap<FGuid, FGameZonePointData>>(*ZoneAsset, TEXT("BakedPoints")).Reset();
    GetAcceptancePropertyValue<FGuid>(*ZoneAsset, TEXT("MapBakeRevision")) = FGuid::NewGuid();

    State->GameInstance.Reset(NewObject<UGameInstance>(GEngine));
    State->GameInstance->InitializeStandalone(TEXT("GameZoneMissingTextureAcceptanceWorld"));
    State->Subsystem.Reset(NewObject<UGameZoneSubsystem>(State->GameInstance.Get()));

    FOnGameZonePresentationReady Completion;
    Completion.BindDynamic(State->Receiver.Get(), &UGameZoneMapTextureTestReceiver::HandlePresentationReady);
    State->PresentationHandle = State->Subsystem->BeginMapPresentation(
        {ZoneAsset->GetId()},
        EMapMarkerDisplayMode::WorldMap,
        Completion);
    TestTrue(TEXT("The missing Texture presentation starts"), State->PresentationHandle.IsValid());
    if (!State->PresentationHandle.IsValid())
    {
        State->RestoreAndShutdown();
        return false;
    }

    ADD_LATENT_AUTOMATION_COMMAND(FWaitForMissingMapTextureFailureCommand(State, this));
    return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
    FWaitForDualMapPresentationsCommand,
    TSharedPtr<FMapPresentationAcceptanceState>,
    State,
    FAutomationTestBase*,
    Test);

bool FWaitForDualMapPresentationsCommand::Update()
{
    UWorld* World = State->GameInstance->GetWorld();
    if (World)
    {
        World->Tick(LEVELTICK_All, 1.0f / 60.0f);
    }

    if (State->Receiver->PresentationCompletionCount == 0
        || State->SecondReceiver->PresentationCompletionCount == 0)
    {
        if (FPlatformTime::Seconds() - State->StartTime <= 5.0)
        {
            return false;
        }

        Test->AddError(TEXT("Timed out waiting for both map presentations."));
        State->RestoreAndShutdown();
        return true;
    }

    const FGuid MiniMapPointId(100, 200, 300, 400);
    const FGuid WorldMapPointId(500, 600, 700, 800);
    Test->TestEqual(TEXT("The MiniMap snapshot contains one eligible Marker"), State->Receiver->LastSnapshot.Markers.Num(), 1);
    if (State->Receiver->LastSnapshot.Markers.Num() == 1)
    {
        Test->TestEqual(
            TEXT("The MiniMap snapshot selects the MiniMap Marker"),
            State->Receiver->LastSnapshot.Markers[0].Data.PointId,
            MiniMapPointId);
    }
    Test->TestEqual(TEXT("The World Map snapshot contains one eligible Marker"), State->SecondReceiver->LastSnapshot.Markers.Num(), 1);
    if (State->SecondReceiver->LastSnapshot.Markers.Num() == 1)
    {
        Test->TestEqual(
            TEXT("The World Map snapshot selects the World Map Marker"),
            State->SecondReceiver->LastSnapshot.Markers[0].Data.PointId,
            WorldMapPointId);
    }

    State->Subsystem->EndMapPresentation(State->PresentationHandle);
    State->PresentationHandle = {};
    FResolvedGameZoneMapSheet ResolvedSheet;
    Test->TestTrue(
        TEXT("Ending MiniMap does not unload the Zone retained by World Map"),
        State->Subsystem->ResolveMapSheetAtLocation(State->ZoneAsset->GetId(), FVector::ZeroVector, ResolvedSheet));

    State->RestoreAndShutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapPresentationDualSessionSelectionTest,
    "IronicRPG.GameZone.Presentation.DualSessionsKeepIndependentMarkerSelections",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapPresentationDualSessionSelectionTest::RunTest(const FString& Parameters)
{
    UGameZoneAsset* ZoneAsset = LoadObject<UGameZoneAsset>(nullptr, TEXT("/Game/DataAssets/Zone/DA_NewWorld.DA_NewWorld"));
    TestNotNull(TEXT("The test host provides DA_NewWorld"), ZoneAsset);
    if (!ZoneAsset)
    {
        return false;
    }

    TSharedPtr<FMapPresentationAcceptanceState> State = MakeShared<FMapPresentationAcceptanceState>();
    SaveAcceptanceAssetState(*State, *ZoneAsset);
    State->Receiver.Reset(NewObject<UGameZoneMapTextureTestReceiver>());
    State->SecondReceiver.Reset(NewObject<UGameZoneMapTextureTestReceiver>());

    FGameZoneMapLayer Layer;
    Layer.LayerId = FGameZoneMapLayerId(TEXT("Acceptance_Dual"));
    FGameZoneMapSheet Sheet;
    Sheet.SheetId = FGameZoneMapSheetId(TEXT("Acceptance_Dual"));
    Sheet.LayerId = Layer.LayerId;
    FGameZoneMapSheetMapping Mapping;
    Mapping.SheetId = Sheet.SheetId;
    Mapping.WorldSize = FVector2D(1000.0, 1000.0);

    FGameZonePointData MiniMapPoint;
    MiniMapPoint.PointId = FGuid(100, 200, 300, 400);
    MiniMapPoint.ZoneId = ZoneAsset->GetId();
    MiniMapPoint.DisplayMode = static_cast<int32>(EMapMarkerDisplayMode::MiniMap);
    FGameZonePointData WorldMapPoint;
    WorldMapPoint.PointId = FGuid(500, 600, 700, 800);
    WorldMapPoint.ZoneId = ZoneAsset->GetId();
    WorldMapPoint.DisplayMode = static_cast<int32>(EMapMarkerDisplayMode::WorldMap);

    GetAcceptancePropertyValue<TArray<FGameZoneMapLayer>>(*ZoneAsset, TEXT("MapLayers")) = {Layer};
    GetAcceptancePropertyValue<TArray<FGameZoneMapSheet>>(*ZoneAsset, TEXT("MapSheets")) = {Sheet};
    GetAcceptancePropertyValue<FGameZoneMapSheetId>(*ZoneAsset, TEXT("DefaultSheetId")) = Sheet.SheetId;
    GetAcceptancePropertyValue<TArray<FGameZoneMapSheetMapping>>(*ZoneAsset, TEXT("BakedSheetMappings")) = {Mapping};
    GetAcceptancePropertyValue<TArray<FGameZoneMapRegion>>(*ZoneAsset, TEXT("BakedMapRegions")).Reset();
    GetAcceptancePropertyValue<TMap<FGuid, FGameZonePointData>>(*ZoneAsset, TEXT("BakedPoints")) = {
        {MiniMapPoint.PointId, MiniMapPoint},
        {WorldMapPoint.PointId, WorldMapPoint},
    };
    GetAcceptancePropertyValue<FGuid>(*ZoneAsset, TEXT("MapBakeRevision")) = FGuid::NewGuid();

    State->GameInstance.Reset(NewObject<UGameInstance>(GEngine));
    State->GameInstance->InitializeStandalone(TEXT("GameZoneDualPresentationAcceptanceWorld"));
    State->Subsystem.Reset(NewObject<UGameZoneSubsystem>(State->GameInstance.Get()));

    FOnGameZonePresentationReady MiniMapCompletion;
    MiniMapCompletion.BindDynamic(State->Receiver.Get(), &UGameZoneMapTextureTestReceiver::HandlePresentationReady);
    State->PresentationHandle = State->Subsystem->BeginMapPresentation(
        {ZoneAsset->GetId()},
        EMapMarkerDisplayMode::MiniMap,
        MiniMapCompletion);
    FOnGameZonePresentationReady WorldMapCompletion;
    WorldMapCompletion.BindDynamic(State->SecondReceiver.Get(), &UGameZoneMapTextureTestReceiver::HandlePresentationReady);
    State->SecondPresentationHandle = State->Subsystem->BeginMapPresentation(
        {ZoneAsset->GetId()},
        EMapMarkerDisplayMode::WorldMap,
        WorldMapCompletion);

    TestTrue(TEXT("The MiniMap presentation starts"), State->PresentationHandle.IsValid());
    TestTrue(TEXT("The World Map presentation starts"), State->SecondPresentationHandle.IsValid());
    TestNotEqual(
        TEXT("Concurrent presentations use distinct handles"),
        State->PresentationHandle.SessionId,
        State->SecondPresentationHandle.SessionId);
    if (!State->PresentationHandle.IsValid() || !State->SecondPresentationHandle.IsValid())
    {
        State->RestoreAndShutdown();
        return false;
    }

    ADD_LATENT_AUTOMATION_COMMAND(FWaitForDualMapPresentationsCommand(State, this));
    return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
    FWaitForSupersededMapTextureRequestCommand,
    TSharedPtr<FMapPresentationAcceptanceState>,
    State,
    FAutomationTestBase*,
    Test);

bool FWaitForSupersededMapTextureRequestCommand::Update()
{
    UWorld* World = State->GameInstance->GetWorld();
    if (World)
    {
        World->Tick(LEVELTICK_All, 1.0f / 60.0f);
    }

    if (State->Receiver->PresentationCompletionCount == 0)
    {
        if (FPlatformTime::Seconds() - State->StartTime <= 5.0)
        {
            return false;
        }

        Test->AddError(TEXT("Timed out waiting for the superseded Texture request presentation."));
        State->RestoreAndShutdown();
        return true;
    }

    if (!State->bTextureRequestStarted)
    {
        FOnGameZoneMapTextureReady FirstCompletion;
        FirstCompletion.BindDynamic(State->Receiver.Get(), &UGameZoneMapTextureTestReceiver::HandleTextureReady);
        State->TextureRequestId = State->Subsystem->RequestMapSheetTexture(
            State->PresentationHandle,
            State->ZoneAsset->GetId(),
            FGameZoneMapSheetId(TEXT("Acceptance_Stale")),
            FirstCompletion);

        FOnGameZoneMapTextureReady SecondCompletion;
        SecondCompletion.BindDynamic(State->SecondReceiver.Get(), &UGameZoneMapTextureTestReceiver::HandleTextureReady);
        State->SecondTextureRequestId = State->Subsystem->RequestMapSheetTexture(
            State->PresentationHandle,
            State->ZoneAsset->GetId(),
            FGameZoneMapSheetId(TEXT("Acceptance_Current")),
            SecondCompletion);

        Test->TestTrue(TEXT("The first Texture request is accepted"), State->TextureRequestId.IsValid());
        Test->TestTrue(TEXT("The replacement Texture request is accepted"), State->SecondTextureRequestId.IsValid());
        Test->TestNotEqual(TEXT("Replacement Texture requests have distinct identities"), State->TextureRequestId, State->SecondTextureRequestId);
        State->bTextureRequestStarted = true;
        State->StartTime = FPlatformTime::Seconds();
        return false;
    }

    if (State->SecondReceiver->CompletionCount == 0)
    {
        if (FPlatformTime::Seconds() - State->StartTime <= 5.0)
        {
            return false;
        }

        Test->AddError(TEXT("Timed out waiting for the replacement Texture request."));
        State->RestoreAndShutdown();
        return true;
    }

    if (State->VerificationTicksRemaining-- > 0)
    {
        return false;
    }

    Test->TestEqual(TEXT("The superseded request never calls its completion"), State->Receiver->CompletionCount, 0);
    Test->TestEqual(TEXT("Only the replacement request completes"), State->SecondReceiver->CompletionCount, 1);
    Test->TestEqual(
        TEXT("The completion belongs to the replacement request"),
        State->SecondReceiver->LastResult.RequestId,
        State->SecondTextureRequestId);
    Test->TestTrue(TEXT("The replacement request succeeds"), State->SecondReceiver->LastResult.bSucceeded);
    Test->TestEqual(
        TEXT("The replacement request returns its Sheet Texture"),
        State->SecondReceiver->LastResult.Texture.Get(),
        State->SecondTexture.Get());

    State->RestoreAndShutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapPresentationSupersededTextureRequestTest,
    "IronicRPG.GameZone.Presentation.SupersededTextureRequestDoesNotComplete",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapPresentationSupersededTextureRequestTest::RunTest(const FString& Parameters)
{
    UGameZoneAsset* ZoneAsset = LoadObject<UGameZoneAsset>(nullptr, TEXT("/Game/DataAssets/Zone/DA_NewWorld.DA_NewWorld"));
    UTexture2D* CurrentTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Characters/Portraits/T_AtomPortrait.T_AtomPortrait"));
    TestNotNull(TEXT("The test host provides DA_NewWorld"), ZoneAsset);
    TestNotNull(TEXT("The test host provides a resident replacement Texture"), CurrentTexture);
    if (!ZoneAsset || !CurrentTexture)
    {
        return false;
    }

    TSharedPtr<FMapPresentationAcceptanceState> State = MakeShared<FMapPresentationAcceptanceState>();
    SaveAcceptanceAssetState(*State, *ZoneAsset);
    State->SecondTexture.Reset(CurrentTexture);
    State->Receiver.Reset(NewObject<UGameZoneMapTextureTestReceiver>());
    State->SecondReceiver.Reset(NewObject<UGameZoneMapTextureTestReceiver>());
    GetMutableDefault<UGameZoneSystemSettings>()->DefaultMapTexture.Reset();

    FGameZoneMapLayer Layer;
    Layer.LayerId = FGameZoneMapLayerId(TEXT("Acceptance_Stale"));
    FGameZoneMapSheet StaleSheet;
    StaleSheet.SheetId = FGameZoneMapSheetId(TEXT("Acceptance_Stale"));
    StaleSheet.LayerId = Layer.LayerId;
    StaleSheet.MapTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Automation/MissingMapTexture.MissingMapTexture")));
    FGameZoneMapSheet CurrentSheet;
    CurrentSheet.SheetId = FGameZoneMapSheetId(TEXT("Acceptance_Current"));
    CurrentSheet.LayerId = Layer.LayerId;
    CurrentSheet.MapTexture = CurrentTexture;

    FGameZoneMapSheetMapping StaleMapping;
    StaleMapping.SheetId = StaleSheet.SheetId;
    StaleMapping.WorldSize = FVector2D(1000.0, 1000.0);
    FGameZoneMapSheetMapping CurrentMapping = StaleMapping;
    CurrentMapping.SheetId = CurrentSheet.SheetId;
    CurrentMapping.WorldOrigin = FVector(2000.0, 0.0, 0.0);

    GetAcceptancePropertyValue<TArray<FGameZoneMapLayer>>(*ZoneAsset, TEXT("MapLayers")) = {Layer};
    GetAcceptancePropertyValue<TArray<FGameZoneMapSheet>>(*ZoneAsset, TEXT("MapSheets")) = {StaleSheet, CurrentSheet};
    GetAcceptancePropertyValue<FGameZoneMapSheetId>(*ZoneAsset, TEXT("DefaultSheetId")) = StaleSheet.SheetId;
    GetAcceptancePropertyValue<TArray<FGameZoneMapSheetMapping>>(*ZoneAsset, TEXT("BakedSheetMappings")) = {StaleMapping, CurrentMapping};
    GetAcceptancePropertyValue<TArray<FGameZoneMapRegion>>(*ZoneAsset, TEXT("BakedMapRegions")).Reset();
    GetAcceptancePropertyValue<TMap<FGuid, FGameZonePointData>>(*ZoneAsset, TEXT("BakedPoints")).Reset();
    GetAcceptancePropertyValue<FGuid>(*ZoneAsset, TEXT("MapBakeRevision")) = FGuid::NewGuid();

    State->GameInstance.Reset(NewObject<UGameInstance>(GEngine));
    State->GameInstance->InitializeStandalone(TEXT("GameZoneStaleTextureAcceptanceWorld"));
    State->Subsystem.Reset(NewObject<UGameZoneSubsystem>(State->GameInstance.Get()));

    FOnGameZonePresentationReady Completion;
    Completion.BindDynamic(State->Receiver.Get(), &UGameZoneMapTextureTestReceiver::HandlePresentationReady);
    State->PresentationHandle = State->Subsystem->BeginMapPresentation(
        {ZoneAsset->GetId()},
        EMapMarkerDisplayMode::WorldMap,
        Completion);
    TestTrue(TEXT("The stale Texture request presentation starts"), State->PresentationHandle.IsValid());
    if (!State->PresentationHandle.IsValid())
    {
        State->RestoreAndShutdown();
        return false;
    }

    ADD_LATENT_AUTOMATION_COMMAND(FWaitForSupersededMapTextureRequestCommand(State, this));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

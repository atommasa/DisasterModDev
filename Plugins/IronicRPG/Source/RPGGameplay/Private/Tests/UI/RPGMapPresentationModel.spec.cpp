// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/RPGMapPresentationModel.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Levels/GameZoneAsset.h"
#include "Levels/MapMarkerTypeAsset.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace
{
template <typename ValueType>
ValueType& GetPresentationModelPropertyValue(UObject& Object, const FName PropertyName)
{
    FProperty* Property = FindFProperty<FProperty>(Object.GetClass(), PropertyName);
    check(Property);
    return *Property->ContainerPtrToValuePtr<ValueType>(&Object);
}

struct FRPGMapPresentationModelTestState
{
    TStrongObjectPtr<UGameInstance> GameInstance;
    TStrongObjectPtr<UGameZoneSubsystem> Subsystem;
    TStrongObjectPtr<URPGMapPresentationModel> Model;
    TStrongObjectPtr<UGameZoneAsset> ZoneAsset;
    TStrongObjectPtr<UMapMarkerTypeAsset> MarkerType;
    TStrongObjectPtr<UTexture2D> FirstTexture;
    TStrongObjectPtr<UTexture2D> SecondTexture;
    TArray<FGameZoneMapLayer> OriginalLayers;
    TArray<FGameZoneMapSheet> OriginalSheets;
    FGameZoneMapSheetId OriginalDefaultSheetId;
    TArray<FGameZoneMapSheetMapping> OriginalMappings;
    TArray<FGameZoneMapRegion> OriginalRegions;
    TMap<FGuid, FGameZonePointData> OriginalPoints;
    FGuid OriginalMapBakeRevision;
    FGuid VisiblePointId;
    FGameZonePointData VisiblePoint;
    FGameZoneMapSheetId FirstSheetId;
    FGameZoneMapSheetId SecondSheetId;
    FGameZonePresentationSnapshot ReadySnapshot;
    FGameZoneMapTextureResult LastTextureResult;
    FRPGMapPresentationMarkerChange LastMarkerChange;
    int32 ReadyCount = 0;
    int32 TextureCount = 0;
    int32 MarkerChangeCount = 0;
    int32 Phase = 0;
    double PhaseStartTime = FPlatformTime::Seconds();

    void RestoreAndShutdown()
    {
        if (Model.IsValid())
        {
            Model->Stop();
        }
        if (Subsystem.IsValid() && VisiblePointId.IsValid())
        {
            Subsystem->ClearMapMarkerSaveOverride(VisiblePointId);
        }
        if (ZoneAsset.IsValid())
        {
            GetPresentationModelPropertyValue<TArray<FGameZoneMapLayer>>(*ZoneAsset, TEXT("MapLayers")) = OriginalLayers;
            GetPresentationModelPropertyValue<TArray<FGameZoneMapSheet>>(*ZoneAsset, TEXT("MapSheets")) = OriginalSheets;
            GetPresentationModelPropertyValue<FGameZoneMapSheetId>(*ZoneAsset, TEXT("DefaultSheetId")) = OriginalDefaultSheetId;
            GetPresentationModelPropertyValue<TArray<FGameZoneMapSheetMapping>>(*ZoneAsset, TEXT("BakedSheetMappings")) = OriginalMappings;
            GetPresentationModelPropertyValue<TArray<FGameZoneMapRegion>>(*ZoneAsset, TEXT("BakedMapRegions")) = OriginalRegions;
            GetPresentationModelPropertyValue<TMap<FGuid, FGameZonePointData>>(*ZoneAsset, TEXT("BakedPoints")) = OriginalPoints;
            GetPresentationModelPropertyValue<FGuid>(*ZoneAsset, TEXT("MapBakeRevision")) = OriginalMapBakeRevision;
        }

        UWorld* World = GameInstance.IsValid() ? GameInstance->GetWorld() : nullptr;
        Model.Reset();
        Subsystem.Reset();
        if (GameInstance.IsValid())
        {
            GameInstance->Shutdown();
        }
        if (World)
        {
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }
        GameInstance.Reset();
    }
};

void SavePresentationModelAssetState(FRPGMapPresentationModelTestState& State, UGameZoneAsset& ZoneAsset)
{
    State.ZoneAsset.Reset(&ZoneAsset);
    State.OriginalLayers = ZoneAsset.GetMapLayers();
    State.OriginalSheets = ZoneAsset.GetMapSheets();
    State.OriginalDefaultSheetId = ZoneAsset.GetDefaultSheetId();
    State.OriginalMappings = ZoneAsset.GetBakedSheetMappings();
    State.OriginalRegions = ZoneAsset.GetBakedMapRegions();
    State.OriginalPoints = ZoneAsset.GetBakedPoints();
    State.OriginalMapBakeRevision = ZoneAsset.GetMapBakeRevision();
}

void ConfigurePresentationModelAsset(FRPGMapPresentationModelTestState& State)
{
    FGameZoneMapLayer Layer;
    Layer.LayerId = FGameZoneMapLayerId(TEXT("Model_Layer"));

    State.FirstSheetId = FGameZoneMapSheetId(TEXT("Model_First"));
    State.SecondSheetId = FGameZoneMapSheetId(TEXT("Model_Second"));
    FGameZoneMapSheet FirstSheet;
    FirstSheet.SheetId = State.FirstSheetId;
    FirstSheet.LayerId = Layer.LayerId;
    FirstSheet.MapTexture = State.FirstTexture.Get();
    FGameZoneMapSheet SecondSheet;
    SecondSheet.SheetId = State.SecondSheetId;
    SecondSheet.LayerId = Layer.LayerId;
    SecondSheet.MapTexture = State.SecondTexture.Get();

    FGameZoneMapSheetMapping FirstMapping;
    FirstMapping.SheetId = State.FirstSheetId;
    FirstMapping.WorldSize = FVector2D(1000.0, 1000.0);
    FGameZoneMapSheetMapping SecondMapping = FirstMapping;
    SecondMapping.SheetId = State.SecondSheetId;
    SecondMapping.WorldOrigin = FVector(2000.0, 0.0, 0.0);

    State.VisiblePointId = FGuid(10, 20, 30, 40);
    State.VisiblePoint.PointId = State.VisiblePointId;
    State.VisiblePoint.ZoneId = State.ZoneAsset->GetId();
    State.VisiblePoint.MarkerTypeId = State.MarkerType->GetId();
    State.VisiblePoint.MarkerState = EGameZonePointState::Activated;
    State.VisiblePoint.DisplayMode = static_cast<int32>(EMapMarkerDisplayMode::WorldMap);
    State.VisiblePoint.SavePolicy = EGameZonePointSavePolicy::Custom;

    FGameZonePointData MiniMapOnlyPoint = State.VisiblePoint;
    MiniMapOnlyPoint.PointId = FGuid(11, 20, 30, 40);
    MiniMapOnlyPoint.DisplayMode = static_cast<int32>(EMapMarkerDisplayMode::MiniMap);
    FGameZonePointData HiddenPoint = State.VisiblePoint;
    HiddenPoint.PointId = FGuid(12, 20, 30, 40);
    HiddenPoint.MarkerState = EGameZonePointState::Hide;

    GetPresentationModelPropertyValue<TArray<FGameZoneMapLayer>>(*State.ZoneAsset, TEXT("MapLayers")) = {Layer};
    GetPresentationModelPropertyValue<TArray<FGameZoneMapSheet>>(*State.ZoneAsset, TEXT("MapSheets")) = {FirstSheet, SecondSheet};
    GetPresentationModelPropertyValue<FGameZoneMapSheetId>(*State.ZoneAsset, TEXT("DefaultSheetId")) = State.FirstSheetId;
    GetPresentationModelPropertyValue<TArray<FGameZoneMapSheetMapping>>(*State.ZoneAsset, TEXT("BakedSheetMappings")) = {
        FirstMapping,
        SecondMapping,
    };
    GetPresentationModelPropertyValue<TArray<FGameZoneMapRegion>>(*State.ZoneAsset, TEXT("BakedMapRegions")).Reset();
    GetPresentationModelPropertyValue<TMap<FGuid, FGameZonePointData>>(*State.ZoneAsset, TEXT("BakedPoints")) = {
        {State.VisiblePointId, State.VisiblePoint},
        {MiniMapOnlyPoint.PointId, MiniMapOnlyPoint},
        {HiddenPoint.PointId, HiddenPoint},
    };
    GetPresentationModelPropertyValue<FGuid>(*State.ZoneAsset, TEXT("MapBakeRevision")) = FGuid::NewGuid();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRPGMapPresentationModelInvalidConfigTest,
    "IronicRPG.RPGGameplay.MapPresentationModel.InvalidConfigFailsWithoutStarting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGMapPresentationModelInvalidConfigTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>(GEngine));
    UGameZoneSubsystem* Subsystem = NewObject<UGameZoneSubsystem>(GameInstance.Get());
    URPGMapPresentationModel* Model = NewObject<URPGMapPresentationModel>(GameInstance.Get());
    int32 FailureCount = 0;
    Model->OnFailed.AddLambda([&FailureCount]()
        {
            ++FailureCount;
        });

    FRPGMapPresentationConfig Config;
    Config.Mode = EMapMarkerDisplayMode::WorldMap;
    TestFalse(TEXT("A presentation without any valid Zone fails"), Model->Start(*Subsystem, Config));
    TestEqual(TEXT("Invalid configuration emits exactly one failure"), FailureCount, 1);
    TestFalse(TEXT("Invalid configuration leaves the model inactive"), Model->IsActive());
    TestFalse(TEXT("Invalid configuration leaves the model unready"), Model->IsReady());
    return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
    FWaitForMapPresentationModelLifecycleCommand,
    TSharedPtr<FRPGMapPresentationModelTestState>,
    State,
    FAutomationTestBase*,
    Test);

bool FWaitForMapPresentationModelLifecycleCommand::Update()
{
    UWorld* World = State->GameInstance->GetWorld();
    if (World)
    {
        World->Tick(LEVELTICK_All, 1.0f / 60.0f);
    }

    if (FPlatformTime::Seconds() - State->PhaseStartTime > 5.0)
    {
        Test->AddError(FString::Printf(TEXT("Timed out in MapPresentationModel test phase %d."), State->Phase));
        State->RestoreAndShutdown();
        return true;
    }

    if (State->Phase == 0)
    {
        if (State->ReadyCount == 0)
        {
            return false;
        }

        Test->TestTrue(TEXT("The model becomes ready after the subsystem snapshot"), State->Model->IsReady());
        const TArray<FRPGMapPresentationMarker> Markers = State->Model->GetMarkers();
        Test->TestEqual(TEXT("The model filters MiniMap-only and hidden markers"), Markers.Num(), 1);
        if (Markers.Num() != 1 || !Markers[0].MarkerType)
        {
            return false;
        }

        Test->TestEqual(TEXT("The remaining marker is the World Map marker"), Markers[0].Point.Data.PointId, State->VisiblePointId);
        Test->TestEqual(TEXT("The model resolves the marker type before exposing cached data"), Markers[0].MarkerType, State->MarkerType.Get());
        Test->TestTrue(
            TEXT("The first Sheet Texture request is accepted"),
            State->Model->RequestSheetTexture(State->ZoneAsset->GetId(), State->FirstSheetId));
        Test->TestTrue(
            TEXT("A second request supersedes the first"),
            State->Model->RequestSheetTexture(State->ZoneAsset->GetId(), State->SecondSheetId));
        State->Phase = 1;
        State->PhaseStartTime = FPlatformTime::Seconds();
        return false;
    }

    if (State->Phase == 1)
    {
        if (State->TextureCount == 0)
        {
            return false;
        }

        Test->TestEqual(TEXT("Only the latest Texture completion is published"), State->TextureCount, 1);
        Test->TestEqual(TEXT("The published Texture belongs to the latest Sheet"), State->LastTextureResult.SheetId, State->SecondSheetId);
        Test->TestEqual(TEXT("The latest Texture is retained in the result"), State->LastTextureResult.Texture.Get(), State->SecondTexture.Get());

        State->MarkerChangeCount = 0;
        FGameZonePointData Override = State->VisiblePoint;
        Override.MarkerState = EGameZonePointState::Deactivated;
        Override.WorldTransform.SetLocation(FVector(100.0, 200.0, 300.0));
        Test->TestTrue(TEXT("The test override is accepted through the subsystem"), State->Subsystem->SetMapMarkerSaveOverride(Override));

        FRPGMapPresentationMarker ResolvedMarker;
        Test->TestEqual(TEXT("A relevant registry change is published once"), State->MarkerChangeCount, 1);
        Test->TestTrue(TEXT("The model exposes the updated marker"), State->Model->TryGetMarker(State->VisiblePointId, ResolvedMarker));
        Test->TestEqual(TEXT("The updated state is retained"), ResolvedMarker.Point.Data.MarkerState, EGameZonePointState::Deactivated);
        Test->TestEqual(
            TEXT("The updated transform is retained"),
            ResolvedMarker.Point.Data.WorldTransform.GetLocation(),
            FVector(100.0, 200.0, 300.0));

        State->Model->Stop();
        const int32 ChangeCountAfterStop = State->MarkerChangeCount;
        Override.MarkerState = EGameZonePointState::Activated;
        Override.WorldTransform.SetLocation(FVector(400.0, 500.0, 600.0));
        Test->TestTrue(TEXT("The subsystem can still change after the model stops"), State->Subsystem->SetMapMarkerSaveOverride(Override));
        Test->TestEqual(TEXT("A stopped model no longer receives marker changes"), State->MarkerChangeCount, ChangeCountAfterStop);
        Test->TestFalse(TEXT("Stop releases the presentation session"), State->Model->IsActive());
        Test->TestFalse(TEXT("Stop clears ready state"), State->Model->IsReady());

        State->RestoreAndShutdown();
        return true;
    }

    Test->AddError(TEXT("The MapPresentationModel test entered an invalid phase."));
    State->RestoreAndShutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRPGMapPresentationModelLifecycleTest,
    "IronicRPG.RPGGameplay.MapPresentationModel.OwnsFilteredMarkersTextureIdentityAndStopLifetime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGMapPresentationModelLifecycleTest::RunTest(const FString& Parameters)
{
    UGameZoneAsset* ZoneAsset = LoadObject<UGameZoneAsset>(nullptr, TEXT("/Game/DataAssets/Zone/DA_NewWorld.DA_NewWorld"));
    UMapMarkerTypeAsset* MarkerType = LoadObject<UMapMarkerTypeAsset>(nullptr, TEXT("/Game/DataAssets/MapMarker/DA_WayPoint.DA_WayPoint"));
    UTexture2D* FirstTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Characters/Portraits/T_AtomPortrait.T_AtomPortrait"));
    UTexture2D* SecondTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Characters/Portraits/T_GalaxyPortrait.T_GalaxyPortrait"));
    TestNotNull(TEXT("The test host provides DA_NewWorld"), ZoneAsset);
    TestNotNull(TEXT("The test host provides DA_WayPoint"), MarkerType);
    TestNotNull(TEXT("The test host provides the first Texture"), FirstTexture);
    TestNotNull(TEXT("The test host provides the second Texture"), SecondTexture);
    if (!ZoneAsset || !MarkerType || !FirstTexture || !SecondTexture)
    {
        return false;
    }

    TSharedPtr<FRPGMapPresentationModelTestState> State = MakeShared<FRPGMapPresentationModelTestState>();
    SavePresentationModelAssetState(*State, *ZoneAsset);
    State->MarkerType.Reset(MarkerType);
    State->FirstTexture.Reset(FirstTexture);
    State->SecondTexture.Reset(SecondTexture);
    ConfigurePresentationModelAsset(*State);

    State->GameInstance.Reset(NewObject<UGameInstance>(GEngine));
    State->GameInstance->InitializeStandalone(TEXT("RPGMapPresentationModelTestWorld"));
    State->Subsystem.Reset(NewObject<UGameZoneSubsystem>(State->GameInstance.Get()));
    State->Model.Reset(NewObject<URPGMapPresentationModel>(State->GameInstance.Get()));

    TWeakPtr<FRPGMapPresentationModelTestState> WeakState = State;
    State->Model->OnReady.AddLambda([WeakState](const FGameZonePresentationSnapshot& Snapshot)
        {
            if (const TSharedPtr<FRPGMapPresentationModelTestState> PinnedState = WeakState.Pin())
            {
                ++PinnedState->ReadyCount;
                PinnedState->ReadySnapshot = Snapshot;
            }
        });
    State->Model->OnTextureReady.AddLambda([WeakState](const FGameZoneMapTextureResult& Result)
        {
            if (const TSharedPtr<FRPGMapPresentationModelTestState> PinnedState = WeakState.Pin())
            {
                ++PinnedState->TextureCount;
                PinnedState->LastTextureResult = Result;
            }
        });
    State->Model->OnMarkerChanged.AddLambda([WeakState](const FRPGMapPresentationMarkerChange& Change)
        {
            if (const TSharedPtr<FRPGMapPresentationModelTestState> PinnedState = WeakState.Pin())
            {
                ++PinnedState->MarkerChangeCount;
                PinnedState->LastMarkerChange = Change;
            }
        });

    FRPGMapPresentationConfig Config;
    Config.ZoneIds = {ZoneAsset->GetId()};
    Config.Mode = EMapMarkerDisplayMode::WorldMap;
    Config.TrackedMarkerRefreshInterval = 0.0f;
    TestTrue(TEXT("A valid model presentation starts"), State->Model->Start(*State->Subsystem, Config));
    if (!State->Model->IsActive())
    {
        State->RestoreAndShutdown();
        return false;
    }

    ADD_LATENT_AUTOMATION_COMMAND(FWaitForMapPresentationModelLifecycleCommand(State, this));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

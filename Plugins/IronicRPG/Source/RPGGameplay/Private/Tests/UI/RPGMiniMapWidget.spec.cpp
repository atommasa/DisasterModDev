// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UI/RPGMiniMapWidgetTestTypes.h"
#include "Tests/UI/RPGWorldMapWidgetTestTypes.h"

#include "Camera/PlayerCameraManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Levels/GameZoneAsset.h"
#include "Levels/GameZonePointComponent.h"
#include "Levels/MapMarkerTypeAsset.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"
#include "Settings/GameZoneSystemSettings.h"
#include "UI/RPGMapPresentationModel.h"
#include "UI/RPGMiniMapMarkerRenderer.h"
#include "UI/MapTrackingSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#include <limits>

namespace
{
template <typename T>
T& MiniMapProperty(UObject& Object, const FName Name)
{
    FProperty* Property = FindFProperty<FProperty>(Object.GetClass(), Name);
    check(Property);
    return *Property->ContainerPtrToValuePtr<T>(&Object);
}

struct FMiniMapAssetState
{
    TStrongObjectPtr<UGameZoneAsset> Asset;
    TArray<FGameZoneMapLayer> Layers;
    TArray<FGameZoneMapSheet> Sheets;
    TArray<FGameZoneMapSheetMapping> Mappings;
    TArray<FGameZoneMapRegion> Regions;
    TMap<FGuid, FGameZonePointData> Points;
    FGameZoneMapSheetId DefaultSheet;
    FGuid Revision;

    void Configure(UGameZoneAsset* InAsset)
    {
        Asset.Reset(InAsset);
        Layers = Asset->GetMapLayers();
        Sheets = Asset->GetMapSheets();
        Mappings = Asset->GetBakedSheetMappings();
        Regions = Asset->GetBakedMapRegions();
        Points = Asset->GetBakedPoints();
        DefaultSheet = Asset->GetDefaultSheetId();
        Revision = Asset->GetMapBakeRevision();

        FGameZoneMapLayer Layer;
        Layer.LayerId = FGameZoneMapLayerId(TEXT("MiniMap_TestLayer"));
        FGameZoneMapSheet Sheet;
        Sheet.SheetId = FGameZoneMapSheetId(TEXT("MiniMap_TestSheet"));
        Sheet.LayerId = Layer.LayerId;
        FGameZoneMapSheetMapping Mapping;
        Mapping.SheetId = Sheet.SheetId;
        Mapping.WorldSize = FVector2D(20000.0, 10000.0);
        FGameZoneMapLayer UpperLayer;
        UpperLayer.LayerId = FGameZoneMapLayerId(TEXT("MiniMap_UpperLayer"));
        UpperLayer.ElevationOrder = 1;
        FGameZoneMapSheet UpperSheet;
        UpperSheet.SheetId = FGameZoneMapSheetId(TEXT("MiniMap_UpperSheet"));
        UpperSheet.LayerId = UpperLayer.LayerId;
        FGameZoneMapSheetMapping UpperMapping = Mapping;
        UpperMapping.SheetId = UpperSheet.SheetId;
        FGameZoneMapRegion Region;
        Region.RegionId = FGameZoneMapRegionId(TEXT("MiniMap_UpperRegion"));
        Region.SheetId = UpperSheet.SheetId;
        Region.Bounds = FBox(FVector(500.0, 1500.0, 1000.0), FVector(1500.0, 2500.0, 2000.0));
        MiniMapProperty<TArray<FGameZoneMapLayer>>(*Asset, TEXT("MapLayers")) = {Layer, UpperLayer};
        MiniMapProperty<TArray<FGameZoneMapSheet>>(*Asset, TEXT("MapSheets")) = {Sheet, UpperSheet};
        MiniMapProperty<FGameZoneMapSheetId>(*Asset, TEXT("DefaultSheetId")) = Sheet.SheetId;
        MiniMapProperty<TArray<FGameZoneMapSheetMapping>>(*Asset, TEXT("BakedSheetMappings")) = {Mapping, UpperMapping};
        MiniMapProperty<TArray<FGameZoneMapRegion>>(*Asset, TEXT("BakedMapRegions")) = {Region};
        MiniMapProperty<TMap<FGuid, FGameZonePointData>>(*Asset, TEXT("BakedPoints")).Reset();
        MiniMapProperty<FGuid>(*Asset, TEXT("MapBakeRevision")) = FGuid::NewGuid();
    }

    void Restore()
    {
        if (!Asset.IsValid())
        {
            return;
        }
        MiniMapProperty<TArray<FGameZoneMapLayer>>(*Asset, TEXT("MapLayers")) = Layers;
        MiniMapProperty<TArray<FGameZoneMapSheet>>(*Asset, TEXT("MapSheets")) = Sheets;
        MiniMapProperty<FGameZoneMapSheetId>(*Asset, TEXT("DefaultSheetId")) = DefaultSheet;
        MiniMapProperty<TArray<FGameZoneMapSheetMapping>>(*Asset, TEXT("BakedSheetMappings")) = Mappings;
        MiniMapProperty<TArray<FGameZoneMapRegion>>(*Asset, TEXT("BakedMapRegions")) = Regions;
        MiniMapProperty<TMap<FGuid, FGameZonePointData>>(*Asset, TEXT("BakedPoints")) = Points;
        MiniMapProperty<FGuid>(*Asset, TEXT("MapBakeRevision")) = Revision;
    }
};

struct FMiniMapLifecycleState
{
    TStrongObjectPtr<UGameInstance> GameInstance;
    TStrongObjectPtr<ULocalPlayer> LocalPlayer;
    TStrongObjectPtr<URPGMiniMapWidgetTest> Widget;
    TStrongObjectPtr<URPGMiniMapWidgetTest> LateWidget;
    FMiniMapAssetState FirstZone;
    FMiniMapAssetState SecondZone;
    int32 Phase = 0;
    int32 StoppedFrames = 0;
    double StartTime = FPlatformTime::Seconds();
    bool bWorldDestroyed = false;
    TStrongObjectPtr<UImage> Image;
    TStrongObjectPtr<UMaterial> Material;
    TStrongObjectPtr<UTexture2D> FirstTexture;
    TStrongObjectPtr<UTexture2D> SecondTexture;
    TWeakObjectPtr<APawn> Pawn;
    TSoftObjectPtr<UTexture2D> OriginalDefaultTexture;
    bool bRestoreDefaultTexture = false;

    UGameZoneSubsystem* Subsystem() const { return GameInstance->GetSubsystem<UGameZoneSubsystem>(); }

    void SetZone(FRPGId Id)
    {
        MiniMapProperty<FGameZoneContext>(*Subsystem(), TEXT("CurrentContext")).ZoneId = Id;
    }

    void Travel(FRPGId Id)
    {
        SetZone(Id);
        Subsystem()->OnGameZoneTravelCompleted.Broadcast(Subsystem()->GetCurrentContext());
    }

    void Shutdown()
    {
        if (Widget.IsValid()) { Widget->DestructForTest(); }
        if (LateWidget.IsValid()) { LateWidget->DestructForTest(); }
        UWorld* World = GameInstance.IsValid() ? GameInstance->GetWorld() : nullptr;
        if (GameInstance.IsValid()) { GameInstance->Shutdown(); }
        if (World)
        {
            if (!bWorldDestroyed) { World->DestroyWorld(false); }
            GEngine->DestroyWorldContext(World);
        }
        FirstZone.Restore();
        SecondZone.Restore();
        if (bRestoreDefaultTexture) { GetMutableDefault<UGameZoneSystemSettings>()->DefaultMapTexture = OriginalDefaultTexture; }
        Widget.Reset();
        LateWidget.Reset();
        LocalPlayer.Reset();
        GameInstance.Reset();
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiniMapRangeTest, "IronicRPG.RPGGameplay.MiniMap.RangeValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMiniMapRangeTest::RunTest(const FString& Parameters)
{
    URPGMiniMapWidgetTest* Widget = NewObject<URPGMiniMapWidgetTest>();
    Widget->ConfigureRangeForTest(10.0f, 500.0f);
    Widget->SetViewRangeMeters(50.0f);
    TestEqual(TEXT("Range can be configured before Construct"), Widget->GetViewRangeMeters(), 50.0f);
    for (const float Invalid : {0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        Widget->SetViewRangeMeters(Invalid);
        TestEqual(TEXT("Invalid input preserves the last valid range"), Widget->GetViewRangeMeters(), 50.0f);
    }
    Widget->SetViewRangeMeters(1.0f);
    TestEqual(TEXT("Positive input is clamped to minimum"), Widget->GetViewRangeMeters(), 10.0f);
    Widget->SetViewRangeMeters(1000.0f);
    TestEqual(TEXT("Range is clamped to maximum"), Widget->GetViewRangeMeters(), 500.0f);
    Widget->ConfigureRangeForTest(100.0f, 10.0f);
    Widget->SetViewRangeMeters(50.0f);
    TestEqual(TEXT("Inverted configuration collapses to minimum"), Widget->GetViewRangeMeters(), 100.0f);
    TestFalse(TEXT("Range input never starts a session before Construct"), Widget->IsPresentationActive());
    return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FWaitForMiniMapLifecycle, TSharedPtr<FMiniMapLifecycleState>, State, FAutomationTestBase*, Test);

bool FWaitForMiniMapLifecycle::Update()
{
    if (FPlatformTime::Seconds() - State->StartTime > 15.0)
    {
        Test->AddError(FString::Printf(TEXT("MiniMap lifecycle timed out in phase %d"), State->Phase));
        State->Shutdown();
        return true;
    }
    UWorld* World = State->GameInstance->GetWorld();
    World->Tick(LEVELTICK_All, 1.0f / 60.0f);
    URPGMiniMapWidgetTest* Widget = State->Widget.Get();

    if (State->Phase == 0)
    {
        if (Widget->ReadyCount < 1) { return false; }
        Test->TestTrue(TEXT("Zone session becomes ready without a Pawn"), Widget->GetViewForTest().bPresentationReady);
        Test->TestFalse(TEXT("Missing Pawn is a separate waiting state"), Widget->GetViewForTest().bHasFollowTarget);
        Test->TestFalse(TEXT("Ready without geometry does not fabricate a viewport"), Widget->GetViewForTest().bHasValidViewport);
        Test->TestEqual(TEXT("Waiting did not fail initialization"), Widget->FailedCount, 0);

        APlayerController* Controller = World->SpawnActor<APlayerController>();
        // Standalone test worlds have not initialized actors for play, so register the controller explicitly.
        World->AddController(Controller);
        State->LocalPlayer.Reset(NewObject<ULocalPlayer>(GEngine));
        Controller->Player = State->LocalPlayer.Get();
        State->LocalPlayer->PlayerController = Controller;
        Widget->SetPlayerContext(FLocalPlayerContext(State->LocalPlayer.Get(), World));
        Test->TestEqual(TEXT("The fixture provides the Widget's actual owning controller"), Widget->GetOwningPlayer(), Controller);
        Widget->RefreshMiniMap();
        APawn* Pawn = World->SpawnActor<APawn>();
        USceneComponent* Root = NewObject<USceneComponent>(Pawn);
        Pawn->SetRootComponent(Root);
        Root->RegisterComponent();
        Pawn->SetActorLocation(FVector(1000.0, 2000.0, 300.0));
        Controller->Possess(Pawn);
        Test->TestTrue(TEXT("The fixture controller possesses the Pawn"), Controller->GetPawn() == Pawn);
        Test->TestTrue(TEXT("Late possession updates follow state without a BP refresh"), Widget->GetViewForTest().bHasFollowTarget);
        Test->TestTrue(TEXT("Late Pawn resolves its Sheet"), Widget->GetViewForTest().bHasSheet);
        Widget->TickForTest();
        Test->TestEqual(TEXT("Follow sample uses the Pawn location"), Widget->GetViewForTest().CenterWorldLocation, Pawn->GetActorLocation());
        Test->TestTrue(TEXT("256 units and 100m radius gives 1.28 units/m"),
            FMath::IsNearlyEqual(Widget->GetViewForTest().LocalUnitsPerWorldMeter, 1.28f));
        const int32 SheetsBefore = Widget->SheetCount;
        const int32 ViewsBefore = Widget->ViewCount;
        Widget->TickForTest();
        Widget->RefreshMiniMap();
        Test->TestEqual(TEXT("Stationary refresh does not emit redundant view events"), Widget->ViewCount, ViewsBefore);
        Widget->SetViewRangeMeters(50.0f);
        Test->TestTrue(TEXT("Changing radius doubles the shared scale"),
            FMath::IsNearlyEqual(Widget->GetViewForTest().LocalUnitsPerWorldMeter, 2.56f));
        Test->TestEqual(TEXT("Range does not reopen presentation"), Widget->ReadyCount, 1);
        Test->TestEqual(TEXT("Range does not re-announce the same Sheet"), Widget->SheetCount, SheetsBefore);
        Pawn->SetActorLocation(FVector(1000.0, 2000.0, 1500.0));
        Widget->TickForTest();
        Test->TestEqual(TEXT("Z-only movement re-resolves the Region Sheet"),
            Widget->GetSheetForTest().SheetId, FGameZoneMapSheetId(TEXT("MiniMap_UpperSheet")));
        Test->TestEqual(TEXT("The resolved Layer follows the Region"),
            Widget->GetSheetForTest().LayerId, FGameZoneMapLayerId(TEXT("MiniMap_UpperLayer")));
        Pawn->SetActorLocation(FVector(1000.0, 2000.0, 300.0));
        Widget->TickForTest();
        Test->TestEqual(TEXT("Leaving the Region restores the default Sheet"),
            Widget->GetSheetForTest().SheetId, FGameZoneMapSheetId(TEXT("MiniMap_TestSheet")));
        Widget->TickForTest(FVector2D(512.0, 256.0));
        Test->TestTrue(TEXT("Non-square geometry uses its short edge"),
            FMath::IsNearlyEqual(Widget->GetViewForTest().LocalUnitsPerWorldMeter, 2.56f));
        Widget->TickForTest(FVector2D::ZeroVector);
        Test->TestFalse(TEXT("Zero layout invalidates projection geometry"), Widget->GetViewForTest().bHasValidViewport);
        Widget->TickForTest();
        Controller->UnPossess();
        Test->TestFalse(TEXT("Unpossess clears stale center validity"), Widget->GetViewForTest().bHasFollowTarget);
        Test->TestFalse(TEXT("Unpossess clears stale Sheet"), Widget->GetViewForTest().bHasSheet);
        Test->TestTrue(TEXT("Unpossess retains the Zone presentation"), Widget->IsPresentationActive());
        Controller->Possess(Pawn);
        Test->TestTrue(TEXT("Repossess recovers without reconstruction"), Widget->GetViewForTest().bHasSheet);
        Pawn->SetActorLocation(FVector(40000.0, 0.0, 500.0));
        Widget->TickForTest();
        Test->TestEqual(TEXT("Follow center is not clamped to World Map bounds"),
            Widget->GetViewForTest().CenterWorldLocation, Pawn->GetActorLocation());

        State->Travel(State->SecondZone.Asset->GetId());
        Test->TestFalse(TEXT("Travel clears the old ready state"), Widget->GetViewForTest().bPresentationReady);
        State->Phase = 1;
        return false;
    }

    if (State->Phase == 1)
    {
        if (Widget->ReadyCount < 2) { return false; }
        Test->TestEqual(TEXT("New Zone owns the new presentation"), Widget->GetZoneForTest(), State->SecondZone.Asset->GetId());
        Test->TestEqual(TEXT("Sheet belongs to the new Zone"), Widget->GetSheetForTest().ZoneId, State->SecondZone.Asset->GetId());
        State->Travel(State->SecondZone.Asset->GetId());
        Test->TestFalse(TEXT("Same-Zone reload also invalidates the old snapshot"), Widget->GetViewForTest().bPresentationReady);
        State->Phase = 2;
        return false;
    }

    if (State->Phase == 2)
    {
        if (Widget->ReadyCount < 3) { return false; }
        Widget->DestructForTest();
        Widget->DestructForTest();
        Test->TestFalse(TEXT("Stop is idempotent"), Widget->IsPresentationActive());
        Widget->ConstructForTest();
        Widget->DestructForTest();
        Widget->RefreshMiniMap();
        Test->TestFalse(TEXT("Refresh after Destruct cannot start a session"), Widget->IsPresentationActive());
        State->Phase = 3;
        return false;
    }

    if (State->Phase == 3)
    {
        if (++State->StoppedFrames < 5) { return false; }
        Test->TestEqual(TEXT("Stopped pending completion cannot notify the Widget"), Widget->ReadyCount, 3);
        Widget->ConstructForTest();
        State->Phase = 4;
        return false;
    }

    if (State->Phase == 4)
    {
        if (Widget->ReadyCount < 4) { return false; }
        Test->TestTrue(TEXT("Reconstruct binds and follows again"), Widget->GetViewForTest().bHasSheet);
        State->LateWidget.Reset(CreateWidget<URPGMiniMapWidgetTest>(State->GameInstance.Get()));
        State->LateWidget->SetPlayerContext(Widget->GetPlayerContext());
        State->LateWidget->ConstructForTest();
        State->Phase = 5;
        return false;
    }

    if (State->LateWidget->ReadyCount < 1) { return false; }
    Test->TestTrue(TEXT("Late HUD reads current Zone and Pawn after binding"), State->LateWidget->GetViewForTest().bHasSheet);
    Widget->DestructForTest();
    Test->TestTrue(TEXT("Stopping one Widget leaves another session active"), State->LateWidget->IsPresentationActive());
    Test->TestEqual(TEXT("Reconstruct did not duplicate ready delegates"), Widget->ReadyCount, 4);
    World->DestroyWorld(false);
    State->bWorldDestroyed = true;
    Test->TestFalse(TEXT("World cleanup stops a surviving Widget session"), State->LateWidget->IsPresentationActive());
    Test->TestFalse(TEXT("World cleanup invalidates the surviving view"), State->LateWidget->GetViewForTest().bHasFollowTarget);
    State->Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiniMapLifecycleTest, "IronicRPG.RPGGameplay.MiniMap.LifecycleAndFollowReadiness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMiniMapLifecycleTest::RunTest(const FString& Parameters)
{
    UGameZoneAsset* First = LoadObject<UGameZoneAsset>(nullptr, TEXT("/Game/DataAssets/Zone/DA_NewWorld.DA_NewWorld"));
    UGameZoneAsset* Second = LoadObject<UGameZoneAsset>(nullptr, TEXT("/Game/DataAssets/Zone/DA_MainMenu.DA_MainMenu"));
    if (!TestNotNull(TEXT("Host provides first Zone"), First) || !TestNotNull(TEXT("Host provides second Zone"), Second)) { return false; }
    TSharedPtr<FMiniMapLifecycleState> State = MakeShared<FMiniMapLifecycleState>();
    State->FirstZone.Configure(First);
    State->SecondZone.Configure(Second);
    State->GameInstance.Reset(NewObject<UGameInstance>(GEngine));
    State->GameInstance->InitializeStandalone(TEXT("MiniMapLifecycleTestWorld"));
    if (!TestNotNull(TEXT("Standalone GameInstance creates the real subsystem"), State->Subsystem()))
    {
        State->Shutdown();
        return false;
    }
    State->SetZone({});
    State->Widget.Reset(CreateWidget<URPGMiniMapWidgetTest>(State->GameInstance.Get()));
    State->Widget->ConstructForTest();
    TestFalse(TEXT("Missing Zone waits without starting"), State->Widget->IsPresentationActive());
    TestEqual(TEXT("Missing Zone is not an initialization failure"), State->Widget->FailedCount, 0);
    State->SetZone(First->GetId());
    State->Widget->RefreshMiniMap();
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForMiniMapLifecycle(State, this));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiniMapFailedStartTest, "IronicRPG.RPGGameplay.MiniMap.FailedStartRequiresExplicitRetry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMiniMapFailedStartTest::RunTest(const FString& Parameters)
{
    UGameZoneAsset* Zone = LoadObject<UGameZoneAsset>(nullptr, TEXT("/Game/DataAssets/Zone/DA_NewWorld.DA_NewWorld"));
    if (!TestNotNull(TEXT("Host provides a valid Zone identity"), Zone)) { return false; }
    TSharedPtr<FMiniMapLifecycleState> State = MakeShared<FMiniMapLifecycleState>();
    State->GameInstance.Reset(NewObject<UGameInstance>(GEngine));
    State->GameInstance->InitializeStandalone(TEXT("MiniMapFailedStartTestWorld"));
    State->SetZone(Zone->GetId());
    State->Widget.Reset(CreateWidget<URPGMiniMapWidgetTest>(State->GameInstance.Get()));
    State->Widget->ConfigurePollingForTest(-1.0f);
    State->Widget->ConstructForTest();
    TestEqual(TEXT("Invalid Model configuration publishes failure once"), State->Widget->FailedCount, 1);
    State->Widget->TickForTest();
    State->Widget->TickForTest();
    TestEqual(TEXT("Frames do not loop failed initialization"), State->Widget->FailedCount, 1);
    TestFalse(TEXT("Failed presentation remains inactive"), State->Widget->IsPresentationActive());
    State->Widget->ConfigurePollingForTest(0.0f);
    State->Widget->RefreshMiniMap();
    TestTrue(TEXT("Explicit refresh retries corrected configuration"), State->Widget->IsPresentationActive());
    TestFalse(TEXT("Accepted retry is not yet an async-ready snapshot"), State->Widget->GetViewForTest().bPresentationReady);
    State->Shutdown();
    return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FWaitForMiniMapBackground, TSharedPtr<FMiniMapLifecycleState>, State, FAutomationTestBase*, Test);

bool FWaitForMiniMapBackground::Update()
{
    if (FPlatformTime::Seconds() - State->StartTime > 15.0)
    {
        Test->AddError(FString::Printf(TEXT("MiniMap background timed out in phase %d"), State->Phase));
        State->Shutdown();
        return true;
    }
    State->GameInstance->GetWorld()->Tick(LEVELTICK_All, 1.0f / 60.0f);
    URPGMiniMapWidgetTest* Widget = State->Widget.Get();
    UMaterialInstanceDynamic* Material = Widget->GetMaterialForTest();
    APlayerController* Controller = State->LocalPlayer->PlayerController;
    APawn* Pawn = State->Pawn.Get();

    if (State->Phase == 0)
    {
        if (Widget->TextureCount < 1) { return false; }
        Widget->TickForTest();
        Test->TestTrue(TEXT("Dedicated Sheet Texture succeeds"), Widget->LastTextureResult.bSucceeded);
        Test->TestEqual(TEXT("Dedicated Texture is published"), Widget->GetTextureForTest(), State->FirstTexture.Get());
        Test->TestEqual(TEXT("Image uses this Widget's MID"), State->Image->GetBrush().GetResourceObject(), static_cast<UObject*>(Material));
        Test->TestTrue(TEXT("Complete background becomes visible without hit testing"),
            State->Image->GetVisibility() == ESlateVisibility::HitTestInvisible);
        Test->TestEqual(TEXT("Material receives visibility"), Material->K2_GetScalarParameterValue(TEXT("MapVisible")), 1.0f);
        Test->TestEqual(TEXT("Material receives the current Texture"),
            Material->K2_GetTextureParameterValue(TEXT("MapTexture")), static_cast<UTexture*>(State->FirstTexture.Get()));
        const FLinearColor AxisBefore = Material->K2_GetVectorParameterValue(TEXT("MapUVAxisX"));
        const FLinearColor CenterBefore = Material->K2_GetVectorParameterValue(TEXT("MapCenterUV"));
        Test->TestTrue(TEXT("MID receives the sampled center UV"),
            FVector2D(CenterBefore.R, CenterBefore.G).Equals(Widget->GetViewForTest().MapCenterUV, 1.e-6));
        Widget->TickForTest(FVector2D(256.0), 2.0f);
        Test->TestEqual(TEXT("DPI/layout scale does not change the local-rect UV basis"),
            Material->K2_GetVectorParameterValue(TEXT("MapUVAxisX")), AxisBefore);
        Widget->TickForTest(FVector2D::ZeroVector);
        Test->TestFalse(TEXT("Zero viewport invalidates projection"), Widget->GetViewForTest().bHasMapProjection);
        Test->TestEqual(TEXT("Zero viewport masks the background"), Material->K2_GetScalarParameterValue(TEXT("MapVisible")), 0.0f);
        Test->TestEqual(TEXT("Temporary zero geometry retains the loaded Texture"), Widget->GetTextureForTest(), State->FirstTexture.Get());
        Widget->TickForTest();
        Test->TestEqual(TEXT("Valid geometry restores the background"), Material->K2_GetScalarParameterValue(TEXT("MapVisible")), 1.0f);
        Widget->SetViewRangeMeters(50.0f);
        const FLinearColor AxisAfter = Material->K2_GetVectorParameterValue(TEXT("MapUVAxisX"));
        Test->TestTrue(TEXT("Halving radius halves the material UV span"), FMath::IsNearlyEqual(AxisAfter.R, AxisBefore.R * 0.5f));
        Pawn->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
        Controller->SetControlRotation(FRotator(0.0f, 180.0f, 0.0f));
        Widget->TickForTest();
        Test->TestEqual(TEXT("Pawn +Y points north"), Widget->GetViewForTest().PawnAngle, 0.0f);
        Test->TestEqual(TEXT("Missing Camera uses ControlRotation"), Widget->GetViewForTest().CameraAngle, 90.0f);
        APlayerCameraManager* Camera = State->GameInstance->GetWorld()->SpawnActor<APlayerCameraManager>();
        Controller->PlayerCameraManager = Camera;
        Camera->InitializeFor(Controller);
        FMinimalViewInfo CameraPOV;
        CameraPOV.Rotation = FRotator(0.0f, 270.0f, 0.0f);
        Camera->SetCameraCachePOV(CameraPOV);
        Widget->TickForTest();
        Test->TestEqual(TEXT("Camera POV takes precedence over ControlRotation"), Widget->GetViewForTest().CameraAngle, 180.0f);
        Test->TestTrue(TEXT("Pawn and Camera validity are separate"),
            Widget->GetViewForTest().bHasPawnDirection && Widget->GetViewForTest().bHasCameraDirection);
        Test->TestEqual(TEXT("Range, DPI, and rotation did not request another Texture"), Widget->TextureCount, 1);

        Pawn->SetActorLocation(FVector(1000.0, 2000.0, 1500.0));
        Widget->TickForTest();
        Test->TestTrue(TEXT("A new Sheet starts a pending request"), Widget->IsTexturePendingForTest());
        Test->TestNull(TEXT("A new Sheet clears the old current Texture immediately"), Widget->GetTextureForTest());
        Test->TestTrue(TEXT("No old background is shown under new mapping"), State->Image->GetVisibility() == ESlateVisibility::Hidden);
        Test->TestTrue(TEXT("MID no longer overrides MapTexture with the old Sheet"),
            Material->K2_GetTextureParameterValue(TEXT("MapTexture")) != State->FirstTexture.Get());
        Controller->UnPossess();
        Test->TestFalse(TEXT("Losing the Pawn clears pending presentation acceptance"), Widget->IsTexturePendingForTest());
        Test->TestFalse(TEXT("Losing the Pawn clears direction validity"), Widget->GetViewForTest().bHasCameraDirection);
        State->Phase = 1;
        return false;
    }
    if (State->Phase == 1)
    {
        if (++State->StoppedFrames < 5) { return false; }
        Test->TestEqual(TEXT("Sheet-unavailable rejects late completion"), Widget->TextureCount, 1);
        Test->TestNull(TEXT("Late completion cannot restore a stale background"), Widget->GetTextureForTest());
        Pawn->SetActorLocation(FVector(1000.0, 2000.0, 300.0));
        Controller->Possess(Pawn);
        State->Phase = 2;
        return false;
    }
    if (State->Phase == 2)
    {
        if (Widget->TextureCount < 2) { return false; }
        Test->TestEqual(TEXT("Repossess restores dedicated Texture"), Widget->GetTextureForTest(), State->FirstTexture.Get());
        Pawn->SetActorLocation(FVector(1000.0, 2000.0, 1500.0));
        Widget->TickForTest();
        State->Phase = 3;
        return false;
    }
    if (State->Phase == 3)
    {
        if (Widget->TextureCount < 3) { return false; }
        Test->TestTrue(TEXT("Missing dedicated Texture uses subsystem fallback"), Widget->LastTextureResult.bUsedDefaultTexture);
        Test->TestEqual(TEXT("Configured fallback reaches the Widget"), Widget->GetTextureForTest(), State->SecondTexture.Get());
        Pawn->SetActorLocation(FVector(1000.0, 2000.0, 300.0));
        Widget->TickForTest();
        Pawn->SetActorLocation(FVector(1000.0, 2000.0, 1500.0));
        Widget->TickForTest();
        State->Phase = 4;
        return false;
    }
    if (State->Phase == 4)
    {
        if (Widget->TextureCount < 4) { return false; }
        Test->TestEqual(TEXT("Rapid A-B-A publishes only the latest request"), Widget->TextureCount, 4);
        Test->TestEqual(TEXT("Rapid switching retains the correct latest Texture"), Widget->GetTextureForTest(), State->SecondTexture.Get());
        GetMutableDefault<UGameZoneSystemSettings>()->DefaultMapTexture.Reset();
        Pawn->SetActorLocation(FVector(1000.0, 2000.0, 300.0));
        Widget->TickForTest();
        Pawn->SetActorLocation(FVector(1000.0, 2000.0, 1500.0));
        Widget->TickForTest();
        State->Phase = 5;
        return false;
    }
    if (State->Phase == 5)
    {
        if (Widget->TextureCount < 5) { return false; }
        Test->TestFalse(TEXT("No dedicated or fallback Texture publishes failure"), Widget->LastTextureResult.bSucceeded);
        Test->TestNull(TEXT("Failed Texture leaves the current Texture empty"), Widget->GetTextureForTest());
        Test->TestTrue(TEXT("Texture failure does not invalidate world mapping"), Widget->GetViewForTest().bHasMapProjection);
        Test->TestEqual(TEXT("Failed Texture is masked"), Material->K2_GetScalarParameterValue(TEXT("MapVisible")), 0.0f);
        Widget->TickForTest();
        Widget->TickForTest();
        Test->TestFalse(TEXT("Failure does not retry Texture every frame"), Widget->IsTexturePendingForTest());
        Pawn->SetActorLocation(FVector(1000.0, 2000.0, 300.0));
        Widget->TickForTest();
        Widget->DestructForTest();
        Test->TestNull(TEXT("Destruct drops MID ownership"), Widget->GetMaterialForTest());
        Test->TestNull(TEXT("Destruct removes the Image's old material reference"), State->Image->GetBrush().GetResourceObject());
        State->StoppedFrames = 0;
        State->Phase = 6;
        return false;
    }
    if (++State->StoppedFrames < 5) { return false; }
    Test->TestEqual(TEXT("Pending Texture cannot publish after Destruct"), Widget->TextureCount, 5);
    State->Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiniMapBackgroundTest, "IronicRPG.RPGGameplay.MiniMap.BackgroundTextureLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMiniMapBackgroundTest::RunTest(const FString& Parameters)
{
    UGameZoneAsset* Zone = LoadObject<UGameZoneAsset>(nullptr, TEXT("/Game/DataAssets/Zone/DA_NewWorld.DA_NewWorld"));
    UTexture2D* First = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Characters/Portraits/T_AtomPortrait.T_AtomPortrait"));
    UTexture2D* Second = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Characters/Portraits/T_GalaxyPortrait.T_GalaxyPortrait"));
    if (!TestNotNull(TEXT("Host provides Zone"), Zone) || !TestNotNull(TEXT("First Texture"), First)
        || !TestNotNull(TEXT("Fallback Texture"), Second)) { return false; }
    TSharedPtr<FMiniMapLifecycleState> State = MakeShared<FMiniMapLifecycleState>();
    State->FirstZone.Configure(Zone);
    State->FirstTexture.Reset(First);
    State->SecondTexture.Reset(Second);
    MiniMapProperty<TArray<FGameZoneMapSheet>>(*Zone, TEXT("MapSheets"))[0].MapTexture = First;
    State->OriginalDefaultTexture = GetDefault<UGameZoneSystemSettings>()->DefaultMapTexture;
    State->bRestoreDefaultTexture = true;
    GetMutableDefault<UGameZoneSystemSettings>()->DefaultMapTexture = Second;
    State->GameInstance.Reset(NewObject<UGameInstance>(GEngine));
    State->GameInstance->InitializeStandalone(TEXT("MiniMapBackgroundTestWorld"));
    State->SetZone(Zone->GetId());
    UWorld* World = State->GameInstance->GetWorld();
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    World->AddController(Controller);
    State->LocalPlayer.Reset(NewObject<ULocalPlayer>(GEngine));
    Controller->Player = State->LocalPlayer.Get();
    State->LocalPlayer->PlayerController = Controller;
    Controller->PlayerCameraManager = nullptr;
    APawn* Pawn = World->SpawnActor<APawn>();
    State->Pawn = Pawn;
    USceneComponent* Root = NewObject<USceneComponent>(Pawn);
    Pawn->SetRootComponent(Root);
    Root->RegisterComponent();
    Pawn->SetActorLocation(FVector(1000.0, 2000.0, 300.0));
    Controller->Possess(Pawn);
    State->Widget.Reset(CreateWidget<URPGMiniMapWidgetTest>(State->GameInstance.Get()));
    State->Widget->SetPlayerContext(FLocalPlayerContext(State->LocalPlayer.Get(), World));
    State->Image.Reset(NewObject<UImage>(State->Widget.Get()));
    State->Material.Reset(NewObject<UMaterial>());
    State->Material->MaterialDomain = MD_UI;
    State->Widget->ConfigureBackgroundForTest(State->Image.Get(), State->Material.Get());
    State->Widget->ConstructForTest();
    State->Widget->TickForTest();
    if (!TestNotNull(TEXT("Configured UI material creates a MID"), State->Widget->GetMaterialForTest()))
    {
        State->Shutdown();
        return false;
    }
    State->LateWidget.Reset(CreateWidget<URPGMiniMapWidgetTest>(State->GameInstance.Get()));
    State->LateWidget->ConfigureBackgroundForTest(nullptr, State->Material.Get());
    State->LateWidget->ConstructForTest();
    TestTrue(TEXT("Widgets never share mutable material instances"),
        State->Widget->GetMaterialForTest() != State->LateWidget->GetMaterialForTest());
    State->LateWidget->DestructForTest();
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForMiniMapBackground(State, this));
    return true;
}

namespace
{
struct FMiniMapMarkersState
{
    TSharedPtr<FMiniMapLifecycleState> Lifecycle = MakeShared<FMiniMapLifecycleState>();
    TStrongObjectPtr<UCanvasPanel> Canvas;
    TStrongObjectPtr<UMapMarkerTypeAsset> Type;
    TStrongObjectPtr<URPGMiniMapMarkerRenderer> Renderer;
    TStrongObjectPtr<UGameZonePointComponent> Live;
    TSharedPtr<SWidget> SlateWidget;
    FGameZonePointData MainPoint, UpperPoint;
    int32 Phase = 0;
};
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FWaitForMiniMapMarkers, TSharedPtr<FMiniMapMarkersState>, State, FAutomationTestBase*, Test);

bool FWaitForMiniMapMarkers::Update()
{
    FMiniMapLifecycleState& Life = *State->Lifecycle;
    if (FPlatformTime::Seconds() - Life.StartTime > 20.0)
    {
        Test->AddError(FString::Printf(TEXT("MiniMap Marker test timed out in phase %d"), State->Phase));
        Life.Shutdown();
        return true;
    }
    UWorld* World = Life.GameInstance->GetWorld();
    World->Tick(LEVELTICK_All, 1.0f / 60.0f);
    URPGMiniMapWidgetTest* Widget = Life.Widget.Get();
    URPGMiniMapMarkerRenderer* Renderer = State->Renderer.Get();
    Widget->TickForTest();
    APawn* Pawn = Life.Pawn.Get();
    const FGuid MainId = State->MainPoint.PointId;
    const FGuid UpperId = State->UpperPoint.PointId;
    if (State->Phase == 0)
    {
        // Ready and MarkerType async readiness may arrive separately.
        if (Widget->ReadyCount == 0 || !Renderer->FindWidget(MainId) || !Renderer->FindWidget(UpperId)) { return false; }
        Test->TestEqual(TEXT("Only two nearby points create Widgets despite a thousand distant points"), Renderer->NumWidgets(), 2);
        Test->TestEqual(TEXT("Hide and WorldMap-only points never enter the MiniMap index"), Renderer->NumIndexed(), 1002);
        Test->TestEqual(TEXT("Initial creation is bounded by nearby visible points"), Renderer->GetCreatedCount(), uint64(2));
        URPGMapMarkerWidget* Main = Renderer->FindWidget(MainId);
        Test->TestTrue(TEXT("Canvas disables hit tests including children"), State->Canvas->GetVisibility() == ESlateVisibility::HitTestInvisible);
        Test->TestTrue(TEXT("Marker disables hit tests after ApplyMarkerView"), Main->GetVisibility() == ESlateVisibility::HitTestInvisible);
        Test->TestTrue(TEXT("Other Sheet/Layer point is Above, not filtered out"),
            Renderer->FindWidget(UpperId)->GetMarkerView().LayerRelation == EWorldMapMarkerLayerRelation::Above);
        UCanvasPanelSlot* MainSlot = CastChecked<UCanvasPanelSlot>(Main->Slot);
        Test->TestTrue(TEXT("+X west, 10m offset at 1.28 local units/m"), MainSlot->GetPosition().Equals(FVector2D(115.2, 128.0), .001));
        Test->TestTrue(TEXT("Footprint is fixed, not texture DesiredSize"), MainSlot->GetSize().Equals(FVector2D(24.0)));
        const uint64 Queries = Renderer->GetQueryCount(), Applies = Renderer->GetAppliedCount(), Positions = Renderer->GetPositionCount();
        Widget->TickForTest();
        Widget->TickForTest(FVector2D(256.0), 2.0f);
        Pawn->SetActorRotation(FRotator(0.0, 90.0, 0.0));
        Widget->TickForTest();
        Test->TestEqual(TEXT("Stationary/DPI/heading-only frames do not query nearby points"), Renderer->GetQueryCount(), Queries);
        Test->TestEqual(TEXT("Stationary frames do not reapply views"), Renderer->GetAppliedCount(), Applies);
        Test->TestEqual(TEXT("Stationary frames do not set positions"), Renderer->GetPositionCount(), Positions);
        Widget->SetViewRangeMeters(50.0f);
        Widget->TickForTest();
        Test->TestTrue(TEXT("Range changes marker offset at exactly the background scale"), MainSlot->GetPosition().Equals(FVector2D(102.4, 128.0), .001));
        Test->TestTrue(TEXT("Zoom never scales the icon"), MainSlot->GetSize().Equals(FVector2D(24.0)));
        Widget->SetViewRangeMeters(100.0f);
        Widget->TickForTest(FVector2D(512.0, 256.0));
        Test->TestTrue(TEXT("Non-square viewport shares short-side scale and its center"), MainSlot->GetPosition().Equals(FVector2D(243.2, 128.0), .001));
        Widget->TickForTest();
        Pawn->SetActorLocation(FVector(-7500.0, 2000.0, 300.0));
        Widget->TickForTest();
        Test->TestEqual(TEXT("Previously visible markers are retained in the buffer"), Renderer->NumWidgets(), 2);
        Test->TestTrue(TEXT("Entire icon is hidden before crossing the circular edge"), Main->GetVisibility() == ESlateVisibility::Hidden);
        Pawn->SetActorLocation(FVector(1000.0, 2000.0, 300.0));
        Widget->TickForTest();
        Test->TestEqual(TEXT("Reentering the circle reuses the same Widget"), Renderer->FindWidget(MainId), Main);
        Test->TestEqual(TEXT("Boundary crossing does not create new Widgets"), Renderer->GetCreatedCount(), uint64(2));
        Pawn->SetActorLocation(FVector(-40000.0, 2000.0, 300.0));
        Widget->TickForTest();
        Test->TestEqual(TEXT("Leaving expanded query releases all nearby Widgets"), Renderer->NumWidgets(), 0);
        Test->TestEqual(TEXT("Pool has a hard configured bound"), Renderer->NumPooled(), 1);
        Pawn->SetActorLocation(FVector(1000.0, 2000.0, 300.0));
        Widget->TickForTest();
        Test->TestEqual(TEXT("Returning uses one pooled and one new Widget"), Renderer->GetCreatedCount(), uint64(3));

        FGameZonePointData Override = State->MainPoint;
        Override.MarkerState = EGameZonePointState::Deactivated;
        Test->TestTrue(TEXT("Save override accepted by real subsystem"), Life.Subsystem()->SetMapMarkerSaveOverride(Override));
        Widget->TickForTest();
        Main = Renderer->FindWidget(MainId);
        Test->TestTrue(TEXT("Deactivated markers remain visible and publish new state"), Main
            && Main->GetMarkerView().Point.Data.MarkerState == EGameZonePointState::Deactivated);
        Override.MarkerState = EGameZonePointState::Hide;
        Life.Subsystem()->SetMapMarkerSaveOverride(Override);
        Widget->TickForTest();
        Test->TestNull(TEXT("Hide removes the Marker Widget"), Renderer->FindWidget(MainId));
        Life.Subsystem()->ClearMapMarkerSaveOverride(MainId);
        Widget->TickForTest();
        Test->TestNotNull(TEXT("Clearing override restores baked data through Model change"), Renderer->FindWidget(MainId));

        AActor* Actor = World->SpawnActor<AActor>();
        USceneComponent* Root = NewObject<USceneComponent>(Actor);
        Actor->SetRootComponent(Root);
        Root->RegisterComponent();
        Actor->SetActorLocation(FVector(2000.0, 2000.0, 300.0));
        State->Live.Reset(NewObject<UGameZonePointComponent>(Actor));
        FGameZonePointData LiveData = State->MainPoint;
        LiveData.PointId = State->Live->GetPointId();
        LiveData.SavePolicy = EGameZonePointSavePolicy::NotSaveable;
        MiniMapProperty<FGameZonePointData>(*State->Live, TEXT("PointData")) = LiveData;
        MiniMapProperty<EGameZonePointUpdateMode>(*State->Live, TEXT("UpdateMode")) = EGameZonePointUpdateMode::Tracked;
        Life.Subsystem()->RegisterPoint(*State->Live);
        Widget->TickForTest();
        Test->TestNotNull(TEXT("Dynamic Live-only point is materialized"), Renderer->FindWidget(LiveData.PointId));
        Actor->SetActorLocation(FVector(1000.0, 2000.0, 1500.0));
        State->Phase = 1;
        return false;
    }
    if (State->Phase == 1)
    {
        URPGMapMarkerWidget* LiveWidget = Renderer->FindWidget(State->Live->GetPointId());
        if (!LiveWidget || LiveWidget->GetMarkerView().LayerRelation != EWorldMapMarkerLayerRelation::Above) { return false; }
        Test->TestTrue(TEXT("Tracked polling moves a Live marker across Sheet/Layer without BP scanning"),
            CastChecked<UCanvasPanelSlot>(LiveWidget->Slot)->GetPosition().Equals(FVector2D(128.0), .001));
        Pawn->SetActorLocation(FVector(1000.0, 2000.0, 1500.0));
        Widget->TickForTest();
        Test->TestTrue(TEXT("Changing player Layer updates nearby relations"),
            Renderer->FindWidget(MainId)->GetMarkerView().LayerRelation == EWorldMapMarkerLayerRelation::Below);
        Test->TestTrue(TEXT("Same Layer changes Above to Current"), LiveWidget->GetMarkerView().LayerRelation == EWorldMapMarkerLayerRelation::Current);
        Test->TestFalse(TEXT("Ordinary renderer never selects or player-tracks"), LiveWidget->GetMarkerView().bSelected || LiveWidget->GetMarkerView().bPlayerTracked);
        State->Live->GetOwner()->SetActorLocation(FVector(30000.0, 2000.0, 1500.0));
        State->Phase = 2;
        return false;
    }
    if (State->Phase == 2)
    {
        if (Renderer->FindWidget(State->Live->GetPointId())) { return false; }
        Test->TestEqual(TEXT("Cross-cell Live motion leaves only the two baked Widgets"), Renderer->NumWidgets(), 2);
        Life.Subsystem()->UnregisterPoint(*State->Live);
        Widget->TickForTest();
        Test->TestEqual(TEXT("Live-only removal clears index membership"), Renderer->NumIndexed(), 1002);
        Widget->SetVisibility(ESlateVisibility::Hidden);
        const uint64 Before = Renderer->GetAppliedCount();
        FGameZonePointData Override = State->MainPoint;
        Override.MarkerState = EGameZonePointState::Deactivated;
        Life.Subsystem()->SetMapMarkerSaveOverride(Override);
        Widget->TickForTest();
        Test->TestEqual(TEXT("Hidden callbacks/Tick do not update visual state"), Renderer->GetAppliedCount(), Before);
        Widget->SetVisibility(ESlateVisibility::Visible);
        Widget->TickForTest();
        Test->TestTrue(TEXT("Next visible frame consumes dirty state"), Renderer->FindWidget(MainId)
            && Renderer->FindWidget(MainId)->GetMarkerView().Point.Data.MarkerState == EGameZonePointState::Deactivated);
        Widget->TickForTest(FVector2D::ZeroVector);
        Test->TestEqual(TEXT("Invalid geometry clears projected markers"), Renderer->NumWidgets(), 0);
        Widget->TickForTest();
        Test->TestEqual(TEXT("Valid geometry restores nearby markers"), Renderer->NumWidgets(), 2);
        Widget->GetOwningPlayer()->UnPossess();
        Widget->TickForTest();
        Test->TestEqual(TEXT("No Pawn cannot leave old projected markers"), Renderer->NumWidgets(), 0);
        Widget->GetOwningPlayer()->Possess(Pawn);
        Widget->TickForTest();
        Test->TestEqual(TEXT("Repossess restores markers without a new presentation"), Renderer->NumWidgets(), 2);
        Life.Travel(Life.FirstZone.Asset->GetId());
        Test->TestEqual(TEXT("Same-Zone session restart immediately clears widgets"), Renderer->NumWidgets(), 0);
        Test->TestEqual(TEXT("Session restart drops cached-world pool references"), Renderer->NumPooled(), 0);
        Test->TestEqual(TEXT("Session restart clears the previous spatial index"), Renderer->NumIndexed(), 0);
        State->Phase = 3;
        return false;
    }
    if (State->Phase == 3)
    {
        if (Widget->ReadyCount < 2 || Renderer->NumWidgets() != 2) { return false; }
        Test->TestEqual(TEXT("New session reconstructs the index once"), Renderer->NumIndexed(), 1002);
        Widget->DestructForTest();
        Test->TestEqual(TEXT("Destruct clears the index"), Renderer->NumIndexed(), 0);
        Test->TestEqual(TEXT("Destruct clears widgets"), Renderer->NumWidgets(), 0);
        Test->TestEqual(TEXT("Destruct clears pool"), Renderer->NumPooled(), 0);
        Life.Subsystem()->ClearMapMarkerSaveOverride(MainId);
        Test->TestEqual(TEXT("Post-Stop changes cannot repopulate old renderer"), Renderer->NumIndexed(), 0);
        Life.Shutdown();
        return true;
    }
    return false;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiniMapMarkersTest, "IronicRPG.RPGGameplay.MiniMap.NearbyMarkerLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMiniMapMarkersTest::RunTest(const FString& Parameters)
{
    UGameZoneAsset* Zone = LoadObject<UGameZoneAsset>(nullptr, TEXT("/Game/DataAssets/Zone/DA_NewWorld.DA_NewWorld"));
    UMapMarkerTypeAsset* Type = LoadObject<UMapMarkerTypeAsset>(nullptr, TEXT("/Game/DataAssets/MapMarker/DA_WayPoint.DA_WayPoint"));
    if (!TestNotNull(TEXT("Host Zone"), Zone) || !TestNotNull(TEXT("Host Marker Type"), Type)) { return false; }
    TSharedPtr<FMiniMapMarkersState> State = MakeShared<FMiniMapMarkersState>();
    FMiniMapLifecycleState& Life = *State->Lifecycle;
    State->Type.Reset(Type);
    Life.FirstZone.Configure(Zone);
    FGameZonePointData& Main = State->MainPoint;
    Main.PointId = FGuid(401, 1, 1, 1);
    Main.ZoneId = Zone->GetId();
    Main.MarkerTypeId = Type->GetId();
    Main.MarkerState = EGameZonePointState::Activated;
    Main.DisplayMode = static_cast<int32>(EMapMarkerDisplayMode::MiniMap);
    Main.SavePolicy = EGameZonePointSavePolicy::Custom;
    Main.WorldTransform.SetLocation(FVector(2000.0, 2000.0, 300.0));
    State->UpperPoint = Main;
    State->UpperPoint.PointId = FGuid(402, 1, 1, 1);
    State->UpperPoint.WorldTransform.SetLocation(FVector(1000.0, 2000.0, 1500.0));
    TMap<FGuid, FGameZonePointData>& Points = MiniMapProperty<TMap<FGuid, FGameZonePointData>>(*Zone, TEXT("BakedPoints"));
    Points.Add(Main.PointId, Main);
    Points.Add(State->UpperPoint.PointId, State->UpperPoint);
    FGameZonePointData Hidden = Main;
    Hidden.PointId = FGuid(403, 1, 1, 1);
    Hidden.MarkerState = EGameZonePointState::Hide;
    Points.Add(Hidden.PointId, Hidden);
    FGameZonePointData WorldOnly = Main;
    WorldOnly.PointId = FGuid(404, 1, 1, 1);
    WorldOnly.DisplayMode = static_cast<int32>(EMapMarkerDisplayMode::WorldMap);
    Points.Add(WorldOnly.PointId, WorldOnly);
    for (int32 I = 0; I < 1000; ++I)
    {
        FGameZonePointData Far = Main;
        Far.PointId = FGuid(1000 + I, 1, 1, 1);
        Far.WorldTransform.SetLocation(FVector(5000000.0 + I * 20000.0, 0.0, 0.0));
        Points.Add(Far.PointId, Far);
    }
    Life.GameInstance.Reset(NewObject<UGameInstance>(GEngine));
    Life.GameInstance->InitializeStandalone(TEXT("MiniMapMarkerTestWorld"));
    Life.SetZone(Zone->GetId());
    UWorld* World = Life.GameInstance->GetWorld();
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    World->AddController(Controller);
    Life.LocalPlayer.Reset(NewObject<ULocalPlayer>(GEngine));
    Controller->Player = Life.LocalPlayer.Get();
    Life.LocalPlayer->PlayerController = Controller;
    APawn* Pawn = World->SpawnActor<APawn>();
    Life.Pawn = Pawn;
    USceneComponent* Root = NewObject<USceneComponent>(Pawn);
    Pawn->SetRootComponent(Root);
    Root->RegisterComponent();
    Pawn->SetActorLocation(FVector(1000.0, 2000.0, 300.0));
    Controller->Possess(Pawn);
    Life.Widget.Reset(CreateWidget<URPGMiniMapWidgetTest>(Life.GameInstance.Get()));
    Life.Widget->SetPlayerContext(FLocalPlayerContext(Life.LocalPlayer.Get(), World));
    State->Canvas.Reset(Life.Widget->WidgetTree->ConstructWidget<UCanvasPanel>());
    Life.Widget->WidgetTree->RootWidget = State->Canvas.Get();
    Life.Widget->ConfigureMarkersForTest(State->Canvas.Get());
    // IsVisible checks the actual Slate widget. Keep a real visual tree alive, not only manual NativeTick calls.
    State->SlateWidget = Life.Widget->TakeWidget();
    Life.Widget->ConstructForTest();
    State->Renderer.Reset(MiniMapProperty<TObjectPtr<URPGMiniMapMarkerRenderer>>(*Life.Widget, TEXT("MarkerRenderer")));
    Life.Widget->TickForTest();
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForMiniMapMarkers(State, this));
    return true;
}

namespace
{
struct FMiniMapTrackingState
{
    TSharedPtr<FMiniMapLifecycleState> Life = MakeShared<FMiniMapLifecycleState>();
    TStrongObjectPtr<URPGWorldMapWidgetTest> WorldMap;
    TStrongObjectPtr<UGameZonePointComponent> Live;
    TSharedPtr<SWidget> MiniSlate, WorldSlate;
    TStrongObjectPtr<UMapMarkerTypeAsset> Type;
    UCanvasPanel* Normal = nullptr;
    UCanvasPanel* Edge = nullptr;
    UCanvasPanel* WorldEdge = nullptr;
    UMapTrackingSubsystem* Tracking = nullptr;
    FGameZonePointData Near, Far, WorldOnly;
    int32 Phase = 0;
    uint64 ResumeAfterFrame = 0;
    uint64 AppliedBeforeHidden = 0;
    int32 IndexedBeforeHidden = 0;

    URPGMiniMapMarkerRenderer* Renderer() const
    {
        return MiniMapProperty<TObjectPtr<URPGMiniMapMarkerRenderer>>(*Life->Widget, TEXT("MarkerRenderer"));
    }
    URPGMapMarkerWidget* WorldMarker(FGuid Id) const
    {
        return MiniMapProperty<TMap<FGuid, TObjectPtr<URPGMapMarkerWidget>>>(*WorldMap, TEXT("ActiveMarkerWidgets")).FindRef(Id);
    }
    void Shutdown()
    {
        if (WorldMap.IsValid()) { WorldMap->DestructForTest(); }
        Life->Widget->DestructForTest();
        if (Life->LocalPlayer.IsValid()) { Life->LocalPlayer->PlayerRemoved(); }
        Life->Shutdown();
    }
};
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FWaitForMiniMapTracking, TSharedPtr<FMiniMapTrackingState>, State, FAutomationTestBase*, Test);

bool FWaitForMiniMapTracking::Update()
{
    FMiniMapLifecycleState& Life = *State->Life;
    if (FPlatformTime::Seconds() - Life.StartTime > 25.0)
    {
        Test->AddError(FString::Printf(TEXT("MiniMap tracking timed out in phase %d"), State->Phase));
        State->Shutdown();
        return true;
    }
    UWorld* World = Life.GameInstance->GetWorld();
    World->Tick(LEVELTICK_All, 1.0f / 60.0f);
    URPGMiniMapWidgetTest* Widget = Life.Widget.Get();
    URPGMiniMapMarkerRenderer* Renderer = State->Renderer();
    if (State->Phase != 1) { Widget->TickForTest(); }
    if (State->Phase == 0)
    {
        if (!State->WorldMap->IsReadyForTest() || !Renderer->FindWidget(State->Far.PointId)
            || !State->WorldMarker(State->Far.PointId)) { return false; }
        URPGMapMarkerWidget* Far = Renderer->FindWidget(State->Far.PointId);
        Test->TestTrue(TEXT("Late MiniMap reads the already tracked distant target"), Far->GetMarkerView().bPlayerTracked);
        Test->TestTrue(TEXT("Tracked MiniMap target uses the edge marker class"),
            Far->IsA<URPGMiniMapEdgeMarkerWidgetTest>());
        Test->TestTrue(TEXT("Distant target outside nearby buckets clamps"), Far->GetMarkerView().bClampedToEdge);
        Test->TestTrue(TEXT("MiniMap routes tracking to its edge canvas"), Far->GetParent() == State->Edge);
        Test->TestTrue(TEXT("WorldMap independently routes the same target"),
            State->WorldMarker(State->Far.PointId)->GetParent() == State->WorldEdge);
        Test->TestEqual(TEXT("No untracked distant points are materialized"), Renderer->NumWidgets(), 2);
        Test->TestTrue(TEXT("Edge canvas disables all child hit testing"), State->Edge->GetVisibility() == ESlateVisibility::HitTestInvisible);
        Test->TestFalse(TEXT("MiniMap never selects a tracked target"), Far->GetMarkerView().bSelected);

        Test->TestTrue(TEXT("MiniMap can temporarily focus a visible marker"), Widget->FocusOnMarker(State->Near.PointId));
        Widget->TickForTest();
        Test->TestEqual(TEXT("MiniMap focus publishes the focused PointId"), Widget->GetViewForTest().FocusedMarkerId, State->Near.PointId);
        Test->TestTrue(TEXT("MiniMap focus moves the map center to the marker"),
            Widget->GetViewForTest().CenterWorldLocation.Equals(State->Near.WorldTransform.GetLocation()));
        const FVector FocusedCenter = Widget->GetViewForTest().CenterWorldLocation;
        Test->TestFalse(TEXT("Unknown marker focus is rejected"), Widget->FocusOnMarker(FGuid(900, 9, 9, 9)));
        Test->TestTrue(TEXT("Rejected focus preserves the current marker center"), Widget->GetViewForTest().CenterWorldLocation.Equals(FocusedCenter));
        Test->TestTrue(TEXT("MiniMap can explicitly return to its player follow target"), Widget->FocusOnPlayer());
        Widget->TickForTest();
        Test->TestFalse(TEXT("Player focus clears the focused PointId"), Widget->GetViewForTest().FocusedMarkerId.IsValid());
        Test->TestTrue(TEXT("Player focus restores the Pawn center"),
            Widget->GetViewForTest().CenterWorldLocation.Equals(Life.Pawn->GetActorLocation()));

        Test->TestTrue(TEXT("WorldMap can focus the same presentation marker"), State->WorldMap->FocusOnMarker(State->Near.PointId));
        Test->TestTrue(TEXT("WorldMap focus moves the view center to the marker"),
            State->WorldMap->GetViewCenterForTest().Equals(State->Near.WorldTransform.GetLocation()));
        const FVector WorldFocusedCenter = State->WorldMap->GetViewCenterForTest();
        Test->TestFalse(TEXT("WorldMap rejects an unknown marker focus"), State->WorldMap->FocusOnMarker(FGuid(901, 9, 9, 9)));
        Test->TestTrue(TEXT("Rejected WorldMap focus preserves its center"),
            State->WorldMap->GetViewCenterForTest().Equals(WorldFocusedCenter));
        Test->TestTrue(TEXT("WorldMap can return to the player after marker focus"), State->WorldMap->FocusOnPlayer());

        const double Radius = 128.0 - FVector2D(12.0).Size() - 4.0;
        FVector2D Position = CastChecked<UCanvasPanelSlot>(Far->Slot)->GetPosition();
        Test->TestTrue(TEXT("Circle placement includes full icon footprint and padding"),
            Position.Equals(FVector2D(128.0 - Radius, 128.0), .001));
        const uint64 Queries = Renderer->GetQueryCount(), Applies = Renderer->GetAppliedCount();
        Widget->TickForTest();
        Test->TestEqual(TEXT("Stationary tracked target does not repeat spatial queries"), Renderer->GetQueryCount(), Queries);
        Test->TestEqual(TEXT("Stationary tracked target does not repeat visual events"), Renderer->GetAppliedCount(), Applies);

        Test->TestTrue(TEXT("WorldMap can select another Layer independently"),
            State->WorldMap->SelectLayer(FGameZoneMapLayerId(TEXT("MiniMap_UpperLayer"))));
        Widget->TickForTest();
        Test->TestEqual(TEXT("WorldMap Layer selection cannot change MiniMap follow Sheet"),
            Widget->GetSheetForTest().SheetId, FGameZoneMapSheetId(TEXT("MiniMap_TestSheet")));
        Test->TestEqual(TEXT("WorldMap Layer selection cannot restart MiniMap session"), Widget->ReadyCount, 1);
        State->WorldMap->SelectLayer(FGameZoneMapLayerId(TEXT("MiniMap_TestLayer")));

        FGameZonePointData Moved = State->Far;
        Moved.WorldTransform.SetLocation(FVector(1000.0, 2000.0, 1500.0));
        Life.Subsystem()->SetMapMarkerSaveOverride(Moved);
        Widget->TickForTest();
        Test->TestEqual(TEXT("Entering circle preserves the exact Widget"), Renderer->FindWidget(Moved.PointId), Far);
        Test->TestTrue(TEXT("In-range tracked target remains in edge canvas"), Far->GetParent() == State->Edge);
        Test->TestFalse(TEXT("Entering circle clears clamp flag"), Far->GetMarkerView().bClampedToEdge);
        Test->TestTrue(TEXT("Other Layer remains visible with relation"), Far->GetMarkerView().LayerRelation == EWorldMapMarkerLayerRelation::Above);
        Test->TestTrue(TEXT("Coincident XY produces zero bearing"), Far->GetMarkerView().DirectionFromViewCenter.IsZero());
        Test->TestTrue(TEXT("WorldMap focus accepts a marker on another Layer"), State->WorldMap->FocusOnMarker(Moved.PointId));
        Test->TestEqual(TEXT("WorldMap focus displays the marker's Layer"),
            State->WorldMap->GetSheetForTest().LayerId, FGameZoneMapLayerId(TEXT("MiniMap_UpperLayer")));
        Test->TestTrue(TEXT("WorldMap returns to the player's Layer after cross-Layer focus"), State->WorldMap->FocusOnPlayer());
        Life.Subsystem()->ClearMapMarkerSaveOverride(Moved.PointId);
        Widget->TickForTest();
        Test->TestTrue(TEXT("Exiting circle restores clamp flag without reparenting"),
            Far->GetMarkerView().bClampedToEdge && Far->GetParent() == State->Edge);

        State->Tracking->TrackMarker(State->Near.PointId);
        Widget->TickForTest();
        Test->TestNull(TEXT("Replacement releases former distant target"), Renderer->FindWidget(State->Far.PointId));
        Test->TestTrue(TEXT("Replacement moves nearby target into edge canvas"),
            Renderer->FindWidget(State->Near.PointId)->GetParent() == State->Edge);
        Test->TestTrue(TEXT("Tracking swaps a nearby target to the edge marker class"),
            Renderer->FindWidget(State->Near.PointId)->IsA<URPGMiniMapEdgeMarkerWidgetTest>());
        State->Tracking->ClearTrackedMarker();
        Widget->TickForTest();
        Test->TestTrue(TEXT("Clear returns nearby target to ordinary canvas"),
            Renderer->FindWidget(State->Near.PointId)->GetParent() == State->Normal);
        Test->TestTrue(TEXT("Clear restores the default marker class"),
            Renderer->FindWidget(State->Near.PointId)->IsA<URPGMiniMapMarkerWidgetTest>());
        Test->TestFalse(TEXT("Clear resets tracked visual flag"), Renderer->FindWidget(State->Near.PointId)->GetMarkerView().bPlayerTracked);

        State->Tracking->TrackMarker(State->WorldOnly.PointId);
        Widget->TickForTest();
        Test->TestNull(TEXT("Tracking cannot bypass MiniMap display mode"), Renderer->FindWidget(State->WorldOnly.PointId));
        Test->TestNotNull(TEXT("WorldMap still sees its own mode-specific target"), State->WorldMarker(State->WorldOnly.PointId));
        State->Tracking->TrackMarker(State->Far.PointId);
        Moved = State->Far;
        Moved.MarkerState = EGameZonePointState::Hide;
        Life.Subsystem()->SetMapMarkerSaveOverride(Moved);
        Widget->TickForTest();
        Test->TestNull(TEXT("Hide removes tracked MiniMap visual"), Renderer->FindWidget(Moved.PointId));
        Test->TestNull(TEXT("Hide removes tracked WorldMap visual"), State->WorldMarker(Moved.PointId));
        Test->TestEqual(TEXT("Hide preserves intent"), State->Tracking->GetTrackedMarkerId(), Moved.PointId);
        Moved.MarkerState = EGameZonePointState::Deactivated;
        Life.Subsystem()->SetMapMarkerSaveOverride(Moved);
        Widget->TickForTest();
        Test->TestTrue(TEXT("Deactivated restores tracked visual without changing Action availability"),
            Renderer->FindWidget(Moved.PointId) && Renderer->FindWidget(Moved.PointId)->GetMarkerView().bPlayerTracked);

        State->WorldMap->DestructForTest();
        Test->TestTrue(TEXT("Closing WorldMap leaves MiniMap session ready"),
            Widget->IsPresentationActive() && Widget->GetViewForTest().bPresentationReady);
        Test->TestEqual(TEXT("Closing WorldMap keeps player intent"), State->Tracking->GetTrackedMarkerId(), Moved.PointId);
        Life.Subsystem()->ClearMapMarkerSaveOverride(Moved.PointId);

        AActor* Actor = World->SpawnActor<AActor>();
        USceneComponent* Root = NewObject<USceneComponent>(Actor);
        Actor->SetRootComponent(Root);
        Root->RegisterComponent();
        Actor->SetActorLocation(FVector(1000.0, 3000.0, 300.0));
        State->Live.Reset(NewObject<UGameZonePointComponent>(Actor));
        FGameZonePointData LiveData = State->Near;
        LiveData.PointId = State->Live->GetPointId();
        LiveData.SavePolicy = EGameZonePointSavePolicy::NotSaveable;
        MiniMapProperty<FGameZonePointData>(*State->Live, TEXT("PointData")) = LiveData;
        MiniMapProperty<EGameZonePointUpdateMode>(*State->Live, TEXT("UpdateMode")) = EGameZonePointUpdateMode::Tracked;
        Life.Subsystem()->RegisterPoint(*State->Live);
        Widget->TickForTest();
        State->Tracking->TrackMarker(LiveData.PointId);
        Widget->TickForTest();
        State->AppliedBeforeHidden = Renderer->GetAppliedCount();
        State->IndexedBeforeHidden = Renderer->NumIndexed();
        // Simulate ancestor suppression: the child receives no NativeTick for multiple real engine frames.
        // Polling is disabled in this fixture so only resume can refresh the moved Live snapshot.
        State->Live->GetOwner()->SetActorLocation(FVector(1000.0, 50000.0, 300.0));
        State->Tracking->TrackMarker(State->Near.PointId);
        State->Tracking->TrackMarker(LiveData.PointId);
        State->ResumeAfterFrame = GFrameCounter + 3;
        State->Phase = 1;
        return false;
    }
    if (State->Phase == 1)
    {
        if (GFrameCounter < State->ResumeAfterFrame) { return false; }
        Test->TestEqual(TEXT("Hidden tracking callbacks never update visuals"), Renderer->GetAppliedCount(), State->AppliedBeforeHidden);
        Widget->TickForTest();
        URPGMapMarkerWidget* Live = Renderer->FindWidget(State->Live->GetPointId());
        if (!Test->TestNotNull(TEXT("First resumed frame restores tracked Live target"), Live)) { State->Shutdown(); return true; }
        Test->TestTrue(TEXT("Resume refreshes Live position before placement without waiting for polling"), Live->GetMarkerView().bClampedToEdge);
        Test->TestTrue(TEXT("Resume uses fresh northward bearing"), Live->GetMarkerView().DirectionFromViewCenter.Equals(FVector2D(0.0, -1.0)));
        Test->TestEqual(TEXT("Resume does not rebuild index or restart session"), Widget->ReadyCount, 1);
        Test->TestEqual(TEXT("Index membership is retained while hidden"), Renderer->NumIndexed(), State->IndexedBeforeHidden);
        Life.Subsystem()->UnregisterPoint(*State->Live);
        Widget->TickForTest();
        Test->TestNull(TEXT("Unloading Live-only target removes edge visual"), Renderer->FindWidget(State->Live->GetPointId()));
        Test->TestEqual(TEXT("Unloading does not clear intent"), State->Tracking->GetTrackedMarkerId(), State->Live->GetPointId());
        Life.Subsystem()->RegisterPoint(*State->Live);
        Widget->TickForTest();
        Test->TestNotNull(TEXT("Returning data restores the same tracked ID"), Renderer->FindWidget(State->Live->GetPointId()));
        State->Tracking->TrackMarker(FGuid(99999, 8, 7, 6));
        Widget->TickForTest();
        Test->TestNull(TEXT("Unknown target creates no fabricated visual"), Renderer->FindWidget(State->Tracking->GetTrackedMarkerId()));
        State->Tracking->TrackMarker(State->Far.PointId);
        Widget->TickForTest(FVector2D(512.0, 256.0), 2.0f);
        FVector2D Position = CastChecked<UCanvasPanelSlot>(Renderer->FindWidget(State->Far.PointId)->Slot)->GetPosition();
        const double Radius = 128.0 - FVector2D(12.0).Size() - 4.0;
        Test->TestTrue(TEXT("Non-square/DPI uses short-side circle centered in full viewport"),
            Position.Equals(FVector2D(256.0 - Radius, 128.0), .001));
        Widget->TickForTest(FVector2D(10.0));
        Test->TestTrue(TEXT("Impossible footprint is not displayed"), !Renderer->FindWidget(State->Far.PointId)
            || Renderer->FindWidget(State->Far.PointId)->GetVisibility() == ESlateVisibility::Hidden);
        Widget->TickForTest();
        State->WorldMap->ConstructForTest();
        State->Phase = 2;
        return false;
    }
    if (State->Phase == 2)
    {
        if (!State->WorldMap->IsReadyForTest() || !State->WorldMarker(State->Far.PointId)) { return false; }
        Widget->DestructForTest();
        Test->TestTrue(TEXT("Stopping MiniMap does not stop WorldMap"), State->WorldMap->IsReadyForTest());
        Test->TestEqual(TEXT("Stopping MiniMap keeps player intent"), State->Tracking->GetTrackedMarkerId(), State->Far.PointId);
        Widget->ConstructForTest();
        State->Phase = 3;
        return false;
    }
    if (State->Phase == 3)
    {
        if (Widget->ReadyCount < 2 || !State->Renderer()->FindWidget(State->Far.PointId)) { return false; }
        Test->TestTrue(TEXT("Reconstruction binds once and recovers current tracking"),
            State->Renderer()->FindWidget(State->Far.PointId)->GetMarkerView().bPlayerTracked);
        State->Tracking->ClearTrackedMarker();
        Widget->TickForTest();
        Test->TestNull(TEXT("Clear after reconstruction releases the remote visual"), State->Renderer()->FindWidget(State->Far.PointId));
        Widget->DestructForTest();
        Widget->ConfigureMarkersForTest(State->Normal);
        State->Tracking->TrackMarker(State->Far.PointId);
        Widget->ConstructForTest();
        State->Phase = 4;
        return false;
    }
    if (Widget->ReadyCount < 3 || !Renderer->FindWidget(State->Near.PointId)) { return false; }
    Test->TestNull(TEXT("Missing optional EdgeMarkerCanvas does not invent an offscreen fallback"), Renderer->FindWidget(State->Far.PointId));
    State->Tracking->TrackMarker(State->Near.PointId);
    Widget->TickForTest();
    URPGMapMarkerWidget* Near = Renderer->FindWidget(State->Near.PointId);
    Test->TestTrue(TEXT("Missing edge canvas preserves ordinary in-range display"), Near->GetParent() == State->Normal);
    Test->TestTrue(TEXT("Tracking style remains available without optional edge canvas"), Near->GetMarkerView().bPlayerTracked);
    Test->TestTrue(TEXT("Tracked role still uses the edge class without an optional edge canvas"),
        Near->IsA<URPGMiniMapEdgeMarkerWidgetTest>());
    Test->TestFalse(TEXT("Missing edge canvas never claims to clamp"), Near->GetMarkerView().bClampedToEdge);
    State->Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiniMapTrackingTest, "IronicRPG.RPGGameplay.MiniMap.TrackingAndDualMapLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMiniMapTrackingTest::RunTest(const FString& Parameters)
{
    UGameZoneAsset* Zone = LoadObject<UGameZoneAsset>(nullptr, TEXT("/Game/DataAssets/Zone/DA_NewWorld.DA_NewWorld"));
    UMapMarkerTypeAsset* Type = LoadObject<UMapMarkerTypeAsset>(nullptr, TEXT("/Game/DataAssets/MapMarker/DA_WayPoint.DA_WayPoint"));
    if (!TestNotNull(TEXT("Host Zone"), Zone) || !TestNotNull(TEXT("Host Marker Type"), Type)) { return false; }
    TSharedPtr<FMiniMapTrackingState> State = MakeShared<FMiniMapTrackingState>();
    FMiniMapLifecycleState& Life = *State->Life;
    State->Type.Reset(Type);
    Life.FirstZone.Configure(Zone);
    State->Near.PointId = FGuid(501, 1, 1, 1);
    State->Near.ZoneId = Zone->GetId();
    State->Near.MarkerTypeId = Type->GetId();
    State->Near.MarkerState = EGameZonePointState::Activated;
    State->Near.DisplayMode = static_cast<int32>(EMapMarkerDisplayMode::MiniMap) | static_cast<int32>(EMapMarkerDisplayMode::WorldMap);
    State->Near.SavePolicy = EGameZonePointSavePolicy::Custom;
    State->Near.WorldTransform.SetLocation(FVector(2000.0, 2000.0, 300.0));
    State->Far = State->Near;
    State->Far.PointId = FGuid(502, 1, 1, 1);
    State->Far.WorldTransform.SetLocation(FVector(1000000.0, 2000.0, 300.0));
    State->WorldOnly = State->Far;
    State->WorldOnly.PointId = FGuid(503, 1, 1, 1);
    State->WorldOnly.DisplayMode = static_cast<int32>(EMapMarkerDisplayMode::WorldMap);
    TMap<FGuid, FGameZonePointData>& Points = MiniMapProperty<TMap<FGuid, FGameZonePointData>>(*Zone, TEXT("BakedPoints"));
    Points.Add(State->Near.PointId, State->Near);
    Points.Add(State->Far.PointId, State->Far);
    Points.Add(State->WorldOnly.PointId, State->WorldOnly);
    Life.GameInstance.Reset(NewObject<UGameInstance>(GEngine));
    Life.GameInstance->InitializeStandalone(TEXT("MiniMapTrackingTestWorld"));
    Life.SetZone(Zone->GetId());
    UWorld* World = Life.GameInstance->GetWorld();
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    World->AddController(Controller);
    Life.LocalPlayer.Reset(NewObject<ULocalPlayer>(GEngine));
    Controller->Player = Life.LocalPlayer.Get();
    Life.LocalPlayer->PlayerController = Controller;
    // CommonUI validates the engine's configured viewport class when initializing real LocalPlayer subsystems.
    // Use its actual class for this headless fixture only; never change the project's input/viewport configuration.
    UClass* CommonViewport = LoadClass<UGameViewportClient>(nullptr, TEXT("/Script/CommonUI.CommonGameViewportClient"));
    {
        TGuardValue<TSubclassOf<UGameViewportClient>> ViewportClassGuard(
            GEngine->GameViewportClientClass, CommonViewport ? CommonViewport : GEngine->GameViewportClientClass.Get());
        Life.LocalPlayer->PlayerAdded(nullptr, 0);
    }
    State->Tracking = Life.LocalPlayer->GetSubsystem<UMapTrackingSubsystem>();
    if (!TestNotNull(TEXT("Actual LocalPlayer owns tracking subsystem"), State->Tracking))
    {
        Life.LocalPlayer->PlayerRemoved();
        Life.Shutdown();
        return false;
    }
    State->Tracking->TrackMarker(State->Far.PointId);
    APawn* Pawn = World->SpawnActor<APawn>();
    Life.Pawn = Pawn;
    USceneComponent* Root = NewObject<USceneComponent>(Pawn);
    Pawn->SetRootComponent(Root);
    Root->RegisterComponent();
    Pawn->SetActorLocation(FVector(1000.0, 2000.0, 300.0));
    Controller->Possess(Pawn);
    Life.Widget.Reset(CreateWidget<URPGMiniMapWidgetTest>(Life.GameInstance.Get()));
    Life.Widget->SetPlayerContext(FLocalPlayerContext(Life.LocalPlayer.Get(), World));
    UOverlay* Overlay = Life.Widget->WidgetTree->ConstructWidget<UOverlay>();
    Life.Widget->WidgetTree->RootWidget = Overlay;
    State->Normal = Life.Widget->WidgetTree->ConstructWidget<UCanvasPanel>();
    State->Edge = Life.Widget->WidgetTree->ConstructWidget<UCanvasPanel>();
    Overlay->AddChild(State->Normal);
    Overlay->AddChild(State->Edge);
    Life.Widget->ConfigureMarkersForTest(State->Normal, State->Edge);
    Life.Widget->ConfigurePollingForTest(0.0f);
    State->MiniSlate = Life.Widget->TakeWidget();
    Life.Widget->ConstructForTest();

    State->WorldMap.Reset(CreateWidget<URPGWorldMapWidgetTest>(Life.GameInstance.Get()));
    State->WorldMap->SetPlayerContext(Life.Widget->GetPlayerContext());
    UOverlay* WorldOverlay = State->WorldMap->WidgetTree->ConstructWidget<UOverlay>();
    State->WorldMap->WidgetTree->RootWidget = WorldOverlay;
    UCanvasPanel* Content = State->WorldMap->WidgetTree->ConstructWidget<UCanvasPanel>();
    UCanvasPanel* Markers = State->WorldMap->WidgetTree->ConstructWidget<UCanvasPanel>();
    State->WorldEdge = State->WorldMap->WidgetTree->ConstructWidget<UCanvasPanel>();
    WorldOverlay->AddChild(Content);
    Content->AddChild(Markers);
    WorldOverlay->AddChild(State->WorldEdge);
    State->WorldMap->ConfigureCanvasesForTest(Content, Markers, State->WorldEdge, URPGMiniMapMarkerWidgetTest::StaticClass());
    State->WorldSlate = State->WorldMap->TakeWidget();
    ADD_LATENT_AUTOMATION_COMMAND(FWaitForMiniMapTracking(State, this));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

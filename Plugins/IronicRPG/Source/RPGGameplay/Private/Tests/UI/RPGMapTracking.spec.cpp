// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UI/RPGWorldMapWidgetTestTypes.h"

#include "Components/CanvasPanel.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Misc/AutomationTest.h"
#include "UI/MapTrackingSubsystem.h"
#include "UI/RPGMapViewportTransform.h"
#include "UObject/CoreRedirects.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMapTrackingClassRedirectTest,
    "IronicRPG.RPGGameplay.MapTracking.LegacyClassReferenceRedirectsToRenamedSubsystem",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapTrackingClassRedirectTest::RunTest(const FString& Parameters)
{
    const FCoreRedirectObjectName OldName(FString(TEXT("/Script/RPGGameplay.RPGMapTrackingSubsystem")));
    const FCoreRedirectObjectName Redirected = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Class, OldName);
    TestEqual(
        TEXT("Old Blueprint class references resolve to the renamed subsystem"),
        Redirected.ToString(),
        UMapTrackingSubsystem::StaticClass()->GetPathName());
    return true;
}

/** Exercises the adapter's private routing without adding runtime Blueprint testing APIs. */
struct FRPGWorldMapTrackingTestAccess
{
    static void Configure(URPGWorldMapWidget& Widget, UMapTrackingSubsystem& Tracking)
    {
        Widget.CurrentZoneId = FRPGId(TEXT("zone_TrackingTest"));
        Widget.CurrentSheet.SheetId = FGameZoneMapSheetId(TEXT("CurrentSheet"));
        Widget.MarkerLayerMode = EWorldMapMarkerLayerMode::SelectedLayerOnly;
        Widget.MarkerCanvas = NewObject<UCanvasPanel>(&Widget);
        Widget.EdgeMarkerCanvas = NewObject<UCanvasPanel>(&Widget);
        Widget.TrackingSubsystem = &Tracking;
        Tracking.OnTrackingChanged.AddDynamic(&Widget, &URPGWorldMapWidget::HandleTrackingChanged);
    }

    static bool ShouldMaterialize(const URPGWorldMapWidget& Widget, const FWorldMapMarkerView& View)
    {
        return Widget.ShouldMaterializeMarker(View);
    }

    static void Place(URPGWorldMapWidget& Map, URPGMapMarkerWidget& Marker, const FWorldMapMarkerView& View)
    {
        Map.UpdateMarkerWidgetPlacement(View.Point.Data.PointId, Marker, View);
    }

    static UCanvasPanel* EdgeCanvas(const URPGWorldMapWidget& Widget) { return Widget.EdgeMarkerCanvas; }
    static UCanvasPanel* ContentCanvas(const URPGWorldMapWidget& Widget) { return Widget.MarkerCanvas; }
    static void SetWidgetClasses(URPGWorldMapWidget& Widget, TSubclassOf<URPGMapMarkerWidget> Normal,
        TSubclassOf<URPGMapMarkerWidget> Edge)
    {
        Widget.DefaultMarkerWidgetClass = Normal;
        Widget.EdgeMarkerWidgetClass = Edge;
    }
    static UClass* ResolveWidgetClass(const URPGWorldMapWidget& Widget, bool bTracked)
    {
        return Widget.ResolveMarkerWidgetClass(bTracked).Get();
    }
    static void CachePresentedView(URPGWorldMapWidget& Widget, URPGMapMarkerWidget& Marker, const FWorldMapMarkerView& View)
    {
        Marker.ApplyMarkerView(View);
        Widget.ActiveMarkerWidgets.Add(View.Point.Data.PointId, &Marker);
    }
    static bool TryGetPresentedView(
        const URPGWorldMapWidget& Widget,
        const FGuid& PointId,
        bool bSelected,
        FWorldMapMarkerView& OutView)
    {
        return Widget.TryGetPresentedMarkerView(PointId, bSelected, OutView);
    }
    static void Stop(URPGWorldMapWidget& Widget) { Widget.StopPresentation(); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRPGMapTrackingStateTest,
    "IronicRPG.RPGGameplay.MapTracking.SharedPlayerIntentReplacesClearsAndSurvivesMapClose",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGMapTrackingStateTest::RunTest(const FString& Parameters)
{
    ULocalPlayer* Player = NewObject<ULocalPlayer>(GEngine);
    UMapTrackingSubsystem* Tracking = NewObject<UMapTrackingSubsystem>(Player);
    URPGMapTrackingObserverTest* WorldMapObserver = NewObject<URPGMapTrackingObserverTest>();
    URPGMapTrackingObserverTest* MiniMapObserver = NewObject<URPGMapTrackingObserverTest>();
    Tracking->OnTrackingChanged.AddDynamic(WorldMapObserver, &URPGMapTrackingObserverTest::HandleTrackingChanged);
    Tracking->OnTrackingChanged.AddDynamic(MiniMapObserver, &URPGMapTrackingObserverTest::HandleTrackingChanged);
    const FGuid First(1, 2, 3, 4);
    const FGuid Second(5, 6, 7, 8);

    TestFalse(TEXT("An invalid ID never counts as tracked"), Tracking->IsMarkerTracked(FGuid()));
    TestFalse(TEXT("Tracking an invalid ID is rejected"), Tracking->TrackMarker(FGuid()));
    TestTrue(TEXT("A valid unloaded target can be tracked"), Tracking->TrackMarker(First));
    Tracking->TrackMarker(First);
    TestEqual(TEXT("Repeated tracking does not notify twice"), WorldMapObserver->ChangeCount, 1);
    TestEqual(TEXT("Both presentations see the same change"), MiniMapObserver->Current, First);
    TestTrue(TEXT("Tracking replaces the previous target"), Tracking->TrackMarker(Second));
    TestFalse(TEXT("Only one target is tracked"), Tracking->IsMarkerTracked(First));
    TestTrue(TEXT("The replacement is tracked"), Tracking->IsMarkerTracked(Second));
    TestEqual(TEXT("Notifications identify the previous target"), WorldMapObserver->Previous, First);
    Tracking->TrackMarker(FGuid());
    TestEqual(TEXT("Invalid requests preserve the target"), Tracking->GetTrackedMarkerId(), Second);

    URPGWorldMapWidgetTest* Map = NewObject<URPGWorldMapWidgetTest>();
    FRPGWorldMapTrackingTestAccess::Configure(*Map, *Tracking);
    FRPGWorldMapTrackingTestAccess::Stop(*Map);
    TestEqual(TEXT("Closing World Map preserves shared tracking intent"), Tracking->GetTrackedMarkerId(), Second);

    ULocalPlayer* OtherPlayer = NewObject<ULocalPlayer>(GEngine);
    UMapTrackingSubsystem* OtherTracking = NewObject<UMapTrackingSubsystem>(OtherPlayer);
    TestFalse(TEXT("Tracking is isolated per local player"), OtherTracking->IsMarkerTracked(Second));

    Tracking->ClearTrackedMarker();
    Tracking->ClearTrackedMarker();
    TestFalse(TEXT("Clear removes tracking intent"), Tracking->GetTrackedMarkerId().IsValid());
    TestEqual(TEXT("Clear notifies once, after the two target changes"), WorldMapObserver->ChangeCount, 3);
    TestEqual(TEXT("Both presentations receive clear"), MiniMapObserver->ChangeCount, 3);
    TestFalse(TEXT("Clear publishes an invalid current target"), MiniMapObserver->Current.IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRPGWorldMapTrackingRoutingTest,
    "IronicRPG.RPGGameplay.WorldMap.MarkerPresentation.PlayerTrackingControlsRoutingAndSheetBypass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGWorldMapTrackingRoutingTest::RunTest(const FString& Parameters)
{
    ULocalPlayer* Player = NewObject<ULocalPlayer>(GEngine);
    UMapTrackingSubsystem* Tracking = NewObject<UMapTrackingSubsystem>(Player);
    URPGWorldMapWidgetTest* Map = NewObject<URPGWorldMapWidgetTest>();
    URPGMapMarkerWidgetTest* Marker = NewObject<URPGMapMarkerWidgetTest>();
    FRPGWorldMapTrackingTestAccess::Configure(*Map, *Tracking);
    FRPGWorldMapTrackingTestAccess::SetWidgetClasses(
        *Map, URPGMapMarkerWidgetTest::StaticClass(), URPGMapEdgeMarkerWidgetTest::StaticClass());
    TestEqual(TEXT("Ordinary role resolves the default marker class"),
        FRPGWorldMapTrackingTestAccess::ResolveWidgetClass(*Map, false), URPGMapMarkerWidgetTest::StaticClass());
    TestEqual(TEXT("Tracked role resolves the edge marker class"),
        FRPGWorldMapTrackingTestAccess::ResolveWidgetClass(*Map, true), URPGMapEdgeMarkerWidgetTest::StaticClass());
    FRPGWorldMapTrackingTestAccess::SetWidgetClasses(*Map, URPGMapMarkerWidgetTest::StaticClass(), nullptr);
    TestEqual(TEXT("Unset edge class falls back to the default marker class"),
        FRPGWorldMapTrackingTestAccess::ResolveWidgetClass(*Map, true), URPGMapMarkerWidgetTest::StaticClass());

    FWorldMapMarkerView View;
    View.Point.Data.PointId = FGuid(10, 20, 30, 40);
    View.Point.Data.ZoneId = FRPGId(TEXT("zone_TrackingTest"));
    View.Point.Data.DisplayMode = static_cast<int32>(EMapMarkerDisplayMode::WorldMap);
    View.Point.Data.MarkerState = EGameZonePointState::Deactivated;
    View.Point.Data.WorldTransform.SetLocation(FVector(100000.0, 200000.0, 3000.0));
    View.LayerRelation = EWorldMapMarkerLayerRelation::Above;
    TestFalse(TEXT("Untracked markers still obey selected Layer filtering"), FRPGWorldMapTrackingTestAccess::ShouldMaterialize(*Map, View));

    View.bPlayerTracked = true;
    TestTrue(
        TEXT("Tracked deactivated targets bypass current Sheet and Layer bounds"),
        FRPGWorldMapTrackingTestAccess::ShouldMaterialize(*Map, View));
    View.Point.Data.MarkerState = EGameZonePointState::Hide;
    TestFalse(TEXT("Tracking cannot reveal a hidden target"), FRPGWorldMapTrackingTestAccess::ShouldMaterialize(*Map, View));
    View.Point.Data.MarkerState = EGameZonePointState::Activated;
    View.Point.Data.DisplayMode = static_cast<int32>(EMapMarkerDisplayMode::MiniMap);
    TestFalse(TEXT("Tracking cannot override display mode"), FRPGWorldMapTrackingTestAccess::ShouldMaterialize(*Map, View));
    View.Point.Data.DisplayMode = static_cast<int32>(EMapMarkerDisplayMode::WorldMap);
    View.Point.Data.ZoneId = FRPGId(TEXT("zone_Other"));
    TestFalse(TEXT("Tracking cannot reveal another Zone"), FRPGWorldMapTrackingTestAccess::ShouldMaterialize(*Map, View));
    View.Point.Data.ZoneId = FRPGId(TEXT("zone_TrackingTest"));

    // No loaded MarkerType is needed to choose the edge overlay.
    FRPGWorldMapTrackingTestAccess::Place(*Map, *Marker, View);
    TestTrue(TEXT("Player tracking routes to the edge overlay"), Marker->GetParent() == FRPGWorldMapTrackingTestAccess::EdgeCanvas(*Map));
    TestFalse(TEXT("Before layout provides geometry the edge marker remains hidden"), Marker->IsVisible());

    View.bPlayerTracked = false;
    View.bClampedToEdge = true;
    View.DirectionFromViewCenter = FVector2D(1.0, 0.0);
    FRPGWorldMapTrackingTestAccess::Place(*Map, *Marker, View);
    TestTrue(TEXT("Untracking restores the normal content canvas"), Marker->GetParent() == FRPGWorldMapTrackingTestAccess::ContentCanvas(*Map));
    TestFalse(TEXT("Untracking clears the edge visual flag"), Marker->GetMarkerView().bClampedToEdge);
    TestTrue(TEXT("Untracking clears the edge arrow direction"), Marker->GetMarkerView().DirectionFromViewCenter.IsZero());

    View.bPlayerTracked = true;
    View.bClampedToEdge = true;
    View.DirectionFromViewCenter = FVector2D(0.6, -0.8);
    View.bSelected = false;
    FRPGWorldMapTrackingTestAccess::CachePresentedView(*Map, *Marker, View);
    FWorldMapMarkerView PresentedView;
    TestTrue(TEXT("Selection payload can resolve the final presented marker view"),
        FRPGWorldMapTrackingTestAccess::TryGetPresentedView(*Map, View.Point.Data.PointId, true, PresentedView));
    TestTrue(TEXT("Selection payload preserves the placement clamp flag"), PresentedView.bClampedToEdge);
    TestTrue(TEXT("Selection payload preserves the placement direction"),
        PresentedView.DirectionFromViewCenter.Equals(View.DirectionFromViewCenter));
    TestTrue(TEXT("Selection payload applies the current selected state"), PresentedView.bSelected);
    TestFalse(TEXT("A marker without an active presentation widget has no event view"),
        FRPGWorldMapTrackingTestAccess::TryGetPresentedView(*Map, FGuid(99, 88, 77, 66), true, PresentedView));
    FRPGWorldMapTrackingTestAccess::Stop(*Map);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRPGMapTrackingPlacementTest,
    "IronicRPG.RPGGameplay.MapTracking.TrackingAloneControlsRectangleAndCircleOverflow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGMapTrackingPlacementTest::RunTest(const FString& Parameters)
{
    ULocalPlayer* Player = NewObject<ULocalPlayer>(GEngine);
    UMapTrackingSubsystem* Tracking = NewObject<UMapTrackingSubsystem>(Player);
    const FGuid PointId(1, 3, 5, 7);
    for (ERPGMapViewportShape Shape : {ERPGMapViewportShape::Rectangle, ERPGMapViewportShape::Circle})
    {
        FRPGMapViewportParameters View;
        View.ViewportSize = FVector2D(200.0, 200.0);
        View.Shape = Shape;
        const FRPGMapViewportTransform Transform(View);
        const FVector Outside(30000.0, 0.0, 0.0);
        const FVector2D HalfExtent(8.0, 8.0);
        TestFalse(TEXT("Untracked outside targets hide"), Transform.PlaceMarker(Outside, HalfExtent, Tracking->IsMarkerTracked(PointId)).bIsVisible);
        Tracking->TrackMarker(PointId);
        TestTrue(
            TEXT("Tracked outside targets clamp"),
            Transform.PlaceMarker(Outside, HalfExtent, Tracking->IsMarkerTracked(PointId)).bIsClampedToEdge);
        const FRPGMapMarkerPlacement Inside = Transform.PlaceMarker(FVector::ZeroVector, HalfExtent, Tracking->IsMarkerTracked(PointId));
        TestTrue(TEXT("Tracked inside targets stay at their normal location"), Inside.bIsVisible && !Inside.bIsClampedToEdge);
        Tracking->ClearTrackedMarker();
        TestFalse(
            TEXT("Cleared outside targets hide again"),
            Transform.PlaceMarker(Outside, HalfExtent, Tracking->IsMarkerTracked(PointId)).bIsVisible);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

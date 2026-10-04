// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UI/RPGWorldMapWidgetTestTypes.h"

#include "Levels/MapMarkerTypeAsset.h"
#include "Misc/AutomationTest.h"
#include "UI/MapMarkerSelectionResolver.h"
#include "UI/RPGMapInteractionContext.h"
#include "Widgets/Contents/ButtonBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRPGMapInteractiveWidgetContextTest,
    "IronicRPG.RPGGameplay.WorldMap.InteractiveContext.SupportsTypedReplacementAndClear",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGMapInteractiveWidgetContextTest::RunTest(const FString& Parameters)
{
    UButtonBase* Widget = NewObject<UButtonBase>();
    TestFalse(TEXT("A new interactive widget has no context"), Widget->HasContext());

    FMapMarkerActionButtonContext ActionContext;
    ActionContext.ActionTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Map.Marker.Action.FastTravel")), false);
    Widget->SetContext(FInstancedStruct::Make(ActionContext));

    TestTrue(TEXT("Setting a typed context makes the widget context valid"), Widget->HasContext());
    const FInstancedStruct StoredActionContext = Widget->GetContext();
    const FMapMarkerActionButtonContext* StoredAction = StoredActionContext.GetPtr<FMapMarkerActionButtonContext>();
    TestNotNull(TEXT("The action context preserves its concrete type"), StoredAction);
    if (StoredAction)
    {
        TestEqual(TEXT("The action context preserves its action tag"), StoredAction->ActionTag, ActionContext.ActionTag);
    }

    const FGuid PointId(1, 2, 3, 4);
    FMapMarkerCandidateButtonContext CandidateContext;
    CandidateContext.PointId = PointId;
    Widget->SetContext(FInstancedStruct::Make(CandidateContext));

    const FInstancedStruct StoredCandidateContext = Widget->GetContext();
    TestNull(
        TEXT("Replacing the context removes the previous concrete type"),
        StoredCandidateContext.GetPtr<FMapMarkerActionButtonContext>());
    const FMapMarkerCandidateButtonContext* StoredCandidate = StoredCandidateContext.GetPtr<FMapMarkerCandidateButtonContext>();
    TestNotNull(TEXT("The candidate context preserves its concrete type"), StoredCandidate);
    if (StoredCandidate)
    {
        TestEqual(TEXT("The candidate context preserves its PointId"), StoredCandidate->PointId, PointId);
    }

    Widget->ClearContext();
    TestFalse(TEXT("Clearing the context makes it invalid"), Widget->HasContext());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRPGWorldMapWidgetZoomInputTest,
    "IronicRPG.RPGGameplay.WorldMap.ViewInput.ZoomUsesConfigurableStepAndLimits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGWorldMapWidgetZoomInputTest::RunTest(const FString& Parameters)
{
    URPGWorldMapWidgetTest* Widget = NewObject<URPGWorldMapWidgetTest>();
    Widget->ConfigureViewInput(100.0f, 0.5f, 2.0f, 0.25f);
    Widget->SetViewZoom(1.0f);

    Widget->ApplyViewZoomInput(2.0f);
    TestEqual(TEXT("Zoom input uses the configured step"), Widget->GetViewZoomForTest(), 1.5f);

    Widget->ApplyViewZoomInput(10.0f);
    TestEqual(TEXT("Zoom input respects the maximum"), Widget->GetViewZoomForTest(), 2.0f);

    Widget->ApplyViewZoomInput(-10.0f);
    TestEqual(TEXT("Zoom input respects the minimum"), Widget->GetViewZoomForTest(), 0.5f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRPGWorldMapWidgetCanvasProjectionTest,
    "IronicRPG.RPGGameplay.WorldMap.ViewInput.CanvasProjectionUsesSharedNorthUpAxes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGWorldMapWidgetCanvasProjectionTest::RunTest(const FString& Parameters)
{
    URPGWorldMapWidgetTest* Widget = NewObject<URPGWorldMapWidgetTest>();
    Widget->ConfigureViewInput(10.0f, 0.5f, 2.0f, 0.25f);

    TestEqual(
        TEXT("World Map canvas uses world +X as west and world +Y as north"),
        Widget->WorldLocationToCanvasPosition(FVector(100.0, 200.0, 999.0)),
        FVector2D(-10.0, -20.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRPGWorldMapWidgetPanInputTest,
    "IronicRPG.RPGGameplay.WorldMap.ViewInput.PanConvertsScreenDeltaAtCurrentZoom",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGWorldMapWidgetPanInputTest::RunTest(const FString& Parameters)
{
    URPGWorldMapWidgetTest* Widget = NewObject<URPGWorldMapWidgetTest>();
    Widget->ConfigureViewInput(100.0f, 0.5f, 4.0f, 0.05f);
    Widget->ConfigureViewBounds(FVector2D(-1000.0, -1000.0), FVector2D(1000.0, 1000.0), 0.0f);
    Widget->SetViewZoom(2.0f);
    Widget->SetViewCenterWorldLocation(FVector(100.0f, 200.0f, 300.0f));

    Widget->ApplyViewPanInput(FVector2D(20.0f, -10.0f));
    TestEqual(TEXT("Pan converts local screen delta into world distance"), Widget->GetViewCenterForTest(), FVector(110.0f, 195.0f, 300.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRPGWorldMapWidgetPanBoundsClampTest,
    "IronicRPG.RPGGameplay.WorldMap.ViewInput.PanBoundsClampWorldXYWithMeterPadding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGWorldMapWidgetPanBoundsClampTest::RunTest(const FString& Parameters)
{
    URPGWorldMapWidgetTest* Widget = NewObject<URPGWorldMapWidgetTest>();
    Widget->ConfigureViewBounds(FVector2D(0.0, 100.0), FVector2D(1000.0, 900.0), 2.0f);
    Widget->SetViewCenterWorldLocation(FVector(-500.0, 1500.0, 77.0));

    TestEqual(
        TEXT("World Map center clamps XY to bounds expanded by meter padding and preserves Z"),
        Widget->GetViewCenterForTest(),
        FVector(-200.0, 1100.0, 77.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRPGWorldMapWidgetPanWithoutBoundsTest,
    "IronicRPG.RPGGameplay.WorldMap.ViewInput.PanIgnoresInputWithoutValidBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGWorldMapWidgetPanWithoutBoundsTest::RunTest(const FString& Parameters)
{
    URPGWorldMapWidgetTest* Widget = NewObject<URPGWorldMapWidgetTest>();
    Widget->ConfigureViewInput(100.0f, 0.5f, 4.0f, 0.05f);
    Widget->ClearViewBounds();
    Widget->SetViewCenterWorldLocation(FVector(100.0, 200.0, 300.0));

    Widget->ApplyViewPanInput(FVector2D(20.0f, -10.0f));
    TestEqual(
        TEXT("Manual pan leaves the pending focus center unchanged without valid bounds"),
        Widget->GetViewCenterForTest(),
        FVector(100.0, 200.0, 300.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRPGWorldMapMarkerSelectionStateTest,
    "IronicRPG.RPGGameplay.WorldMap.MarkerSelection.IncludesVisibleStatesAndExcludesHiddenMarkers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGWorldMapMarkerSelectionStateTest::RunTest(const FString& Parameters)
{
    auto MakeTarget = [](const FGuid& PointId, EGameZonePointState State, const FVector2D& Minimum, const FVector2D& Maximum)
        {
            FMapMarkerSelectionTarget Target;
            Target.View.Point.Data.PointId = PointId;
            Target.View.Point.Data.MarkerState = State;
            Target.BoundsMinimum = Minimum;
            Target.BoundsMaximum = Maximum;
            return Target;
        };

    const FGuid ActivatedId(1, 0, 0, 0);
    const FGuid DeactivatedId(2, 0, 0, 0);
    const TArray<FMapMarkerSelectionTarget> Targets = {
        MakeTarget(ActivatedId, EGameZonePointState::Activated, FVector2D(0.0, 0.0), FVector2D(10.0, 10.0)),
        MakeTarget(DeactivatedId, EGameZonePointState::Deactivated, FVector2D(14.0, 0.0), FVector2D(24.0, 10.0)),
        MakeTarget(FGuid(3, 0, 0, 0), EGameZonePointState::Hide, FVector2D(14.0, 0.0), FVector2D(24.0, 10.0)),
        MakeTarget(FGuid(4, 0, 0, 0), EGameZonePointState::Activated, FVector2D(100.0, 100.0), FVector2D(110.0, 110.0)),
    };

    const TArray<FWorldMapMarkerView> Candidates = FMapMarkerSelectionResolver::Resolve(FVector2D(15.0, 5.0), 5.0f, Targets);
    TestEqual(TEXT("Only activated and deactivated markers inside the padded bounds are candidates"), Candidates.Num(), 2);
    if (Candidates.Num() == 2)
    {
        TestEqual(TEXT("The marker containing the pointer sorts before a padded hit"), Candidates[0].Point.Data.PointId, DeactivatedId);
        TestEqual(TEXT("The padded activated marker remains selectable"), Candidates[1].Point.Data.PointId, ActivatedId);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRPGWorldMapMarkerSelectionOrderingTest,
    "IronicRPG.RPGGameplay.WorldMap.MarkerSelection.UsesStableZOrderAndPointIdTiebreakers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGWorldMapMarkerSelectionOrderingTest::RunTest(const FString& Parameters)
{
    auto MakeTarget = [](const FGuid& PointId, int32 ZOrder)
        {
            FMapMarkerSelectionTarget Target;
            Target.View.Point.Data.PointId = PointId;
            Target.View.Point.Data.MarkerState = EGameZonePointState::Activated;
            Target.BoundsMinimum = FVector2D::ZeroVector;
            Target.BoundsMaximum = FVector2D(10.0, 10.0);
            Target.ZOrder = ZOrder;
            return Target;
        };

    const FGuid LowerId(1, 0, 0, 0);
    const FGuid HigherId(2, 0, 0, 0);
    const FGuid TopId(3, 0, 0, 0);
    const TArray<FMapMarkerSelectionTarget> Targets = {
        MakeTarget(HigherId, 0),
        MakeTarget(LowerId, 0),
        MakeTarget(TopId, 10),
    };

    const TArray<FWorldMapMarkerView> Candidates = FMapMarkerSelectionResolver::Resolve(FVector2D(5.0, 5.0), 0.0f, Targets);
    TestEqual(TEXT("All overlapping markers are returned"), Candidates.Num(), 3);
    if (Candidates.Num() == 3)
    {
        TestEqual(TEXT("Higher Z order sorts first"), Candidates[0].Point.Data.PointId, TopId);
        TestEqual(TEXT("PointId provides a stable final tiebreaker"), Candidates[1].Point.Data.PointId, LowerId);
        TestEqual(TEXT("PointId ordering is deterministic"), Candidates[2].Point.Data.PointId, HigherId);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

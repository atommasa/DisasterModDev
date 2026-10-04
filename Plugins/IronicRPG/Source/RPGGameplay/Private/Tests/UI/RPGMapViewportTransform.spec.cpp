// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/RPGMapViewportTransform.h"

#include <limits>

namespace
{
bool AreNearlyEqual(const FVector2D& Left, const FVector2D& Right, double Tolerance = UE_KINDA_SMALL_NUMBER)
{
    return Left.Equals(Right, Tolerance);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRPGMapViewportNorthUpProjectionTest,
    "IronicRPG.RPGGameplay.MapViewportTransform.NorthUpProjectionUsesSharedWorldAxes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGMapViewportNorthUpProjectionTest::RunTest(const FString& Parameters)
{
    FRPGMapViewportParameters ViewParameters;
    ViewParameters.ViewportSize = FVector2D(200.0, 100.0);
    ViewParameters.ViewCenterWorldLocation = FVector(1000.0, 2000.0, 300.0);
    ViewParameters.LocalUnitsPerWorldMeter = 10.0f;
    const FRPGMapViewportTransform Transform(ViewParameters);

    const FRPGMapMarkerPlacement Center = Transform.PlaceMarker(
        ViewParameters.ViewCenterWorldLocation,
        FVector2D::ZeroVector,
        false);
    TestTrue(TEXT("The view configuration is valid"), Transform.IsValid());
    TestTrue(TEXT("The view center projects to the viewport center"), AreNearlyEqual(Center.LocalPosition, FVector2D(100.0, 50.0)));

    const FRPGMapMarkerPlacement NorthWest = Transform.PlaceMarker(
        FVector(1100.0, 2100.0, -500.0),
        FVector2D::ZeroVector,
        false);
    TestTrue(TEXT("World +X projects west and world +Y projects north"), AreNearlyEqual(NorthWest.LocalPosition, FVector2D(90.0, 40.0)));
    TestTrue(TEXT("Z does not affect map placement"), NorthWest.bIsInsideViewport);
    TestTrue(TEXT("An inside marker remains visible"), NorthWest.bIsVisible);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRPGMapViewportRectangleOverflowTest,
    "IronicRPG.RPGGameplay.MapViewportTransform.RectanglePreservesBearingWhenClamped",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGMapViewportRectangleOverflowTest::RunTest(const FString& Parameters)
{
    FRPGMapViewportParameters ViewParameters;
    ViewParameters.ViewportSize = FVector2D(200.0, 100.0);
    ViewParameters.LocalUnitsPerWorldMeter = 1.0f;
    ViewParameters.EdgePadding = 5.0f;
    ViewParameters.Shape = ERPGMapViewportShape::Rectangle;
    const FRPGMapViewportTransform Transform(ViewParameters);
    const FVector MarkerWorldLocation(-15000.0, -5000.0, 0.0);
    const FVector2D MarkerHalfExtent(10.0, 5.0);

    const FRPGMapMarkerPlacement Hidden = Transform.PlaceMarker(
        MarkerWorldLocation,
        MarkerHalfExtent,
        false);
    TestTrue(TEXT("The raw rectangle placement remains available"), AreNearlyEqual(Hidden.UnclampedLocalPosition, FVector2D(250.0, 100.0)));
    TestFalse(TEXT("An outside marker is not inside the effective rectangle"), Hidden.bIsInsideViewport);
    TestFalse(TEXT("Hide makes an outside marker invisible"), Hidden.bIsVisible);
    TestFalse(TEXT("Hide does not report an edge clamp"), Hidden.bIsClampedToEdge);

    const FRPGMapMarkerPlacement Clamped = Transform.PlaceMarker(
        MarkerWorldLocation,
        MarkerHalfExtent,
        true);
    TestTrue(TEXT("Clamp keeps an outside marker visible"), Clamped.bIsVisible);
    TestTrue(TEXT("Clamp reports edge placement"), Clamped.bIsClampedToEdge);
    TestTrue(
        TEXT("Rectangle clamping intersects the center ray with the inset edge"),
        AreNearlyEqual(Clamped.LocalPosition, FVector2D(185.0, 78.333333), 0.001));
    TestTrue(
        TEXT("The unclamped direction is preserved for edge visuals"),
        AreNearlyEqual(Clamped.DirectionFromCenter, FVector2D(150.0, 50.0).GetSafeNormal()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRPGMapViewportCircleOverflowTest,
    "IronicRPG.RPGGameplay.MapViewportTransform.CircleUsesMarkerRadiusAndPadding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGMapViewportCircleOverflowTest::RunTest(const FString& Parameters)
{
    FRPGMapViewportParameters ViewParameters;
    ViewParameters.ViewportSize = FVector2D(200.0, 160.0);
    ViewParameters.LocalUnitsPerWorldMeter = 1.0f;
    ViewParameters.EdgePadding = 5.0f;
    ViewParameters.Shape = ERPGMapViewportShape::Circle;
    const FRPGMapViewportTransform Transform(ViewParameters);
    const FVector2D MarkerHalfExtent(6.0, 8.0);

    const FRPGMapMarkerPlacement Inside = Transform.PlaceMarker(
        FVector(-3000.0, -4000.0, 0.0),
        MarkerHalfExtent,
        true);
    TestTrue(TEXT("A marker inside the effective circle is not clamped"), Inside.bIsInsideViewport && !Inside.bIsClampedToEdge);
    TestTrue(TEXT("Inside circle placement remains unchanged"), AreNearlyEqual(Inside.LocalPosition, FVector2D(130.0, 120.0)));

    const FRPGMapMarkerPlacement Clamped = Transform.PlaceMarker(
        FVector(-6000.0, -8000.0, 0.0),
        MarkerHalfExtent,
        true);
    TestTrue(
        TEXT("The circle subtracts the marker radius and padding before clamping"),
        AreNearlyEqual(Clamped.LocalPosition, FVector2D(139.0, 132.0)));
    TestTrue(TEXT("The outside circle marker is clamped"), Clamped.bIsClampedToEdge);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRPGMapViewportInvalidGeometryTest,
    "IronicRPG.RPGGameplay.MapViewportTransform.InvalidGeometryIsNotRenderable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGMapViewportInvalidGeometryTest::RunTest(const FString& Parameters)
{
    FRPGMapViewportParameters ZeroViewportParameters;
    ZeroViewportParameters.ViewportSize = FVector2D::ZeroVector;
    const FRPGMapViewportTransform ZeroViewportTransform(ZeroViewportParameters);
    TestFalse(TEXT("A zero-size viewport is invalid"), ZeroViewportTransform.IsValid());

    FRPGMapViewportParameters ConsumedViewportParameters;
    ConsumedViewportParameters.ViewportSize = FVector2D(20.0, 20.0);
    ConsumedViewportParameters.Shape = ERPGMapViewportShape::Circle;
    const FRPGMapViewportTransform ConsumedViewportTransform(ConsumedViewportParameters);
    const FRPGMapMarkerPlacement Consumed = ConsumedViewportTransform.PlaceMarker(
        FVector::ZeroVector,
        FVector2D(10.0, 10.0),
        true);
    TestFalse(TEXT("A marker that consumes the safe circle has no valid placement geometry"), Consumed.bHasValidGeometry);
    TestFalse(TEXT("Invalid placement geometry never renders"), Consumed.bIsVisible);

    FRPGMapViewportParameters ValidParameters;
    ValidParameters.ViewportSize = FVector2D(100.0, 100.0);
    const FRPGMapViewportTransform ValidTransform(ValidParameters);
    const FRPGMapMarkerPlacement InvalidLocation = ValidTransform.PlaceMarker(
        FVector(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0),
        FVector2D::ZeroVector,
        true);
    TestFalse(TEXT("A non-finite world location is rejected"), InvalidLocation.bHasValidGeometry);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

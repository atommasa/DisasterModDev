// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/RPGMiniMapProjection.h"
#include "UI/RPGMapViewportTransform.h"
#include "Misc/AutomationTest.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiniMapUVBasisTest, "IronicRPG.RPGGameplay.MiniMap.MaterialBasisMatchesViewportProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMiniMapUVBasisTest::RunTest(const FString& Parameters)
{
    FResolvedGameZoneMapSheet Sheet;
    Sheet.WorldOrigin = FVector(2300.0, -4100.0, 500.0);
    Sheet.WorldSize = FVector2D(40000.0, 20000.0);
    const FVector Center = Sheet.WorldOrigin + FVector(1700.0, 100.0, 600.0);
    for (const float Yaw : {0.0f, 90.0f, -90.0f, 37.0f})
    {
        Sheet.WorldYaw = Yaw;
        for (const FVector2D Size : {FVector2D(256.0, 256.0), FVector2D(512.0, 256.0), FVector2D(128.0, 512.0)})
        {
            for (const float Radius : {50.0f, 100.0f})
            {
                const float Scale = FMath::Min(Size.X, Size.Y) / (2.0 * Radius);
                const FRPGMiniMapUVBasis Basis = FRPGMiniMapProjection::BuildUVBasis(Sheet, Center, Size, Scale);
                TestTrue(TEXT("Finite rotated/non-square Sheet yields a valid basis"), Basis.bIsValid);
                FGameZoneMapSheetMapping Mapping;
                Mapping.WorldOrigin = Sheet.WorldOrigin;
                Mapping.WorldSize = Sheet.WorldSize;
                Mapping.WorldYaw = Sheet.WorldYaw;
                for (const FVector Offset : {FVector::ZeroVector, FVector(1000.0, 0.0, 0.0), FVector(0.0, 1000.0, 0.0), FVector(-700.0, -900.0, 0.0)})
                {
                    const FVector2D LocalOffset = FRPGMapViewportTransform::ProjectNorthUpWorldOffset(Offset, Scale);
                    const FVector2D SampleUV = Basis.Center + LocalOffset.X / Size.X * Basis.AxisX + LocalOffset.Y / Size.Y * Basis.AxisY;
                    FGameZoneMapProjection Expected;
                    Mapping.ProjectWorldLocation(Center + Offset, Expected);
                    TestTrue(TEXT("A projected Marker and the material sample agree without swapped axes or magic scale"),
                        SampleUV.Equals(Expected.UV, 1.e-6));
                }
            }
        }
    }
    Sheet.WorldYaw = 0.0f;
    const FRPGMiniMapUVBasis Outside = FRPGMiniMapProjection::BuildUVBasis(Sheet, FVector(1000000.0), FVector2D(256.0), 1.28f);
    TestTrue(TEXT("Outside-Sheet center is valid; the material masks rather than clamps UV"), Outside.bIsValid);
    TestTrue(TEXT("Outside UV is not clamped to texture edges"), Outside.Center.X > 1.0);
    TestFalse(TEXT("Zero viewport is rejected"), FRPGMiniMapProjection::BuildUVBasis(Sheet, Center, FVector2D::ZeroVector, 1.0f).bIsValid);
    TestFalse(TEXT("Zero density is rejected"), FRPGMiniMapProjection::BuildUVBasis(Sheet, Center, FVector2D(256.0), 0.0f).bIsValid);
    Sheet.WorldYaw = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Invalid yaw is rejected"), FRPGMiniMapProjection::BuildUVBasis(Sheet, Center, FVector2D(256.0), 1.0f).bIsValid);
    Sheet.WorldYaw = 0.0f;
    Sheet.WorldSize.X = 0.0;
    TestFalse(TEXT("Missing mapping size is rejected"), FRPGMiniMapProjection::BuildUVBasis(Sheet, Center, FVector2D(256.0), 1.0f).bIsValid);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiniMapNorthUpAngleTest, "IronicRPG.RPGGameplay.MiniMap.NorthUpIconAngles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMiniMapNorthUpAngleTest::RunTest(const FString& Parameters)
{
    float Angle = 0.0f;
    FRPGMiniMapProjection::TryGetNorthUpAngle(0.0f, Angle);
    TestEqual(TEXT("World +X is west"), Angle, -90.0f);
    FRPGMiniMapProjection::TryGetNorthUpAngle(90.0f, Angle);
    TestEqual(TEXT("World +Y is north"), Angle, 0.0f);
    FRPGMiniMapProjection::TryGetNorthUpAngle(180.0f, Angle);
    TestEqual(TEXT("World -X is east"), Angle, 90.0f);
    FRPGMiniMapProjection::TryGetNorthUpAngle(270.0f, Angle);
    TestEqual(TEXT("World -Y is south"), Angle, 180.0f);
    FRPGMiniMapProjection::TryGetNorthUpAngle(450.0f, Angle);
    TestEqual(TEXT("Full rotations normalize"), Angle, 0.0f);
    TestFalse(TEXT("Invalid direction is rejected"), FRPGMiniMapProjection::TryGetNorthUpAngle(std::numeric_limits<float>::infinity(), Angle));
    TestEqual(TEXT("Invalid direction clears stale angle"), Angle, 0.0f);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

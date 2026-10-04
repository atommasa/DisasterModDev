// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Levels/GameZoneMapData.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapProjectionAxisAlignedTest,
    "IronicRPG.GameZone.Map.Projection.AxisAlignedMappingUsesExpectedUV",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapProjectionAxisAlignedTest::RunTest(const FString& Parameters)
{
    FGameZoneMapSheetMapping Mapping;
    Mapping.WorldOrigin = FVector(1000.0, 2000.0, 300.0);
    Mapping.WorldSize = FVector2D(200.0, 100.0);

    FGameZoneMapProjection Center;
    TestTrue(
        TEXT("A valid mapping projects its center"),
        Mapping.ProjectWorldLocation(FVector(1000.0, 2000.0, -500.0), Center));
    TestEqual(TEXT("Mapping center has centered UV"), Center.UV, FVector2D(0.5, 0.5));
    TestTrue(TEXT("Mapping center is inside"), Center.bIsInsideSheet);

    FGameZoneMapProjection PositiveXEdge;
    TestTrue(
        TEXT("A valid mapping projects its positive X edge"),
        Mapping.ProjectWorldLocation(FVector(1100.0, 2000.0, 300.0), PositiveXEdge));
    TestEqual(TEXT("Positive X maps to U one"), PositiveXEdge.UV, FVector2D(1.0, 0.5));

    FGameZoneMapProjection PositiveYEdge;
    TestTrue(
        TEXT("A valid mapping projects its positive Y edge"),
        Mapping.ProjectWorldLocation(FVector(1000.0, 2050.0, 300.0), PositiveYEdge));
    TestEqual(TEXT("Positive Y maps to V zero"), PositiveYEdge.UV, FVector2D(0.5, 0.0));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapProjectionRotatedOutsideTest,
    "IronicRPG.GameZone.Map.Projection.RotatedMappingReturnsUnclampedOutsideUV",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapProjectionRotatedOutsideTest::RunTest(const FString& Parameters)
{
    FGameZoneMapSheetMapping Mapping;
    Mapping.WorldSize = FVector2D(200.0, 100.0);
    Mapping.WorldYaw = 90.0f;

    FGameZoneMapProjection PositiveLocalX;
    TestTrue(
        TEXT("A rotated mapping projects a world-space point"),
        Mapping.ProjectWorldLocation(FVector(0.0, 100.0, 500.0), PositiveLocalX));
    TestTrue(
        TEXT("Positive world Y becomes positive local X at ninety degrees"),
        PositiveLocalX.UV.Equals(FVector2D(1.0, 0.5), UE_KINDA_SMALL_NUMBER));
    TestTrue(TEXT("The rotated edge remains inside"), PositiveLocalX.bIsInsideSheet);

    FGameZoneMapProjection Outside;
    TestTrue(
        TEXT("A valid mapping returns projection data outside its bounds"),
        Mapping.ProjectWorldLocation(FVector(0.0, 150.0, 0.0), Outside));
    TestTrue(
        TEXT("Outside projection remains unclamped"),
        Outside.UV.Equals(FVector2D(1.25, 0.5), UE_KINDA_SMALL_NUMBER));
    TestFalse(TEXT("Outside projection is explicitly identified"), Outside.bIsInsideSheet);

    FGameZoneMapSheetMapping InvalidMapping;
    FGameZoneMapProjection InvalidProjection;
    TestFalse(
        TEXT("A mapping without positive world size cannot project"),
        InvalidMapping.ProjectWorldLocation(FVector::ZeroVector, InvalidProjection));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

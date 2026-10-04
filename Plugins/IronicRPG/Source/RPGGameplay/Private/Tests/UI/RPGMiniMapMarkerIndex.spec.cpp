// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/RPGMiniMapMarkerIndex.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiniMapMarkerIndexTest, "IronicRPG.RPGGameplay.MiniMap.SpatialIndexBoundariesAndMutation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMiniMapMarkerIndexTest::RunTest(const FString& Parameters)
{
    FRPGMiniMapMarkerIndex Index;
    const FGuid A(1, 0, 0, 0), B(2, 0, 0, 0), C(3, 0, 0, 0);
    Index.Upsert(A, FVector(-1.0, -10000.0, 0.0));
    Index.Upsert(B, FVector(0.0, 0.0, 0.0));
    Index.Upsert(C, FVector(10000.0, 10000.0, 0.0));
    TArray<FGuid> Found;
    Index.Query(FVector2D::ZeroVector, 10000.0, Found);
    TestEqual(TEXT("Inclusive query across negative and positive cell boundaries"), Found.Num(), 3);
    Index.Query(FVector2D(-1.0, -10000.0), 0.0, Found);
    TestTrue(TEXT("Zero-radius query retains exact negative position"), Found.Num() == 1 && Found[0] == A);
    Index.Upsert(A, FVector(30000.0, 0.0, 0.0));
    Index.Query(FVector2D(-1.0, -10000.0), 1.0, Found);
    TestTrue(TEXT("Moving across cells removes old membership"), Found.IsEmpty());
    Index.Upsert(A, FVector(30001.0, 0.0, 0.0));
    Index.Query(FVector2D(30001.0, 0.0), 0.0, Found);
    TestTrue(TEXT("Moving inside a cell updates exact coordinates"), Found.Num() == 1 && Found[0] == A);
    Index.Upsert(A, FVector(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0));
    TestEqual(TEXT("Invalid position removes stale membership"), Index.Num(), 2);
    Index.Remove(B);
    Index.Remove(B);
    Index.Query(FVector2D::ZeroVector, 1.e12, Found);
    TestTrue(TEXT("Huge query visits occupied cells without truncation"), Found.Num() == 1 && Found[0] == C);
    Index.Reset();
    TestEqual(TEXT("Reset clears all entries"), Index.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiniMapMarkerIndexScalingTest, "IronicRPG.RPGGameplay.MiniMap.SpatialIndexSparseAndDenseCandidates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMiniMapMarkerIndexScalingTest::RunTest(const FString& Parameters)
{
    FRPGMiniMapMarkerIndex Index;
    for (int32 I = 0; I < 10000; ++I) { Index.Upsert(FGuid(I + 1, 0, 0, 0), FVector(I * 20000.0, 0.0, 0.0)); }
    TArray<FGuid> Found;
    Index.Query(FVector2D::ZeroVector, 12000.0, Found);
    TestEqual(TEXT("Ten thousand sparse points yield one nearby candidate"), Found.Num(), 1);
    for (int32 I = 0; I < 10000; ++I) { Index.Upsert(FGuid(I + 1, 0, 0, 0), FVector::ZeroVector); }
    Index.Query(FVector2D::ZeroVector, 12000.0, Found);
    TestEqual(TEXT("Dense worst case does not silently cap candidates"), Found.Num(), 10000);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

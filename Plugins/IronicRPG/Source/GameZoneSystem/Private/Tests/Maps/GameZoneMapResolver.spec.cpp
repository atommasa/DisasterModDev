// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Maps/GameZoneMapResolver.h"

#include "Misc/AutomationTest.h"

namespace
{
    FGameZoneMapLayer MakeLayer(const FName LayerId)
    {
        FGameZoneMapLayer Layer;
        Layer.LayerId = FGameZoneMapLayerId(LayerId);
        return Layer;
    }

    FGameZoneMapSheet MakeSheet(const FName SheetId, const FName LayerId)
    {
        FGameZoneMapSheet Sheet;
        Sheet.SheetId = FGameZoneMapSheetId(SheetId);
        Sheet.LayerId = FGameZoneMapLayerId(LayerId);
        return Sheet;
    }

    FGameZoneMapSheetMapping MakeMapping(
        const FName SheetId,
        const FVector& Origin,
        const FVector2D& WorldSize = FVector2D(1000.0, 1000.0),
        float WorldYaw = 0.0f)
    {
        FGameZoneMapSheetMapping Mapping;
        Mapping.SheetId = FGameZoneMapSheetId(SheetId);
        Mapping.WorldOrigin = Origin;
        Mapping.WorldSize = WorldSize;
        Mapping.WorldYaw = WorldYaw;
        return Mapping;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapResolverRegionAndDefaultTest,
    "IronicRPG.GameZone.Map.Resolver.RegionSelectionFallsBackToDefaultSheet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapResolverRegionAndDefaultTest::RunTest(const FString& Parameters)
{
    const TArray<FGameZoneMapLayer> Layers{
        MakeLayer(TEXT("Outdoor")),
        MakeLayer(TEXT("Interior"))};
    const TArray<FGameZoneMapSheet> Sheets{
        MakeSheet(TEXT("Outdoor_Main"), TEXT("Outdoor")),
        MakeSheet(TEXT("Interior_1F"), TEXT("Interior"))};
    const TArray<FGameZoneMapSheetMapping> Mappings{
        MakeMapping(TEXT("Outdoor_Main"), FVector::ZeroVector),
        MakeMapping(TEXT("Interior_1F"), FVector(100.0, 100.0, 0.0))};

    FGameZoneMapRegion InteriorRegion;
    InteriorRegion.RegionId = FGameZoneMapRegionId(TEXT("InteriorRoom"));
    InteriorRegion.SheetId = FGameZoneMapSheetId(TEXT("Interior_1F"));
    InteriorRegion.Priority = 10;
    InteriorRegion.Bounds = FBox(
        FVector(50.0, 50.0, -100.0),
        FVector(150.0, 150.0, 100.0));

    const FGameZoneMapResolver Resolver(
        FRPGId(TEXT("z.TestZone")),
        Layers,
        Sheets,
        Mappings,
        TArray<FGameZoneMapRegion>{InteriorRegion},
        FGameZoneMapSheetId(TEXT("Outdoor_Main")),
        FGuid(1, 2, 3, 4));

    FResolvedGameZoneMapSheet Interior;
    TestTrue(
        TEXT("A location inside the region resolves"),
        Resolver.TryResolveAtLocation(FVector(100.0, 100.0, 0.0), {}, Interior));
    TestEqual(
        TEXT("The matching region selects its sheet"),
        Interior.SheetId,
        FGameZoneMapSheetId(TEXT("Interior_1F")));
    TestEqual(
        TEXT("The resolved sheet exposes its world origin"),
        Interior.WorldOrigin,
        FVector(100.0, 100.0, 0.0));
    TestEqual(
        TEXT("The resolved sheet includes the query projection"),
        Interior.QueryProjection.UV,
        FVector2D(0.5, 0.5));

    FResolvedGameZoneMapSheet Outdoor;
    TestTrue(
        TEXT("A location outside every region resolves through the default"),
        Resolver.TryResolveAtLocation(FVector(-200.0, -200.0, 0.0), {}, Outdoor));
    TestEqual(
        TEXT("No matching region selects the default sheet"),
        Outdoor.SheetId,
        FGameZoneMapSheetId(TEXT("Outdoor_Main")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapResolverLayerSelectionTest,
    "IronicRPG.GameZone.Map.Resolver.LayerSelectionUsesContainingThenStableSheet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapResolverLayerSelectionTest::RunTest(const FString& Parameters)
{
    const TArray<FGameZoneMapLayer> Layers{MakeLayer(TEXT("Outdoor"))};

    FGameZoneMapSheet East = MakeSheet(TEXT("East"), TEXT("Outdoor"));
    East.SortOrder = 20;
    FGameZoneMapSheet West = MakeSheet(TEXT("West"), TEXT("Outdoor"));
    West.SortOrder = 10;

    const FGameZoneMapResolver Resolver(
        FRPGId(TEXT("z.TestZone")),
        Layers,
        TArray<FGameZoneMapSheet>{East, West},
        TArray<FGameZoneMapSheetMapping>{
            MakeMapping(TEXT("East"), FVector(1000.0, 0.0, 0.0)),
            MakeMapping(TEXT("West"), FVector(-1000.0, 0.0, 0.0))},
        {},
        FGameZoneMapSheetId(TEXT("West")),
        FGuid());

    FResolvedGameZoneMapSheet Resolved;
    TestTrue(
        TEXT("A Layer resolves the Sheet containing the focus location"),
        Resolver.TryResolveInLayerAtLocation(
            FGameZoneMapLayerId(TEXT("Outdoor")),
            FVector(1000.0, 0.0, 0.0),
            Resolved));
    TestEqual(
        TEXT("The containing Sheet wins regardless of authored order"),
        Resolved.SheetId,
        FGameZoneMapSheetId(TEXT("East")));

    TestTrue(
        TEXT("A Layer still resolves when the focus is outside every Sheet"),
        Resolver.TryResolveInLayerAtLocation(
            FGameZoneMapLayerId(TEXT("Outdoor")),
            FVector(5000.0, 0.0, 0.0),
            Resolved));
    TestEqual(
        TEXT("Outside focus falls back to the stable first Sheet"),
        Resolved.SheetId,
        FGameZoneMapSheetId(TEXT("West")));
    TestFalse(
        TEXT("An unknown Layer is rejected"),
        Resolver.TryResolveInLayerAtLocation(
            FGameZoneMapLayerId(TEXT("Missing")),
            FVector::ZeroVector,
            Resolved));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapResolverAmbiguousRegionTest,
    "IronicRPG.GameZone.Map.Resolver.AmbiguousRegionsFailResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapResolverAmbiguousRegionTest::RunTest(const FString& Parameters)
{
    const TArray<FGameZoneMapLayer> Layers{MakeLayer(TEXT("Layer"))};
    const TArray<FGameZoneMapSheet> Sheets{
        MakeSheet(TEXT("Default"), TEXT("Layer")),
        MakeSheet(TEXT("First"), TEXT("Layer")),
        MakeSheet(TEXT("Second"), TEXT("Layer"))};
    const TArray<FGameZoneMapSheetMapping> Mappings{
        MakeMapping(TEXT("Default"), FVector::ZeroVector),
        MakeMapping(TEXT("First"), FVector::ZeroVector),
        MakeMapping(TEXT("Second"), FVector::ZeroVector)};

    FGameZoneMapRegion FirstRegion;
    FirstRegion.RegionId = FGameZoneMapRegionId(TEXT("FirstRegion"));
    FirstRegion.SheetId = FGameZoneMapSheetId(TEXT("First"));
    FirstRegion.Priority = 5;
    FirstRegion.Bounds = FBox(FVector(-100.0), FVector(100.0));

    FGameZoneMapRegion SecondRegion = FirstRegion;
    SecondRegion.RegionId = FGameZoneMapRegionId(TEXT("SecondRegion"));
    SecondRegion.SheetId = FGameZoneMapSheetId(TEXT("Second"));

    const FGameZoneMapResolver Resolver(
        FRPGId(TEXT("z.TestZone")),
        Layers,
        Sheets,
        Mappings,
        TArray<FGameZoneMapRegion>{FirstRegion, SecondRegion},
        FGameZoneMapSheetId(TEXT("Default")),
        FGuid(1, 2, 3, 4));

    FResolvedGameZoneMapSheet Resolved;
    TestFalse(
        TEXT("An ambiguous region match does not silently use the default sheet"),
        Resolver.TryResolveAtLocation(FVector::ZeroVector, {}, Resolved));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapResolverRegionPrecedenceTest,
    "IronicRPG.GameZone.Map.Resolver.RegionPriorityThenSpecificityWins",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapResolverRegionPrecedenceTest::RunTest(const FString& Parameters)
{
    const TArray<FGameZoneMapLayer> Layers{MakeLayer(TEXT("Layer"))};
    const TArray<FGameZoneMapSheet> Sheets{
        MakeSheet(TEXT("Default"), TEXT("Layer")),
        MakeSheet(TEXT("HighPriority"), TEXT("Layer")),
        MakeSheet(TEXT("Specific"), TEXT("Layer")),
        MakeSheet(TEXT("Broad"), TEXT("Layer"))};
    const TArray<FGameZoneMapSheetMapping> Mappings{
        MakeMapping(TEXT("Default"), FVector::ZeroVector),
        MakeMapping(TEXT("HighPriority"), FVector::ZeroVector),
        MakeMapping(TEXT("Specific"), FVector::ZeroVector),
        MakeMapping(TEXT("Broad"), FVector::ZeroVector)};

    FGameZoneMapRegion HighPriority;
    HighPriority.RegionId = FGameZoneMapRegionId(TEXT("HighPriorityRegion"));
    HighPriority.SheetId = FGameZoneMapSheetId(TEXT("HighPriority"));
    HighPriority.Priority = 10;
    HighPriority.Bounds = FBox(FVector(-200.0), FVector(200.0));

    FGameZoneMapRegion Specific;
    Specific.RegionId = FGameZoneMapRegionId(TEXT("SpecificRegion"));
    Specific.SheetId = FGameZoneMapSheetId(TEXT("Specific"));
    Specific.Priority = 5;
    Specific.Bounds = FBox(FVector(-25.0), FVector(25.0));

    FGameZoneMapResolver PriorityResolver(
        FRPGId(TEXT("z.TestZone")),
        Layers,
        Sheets,
        Mappings,
        TArray<FGameZoneMapRegion>{Specific, HighPriority},
        FGameZoneMapSheetId(TEXT("Default")),
        FGuid());

    FResolvedGameZoneMapSheet Resolved;
    TestTrue(
        TEXT("Overlapping regions resolve by priority"),
        PriorityResolver.TryResolveAtLocation(FVector::ZeroVector, {}, Resolved));
    TestEqual(
        TEXT("Higher priority wins even when its bounds are larger"),
        Resolved.SheetId,
        FGameZoneMapSheetId(TEXT("HighPriority")));

    FGameZoneMapRegion Broad = HighPriority;
    Broad.RegionId = FGameZoneMapRegionId(TEXT("BroadRegion"));
    Broad.SheetId = FGameZoneMapSheetId(TEXT("Broad"));
    Broad.Priority = 5;

    FGameZoneMapResolver SpecificityResolver(
        FRPGId(TEXT("z.TestZone")),
        Layers,
        Sheets,
        Mappings,
        TArray<FGameZoneMapRegion>{Broad, Specific},
        FGameZoneMapSheetId(TEXT("Default")),
        FGuid());

    TestTrue(
        TEXT("Equal-priority overlapping regions resolve by specificity"),
        SpecificityResolver.TryResolveAtLocation(FVector::ZeroVector, {}, Resolved));
    TestEqual(
        TEXT("Smaller bounds win when priority is equal"),
        Resolved.SheetId,
        FGameZoneMapSheetId(TEXT("Specific")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapResolverOverrideTest,
    "IronicRPG.GameZone.Map.Resolver.OverridePriorityAndSequenceWins",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapResolverOverrideTest::RunTest(const FString& Parameters)
{
    const TArray<FGameZoneMapLayer> Layers{MakeLayer(TEXT("Layer"))};
    const TArray<FGameZoneMapSheet> Sheets{
        MakeSheet(TEXT("Default"), TEXT("Layer")),
        MakeSheet(TEXT("Region"), TEXT("Layer")),
        MakeSheet(TEXT("OlderOverride"), TEXT("Layer")),
        MakeSheet(TEXT("LatestOverride"), TEXT("Layer"))};
    const TArray<FGameZoneMapSheetMapping> Mappings{
        MakeMapping(TEXT("Default"), FVector::ZeroVector),
        MakeMapping(TEXT("Region"), FVector::ZeroVector),
        MakeMapping(TEXT("OlderOverride"), FVector::ZeroVector),
        MakeMapping(TEXT("LatestOverride"), FVector::ZeroVector)};

    FGameZoneMapRegion Region;
    Region.RegionId = FGameZoneMapRegionId(TEXT("Region"));
    Region.SheetId = FGameZoneMapSheetId(TEXT("Region"));
    Region.Priority = 100;
    Region.Bounds = FBox(FVector(-100.0), FVector(100.0));

    const FGameZoneMapResolver Resolver(
        FRPGId(TEXT("z.TestZone")),
        Layers,
        Sheets,
        Mappings,
        TArray<FGameZoneMapRegion>{Region},
        FGameZoneMapSheetId(TEXT("Default")),
        FGuid());

    FGameZoneMapSheetOverride OlderOverride;
    OlderOverride.SheetId = FGameZoneMapSheetId(TEXT("OlderOverride"));
    OlderOverride.Priority = 5;
    OlderOverride.Sequence = 10;

    FGameZoneMapSheetOverride LatestOverride;
    LatestOverride.SheetId = FGameZoneMapSheetId(TEXT("LatestOverride"));
    LatestOverride.Priority = 5;
    LatestOverride.Sequence = 20;

    FResolvedGameZoneMapSheet Resolved;
    TestTrue(
        TEXT("A valid override resolves before a matching region"),
        Resolver.TryResolveAtLocation(
            FVector::ZeroVector,
            TArray<FGameZoneMapSheetOverride>{OlderOverride, LatestOverride},
            Resolved));
    TestEqual(
        TEXT("Latest override wins when priority is equal"),
        Resolved.SheetId,
        FGameZoneMapSheetId(TEXT("LatestOverride")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapResolverInvalidSnapshotTest,
    "IronicRPG.GameZone.Map.Resolver.DuplicateSheetInvalidatesSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapResolverInvalidSnapshotTest::RunTest(const FString& Parameters)
{
    const TArray<FGameZoneMapLayer> Layers{MakeLayer(TEXT("Layer"))};
    const FGameZoneMapSheet Sheet = MakeSheet(TEXT("Duplicate"), TEXT("Layer"));
    const TArray<FGameZoneMapSheetMapping> Mappings{
        MakeMapping(TEXT("Duplicate"), FVector::ZeroVector)};

    const FGameZoneMapResolver Resolver(
        FRPGId(TEXT("z.TestZone")),
        Layers,
        TArray<FGameZoneMapSheet>{Sheet, Sheet},
        Mappings,
        {},
        FGameZoneMapSheetId(TEXT("Duplicate")),
        FGuid());

    TestFalse(TEXT("A snapshot with duplicate SheetIds is invalid"), Resolver.IsValid());

    FResolvedGameZoneMapSheet Resolved;
    TestFalse(
        TEXT("An invalid snapshot cannot resolve a location"),
        Resolver.TryResolveAtLocation(FVector::ZeroVector, {}, Resolved));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapResolverWorldBoundsTest,
    "IronicRPG.GameZone.Map.Resolver.WorldBoundsUnionsRotatedSheetsAcrossLayers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapResolverWorldBoundsTest::RunTest(const FString& Parameters)
{
    const TArray<FGameZoneMapLayer> Layers{
        MakeLayer(TEXT("Surface")),
        MakeLayer(TEXT("Underground"))};
    const TArray<FGameZoneMapSheet> Sheets{
        MakeSheet(TEXT("SurfaceMain"), TEXT("Surface")),
        MakeSheet(TEXT("UndergroundEast"), TEXT("Underground")),
        MakeSheet(TEXT("Invalid"), TEXT("Underground"))};
    const FGuid BakeRevision(1, 2, 3, 4);
    const FGameZoneMapResolver Resolver(
        FRPGId(TEXT("z.TestZone")),
        Layers,
        Sheets,
        TArray<FGameZoneMapSheetMapping>{
            MakeMapping(TEXT("SurfaceMain"), FVector(100.0, 200.0, 0.0), FVector2D(400.0, 200.0)),
            MakeMapping(TEXT("UndergroundEast"), FVector(1000.0, -500.0, 0.0), FVector2D(200.0, 400.0), 90.0f),
            MakeMapping(TEXT("Invalid"), FVector(100000.0, 100000.0, 0.0), FVector2D::ZeroVector)},
        {},
        FGameZoneMapSheetId(TEXT("SurfaceMain")),
        BakeRevision);

    const FGameZoneMapWorldBounds& Bounds = Resolver.GetWorldBounds();
    TestTrue(TEXT("At least one valid mapping produces valid world bounds"), Bounds.bIsValid);
    TestEqual(TEXT("World bounds retain the Zone identity"), Bounds.ZoneId, FRPGId(TEXT("z.TestZone")));
    TestEqual(TEXT("World bounds retain the bake revision"), Bounds.MapBakeRevision, BakeRevision);
    TestTrue(
        TEXT("The union minimum includes the unrotated and rotated Sheet corners"),
        Bounds.MinimumWorldXY.Equals(FVector2D(-100.0, -600.0), UE_KINDA_SMALL_NUMBER));
    TestTrue(
        TEXT("The union maximum excludes the invalid Mapping"),
        Bounds.MaximumWorldXY.Equals(FVector2D(1200.0, 300.0), UE_KINDA_SMALL_NUMBER));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapResolverInvalidWorldBoundsTest,
    "IronicRPG.GameZone.Map.Resolver.WorldBoundsRemainInvalidWithoutValidMappings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapResolverInvalidWorldBoundsTest::RunTest(const FString& Parameters)
{
    const FGuid BakeRevision(5, 6, 7, 8);
    const FGameZoneMapResolver Resolver(
        FRPGId(TEXT("z.EmptyMap")),
        TArray<FGameZoneMapLayer>{MakeLayer(TEXT("Layer"))},
        TArray<FGameZoneMapSheet>{MakeSheet(TEXT("Invalid"), TEXT("Layer"))},
        TArray<FGameZoneMapSheetMapping>{
            MakeMapping(TEXT("Invalid"), FVector::ZeroVector, FVector2D(1000.0, 0.0))},
        {},
        FGameZoneMapSheetId(TEXT("Invalid")),
        BakeRevision);

    const FGameZoneMapWorldBounds& Bounds = Resolver.GetWorldBounds();
    TestFalse(TEXT("No valid Mapping leaves world bounds invalid"), Bounds.bIsValid);
    TestEqual(TEXT("Invalid bounds still identify their Zone"), Bounds.ZoneId, FRPGId(TEXT("z.EmptyMap")));
    TestEqual(TEXT("Invalid bounds still retain the bake revision"), Bounds.MapBakeRevision, BakeRevision);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

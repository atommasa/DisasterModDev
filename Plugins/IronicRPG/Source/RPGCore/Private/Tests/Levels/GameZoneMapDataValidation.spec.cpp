// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Levels/GameZoneMapDataValidation.h"

#include "Engine/Texture2D.h"
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapAssetStableIdValidationTest,
    "IronicRPG.GameZone.Map.AssetValidation.StableIdsCannotBeNoneOrDuplicated",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapAssetStableIdValidationTest::RunTest(const FString& Parameters)
{
    FDataValidationContext NoneLayerContext;
    TestEqual(
        TEXT("A None LayerId invalidates map data"),
        ValidateGameZoneMapAssetData(
            TArray<FGameZoneMapLayer>{MakeLayer(NAME_None)},
            {},
            {},
            {},
            {},
            NoneLayerContext),
        EDataValidationResult::Invalid);

    const TArray<FGameZoneMapLayer> Layers{MakeLayer(TEXT("Outdoor"))};
    const FGameZoneMapSheet Sheet = MakeSheet(TEXT("Outdoor_Main"), TEXT("Outdoor"));

    FDataValidationContext DuplicateSheetContext;
    TestEqual(
        TEXT("Duplicate SheetIds invalidate map data"),
        ValidateGameZoneMapAssetData(
            Layers,
            TArray<FGameZoneMapSheet>{Sheet, Sheet},
            FGameZoneMapSheetId(TEXT("Outdoor_Main")),
            {},
            {},
            DuplicateSheetContext),
        EDataValidationResult::Invalid);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapAssetLayerIdValidationTest,
    "IronicRPG.GameZone.Map.AssetValidation.LayerIdsMustBeUnique",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapAssetLayerIdValidationTest::RunTest(const FString& Parameters)
{
    FDataValidationContext EmptyContext;
    TestEqual(
        TEXT("A zone without map data remains valid"),
        ValidateGameZoneMapAssetData({}, {}, {}, {}, {}, EmptyContext),
        EDataValidationResult::Valid);

    FGameZoneMapLayer FirstLayer;
    FirstLayer.LayerId = FGameZoneMapLayerId(TEXT("Outdoor"));

    FGameZoneMapLayer DuplicateLayer = FirstLayer;
    DuplicateLayer.DisplayName = FText::FromString(TEXT("Duplicate"));

    FDataValidationContext DuplicateContext;
    TestEqual(
        TEXT("Duplicate LayerIds invalidate the zone map data"),
        ValidateGameZoneMapAssetData(
            TArray<FGameZoneMapLayer>{FirstLayer, DuplicateLayer},
            {},
            {},
            {},
            {},
            DuplicateContext),
        EDataValidationResult::Invalid);
    TestEqual(TEXT("Duplicate LayerIds emit one error"), DuplicateContext.GetNumErrors(), 1u);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapAssetReferenceValidationTest,
    "IronicRPG.GameZone.Map.AssetValidation.IdsAndReferencesMustResolve",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapAssetReferenceValidationTest::RunTest(const FString& Parameters)
{
    const TArray<FGameZoneMapLayer> Layers{MakeLayer(TEXT("Outdoor"))};
    const TArray<FGameZoneMapSheet> Sheets{MakeSheet(TEXT("Outdoor_Main"), TEXT("MissingLayer"))};

    FDataValidationContext InvalidReferenceContext;
    TestEqual(
        TEXT("A Sheet cannot reference a missing Layer"),
        ValidateGameZoneMapAssetData(
            Layers,
            Sheets,
            FGameZoneMapSheetId(TEXT("MissingDefault")),
            {},
            {},
            InvalidReferenceContext),
        EDataValidationResult::Invalid);
    TestEqual(
        TEXT("The invalid Layer and Default Sheet references each emit an error"),
        InvalidReferenceContext.GetNumErrors(),
        2u);

    FDataValidationContext ValidContext;
    TestEqual(
        TEXT("A Sheet with valid references passes authored-data validation"),
        ValidateGameZoneMapAssetData(
            Layers,
            TArray<FGameZoneMapSheet>{MakeSheet(TEXT("Outdoor_Main"), TEXT("Outdoor"))},
            FGameZoneMapSheetId(TEXT("Outdoor_Main")),
            {},
            {},
            ValidContext),
        EDataValidationResult::Valid);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapAssetMappingAndTextureValidationTest,
    "IronicRPG.GameZone.Map.AssetValidation.MappingAndTextureMustAgree",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapAssetMappingAndTextureValidationTest::RunTest(const FString& Parameters)
{
    const TArray<FGameZoneMapLayer> Layers{MakeLayer(TEXT("Outdoor"))};
    FGameZoneMapSheet Sheet = MakeSheet(TEXT("Outdoor_Main"), TEXT("Outdoor"));

    FGameZoneMapSheetMapping Mapping;
    Mapping.SheetId = Sheet.SheetId;
    Mapping.WorldSize = FVector2D(2000.0, 1000.0);

    FDataValidationContext MissingTextureContext;
    TestEqual(
        TEXT("A missing dedicated Texture remains valid because presentation can use the fallback"),
        ValidateGameZoneMapAssetData(
            Layers,
            TArray<FGameZoneMapSheet>{Sheet},
            Sheet.SheetId,
            TArray<FGameZoneMapSheetMapping>{Mapping},
            {},
            MissingTextureContext),
        EDataValidationResult::Valid);
    TestEqual(TEXT("A missing dedicated Texture emits one warning"), MissingTextureContext.GetNumWarnings(), 1u);

    UTexture2D* Texture = UTexture2D::CreateTransient(1000, 1000);
    Sheet.MapTexture = Texture;

    FDataValidationContext AspectContext;
    TestEqual(
        TEXT("A dedicated Texture with a different aspect ratio invalidates map data"),
        ValidateGameZoneMapAssetData(
            Layers,
            TArray<FGameZoneMapSheet>{Sheet},
            Sheet.SheetId,
            TArray<FGameZoneMapSheetMapping>{Mapping},
            {},
            AspectContext),
        EDataValidationResult::Invalid);

    FDataValidationContext DuplicateMappingContext;
    TestEqual(
        TEXT("Multiple Mappings for one Sheet invalidate baked map data"),
        ValidateGameZoneMapAssetData(
            Layers,
            TArray<FGameZoneMapSheet>{Sheet},
            Sheet.SheetId,
            TArray<FGameZoneMapSheetMapping>{Mapping, Mapping},
            {},
            DuplicateMappingContext),
        EDataValidationResult::Invalid);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapBakeTopologyValidationTest,
    "IronicRPG.GameZone.Map.AssetValidation.BakeRequiresOneNonOverlappingMappingPerSheet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapBakeTopologyValidationTest::RunTest(const FString& Parameters)
{
    const TArray<FGameZoneMapLayer> Layers{MakeLayer(TEXT("Outdoor"))};
    const TArray<FGameZoneMapSheet> Sheets{
        MakeSheet(TEXT("West"), TEXT("Outdoor")),
        MakeSheet(TEXT("East"), TEXT("Outdoor"))};

    FGameZoneMapSheetMapping WestMapping;
    WestMapping.SheetId = FGameZoneMapSheetId(TEXT("West"));
    WestMapping.WorldSize = FVector2D(1000.0, 1000.0);

    FDataValidationContext MissingMappingContext;
    TestEqual(
        TEXT("Every authored Sheet requires exactly one Mapping at Bake time"),
        ValidateGameZoneMapBakeData(
            Layers,
            Sheets,
            FGameZoneMapSheetId(TEXT("West")),
            TArray<FGameZoneMapSheetMapping>{WestMapping},
            {},
            MissingMappingContext),
        EDataValidationResult::Invalid);

    FGameZoneMapSheetMapping OverlappingEastMapping = WestMapping;
    OverlappingEastMapping.SheetId = FGameZoneMapSheetId(TEXT("East"));

    FDataValidationContext OverlapContext;
    TestEqual(
        TEXT("Mappings in one Layer cannot overlap"),
        ValidateGameZoneMapBakeData(
            Layers,
            Sheets,
            FGameZoneMapSheetId(TEXT("West")),
            TArray<FGameZoneMapSheetMapping>{WestMapping, OverlappingEastMapping},
            {},
            OverlapContext),
        EDataValidationResult::Invalid);

    FGameZoneMapSheetMapping AdjacentEastMapping = OverlappingEastMapping;
    AdjacentEastMapping.WorldOrigin.X = 1000.0;

    FDataValidationContext AdjacentContext;
    TestEqual(
        TEXT("Mappings that only share an edge remain valid"),
        ValidateGameZoneMapBakeData(
            Layers,
            Sheets,
            FGameZoneMapSheetId(TEXT("West")),
            TArray<FGameZoneMapSheetMapping>{WestMapping, AdjacentEastMapping},
            {},
            AdjacentContext),
        EDataValidationResult::Valid);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

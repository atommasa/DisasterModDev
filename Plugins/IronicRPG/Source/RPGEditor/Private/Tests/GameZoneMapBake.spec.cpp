// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Levels/GameZoneAsset.h"
#include "Levels/GameZoneMapRegionVolume.h"
#include "Levels/GameZoneMapSheetBounds.h"
#include "Levels/GameZonePointComponent.h"
#include "Levels/RPGWorldSettings.h"

#include "Algo/Reverse.h"
#include "Components/BoxComponent.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "UObject/UnrealType.h"

namespace
{
    template <typename ValueType>
    ValueType& GetPropertyValue(UObject& Object, const FName PropertyName)
    {
        FProperty* Property = FindFProperty<FProperty>(Object.GetClass(), PropertyName);
        check(Property);
        return *Property->ContainerPtrToValuePtr<ValueType>(&Object);
    }

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

    bool BindForBake(UGameZoneAsset& ZoneAsset, UWorld& World)
    {
        ARPGWorldSettings* Settings = Cast<ARPGWorldSettings>(World.GetWorldSettings());
        if (!Settings)
        {
            return false;
        }

        const FGuid BindingId = FGuid::NewGuid();
        GetPropertyValue<FGuid>(ZoneAsset, TEXT("GameZoneBindingId")) = BindingId;
        GetPropertyValue<FRPGId>(*Settings, TEXT("GameZoneId")) = ZoneAsset.GetId();
        GetPropertyValue<FGuid>(*Settings, TEXT("GameZoneBindingId")) = BindingId;
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapBakeAtomicCommitTest,
    "IronicRPG.GameZone.Map.Bake.InvalidCandidatePreservesPreviousSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapBakeAtomicCommitTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    TestNotNull(TEXT("A transient editor World is created"), World);
    if (!World)
    {
        return false;
    }

    UGameZoneAsset* ZoneAsset = NewObject<UGameZoneAsset>();
    TestNotNull(TEXT("A transient Zone Asset is created"), ZoneAsset);
    if (!ZoneAsset)
    {
        return false;
    }

    GetPropertyValue<FRPGId>(*ZoneAsset, TEXT("Id")) = FRPGId(TEXT("z.BakeTest"));
    GetPropertyValue<TSoftObjectPtr<UWorld>>(*ZoneAsset, TEXT("LevelToLoad")) = World;
    GetPropertyValue<TArray<FGameZoneMapLayer>>(*ZoneAsset, TEXT("MapLayers")) = {
        MakeLayer(TEXT("Outdoor"))};
    GetPropertyValue<TArray<FGameZoneMapSheet>>(*ZoneAsset, TEXT("MapSheets")) = {
        MakeSheet(TEXT("Outdoor_Main"), TEXT("Outdoor"))};
    GetPropertyValue<FGameZoneMapSheetId>(*ZoneAsset, TEXT("DefaultSheetId")) =
        FGameZoneMapSheetId(TEXT("Outdoor_Main"));
    TestTrue(TEXT("The Bake fixture has an exact Level binding"), BindForBake(*ZoneAsset, *World));

    AGameZoneMapSheetBounds* Bounds = World->SpawnActor<AGameZoneMapSheetBounds>();
    TestNotNull(TEXT("A Sheet Bounds actor is spawned"), Bounds);
    if (!Bounds)
    {
        return false;
    }

    Bounds->SheetId = FGameZoneMapSheetId(TEXT("Outdoor_Main"));
    Bounds->SetActorLocation(FVector(1000.0, 2000.0, 300.0));
    Bounds->SetActorRotation(FRotator(0.0, 30.0, 0.0));
    Bounds->GetBoundsComponent()->SetBoxExtent(FVector(500.0, 250.0, 100.0));

    ZoneAsset->BakeMapData();

    TestEqual(TEXT("A successful Bake commits one Mapping"), ZoneAsset->GetBakedSheetMappings().Num(), 1);
    TestEqual(
        TEXT("The Mapping uses the Bounds Actor location"),
        ZoneAsset->GetBakedSheetMappings()[0].WorldOrigin,
        FVector(1000.0, 2000.0, 300.0));
    TestEqual(
        TEXT("Box half extents are converted to full Mapping size"),
        ZoneAsset->GetBakedSheetMappings()[0].WorldSize,
        FVector2D(1000.0, 500.0));
    TestTrue(TEXT("A successful Bake creates a revision"), ZoneAsset->GetMapBakeRevision().IsValid());
    TestEqual(TEXT("A successful Bake records the current schema version"), ZoneAsset->GetMapDataSchemaVersion(), 2);

    const FGuid SuccessfulRevision = ZoneAsset->GetMapBakeRevision();
    const FGameZoneMapSheetMapping SuccessfulMapping = ZoneAsset->GetBakedSheetMappings()[0];
    const FString SuccessfulInputFingerprint = ZoneAsset->GetMapBakeInputFingerprint();
    const FString SuccessfulOutputFingerprint = ZoneAsset->GetMapBakeOutputFingerprint();

    ZoneAsset->GetOutermost()->SetDirtyFlag(false);
    const EGameZoneMapBakeResult NoChangeResult = ZoneAsset->BakeMapDataWithResult();
    TestEqual(TEXT("An identical Bake is a no-op"), NoChangeResult, EGameZoneMapBakeResult::NoChange);
    TestEqual(TEXT("A no-op Bake preserves the revision"), ZoneAsset->GetMapBakeRevision(), SuccessfulRevision);
    TestFalse(TEXT("A no-op Bake does not dirty the Asset package"), ZoneAsset->GetOutermost()->IsDirty());

    AddExpectedError(TEXT("dirty or has never been saved"), EAutomationExpectedErrorFlags::Contains, 1);
    TestEqual(
        TEXT("A persistent Bake rejects an unsaved Level source"),
        ZoneAsset->BakeMapDataWithResult(true),
        EGameZoneMapBakeResult::Failed);
    TestEqual(
        TEXT("Rejecting an unsaved source preserves the revision"),
        ZoneAsset->GetMapBakeRevision(),
        SuccessfulRevision);

    AGameZoneMapSheetBounds* DuplicateBounds = World->SpawnActor<AGameZoneMapSheetBounds>();
    TestNotNull(TEXT("A duplicate Sheet Bounds actor is spawned"), DuplicateBounds);
    if (!DuplicateBounds)
    {
        return false;
    }

    DuplicateBounds->SheetId = Bounds->SheetId;
    DuplicateBounds->GetBoundsComponent()->SetBoxExtent(FVector(100.0));

    AddExpectedError(TEXT("multiple Bounds"), EAutomationExpectedErrorFlags::Contains, 1);
    ZoneAsset->BakeMapData();

    TestEqual(
        TEXT("A failed Bake preserves the successful revision"),
        ZoneAsset->GetMapBakeRevision(),
        SuccessfulRevision);
    TestEqual(TEXT("A failed Bake preserves the Mapping count"), ZoneAsset->GetBakedSheetMappings().Num(), 1);
    TestEqual(
        TEXT("A failed Bake preserves the previous Mapping origin"),
        ZoneAsset->GetBakedSheetMappings()[0].WorldOrigin,
        SuccessfulMapping.WorldOrigin);
    TestEqual(
        TEXT("A failed Bake preserves the previous Mapping size"),
        ZoneAsset->GetBakedSheetMappings()[0].WorldSize,
        SuccessfulMapping.WorldSize);
    TestEqual(
        TEXT("A failed Bake preserves the input fingerprint"),
        ZoneAsset->GetMapBakeInputFingerprint(),
        SuccessfulInputFingerprint);
    TestEqual(
        TEXT("A failed Bake preserves the output fingerprint"),
        ZoneAsset->GetMapBakeOutputFingerprint(),
        SuccessfulOutputFingerprint);
    TestEqual(
        TEXT("A failed Bake is visible in verification state"),
        ZoneAsset->GetBindingVerificationStatus(),
        EGameZoneBindingVerificationStatus::BakeFailed);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapBakeRegionGeometryTest,
    "IronicRPG.GameZone.Map.Bake.ConvexRegionProducesRuntimeGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapBakeRegionGeometryTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    TestNotNull(TEXT("A transient editor World is created"), World);
    if (!World)
    {
        return false;
    }

    UGameZoneAsset* ZoneAsset = NewObject<UGameZoneAsset>();
    GetPropertyValue<FRPGId>(*ZoneAsset, TEXT("Id")) = FRPGId(TEXT("z.RegionBakeTest"));
    GetPropertyValue<TSoftObjectPtr<UWorld>>(*ZoneAsset, TEXT("LevelToLoad")) = World;
    GetPropertyValue<TArray<FGameZoneMapLayer>>(*ZoneAsset, TEXT("MapLayers")) = {
        MakeLayer(TEXT("Interior"))};
    GetPropertyValue<TArray<FGameZoneMapSheet>>(*ZoneAsset, TEXT("MapSheets")) = {
        MakeSheet(TEXT("Interior_1F"), TEXT("Interior"))};
    GetPropertyValue<FGameZoneMapSheetId>(*ZoneAsset, TEXT("DefaultSheetId")) =
        FGameZoneMapSheetId(TEXT("Interior_1F"));
    TestTrue(TEXT("The Region fixture has an exact Level binding"), BindForBake(*ZoneAsset, *World));

    AGameZoneMapSheetBounds* Bounds = World->SpawnActor<AGameZoneMapSheetBounds>();
    Bounds->SheetId = FGameZoneMapSheetId(TEXT("Interior_1F"));
    Bounds->GetBoundsComponent()->SetBoxExtent(FVector(10000.0, 10000.0, 1000.0));

    TestTrue(
        TEXT("The Editor creates a Brush Volume through the real placement path"),
        GUnrealEd->Exec(World, TEXT("BRUSH ADDVOLUME CLASS=/Script/RPGCore.GameZoneMapRegionVolume")));

    AGameZoneMapRegionVolume* RegionVolume = nullptr;
    for (TActorIterator<AGameZoneMapRegionVolume> ActorIt(World); ActorIt; ++ActorIt)
    {
        RegionVolume = *ActorIt;
    }

    TestNotNull(TEXT("The Brush command creates a Zone Map Region"), RegionVolume);
    if (!RegionVolume)
    {
        return false;
    }

    RegionVolume->RegionId = FGameZoneMapRegionId(TEXT("InteriorRoom"));
    RegionVolume->SheetId = FGameZoneMapSheetId(TEXT("Interior_1F"));
    RegionVolume->Priority = 10;

    ZoneAsset->BakeMapData();

    TestEqual(TEXT("The convex Volume commits one Region"), ZoneAsset->GetBakedMapRegions().Num(), 1);
    if (ZoneAsset->GetBakedMapRegions().Num() != 1)
    {
        return false;
    }

    const FGameZoneMapRegion& BakedRegion = ZoneAsset->GetBakedMapRegions()[0];
    TestTrue(TEXT("The baked Region has valid broad-phase bounds"), BakedRegion.Bounds.IsValid != 0);
    TestTrue(TEXT("The baked Region has convex planes"), !BakedRegion.Planes.IsEmpty());
    TestTrue(
        TEXT("The baked Region contains its own bounds center"),
        BakedRegion.Contains(BakedRegion.Bounds.GetCenter()));

    const FGuid SuccessfulRevision = ZoneAsset->GetMapBakeRevision();
    RegionVolume->SetActorLocation(FVector(50000.0, 0.0, 0.0));

    AddExpectedError(TEXT("extends outside Sheet Mapping"), EAutomationExpectedErrorFlags::Contains, 1);
    ZoneAsset->BakeMapData();

    TestEqual(
        TEXT("An out-of-bounds Region does not replace the successful revision"),
        ZoneAsset->GetMapBakeRevision(),
        SuccessfulRevision);
    TestEqual(
        TEXT("An out-of-bounds Region preserves the previous Region snapshot"),
        ZoneAsset->GetBakedMapRegions().Num(),
        1);
    TestTrue(
        TEXT("The preserved Region remains at its previous location"),
        ZoneAsset->GetBakedMapRegions()[0].Contains(BakedRegion.Bounds.GetCenter()));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapBakeFingerprintOrderingTest,
    "IronicRPG.GameZone.Map.Bake.FingerprintIgnoresAuthoredArrayOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapBakeFingerprintOrderingTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UGameZoneAsset* ZoneAsset = NewObject<UGameZoneAsset>();
    TestNotNull(TEXT("A transient editor World is created"), World);
    TestNotNull(TEXT("A transient Zone Asset is created"), ZoneAsset);
    if (!World || !ZoneAsset)
    {
        return false;
    }

    GetPropertyValue<FRPGId>(*ZoneAsset, TEXT("Id")) = FRPGId(TEXT("z.FingerprintOrder"));
    GetPropertyValue<TSoftObjectPtr<UWorld>>(*ZoneAsset, TEXT("LevelToLoad")) = World;
    TArray<FGameZoneMapLayer>& Layers = GetPropertyValue<TArray<FGameZoneMapLayer>>(*ZoneAsset, TEXT("MapLayers"));
    Layers = {MakeLayer(TEXT("Ground")), MakeLayer(TEXT("Upper"))};
    TArray<FGameZoneMapSheet>& Sheets = GetPropertyValue<TArray<FGameZoneMapSheet>>(*ZoneAsset, TEXT("MapSheets"));
    Sheets = {MakeSheet(TEXT("Ground_Main"), TEXT("Ground")), MakeSheet(TEXT("Upper_Main"), TEXT("Upper"))};
    GetPropertyValue<FGameZoneMapSheetId>(*ZoneAsset, TEXT("DefaultSheetId")) =
        FGameZoneMapSheetId(TEXT("Ground_Main"));
    TestTrue(TEXT("The ordering fixture has an exact Level binding"), BindForBake(*ZoneAsset, *World));

    AGameZoneMapSheetBounds* GroundBounds = World->SpawnActor<AGameZoneMapSheetBounds>();
    AGameZoneMapSheetBounds* UpperBounds = World->SpawnActor<AGameZoneMapSheetBounds>();
    GroundBounds->SheetId = FGameZoneMapSheetId(TEXT("Ground_Main"));
    UpperBounds->SheetId = FGameZoneMapSheetId(TEXT("Upper_Main"));
    GroundBounds->GetBoundsComponent()->SetBoxExtent(FVector(100.0));
    UpperBounds->GetBoundsComponent()->SetBoxExtent(FVector(100.0));
    UpperBounds->SetActorLocation(FVector(1000.0, 0.0, 0.0));

    TestEqual(
        TEXT("The initial Bake succeeds"),
        ZoneAsset->BakeMapDataWithResult(),
        EGameZoneMapBakeResult::Succeeded);
    const FGuid Revision = ZoneAsset->GetMapBakeRevision();
    const FString InputFingerprint = ZoneAsset->GetMapBakeInputFingerprint();

    Algo::Reverse(Layers);
    Algo::Reverse(Sheets);
    ZoneAsset->GetOutermost()->SetDirtyFlag(false);

    TestEqual(
        TEXT("Reordering stable-Id authored arrays is a no-op"),
        ZoneAsset->BakeMapDataWithResult(),
        EGameZoneMapBakeResult::NoChange);
    TestEqual(TEXT("Reordering preserves the revision"), ZoneAsset->GetMapBakeRevision(), Revision);
    TestEqual(
        TEXT("Reordering preserves the input fingerprint"),
        ZoneAsset->GetMapBakeInputFingerprint(),
        InputFingerprint);
    TestFalse(TEXT("Reordering does not dirty the Asset package"), ZoneAsset->GetOutermost()->IsDirty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapBakeAuthoredFieldStaleTest,
    "IronicRPG.GameZone.Map.Bake.AuthoredMapChangeMarksSourceStale",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapBakeAuthoredFieldStaleTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UGameZoneAsset* ZoneAsset = NewObject<UGameZoneAsset>();
    if (!World || !ZoneAsset)
    {
        return false;
    }

    GetPropertyValue<FRPGId>(*ZoneAsset, TEXT("Id")) = FRPGId(TEXT("z.AuthoredFieldStale"));
    GetPropertyValue<TSoftObjectPtr<UWorld>>(*ZoneAsset, TEXT("LevelToLoad")) = World;
    GetPropertyValue<TArray<FGameZoneMapLayer>>(*ZoneAsset, TEXT("MapLayers")) = {MakeLayer(TEXT("Ground"))};
    GetPropertyValue<TArray<FGameZoneMapSheet>>(*ZoneAsset, TEXT("MapSheets")) = {
        MakeSheet(TEXT("Ground_Main"), TEXT("Ground"))};
    GetPropertyValue<FGameZoneMapSheetId>(*ZoneAsset, TEXT("DefaultSheetId")) =
        FGameZoneMapSheetId(TEXT("Ground_Main"));
    TestTrue(TEXT("The authored-field fixture has an exact Level binding"), BindForBake(*ZoneAsset, *World));

    AGameZoneMapSheetBounds* Bounds = World->SpawnActor<AGameZoneMapSheetBounds>();
    Bounds->SheetId = FGameZoneMapSheetId(TEXT("Ground_Main"));
    Bounds->GetBoundsComponent()->SetBoxExtent(FVector(100.0));
    TestEqual(
        TEXT("The initial Bake succeeds"),
        ZoneAsset->BakeMapDataWithResult(),
        EGameZoneMapBakeResult::Succeeded);

    FProperty* LayersProperty = FindFProperty<FProperty>(ZoneAsset->GetClass(), TEXT("MapLayers"));
    TestNotNull(TEXT("The MapLayers reflection property exists"), LayersProperty);
    if (!LayersProperty)
    {
        return false;
    }

    GetPropertyValue<TArray<FGameZoneMapLayer>>(*ZoneAsset, TEXT("MapLayers"))[0].SortOrder = 1;
    FPropertyChangedEvent PropertyChangedEvent(LayersProperty);
    static_cast<UObject*>(ZoneAsset)->PostEditChangeProperty(PropertyChangedEvent);
    TestEqual(
        TEXT("Editing authored Map data marks the verified Bake stale"),
        ZoneAsset->GetBindingVerificationStatus(),
        EGameZoneBindingVerificationStatus::SourceStale);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapBakePIEAuditTest,
    "IronicRPG.GameZone.Map.Bake.PIEAuditIsReadOnlyAndDetectsCandidateChanges",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapBakePIEAuditTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UGameZoneAsset* ZoneAsset = NewObject<UGameZoneAsset>();
    if (!World || !ZoneAsset)
    {
        return false;
    }

    GetPropertyValue<FRPGId>(*ZoneAsset, TEXT("Id")) = FRPGId(TEXT("z.PIEAudit"));
    GetPropertyValue<TSoftObjectPtr<UWorld>>(*ZoneAsset, TEXT("LevelToLoad")) = World;
    GetPropertyValue<TArray<FGameZoneMapLayer>>(*ZoneAsset, TEXT("MapLayers")) = {MakeLayer(TEXT("Ground"))};
    GetPropertyValue<TArray<FGameZoneMapSheet>>(*ZoneAsset, TEXT("MapSheets")) = {
        MakeSheet(TEXT("Ground_Main"), TEXT("Ground"))};
    GetPropertyValue<FGameZoneMapSheetId>(*ZoneAsset, TEXT("DefaultSheetId")) =
        FGameZoneMapSheetId(TEXT("Ground_Main"));
    TestTrue(TEXT("The PIE audit fixture has an exact Level binding"), BindForBake(*ZoneAsset, *World));

    AGameZoneMapSheetBounds* Bounds = World->SpawnActor<AGameZoneMapSheetBounds>();
    Bounds->SheetId = FGameZoneMapSheetId(TEXT("Ground_Main"));
    Bounds->GetBoundsComponent()->SetBoxExtent(FVector(100.0));
    TestEqual(
        TEXT("The audit fixture Bake succeeds"),
        ZoneAsset->BakeMapDataWithResult(),
        EGameZoneMapBakeResult::Succeeded);

    const FGuid Revision = ZoneAsset->GetMapBakeRevision();
    const FString InputFingerprint = ZoneAsset->GetMapBakeInputFingerprint();
    const FString OutputFingerprint = ZoneAsset->GetMapBakeOutputFingerprint();
    const FGameZoneMapBakeAuditResult CurrentAudit = ZoneAsset->AuditMapDataForPIE();
    TestTrue(TEXT("The transient source candidate can be fully evaluated"), CurrentAudit.bCanEvaluate);
    TestTrue(TEXT("The in-memory snapshot matches immediately after Bake"), CurrentAudit.bSnapshotMatches);
    TestTrue(TEXT("An unsaved transient Level is reported"), CurrentAudit.bHasUnsavedLevelChanges);
    TestFalse(TEXT("Unsaved authoring is not PIE-verified"), CurrentAudit.IsVerifiedForPIE());

    Bounds->SetActorLocation(FVector(500.0, 0.0, 0.0));
    const FGameZoneMapBakeAuditResult ChangedAudit = ZoneAsset->AuditMapDataForPIE();
    TestTrue(TEXT("The edited candidate remains fully evaluable"), ChangedAudit.bCanEvaluate);
    TestFalse(TEXT("A source geometry edit makes the existing Bake stale"), ChangedAudit.bSnapshotMatches);
    TestNotEqual(
        TEXT("A source geometry edit changes the issue fingerprint"),
        CurrentAudit.IssueFingerprint,
        ChangedAudit.IssueFingerprint);
    TestEqual(TEXT("PIE audit does not replace the Bake revision"), ZoneAsset->GetMapBakeRevision(), Revision);
    TestEqual(
        TEXT("PIE audit does not replace the stored input fingerprint"),
        ZoneAsset->GetMapBakeInputFingerprint(),
        InputFingerprint);
    TestEqual(
        TEXT("PIE audit does not replace the stored output fingerprint"),
        ZoneAsset->GetMapBakeOutputFingerprint(),
        OutputFingerprint);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMapBakeTextIdentityTest,
    "IronicRPG.GameZone.Map.Bake.TextLocalizationIdentityDoesNotInvalidateSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMapBakeTextIdentityTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UGameZoneAsset* ZoneAsset = NewObject<UGameZoneAsset>();
    if (!World || !ZoneAsset)
    {
        return false;
    }

    GetPropertyValue<FRPGId>(*ZoneAsset, TEXT("Id")) = FRPGId(TEXT("z.TextIdentity"));
    GetPropertyValue<TSoftObjectPtr<UWorld>>(*ZoneAsset, TEXT("LevelToLoad")) = World;
    TestTrue(TEXT("The text identity fixture has an exact Level binding"), BindForBake(*ZoneAsset, *World));

    AActor* PointOwner = World->SpawnActor<AActor>();
    UGameZonePointComponent* Point = NewObject<UGameZonePointComponent>(PointOwner);
    PointOwner->AddInstanceComponent(Point);
    Point->RegisterComponent();
    GetPropertyValue<bool>(*Point, TEXT("bBakeMarker")) = true;
    FGameZonePointData& PointData = GetPropertyValue<FGameZonePointData>(*Point, TEXT("PointData"));
    PointData.MarkerTypeId = FRPGId(TEXT("mm.TextIdentity"));
    PointData.DisplayName = FText::ChangeKey(
        FTextKey(TEXT("LevelPackageNamespace")),
        FTextKey(TEXT("LevelPackageKey")),
        FText::FromString(TEXT("Stable Source Text")));

    TestEqual(
        TEXT("The text identity fixture Bake succeeds"),
        ZoneAsset->BakeMapDataWithResult(),
        EGameZoneMapBakeResult::Succeeded);

    TMap<FGuid, FGameZonePointData>& BakedPoints =
        GetPropertyValue<TMap<FGuid, FGameZonePointData>>(*ZoneAsset, TEXT("BakedPoints"));
    FGameZonePointData* BakedPoint = BakedPoints.Find(Point->GetPointId());
    TestNotNull(TEXT("The fixture Point is present in the Baked snapshot"), BakedPoint);
    if (!BakedPoint)
    {
        return false;
    }

    BakedPoint->DisplayName = FText::ChangeKey(
        FTextKey(TEXT("AssetPackageNamespace")),
        FTextKey(TEXT("AssetPackageKey")),
        BakedPoint->DisplayName);

    const FGameZoneMapBakeAuditResult Audit = ZoneAsset->AuditMapDataForPIE();
    TestTrue(
        TEXT("A package-specific localization identity change preserves the Bake snapshot"),
        Audit.bSnapshotMatches);

    PointData.DisplayName = FText::ChangeKey(
        FTextKey(TEXT("LevelPackageNamespace")),
        FTextKey(TEXT("LevelPackageKey")),
        FText::FromString(TEXT("Changed Source Text")));
    const FGameZoneMapBakeAuditResult ChangedContentAudit = ZoneAsset->AuditMapDataForPIE();
    TestFalse(TEXT("Changing the authored source text invalidates the Bake snapshot"), ChangedContentAudit.bSnapshotMatches);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "MapMarkers/GameZoneMarkerRegistry.h"

#include "Levels/GameZonePointComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerRegistryBakedOnlyTest,
    "IronicRPG.GameZone.MarkerRegistry.BakedOnlyResolvesBakedSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerRegistryBakedOnlyTest::RunTest(const FString& Parameters)
{
    FGameZoneMarkerRegistry Registry;

    const FRPGId ZoneId(TEXT("z.TestZone"));
    const FGuid PointId(1, 2, 3, 4);

    FGameZonePointData BakedPoint;
    BakedPoint.PointId = PointId;
    BakedPoint.ZoneId = ZoneId;
    BakedPoint.MarkerState = EGameZonePointState::Deactivated;

    TMap<FGuid, FGameZonePointData> BakedPoints;
    BakedPoints.Add(PointId, BakedPoint);

    const FGameZoneMarkerMutationResult CommitResult =
        Registry.CommitBakedZone(ZoneId, BakedPoints);

    TestTrue(TEXT("Baked zone commit succeeds"), CommitResult.WasApplied());

    FResolvedGameZonePoint ResolvedPoint;
    TestTrue(TEXT("Committed point resolves"), Registry.TryResolve(PointId, ResolvedPoint));
    TestEqual(TEXT("Resolved point keeps its id"), ResolvedPoint.Data.PointId, PointId);
    TestEqual(
        TEXT("Resolved point keeps its baked state"),
        ResolvedPoint.Data.MarkerState,
        EGameZonePointState::Deactivated);
    TestEqual(
        TEXT("Resolved source is baked"),
        ResolvedPoint.Source,
        EGameZonePointResolvedSource::Baked);
    TestEqual(
        TEXT("Baked points are event driven"),
        ResolvedPoint.UpdateMode,
        EGameZonePointUpdateMode::EventDriven);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerRegistryLiveOnlyTest,
    "IronicRPG.GameZone.MarkerRegistry.LiveOnlyResolvesLiveSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerRegistryLiveOnlyTest::RunTest(const FString& Parameters)
{
    FGameZoneMarkerRegistry Registry;
    UGameZonePointComponent* Component = NewObject<UGameZonePointComponent>();
    Component->SetMarkerState(EGameZonePointState::Deactivated);

    const FGuid PointId = Component->GetPointId();
    const FRPGId ZoneId(TEXT("z.TestZone"));

    const FGameZoneMarkerMutationResult RegisterResult =
        Registry.RegisterLive(*Component, ZoneId);

    TestTrue(TEXT("Live point registration succeeds"), RegisterResult.WasApplied());

    FResolvedGameZonePoint ResolvedPoint;
    TestTrue(TEXT("Registered live point resolves"), Registry.TryResolve(PointId, ResolvedPoint));
    TestEqual(TEXT("Resolved live point keeps its id"), ResolvedPoint.Data.PointId, PointId);
    TestEqual(
        TEXT("Resolved live point keeps its runtime state"),
        ResolvedPoint.Data.MarkerState,
        EGameZonePointState::Deactivated);
    TestEqual(
        TEXT("Resolved source is live"),
        ResolvedPoint.Source,
        EGameZonePointResolvedSource::Live);
    TestEqual(
        TEXT("A default live component is event driven"),
        ResolvedPoint.UpdateMode,
        EGameZonePointUpdateMode::EventDriven);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerRegistryLiveFallbackTest,
    "IronicRPG.GameZone.MarkerRegistry.UnregisteringLiveFallsBackToBaked",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerRegistryLiveFallbackTest::RunTest(const FString& Parameters)
{
    FGameZoneMarkerRegistry Registry;
    const FRPGId ZoneId(TEXT("z.TestZone"));

    UGameZonePointComponent* Component = NewObject<UGameZonePointComponent>();
    Component->SetMarkerState(EGameZonePointState::Deactivated);
    const FGuid PointId = Component->GetPointId();

    FGameZonePointData BakedPoint;
    BakedPoint.PointId = PointId;
    BakedPoint.ZoneId = ZoneId;
    BakedPoint.MarkerState = EGameZonePointState::Activated;
    BakedPoint.SavePolicy = EGameZonePointSavePolicy::Saveable;

    TMap<FGuid, FGameZonePointData> BakedPoints;
    BakedPoints.Add(PointId, BakedPoint);

    Registry.CommitBakedZone(ZoneId, BakedPoints);
    Registry.RegisterLive(*Component, ZoneId);

    FResolvedGameZonePoint LivePoint;
    TestTrue(TEXT("Combined marker resolves before unregister"), Registry.TryResolve(PointId, LivePoint));
    TestEqual(
        TEXT("Live source wins while registered"),
        LivePoint.Source,
        EGameZonePointResolvedSource::Live);

    const FGameZoneMarkerMutationResult UnregisterResult = Registry.UnregisterLive(*Component);
    TestTrue(TEXT("Live unregister succeeds"), UnregisterResult.WasApplied());
    TestEqual(TEXT("Fallback emits one change"), UnregisterResult.Changes.Num(), 1);
    if (UnregisterResult.Changes.Num() == 1)
    {
        TestEqual(
            TEXT("Fallback is a Changed event"),
            UnregisterResult.Changes[0].Kind,
            EMapMarkerChangeKind::Changed);
    }

    FResolvedGameZonePoint FallbackPoint;
    TestTrue(TEXT("Marker still resolves after live unregister"), Registry.TryResolve(PointId, FallbackPoint));
    TestEqual(
        TEXT("Resolved source falls back to baked"),
        FallbackPoint.Source,
        EGameZonePointResolvedSource::Baked);
    TestEqual(
        TEXT("Fallback restores baked state"),
        FallbackPoint.Data.MarkerState,
        EGameZonePointState::Activated);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerRegistryLiveRemovalTest,
    "IronicRPG.GameZone.MarkerRegistry.UnregisteringLiveOnlyRemovesMarker",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerRegistryLiveRemovalTest::RunTest(const FString& Parameters)
{
    FGameZoneMarkerRegistry Registry;
    UGameZonePointComponent* Component = NewObject<UGameZonePointComponent>();
    const FGuid PointId = Component->GetPointId();

    Registry.RegisterLive(*Component, FRPGId(TEXT("z.TestZone")));

    const FGameZoneMarkerMutationResult UnregisterResult = Registry.UnregisterLive(*Component);
    TestTrue(TEXT("Live-only unregister succeeds"), UnregisterResult.WasApplied());
    TestEqual(TEXT("Live-only unregister emits one change"), UnregisterResult.Changes.Num(), 1);
    if (UnregisterResult.Changes.Num() == 1)
    {
        TestEqual(
            TEXT("Live-only unregister emits Removed"),
            UnregisterResult.Changes[0].Kind,
            EMapMarkerChangeKind::Removed);
    }

    FResolvedGameZonePoint RemovedPoint;
    TestFalse(TEXT("Removed live-only marker no longer resolves"), Registry.TryResolve(PointId, RemovedPoint));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerRegistryDuplicateLiveTest,
    "IronicRPG.GameZone.MarkerRegistry.DuplicateLiveRegistrationIsRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerRegistryDuplicateLiveTest::RunTest(const FString& Parameters)
{
    FGameZoneMarkerRegistry Registry;
    const FRPGId ZoneId(TEXT("z.TestZone"));

    UGameZonePointComponent* First = NewObject<UGameZonePointComponent>();
    First->SetMarkerState(EGameZonePointState::Activated);

    UGameZonePointComponent* Duplicate = DuplicateObject<UGameZonePointComponent>(
        First,
        GetTransientPackage());
    Duplicate->SetMarkerState(EGameZonePointState::Deactivated);

    Registry.RegisterLive(*First, ZoneId);
    const int64 RevisionBeforeDuplicate = Registry.GetRevision();

    const FGameZoneMarkerMutationResult DuplicateResult =
        Registry.RegisterLive(*Duplicate, ZoneId);

    TestEqual(
        TEXT("Duplicate live registration reports the collision"),
        DuplicateResult.Error,
        EGameZoneMarkerMutationError::DuplicateLiveSource);
    TestFalse(TEXT("Duplicate live registration is not applied"), DuplicateResult.WasApplied());
    TestEqual(
        TEXT("Rejected duplicate does not advance revision"),
        Registry.GetRevision(),
        RevisionBeforeDuplicate);

    FResolvedGameZonePoint ResolvedPoint;
    TestTrue(TEXT("Original live marker still resolves"), Registry.TryResolve(First->GetPointId(), ResolvedPoint));
    TestEqual(
        TEXT("Original live marker remains authoritative"),
        ResolvedPoint.Data.MarkerState,
        EGameZonePointState::Activated);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerRegistryDuplicateBakedTest,
    "IronicRPG.GameZone.MarkerRegistry.DuplicateBakedCommitIsRejectedAtomically",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerRegistryDuplicateBakedTest::RunTest(const FString& Parameters)
{
    FGameZoneMarkerRegistry Registry;
    const FGuid PointId(10, 20, 30, 40);

    FGameZonePointData OriginalPoint;
    OriginalPoint.PointId = PointId;
    OriginalPoint.ZoneId = FRPGId(TEXT("z.First"));
    OriginalPoint.MarkerState = EGameZonePointState::Activated;

    TMap<FGuid, FGameZonePointData> FirstZonePoints;
    FirstZonePoints.Add(PointId, OriginalPoint);
    Registry.CommitBakedZone(OriginalPoint.ZoneId, FirstZonePoints);

    const int64 RevisionBeforeDuplicate = Registry.GetRevision();

    FGameZonePointData DuplicatePoint = OriginalPoint;
    DuplicatePoint.ZoneId = FRPGId(TEXT("z.Second"));
    DuplicatePoint.MarkerState = EGameZonePointState::Deactivated;

    TMap<FGuid, FGameZonePointData> SecondZonePoints;
    SecondZonePoints.Add(PointId, DuplicatePoint);

    const FGameZoneMarkerMutationResult DuplicateResult =
        Registry.CommitBakedZone(DuplicatePoint.ZoneId, SecondZonePoints);

    TestEqual(
        TEXT("Duplicate baked commit reports the collision"),
        DuplicateResult.Error,
        EGameZoneMarkerMutationError::DuplicateBakedSource);
    TestFalse(TEXT("Duplicate baked commit is not applied"), DuplicateResult.WasApplied());
    TestEqual(
        TEXT("Rejected baked commit does not advance revision"),
        Registry.GetRevision(),
        RevisionBeforeDuplicate);

    FResolvedGameZonePoint ResolvedPoint;
    TestTrue(TEXT("Original baked point still resolves"), Registry.TryResolve(PointId, ResolvedPoint));
    TestEqual(
        TEXT("Original baked point remains unchanged"),
        ResolvedPoint.Data.MarkerState,
        EGameZonePointState::Activated);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerRegistryRemoveBakedZoneTest,
    "IronicRPG.GameZone.MarkerRegistry.RemovingBakedZoneRemovesBakedOnlyMarker",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerRegistryRemoveBakedZoneTest::RunTest(const FString& Parameters)
{
    FGameZoneMarkerRegistry Registry;
    const FRPGId ZoneId(TEXT("z.TestZone"));
    const FGuid PointId(100, 200, 300, 400);

    FGameZonePointData BakedPoint;
    BakedPoint.PointId = PointId;
    BakedPoint.ZoneId = ZoneId;

    TMap<FGuid, FGameZonePointData> BakedPoints;
    BakedPoints.Add(PointId, BakedPoint);
    Registry.CommitBakedZone(ZoneId, BakedPoints);

    const FGameZoneMarkerMutationResult RemoveResult = Registry.RemoveBakedZone(ZoneId);
    TestTrue(TEXT("Baked zone removal succeeds"), RemoveResult.WasApplied());
    TestEqual(TEXT("Baked-only removal emits one change"), RemoveResult.Changes.Num(), 1);
    if (RemoveResult.Changes.Num() == 1)
    {
        TestEqual(
            TEXT("Baked-only removal emits Removed"),
            RemoveResult.Changes[0].Kind,
            EMapMarkerChangeKind::Removed);
    }

    FResolvedGameZonePoint RemovedPoint;
    TestFalse(TEXT("Removed baked marker no longer resolves"), Registry.TryResolve(PointId, RemovedPoint));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerRegistrySanitizeFallbackTest,
    "IronicRPG.GameZone.MarkerRegistry.InvalidLiveFallsBackToBakedOnSanitize",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerRegistrySanitizeFallbackTest::RunTest(const FString& Parameters)
{
    FGameZoneMarkerRegistry Registry;
    const FRPGId ZoneId(TEXT("z.TestZone"));

    TStrongObjectPtr<UGameZonePointComponent> Component(
        NewObject<UGameZonePointComponent>());
    const FGuid PointId = Component->GetPointId();

    FGameZonePointData BakedPoint;
    BakedPoint.PointId = PointId;
    BakedPoint.ZoneId = ZoneId;
    BakedPoint.MarkerState = EGameZonePointState::Activated;

    TMap<FGuid, FGameZonePointData> BakedPoints;
    BakedPoints.Add(PointId, BakedPoint);
    Registry.CommitBakedZone(ZoneId, BakedPoints);
    Registry.RegisterLive(*Component, ZoneId);

    Component.Reset();
    CollectGarbage(RF_NoFlags);

    const int64 RevisionBeforeResolve = Registry.GetRevision();
    FResolvedGameZonePoint FallbackBeforeSanitize;
    TestTrue(
        TEXT("Invalid live can already resolve through baked data"),
        Registry.TryResolve(PointId, FallbackBeforeSanitize));
    TestEqual(
        TEXT("Read-only resolve falls back to baked"),
        FallbackBeforeSanitize.Source,
        EGameZonePointResolvedSource::Baked);
    TestEqual(
        TEXT("Read-only resolve does not advance revision"),
        Registry.GetRevision(),
        RevisionBeforeResolve);

    const FGameZoneMarkerMutationResult SanitizeResult =
        Registry.SanitizeInvalidLive();

    TestTrue(TEXT("Sanitize applies the fallback transition"), SanitizeResult.WasApplied());
    TestEqual(TEXT("Fallback emits one change"), SanitizeResult.Changes.Num(), 1);
    if (SanitizeResult.Changes.Num() == 1)
    {
        TestEqual(
            TEXT("Fallback emits Changed"),
            SanitizeResult.Changes[0].Kind,
            EMapMarkerChangeKind::Changed);
        TestEqual(
            TEXT("Fallback change identifies the marker"),
            SanitizeResult.Changes[0].PointId,
            PointId);
    }
    TestEqual(
        TEXT("Fallback advances revision once"),
        Registry.GetRevision(),
        RevisionBeforeResolve + 1);

    FResolvedGameZonePoint FallbackAfterSanitize;
    TestTrue(
        TEXT("Sanitized marker still resolves"),
        Registry.TryResolve(PointId, FallbackAfterSanitize));
    TestEqual(
        TEXT("Sanitized marker resolves from baked"),
        FallbackAfterSanitize.Source,
        EGameZonePointResolvedSource::Baked);
    TestEqual(
        TEXT("Resolved revision matches sanitize result"),
        FallbackAfterSanitize.Revision,
        SanitizeResult.Revision);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerRegistrySanitizeRemovalTest,
    "IronicRPG.GameZone.MarkerRegistry.InvalidLiveOnlyIsRemovedOnSanitize",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerRegistrySanitizeRemovalTest::RunTest(const FString& Parameters)
{
    FGameZoneMarkerRegistry Registry;
    TStrongObjectPtr<UGameZonePointComponent> Component(
        NewObject<UGameZonePointComponent>());
    const FGuid PointId = Component->GetPointId();

    Registry.RegisterLive(*Component, FRPGId(TEXT("z.TestZone")));
    Component.Reset();
    CollectGarbage(RF_NoFlags);

    const int64 RevisionBeforeSanitize = Registry.GetRevision();
    FResolvedGameZonePoint PointBeforeSanitize;
    TestFalse(
        TEXT("Invalid live-only marker cannot resolve"),
        Registry.TryResolve(PointId, PointBeforeSanitize));
    TestEqual(
        TEXT("Read-only resolve does not remove invalid live-only marker"),
        Registry.GetRevision(),
        RevisionBeforeSanitize);

    const FGameZoneMarkerMutationResult SanitizeResult =
        Registry.SanitizeInvalidLive();

    TestTrue(TEXT("Sanitize applies the removal transition"), SanitizeResult.WasApplied());
    TestEqual(TEXT("Removal emits one change"), SanitizeResult.Changes.Num(), 1);
    if (SanitizeResult.Changes.Num() == 1)
    {
        TestEqual(
            TEXT("Invalid live-only marker emits Removed"),
            SanitizeResult.Changes[0].Kind,
            EMapMarkerChangeKind::Removed);
        TestEqual(
            TEXT("Removal change identifies the marker"),
            SanitizeResult.Changes[0].PointId,
            PointId);
    }
    TestEqual(
        TEXT("Removal advances revision once"),
        Registry.GetRevision(),
        RevisionBeforeSanitize + 1);

    FResolvedGameZonePoint RemovedPoint;
    TestFalse(
        TEXT("Sanitized live-only marker remains unresolved"),
        Registry.TryResolve(PointId, RemovedPoint));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerRegistryPresentationSnapshotTest,
    "IronicRPG.GameZone.MarkerRegistry.SnapshotFiltersZoneAndDisplayMode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerRegistryPresentationSnapshotTest::RunTest(const FString& Parameters)
{
    FGameZoneMarkerRegistry Registry;
    const FRPGId IncludedZone(TEXT("z.Included"));
    const FRPGId OtherZone(TEXT("z.Other"));
    const FGuid WorldMapPointId(1, 10, 100, 1000);
    const FGuid MiniMapPointId(2, 20, 200, 2000);
    const FGuid HiddenPointId(3, 30, 300, 3000);
    const FGuid OtherZonePointId(4, 40, 400, 4000);

    FGameZonePointData WorldMapPoint;
    WorldMapPoint.PointId = WorldMapPointId;
    WorldMapPoint.ZoneId = IncludedZone;
    WorldMapPoint.DisplayMode = static_cast<int32>(EMapMarkerDisplayMode::WorldMap);

    FGameZonePointData MiniMapPoint;
    MiniMapPoint.PointId = MiniMapPointId;
    MiniMapPoint.ZoneId = IncludedZone;
    MiniMapPoint.DisplayMode = static_cast<int32>(EMapMarkerDisplayMode::MiniMap);

    FGameZonePointData HiddenPoint;
    HiddenPoint.PointId = HiddenPointId;
    HiddenPoint.ZoneId = IncludedZone;
    HiddenPoint.DisplayMode = static_cast<int32>(EMapMarkerDisplayMode::WorldMap);
    HiddenPoint.MarkerState = EGameZonePointState::Hide;

    TMap<FGuid, FGameZonePointData> IncludedPoints;
    IncludedPoints.Add(WorldMapPointId, WorldMapPoint);
    IncludedPoints.Add(MiniMapPointId, MiniMapPoint);
    IncludedPoints.Add(HiddenPointId, HiddenPoint);
    Registry.CommitBakedZone(IncludedZone, IncludedPoints);

    FGameZonePointData OtherZonePoint = WorldMapPoint;
    OtherZonePoint.PointId = OtherZonePointId;
    OtherZonePoint.ZoneId = OtherZone;
    TMap<FGuid, FGameZonePointData> OtherPoints;
    OtherPoints.Add(OtherZonePointId, OtherZonePoint);
    Registry.CommitBakedZone(OtherZone, OtherPoints);

    const TArray<FResolvedGameZonePoint> Snapshot = Registry.BuildSnapshot(
        TSet<FRPGId>{IncludedZone},
        EMapMarkerDisplayMode::WorldMap);

    TestEqual(TEXT("Snapshot contains one visible marker"), Snapshot.Num(), 1);
    if (Snapshot.Num() == 1)
    {
        TestEqual(
            TEXT("Snapshot contains the matching marker"),
            Snapshot[0].Data.PointId,
            WorldMapPointId);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerRegistrySavedFallbackTest,
    "IronicRPG.GameZone.MarkerRegistry.SavedOverrideFallsBackOverBaked",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerRegistrySavedFallbackTest::RunTest(const FString& Parameters)
{
    FGameZoneMarkerRegistry Registry;
    const FRPGId ZoneId(TEXT("z.SavedFallback"));
    const FGuid PointId(101, 202, 303, 404);

    FGameZonePointData BakedPoint;
    BakedPoint.PointId = PointId;
    BakedPoint.ZoneId = ZoneId;
    BakedPoint.MarkerState = EGameZonePointState::Activated;
    BakedPoint.SavePolicy = EGameZonePointSavePolicy::Saveable;
    Registry.CommitBakedZone(ZoneId, {{PointId, BakedPoint}});

    FGameZonePointData SavedPoint = BakedPoint;
    SavedPoint.MarkerState = EGameZonePointState::Deactivated;
    SavedPoint.SavePolicy = EGameZonePointSavePolicy::Saveable;
    const FGameZoneMarkerMutationResult SetResult = Registry.SetSaveOverride(SavedPoint);
    TestTrue(TEXT("A saved fallback changes the visible baked marker"), SetResult.WasApplied());

    FResolvedGameZonePoint ResolvedPoint;
    TestTrue(TEXT("The marker resolves through its saved fallback"), Registry.TryResolve(PointId, ResolvedPoint));
    TestEqual(TEXT("The resolved source is saved"), ResolvedPoint.Source, EGameZonePointResolvedSource::Saved);
    TestEqual(TEXT("The saved state overrides the baked default"), ResolvedPoint.Data.MarkerState, EGameZonePointState::Deactivated);

    const FGameZoneMarkerMutationResult ClearResult = Registry.ClearSaveOverride(PointId);
    TestTrue(TEXT("Clearing a visible saved fallback emits a change"), ClearResult.WasApplied());
    TestTrue(TEXT("The marker still resolves after clearing its override"), Registry.TryResolve(PointId, ResolvedPoint));
    TestEqual(TEXT("The marker falls back to baked"), ResolvedPoint.Source, EGameZonePointResolvedSource::Baked);
    TestEqual(TEXT("The baked default is restored"), ResolvedPoint.Data.MarkerState, EGameZonePointState::Activated);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerRegistrySavedInitializationTest,
    "IronicRPG.GameZone.MarkerRegistry.SaveOnlyHydratesLiveWithoutCreatingGhost",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerRegistrySavedInitializationTest::RunTest(const FString& Parameters)
{
    FGameZoneMarkerRegistry Registry;
    UGameZonePointComponent* Component = NewObject<UGameZonePointComponent>();
    const FGuid PointId = Component->GetPointId();
    const FRPGId ZoneId(TEXT("z.Dynamic"));

    FGameZonePointData AuthoredPoint = Component->MakePointSnapshot();
    AuthoredPoint.SavePolicy = EGameZonePointSavePolicy::Saveable;
    Component->ApplyPointDataInitialization(AuthoredPoint);

    FGameZonePointData SavedPoint = Component->MakePointSnapshot();
    SavedPoint.PointId = PointId;
    SavedPoint.ZoneId = ZoneId;
    SavedPoint.MarkerState = EGameZonePointState::Deactivated;
    SavedPoint.SavePolicy = EGameZonePointSavePolicy::Saveable;

    const FGameZoneMarkerMutationResult SetResult = Registry.SetSaveOverride(SavedPoint);
    TestEqual(TEXT("An invisible save-only record does not emit a map change"), SetResult.Changes.Num(), 0);

    FResolvedGameZonePoint ResolvedPoint;
    TestFalse(TEXT("A save-only dynamic marker does not create a ghost"), Registry.TryResolve(PointId, ResolvedPoint));

    const FGameZoneMarkerMutationResult RegisterResult = Registry.RegisterLive(*Component, ZoneId);
    TestTrue(TEXT("The matching live component registers"), RegisterResult.WasApplied());
    TestTrue(TEXT("The registered marker resolves"), Registry.TryResolve(PointId, ResolvedPoint));
    TestEqual(TEXT("Hydration still resolves as live"), ResolvedPoint.Source, EGameZonePointResolvedSource::Live);
    TestEqual(TEXT("The live component receives the saved state"), ResolvedPoint.Data.MarkerState, EGameZonePointState::Deactivated);

    Component->SetMarkerState(EGameZonePointState::Activated);
    Registry.NotifyLiveChanged(*Component);
    TestTrue(TEXT("The runtime marker still resolves after mutation"), Registry.TryResolve(PointId, ResolvedPoint));
    TestEqual(TEXT("Runtime live state wins after hydration"), ResolvedPoint.Data.MarkerState, EGameZonePointState::Activated);

    Registry.UnregisterLive(*Component);
    TestFalse(TEXT("A saved dynamic marker disappears with its actor"), Registry.TryResolve(PointId, ResolvedPoint));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerRegistrySaveableRuntimeReloadTest,
    "IronicRPG.GameZone.MarkerRegistry.SaveableRuntimeStateSurvivesLiveReload",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerRegistrySaveableRuntimeReloadTest::RunTest(const FString& Parameters)
{
    FGameZoneMarkerRegistry Registry;
    const FRPGId ZoneId(TEXT("z.RuntimeReload"));

    UGameZonePointComponent* AuthoredTemplate = NewObject<UGameZonePointComponent>();
    FGameZonePointData AuthoredPoint = AuthoredTemplate->MakePointSnapshot();
    AuthoredPoint.MarkerState = EGameZonePointState::Deactivated;
    AuthoredPoint.SavePolicy = EGameZonePointSavePolicy::Saveable;
    AuthoredTemplate->ApplyPointDataInitialization(AuthoredPoint);

    UGameZonePointComponent* FirstInstance = DuplicateObject<UGameZonePointComponent>(AuthoredTemplate, GetTransientPackage());
    UGameZonePointComponent* ReloadedInstance = DuplicateObject<UGameZonePointComponent>(AuthoredTemplate, GetTransientPackage());
    const FGuid PointId = FirstInstance->GetPointId();

    FGameZonePointData BakedPoint = AuthoredPoint;
    BakedPoint.PointId = PointId;
    BakedPoint.ZoneId = ZoneId;
    Registry.CommitBakedZone(ZoneId, {{PointId, BakedPoint}});
    Registry.RegisterLive(*FirstInstance, ZoneId);

    FirstInstance->SetMarkerState(EGameZonePointState::Activated);
    Registry.NotifyLiveChanged(*FirstInstance);
    Registry.UnregisterLive(*FirstInstance);

    FResolvedGameZonePoint ResolvedPoint;
    TestTrue(TEXT("The unloaded authored marker still resolves"), Registry.TryResolve(PointId, ResolvedPoint));
    TestEqual(TEXT("The unloaded marker resolves from its synchronized data"), ResolvedPoint.Source, EGameZonePointResolvedSource::Saved);
    TestEqual(TEXT("The unloaded marker keeps its runtime state"), ResolvedPoint.Data.MarkerState, EGameZonePointState::Activated);

    Registry.RegisterLive(*ReloadedInstance, ZoneId);
    TestTrue(TEXT("The reloaded live marker resolves"), Registry.TryResolve(PointId, ResolvedPoint));
    TestEqual(TEXT("The reloaded marker resolves as live"), ResolvedPoint.Source, EGameZonePointResolvedSource::Live);
    TestEqual(TEXT("The reloaded component is hydrated from synchronized data"), ResolvedPoint.Data.MarkerState, EGameZonePointState::Activated);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerRegistrySaveableUnregisterCaptureTest,
    "IronicRPG.GameZone.MarkerRegistry.UnregisterCapturesSaveableFinalState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerRegistrySaveableUnregisterCaptureTest::RunTest(const FString& Parameters)
{
    FGameZoneMarkerRegistry Registry;
    const FRPGId ZoneId(TEXT("z.UnregisterCapture"));
    UGameZonePointComponent* Component = NewObject<UGameZonePointComponent>();

    FGameZonePointData AuthoredPoint = Component->MakePointSnapshot();
    AuthoredPoint.MarkerState = EGameZonePointState::Deactivated;
    AuthoredPoint.SavePolicy = EGameZonePointSavePolicy::Saveable;
    Component->ApplyPointDataInitialization(AuthoredPoint);

    const FGuid PointId = Component->GetPointId();
    FGameZonePointData BakedPoint = AuthoredPoint;
    BakedPoint.PointId = PointId;
    BakedPoint.ZoneId = ZoneId;
    Registry.CommitBakedZone(ZoneId, {{PointId, BakedPoint}});
    Registry.RegisterLive(*Component, ZoneId);

    Component->SetMarkerState(EGameZonePointState::Activated);
    Registry.UnregisterLive(*Component);

    FResolvedGameZonePoint ResolvedPoint;
    TestTrue(TEXT("The authored marker resolves after unregister"), Registry.TryResolve(PointId, ResolvedPoint));
    TestEqual(TEXT("Unregister creates the saved fallback"), ResolvedPoint.Source, EGameZonePointResolvedSource::Saved);
    TestEqual(TEXT("Unregister captures the final live state"), ResolvedPoint.Data.MarkerState, EGameZonePointState::Activated);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerRegistryNotSaveableRuntimeReloadTest,
    "IronicRPG.GameZone.MarkerRegistry.NotSaveableRuntimeStateResetsOnLiveReload",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerRegistryNotSaveableRuntimeReloadTest::RunTest(const FString& Parameters)
{
    FGameZoneMarkerRegistry Registry;
    const FRPGId ZoneId(TEXT("z.NotSaveableReload"));

    UGameZonePointComponent* AuthoredTemplate = NewObject<UGameZonePointComponent>();
    FGameZonePointData AuthoredPoint = AuthoredTemplate->MakePointSnapshot();
    AuthoredPoint.MarkerState = EGameZonePointState::Deactivated;
    AuthoredPoint.SavePolicy = EGameZonePointSavePolicy::NotSaveable;
    AuthoredTemplate->ApplyPointDataInitialization(AuthoredPoint);

    UGameZonePointComponent* FirstInstance = DuplicateObject<UGameZonePointComponent>(AuthoredTemplate, GetTransientPackage());
    UGameZonePointComponent* ReloadedInstance = DuplicateObject<UGameZonePointComponent>(AuthoredTemplate, GetTransientPackage());
    const FGuid PointId = FirstInstance->GetPointId();

    FGameZonePointData BakedPoint = AuthoredPoint;
    BakedPoint.PointId = PointId;
    BakedPoint.ZoneId = ZoneId;
    Registry.CommitBakedZone(ZoneId, {{PointId, BakedPoint}});
    Registry.RegisterLive(*FirstInstance, ZoneId);

    FirstInstance->SetMarkerState(EGameZonePointState::Activated);
    Registry.NotifyLiveChanged(*FirstInstance);
    Registry.UnregisterLive(*FirstInstance);

    FResolvedGameZonePoint ResolvedPoint;
    TestTrue(TEXT("The unloaded authored marker falls back"), Registry.TryResolve(PointId, ResolvedPoint));
    TestEqual(TEXT("NotSaveable does not create saved fallback data"), ResolvedPoint.Source, EGameZonePointResolvedSource::Baked);
    TestEqual(TEXT("The fallback uses the authored state"), ResolvedPoint.Data.MarkerState, EGameZonePointState::Deactivated);

    Registry.RegisterLive(*ReloadedInstance, ZoneId);
    TestTrue(TEXT("The reloaded NotSaveable marker resolves"), Registry.TryResolve(PointId, ResolvedPoint));
    TestEqual(TEXT("The reloaded NotSaveable marker uses its authored state"), ResolvedPoint.Data.MarkerState, EGameZonePointState::Deactivated);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerRegistryCapturePoliciesTest,
    "IronicRPG.GameZone.MarkerRegistry.CaptureHonorsSavePolicies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerRegistryCapturePoliciesTest::RunTest(const FString& Parameters)
{
    FGameZoneMarkerRegistry Registry;
    const FRPGId ZoneId(TEXT("z.SavePolicies"));

    UGameZonePointComponent* SaveableComponent = NewObject<UGameZonePointComponent>();
    FGameZonePointData SaveableData = SaveableComponent->MakePointSnapshot();
    SaveableData.SavePolicy = EGameZonePointSavePolicy::Saveable;
    SaveableData.MarkerState = EGameZonePointState::Deactivated;
    SaveableComponent->ApplyPointDataInitialization(SaveableData);
    Registry.RegisterLive(*SaveableComponent, ZoneId);

    UGameZonePointComponent* NotSaveableComponent = NewObject<UGameZonePointComponent>();
    FGameZonePointData NotSaveableData = NotSaveableComponent->MakePointSnapshot();
    NotSaveableData.SavePolicy = EGameZonePointSavePolicy::NotSaveable;
    NotSaveableComponent->ApplyPointDataInitialization(NotSaveableData);
    Registry.RegisterLive(*NotSaveableComponent, ZoneId);

    UGameZonePointComponent* CustomComponent = NewObject<UGameZonePointComponent>();
    FGameZonePointData CustomData = CustomComponent->MakePointSnapshot();
    CustomData.SavePolicy = EGameZonePointSavePolicy::Custom;
    CustomData.MarkerState = EGameZonePointState::Deactivated;
    CustomComponent->ApplyPointDataInitialization(CustomData);
    Registry.SetSaveOverride(CustomData);
    Registry.RegisterLive(*CustomComponent, ZoneId);
    CustomComponent->SetMarkerState(EGameZonePointState::Activated);
    Registry.NotifyLiveChanged(*CustomComponent);

    const TMap<FGuid, FGameZonePointData> Overrides = Registry.CaptureSaveOverrides();
    TestTrue(TEXT("Saveable captures its live snapshot"), Overrides.Contains(SaveableComponent->GetPointId()));
    TestFalse(TEXT("NotSaveable is omitted"), Overrides.Contains(NotSaveableComponent->GetPointId()));
    TestTrue(TEXT("Custom preserves its explicit override"), Overrides.Contains(CustomComponent->GetPointId()));
    if (const FGameZonePointData* CapturedCustom = Overrides.Find(CustomComponent->GetPointId()))
    {
        TestEqual(TEXT("Custom is not overwritten by the live snapshot"), CapturedCustom->MarkerState, EGameZonePointState::Deactivated);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerRegistryReplaceOverridesTest,
    "IronicRPG.GameZone.MarkerRegistry.ReplacingOverridesRehydratesLive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerRegistryReplaceOverridesTest::RunTest(const FString& Parameters)
{
    FGameZoneMarkerRegistry Registry;
    UGameZonePointComponent* Component = NewObject<UGameZonePointComponent>();
    const FGuid PointId = Component->GetPointId();
    const FRPGId ZoneId(TEXT("z.Reload"));
    FGameZonePointData AuthoredPoint = Component->MakePointSnapshot();
    AuthoredPoint.SavePolicy = EGameZonePointSavePolicy::Saveable;
    Component->ApplyPointDataInitialization(AuthoredPoint);
    Registry.RegisterLive(*Component, ZoneId);

    FGameZonePointData SavedPoint = Component->MakePointSnapshot();
    SavedPoint.ZoneId = ZoneId;
    SavedPoint.MarkerState = EGameZonePointState::Deactivated;
    SavedPoint.SavePolicy = EGameZonePointSavePolicy::Saveable;
    Registry.ReplaceSaveOverrides({{PointId, SavedPoint}});

    FResolvedGameZonePoint ResolvedPoint;
    TestTrue(TEXT("The live marker resolves after loading an override"), Registry.TryResolve(PointId, ResolvedPoint));
    TestEqual(TEXT("Load hydrates the live marker"), ResolvedPoint.Data.MarkerState, EGameZonePointState::Deactivated);

    Registry.ReplaceSaveOverrides({});
    TestTrue(TEXT("The live marker resolves after loading an empty override set"), Registry.TryResolve(PointId, ResolvedPoint));
    TestEqual(TEXT("A later empty load restores the registration baseline"), ResolvedPoint.Data.MarkerState, EGameZonePointState::Activated);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneMarkerRegistryNotSaveableAuthorityTest,
    "IronicRPG.GameZone.MarkerRegistry.NotSaveableAuthoredDataRejectsOverrides",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneMarkerRegistryNotSaveableAuthorityTest::RunTest(const FString& Parameters)
{
    FGameZoneMarkerRegistry Registry;
    const FRPGId ZoneId(TEXT("z.NotSaveable"));

    UGameZonePointComponent* Component = NewObject<UGameZonePointComponent>();
    Registry.RegisterLive(*Component, ZoneId);

    FGameZonePointData LiveOverride = Component->MakePointSnapshot();
    LiveOverride.SavePolicy = EGameZonePointSavePolicy::Custom;
    LiveOverride.MarkerState = EGameZonePointState::Deactivated;
    const FGameZoneMarkerMutationResult LiveSetResult = Registry.SetSaveOverride(LiveOverride);
    TestEqual(
        TEXT("A caller cannot bypass the authored NotSaveable policy"),
        LiveSetResult.Error,
        EGameZoneMarkerMutationError::InvalidSaveOverride);

    FResolvedGameZonePoint ResolvedPoint;
    TestTrue(TEXT("The rejected live marker still resolves"), Registry.TryResolve(Component->GetPointId(), ResolvedPoint));
    TestEqual(TEXT("The rejected override does not change live state"), ResolvedPoint.Data.MarkerState, EGameZonePointState::Activated);

    const FGuid BakedPointId(700, 800, 900, 1000);
    FGameZonePointData LoadedOverride;
    LoadedOverride.PointId = BakedPointId;
    LoadedOverride.ZoneId = ZoneId;
    LoadedOverride.SavePolicy = EGameZonePointSavePolicy::Saveable;
    LoadedOverride.MarkerState = EGameZonePointState::Deactivated;
    Registry.ReplaceSaveOverrides({{BakedPointId, LoadedOverride}});

    FGameZonePointData BakedPoint = LoadedOverride;
    BakedPoint.SavePolicy = EGameZonePointSavePolicy::NotSaveable;
    BakedPoint.MarkerState = EGameZonePointState::Activated;
    Registry.CommitBakedZone(ZoneId, {{BakedPointId, BakedPoint}});

    TestTrue(TEXT("The authored baked marker resolves"), Registry.TryResolve(BakedPointId, ResolvedPoint));
    TestEqual(TEXT("NotSaveable baked data discards a stale loaded override"), ResolvedPoint.Source, EGameZonePointResolvedSource::Baked);
    TestEqual(TEXT("The baked state remains authoritative"), ResolvedPoint.Data.MarkerState, EGameZonePointState::Activated);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

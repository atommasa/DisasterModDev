// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "GameZoneBindingCoordinator.h"

#include "Levels/GameZoneAsset.h"
#include "Levels/RPGWorldSettings.h"

#include "Engine/World.h"
#include "Editor.h"
#include "EditorWorldUtils.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
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

    TUniquePtr<FScopedEditorWorld> CreateBindingTestWorld(const TCHAR* Label)
    {
        static int32 WorldIndex = 0;
        const FString WorldName = FString::Printf(TEXT("BindingMutation_%s_%d"), Label, ++WorldIndex);
        UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/IronicRPG/%s"), *WorldName));
        const UWorld::InitializationValues InitializationValues = UWorld::InitializationValues()
            .AllowAudioPlayback(false)
            .CreatePhysicsScene(false)
            .CreateNavigation(false)
            .CreateAISystem(false)
            .ShouldSimulatePhysics(false)
            .CreateFXSystem(false);
        UWorld* World = UWorld::CreateWorld(
            EWorldType::Editor,
            false,
            FName(*WorldName),
            Package,
            false,
            ERHIFeatureLevel::Num,
            &InitializationValues,
            true);
        return MakeUnique<FScopedEditorWorld>(World, InitializationValues);
    }

    void SetAssetBinding(
        UGameZoneAsset& ZoneAsset,
        UWorld* World,
        const FRPGId& ZoneId,
        const FGuid& BindingId,
        const EGameZoneBindingVerificationStatus Status = EGameZoneBindingVerificationStatus::Verified)
    {
        GetPropertyValue<FRPGId>(ZoneAsset, TEXT("Id")) = ZoneId;
        GetPropertyValue<TSoftObjectPtr<UWorld>>(ZoneAsset, TEXT("LevelToLoad")) = World;
        GetPropertyValue<FGuid>(ZoneAsset, TEXT("GameZoneBindingId")) = BindingId;
        GetPropertyValue<EGameZoneBindingVerificationStatus>(ZoneAsset, TEXT("BindingVerificationStatus")) = Status;
    }

    void SetWorldClaim(ARPGWorldSettings& WorldSettings, const FRPGId& ZoneId, const FGuid& BindingId)
    {
        GetPropertyValue<FRPGId>(WorldSettings, TEXT("GameZoneId")) = ZoneId;
        GetPropertyValue<FGuid>(WorldSettings, TEXT("GameZoneBindingId")) = BindingId;
    }

    FGameZoneBindingPropertyChangeRequest MakeLevelRequest(const UGameZoneAsset& ZoneAsset)
    {
        FGameZoneBindingPropertyChangeRequest Request;
        Request.ZoneAsset = const_cast<UGameZoneAsset*>(&ZoneAsset);
        Request.Change = EGameZoneBindingPropertyChange::LevelToLoad;
        Request.PreviousLevel = ZoneAsset.GetLevelToLoad().ToSoftObjectPath();
        Request.PreviousZoneId = ZoneAsset.GetId();
        Request.PreviousBindingId = ZoneAsset.GetGameZoneBindingId();
        Request.PreviousVerificationStatus = ZoneAsset.GetBindingVerificationStatus();
        return Request;
    }

    struct FTwoWorldBindingFixture
    {
        FTwoWorldBindingFixture()
        {
            PreviousWorldScope = CreateBindingTestWorld(TEXT("Previous"));
            NewWorldScope = CreateBindingTestWorld(TEXT("New"));
            PreviousWorld = PreviousWorldScope ? PreviousWorldScope->GetWorld() : nullptr;
            NewWorld = NewWorldScope ? NewWorldScope->GetWorld() : nullptr;
            ZoneAsset = NewObject<UGameZoneAsset>();
            PreviousSettings = PreviousWorld ? Cast<ARPGWorldSettings>(PreviousWorld->GetWorldSettings()) : nullptr;
            NewSettings = NewWorld ? Cast<ARPGWorldSettings>(NewWorld->GetWorldSettings()) : nullptr;
        }

        bool IsValid() const
        {
            return PreviousWorld && NewWorld && ZoneAsset && PreviousSettings && NewSettings;
        }

        TUniquePtr<FScopedEditorWorld> PreviousWorldScope;
        TUniquePtr<FScopedEditorWorld> NewWorldScope;
        UWorld* PreviousWorld = nullptr;
        UWorld* NewWorld = nullptr;
        UGameZoneAsset* ZoneAsset = nullptr;
        ARPGWorldSettings* PreviousSettings = nullptr;
        ARPGWorldSettings* NewSettings = nullptr;
    };

    UGameZoneAsset* CreatePackageZoneAsset(const TCHAR* Label)
    {
        static int32 AssetIndex = 0;
        const FString AssetName = FString::Printf(TEXT("ZoneLifecycle_%s_%d"), Label, ++AssetIndex);
        UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/IronicRPG/%s"), *AssetName));
        return NewObject<UGameZoneAsset>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
    }

    int32 CountGlobalIssues(
        const FGameZoneGlobalBindingAuditResult& Result,
        const EGameZoneBindingIssue Issue)
    {
        return Result.Issues.FilterByPredicate(
            [Issue](const FGameZoneGlobalBindingIssue& Entry)
            {
                return Entry.Issue == Issue;
            }).Num();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneBindingRebindTest,
    "IronicRPG.GameZone.Binding.Mutation.RebindUsesCompareAndClear",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneBindingRebindTest::RunTest(const FString& Parameters)
{
    FTwoWorldBindingFixture Fixture;
    TestTrue(TEXT("The two-World fixture uses RPG World Settings"), Fixture.IsValid());
    if (!Fixture.IsValid())
    {
        return false;
    }

    const FRPGId ZoneId(TEXT("z.MutationRebind"));
    const FGuid PreviousBindingId = FGuid::NewGuid();
    SetAssetBinding(*Fixture.ZoneAsset, Fixture.PreviousWorld, ZoneId, PreviousBindingId);
    SetWorldClaim(*Fixture.PreviousSettings, ZoneId, PreviousBindingId);
    const FGameZoneBindingPropertyChangeRequest Request = MakeLevelRequest(*Fixture.ZoneAsset);
    GetPropertyValue<TSoftObjectPtr<UWorld>>(*Fixture.ZoneAsset, TEXT("LevelToLoad")) = Fixture.NewWorld;

    const FGameZoneBindingMutationResult Result = FGameZoneBindingCoordinator::ApplyPropertyChange(Request);
    TestTrue(TEXT("A valid rebind succeeds"), Result.IsSuccess());
    TestFalse(TEXT("The previous Level Id is cleared"), Fixture.PreviousSettings->GetGameZoneId().IsValid());
    TestFalse(TEXT("The previous Level binding generation is cleared"), Fixture.PreviousSettings->GetGameZoneBindingId().IsValid());
    TestEqual(TEXT("The new Level receives the Zone Id"), Fixture.NewSettings->GetGameZoneId(), ZoneId);
    TestTrue(TEXT("The new binding generation is valid"), Fixture.ZoneAsset->GetGameZoneBindingId().IsValid());
    TestEqual(
        TEXT("The Asset and new Level share one binding generation"),
        Fixture.NewSettings->GetGameZoneBindingId(),
        Fixture.ZoneAsset->GetGameZoneBindingId());
    TestNotEqual(
        TEXT("Rebinding creates a new generation"),
        Fixture.ZoneAsset->GetGameZoneBindingId(),
        PreviousBindingId);
    TestEqual(
        TEXT("A changed binding invalidates source freshness"),
        Fixture.ZoneAsset->GetBindingVerificationStatus(),
        EGameZoneBindingVerificationStatus::SourceStale);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneBindingNewConflictTest,
    "IronicRPG.GameZone.Binding.Mutation.NewConflictPreservesPreviousBinding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneBindingNewConflictTest::RunTest(const FString& Parameters)
{
    FTwoWorldBindingFixture Fixture;
    TestTrue(TEXT("The two-World fixture uses RPG World Settings"), Fixture.IsValid());
    if (!Fixture.IsValid())
    {
        return false;
    }

    const FRPGId ZoneId(TEXT("z.MutationPreserved"));
    const FGuid PreviousBindingId = FGuid::NewGuid();
    const FRPGId ForeignId(TEXT("z.MutationForeign"));
    const FGuid ForeignBindingId = FGuid::NewGuid();
    SetAssetBinding(*Fixture.ZoneAsset, Fixture.PreviousWorld, ZoneId, PreviousBindingId);
    SetWorldClaim(*Fixture.PreviousSettings, ZoneId, PreviousBindingId);
    SetWorldClaim(*Fixture.NewSettings, ForeignId, ForeignBindingId);
    const FGameZoneBindingPropertyChangeRequest Request = MakeLevelRequest(*Fixture.ZoneAsset);
    GetPropertyValue<TSoftObjectPtr<UWorld>>(*Fixture.ZoneAsset, TEXT("LevelToLoad")) = Fixture.NewWorld;

    const FGameZoneBindingMutationResult Result = FGameZoneBindingCoordinator::ApplyPropertyChange(Request);
    TestEqual(TEXT("A foreign new claim is rejected"), Result.Issue, EGameZoneBindingIssue::BindingConflict);
    TestEqual(TEXT("The Asset Level is restored"), Fixture.ZoneAsset->GetLevelToLoad().Get(), Fixture.PreviousWorld);
    TestEqual(TEXT("The Asset generation is restored"), Fixture.ZoneAsset->GetGameZoneBindingId(), PreviousBindingId);
    TestEqual(TEXT("The previous Level Id is preserved"), Fixture.PreviousSettings->GetGameZoneId(), ZoneId);
    TestEqual(
        TEXT("The previous Level generation is preserved"),
        Fixture.PreviousSettings->GetGameZoneBindingId(),
        PreviousBindingId);
    TestEqual(TEXT("The foreign Level Id is preserved"), Fixture.NewSettings->GetGameZoneId(), ForeignId);
    TestEqual(TEXT("The foreign Level generation is preserved"), Fixture.NewSettings->GetGameZoneBindingId(), ForeignBindingId);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneBindingAmbiguousLegacyTargetTest,
    "IronicRPG.GameZone.Binding.Mutation.AmbiguousLegacyTargetIsRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneBindingAmbiguousLegacyTargetTest::RunTest(const FString& Parameters)
{
    FTwoWorldBindingFixture Fixture;
    TestTrue(TEXT("The legacy-target fixture uses RPG World Settings"), Fixture.IsValid());
    if (!Fixture.IsValid())
    {
        return false;
    }

    const FRPGId ZoneId(TEXT("z.MutationAmbiguousLegacy"));
    const FGuid PreviousBindingId = FGuid::NewGuid();
    SetAssetBinding(*Fixture.ZoneAsset, Fixture.PreviousWorld, ZoneId, PreviousBindingId);
    SetWorldClaim(*Fixture.PreviousSettings, ZoneId, PreviousBindingId);
    SetWorldClaim(*Fixture.NewSettings, ZoneId, FGuid());
    const FGameZoneBindingPropertyChangeRequest Request = MakeLevelRequest(*Fixture.ZoneAsset);
    GetPropertyValue<TSoftObjectPtr<UWorld>>(*Fixture.ZoneAsset, TEXT("LevelToLoad")) = Fixture.NewWorld;

    const FGameZoneBindingMutationResult Result = FGameZoneBindingCoordinator::ApplyPropertyChange(Request);
    TestEqual(TEXT("A legacy claim on a different Level is ambiguous"), Result.Issue, EGameZoneBindingIssue::BindingConflict);
    TestEqual(TEXT("The Asset remains on its previous Level"), Fixture.ZoneAsset->GetLevelToLoad().Get(), Fixture.PreviousWorld);
    TestEqual(TEXT("The previous exact claim is preserved"), Fixture.PreviousSettings->GetGameZoneBindingId(), PreviousBindingId);
    TestFalse(TEXT("The ambiguous target does not gain a generation"), Fixture.NewSettings->GetGameZoneBindingId().IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneBindingOldConflictTest,
    "IronicRPG.GameZone.Binding.Mutation.OldConflictIsNeverCleared",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneBindingOldConflictTest::RunTest(const FString& Parameters)
{
    FTwoWorldBindingFixture Fixture;
    TestTrue(TEXT("The two-World fixture uses RPG World Settings"), Fixture.IsValid());
    if (!Fixture.IsValid())
    {
        return false;
    }

    const FRPGId ZoneId(TEXT("z.MutationOldConflict"));
    const FGuid AssetBindingId = FGuid::NewGuid();
    const FGuid ForeignBindingId = FGuid::NewGuid();
    SetAssetBinding(*Fixture.ZoneAsset, Fixture.PreviousWorld, ZoneId, AssetBindingId);
    SetWorldClaim(*Fixture.PreviousSettings, ZoneId, ForeignBindingId);
    const FGameZoneBindingPropertyChangeRequest Request = MakeLevelRequest(*Fixture.ZoneAsset);
    GetPropertyValue<TSoftObjectPtr<UWorld>>(*Fixture.ZoneAsset, TEXT("LevelToLoad")) = Fixture.NewWorld;

    const FGameZoneBindingMutationResult Result = FGameZoneBindingCoordinator::ApplyPropertyChange(Request);
    TestEqual(TEXT("A torn previous claim is rejected"), Result.Issue, EGameZoneBindingIssue::BindingConflict);
    TestEqual(TEXT("The foreign previous generation is not cleared"), Fixture.PreviousSettings->GetGameZoneBindingId(), ForeignBindingId);
    TestFalse(TEXT("The staged new Level remains unclaimed"), Fixture.NewSettings->GetGameZoneId().IsValid());
    TestEqual(TEXT("The Asset Level is restored"), Fixture.ZoneAsset->GetLevelToLoad().Get(), Fixture.PreviousWorld);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneBindingLegacyMigrationTest,
    "IronicRPG.GameZone.Binding.Mutation.LegacyBindingMigratesExplicitly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneBindingLegacyMigrationTest::RunTest(const FString& Parameters)
{
    TUniquePtr<FScopedEditorWorld> WorldScope = CreateBindingTestWorld(TEXT("Legacy"));
    UWorld* World = WorldScope ? WorldScope->GetWorld() : nullptr;
    UGameZoneAsset* ZoneAsset = NewObject<UGameZoneAsset>();
    ARPGWorldSettings* WorldSettings = World ? Cast<ARPGWorldSettings>(World->GetWorldSettings()) : nullptr;
    TestTrue(TEXT("The legacy fixture uses RPG World Settings"), World && ZoneAsset && WorldSettings);
    if (!World || !ZoneAsset || !WorldSettings)
    {
        return false;
    }

    const FRPGId ZoneId(TEXT("z.MutationLegacy"));
    SetAssetBinding(*ZoneAsset, World, ZoneId, FGuid(), EGameZoneBindingVerificationStatus::LegacyUnverified);
    SetWorldClaim(*WorldSettings, ZoneId, FGuid());

    ZoneAsset->MigrateLegacyBinding();
    TestTrue(TEXT("Migration creates a binding generation"), ZoneAsset->GetGameZoneBindingId().IsValid());
    TestEqual(
        TEXT("Migration writes the same generation to the Level"),
        WorldSettings->GetGameZoneBindingId(),
        ZoneAsset->GetGameZoneBindingId());
    TestEqual(
        TEXT("Migration marks the source stale until it is baked"),
        ZoneAsset->GetBindingVerificationStatus(),
        EGameZoneBindingVerificationStatus::SourceStale);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneBindingVerifiedMigrationNoOpTest,
    "IronicRPG.GameZone.Binding.Mutation.VerifiedMigrationIsNoOp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneBindingVerifiedMigrationNoOpTest::RunTest(const FString& Parameters)
{
    TUniquePtr<FScopedEditorWorld> WorldScope = CreateBindingTestWorld(TEXT("VerifiedMigration"));
    UWorld* World = WorldScope ? WorldScope->GetWorld() : nullptr;
    UGameZoneAsset* ZoneAsset = NewObject<UGameZoneAsset>();
    ARPGWorldSettings* WorldSettings = World ? Cast<ARPGWorldSettings>(World->GetWorldSettings()) : nullptr;
    TestTrue(TEXT("The verified fixture uses RPG World Settings"), World && ZoneAsset && WorldSettings);
    if (!World || !ZoneAsset || !WorldSettings)
    {
        return false;
    }

    const FRPGId ZoneId(TEXT("z.MutationVerified"));
    const FGuid BindingId = FGuid::NewGuid();
    SetAssetBinding(*ZoneAsset, World, ZoneId, BindingId, EGameZoneBindingVerificationStatus::Verified);
    SetWorldClaim(*WorldSettings, ZoneId, BindingId);

    ZoneAsset->MigrateLegacyBinding();
    TestEqual(TEXT("A verified generation is unchanged"), ZoneAsset->GetGameZoneBindingId(), BindingId);
    TestEqual(
        TEXT("A verified migration does not invalidate freshness"),
        ZoneAsset->GetBindingVerificationStatus(),
        EGameZoneBindingVerificationStatus::Verified);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneBindingPropertyCallbackTest,
    "IronicRPG.GameZone.Binding.Mutation.LevelPropertyCallbackUsesCoordinator",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneBindingPropertyCallbackTest::RunTest(const FString& Parameters)
{
    FTwoWorldBindingFixture Fixture;
    TestTrue(TEXT("The callback fixture uses RPG World Settings"), Fixture.IsValid());
    if (!Fixture.IsValid())
    {
        return false;
    }

    const FRPGId ZoneId(TEXT("z.MutationCallback"));
    const FGuid PreviousBindingId = FGuid::NewGuid();
    SetAssetBinding(*Fixture.ZoneAsset, Fixture.PreviousWorld, ZoneId, PreviousBindingId);
    SetWorldClaim(*Fixture.PreviousSettings, ZoneId, PreviousBindingId);

    FProperty* LevelProperty = FindFProperty<FProperty>(
        Fixture.ZoneAsset->GetClass(),
        TEXT("LevelToLoad"));
    TestNotNull(TEXT("LevelToLoad reflection property exists"), LevelProperty);
    if (!LevelProperty)
    {
        return false;
    }

    UObject* AssetObject = Fixture.ZoneAsset;
    AssetObject->PreEditChange(LevelProperty);
    GetPropertyValue<TSoftObjectPtr<UWorld>>(*Fixture.ZoneAsset, TEXT("LevelToLoad")) = Fixture.NewWorld;
    FPropertyChangedEvent PropertyChangedEvent(LevelProperty);
    AssetObject->PostEditChangeProperty(PropertyChangedEvent);

    TestEqual(TEXT("The callback keeps the selected new Level"), Fixture.ZoneAsset->GetLevelToLoad().Get(), Fixture.NewWorld);
    TestFalse(TEXT("The callback clears the exact previous claim"), Fixture.PreviousSettings->GetGameZoneId().IsValid());
    TestEqual(TEXT("The callback claims the new Level"), Fixture.NewSettings->GetGameZoneId(), ZoneId);
    TestEqual(
        TEXT("The callback synchronizes binding generations"),
        Fixture.NewSettings->GetGameZoneBindingId(),
        Fixture.ZoneAsset->GetGameZoneBindingId());
    TestTrue(TEXT("Selecting a new Level automatically creates a Bake revision"), Fixture.ZoneAsset->GetMapBakeRevision().IsValid());
    TestEqual(
        TEXT("The automatic Bake verifies the selected Level source"),
        Fixture.ZoneAsset->GetBindingVerificationStatus(),
        EGameZoneBindingVerificationStatus::Verified);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneBindingUnbindTest,
    "IronicRPG.GameZone.Binding.Mutation.UnbindClearsOnlyExactClaim",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneBindingUnbindTest::RunTest(const FString& Parameters)
{
    TUniquePtr<FScopedEditorWorld> WorldScope = CreateBindingTestWorld(TEXT("Unbind"));
    UWorld* World = WorldScope ? WorldScope->GetWorld() : nullptr;
    UGameZoneAsset* ZoneAsset = NewObject<UGameZoneAsset>();
    ARPGWorldSettings* WorldSettings = World ? Cast<ARPGWorldSettings>(World->GetWorldSettings()) : nullptr;
    TestTrue(TEXT("The unbind fixture uses RPG World Settings"), World && ZoneAsset && WorldSettings);
    if (!World || !ZoneAsset || !WorldSettings)
    {
        return false;
    }

    const FRPGId ZoneId(TEXT("z.MutationUnbind"));
    const FGuid BindingId = FGuid::NewGuid();
    SetAssetBinding(*ZoneAsset, World, ZoneId, BindingId);
    SetWorldClaim(*WorldSettings, ZoneId, BindingId);
    const FGameZoneBindingPropertyChangeRequest Request = MakeLevelRequest(*ZoneAsset);
    GetPropertyValue<TSoftObjectPtr<UWorld>>(*ZoneAsset, TEXT("LevelToLoad")) = nullptr;

    const FGameZoneBindingMutationResult Result = FGameZoneBindingCoordinator::ApplyPropertyChange(Request);
    TestTrue(TEXT("An exact unbind succeeds"), Result.IsSuccess());
    TestTrue(TEXT("The Asset Level remains empty"), ZoneAsset->GetLevelToLoad().IsNull());
    TestFalse(TEXT("The Asset binding generation is cleared"), ZoneAsset->GetGameZoneBindingId().IsValid());
    TestFalse(TEXT("The Level Id is cleared"), WorldSettings->GetGameZoneId().IsValid());
    TestFalse(TEXT("The Level binding generation is cleared"), WorldSettings->GetGameZoneBindingId().IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneBindingBoundIdEditTest,
    "IronicRPG.GameZone.Binding.Mutation.BoundIdEditIsRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneBindingBoundIdEditTest::RunTest(const FString& Parameters)
{
    TUniquePtr<FScopedEditorWorld> WorldScope = CreateBindingTestWorld(TEXT("IdEdit"));
    UWorld* World = WorldScope ? WorldScope->GetWorld() : nullptr;
    UGameZoneAsset* ZoneAsset = NewObject<UGameZoneAsset>();
    ARPGWorldSettings* WorldSettings = World ? Cast<ARPGWorldSettings>(World->GetWorldSettings()) : nullptr;
    TestTrue(TEXT("The Id-edit fixture uses RPG World Settings"), World && ZoneAsset && WorldSettings);
    if (!World || !ZoneAsset || !WorldSettings)
    {
        return false;
    }

    const FRPGId PreviousId(TEXT("z.MutationIdPrevious"));
    const FGuid BindingId = FGuid::NewGuid();
    SetAssetBinding(*ZoneAsset, World, PreviousId, BindingId);
    SetWorldClaim(*WorldSettings, PreviousId, BindingId);
    FGameZoneBindingPropertyChangeRequest Request = MakeLevelRequest(*ZoneAsset);
    Request.Change = EGameZoneBindingPropertyChange::ZoneId;
    GetPropertyValue<FRPGId>(*ZoneAsset, TEXT("Id")) = FRPGId(TEXT("z.MutationIdNew"));

    const FGameZoneBindingMutationResult Result = FGameZoneBindingCoordinator::ApplyPropertyChange(Request);
    TestEqual(TEXT("A direct bound Id edit is rejected"), Result.Issue, EGameZoneBindingIssue::BoundZoneIdEdit);
    TestEqual(TEXT("The Asset Id is restored"), ZoneAsset->GetId(), PreviousId);
    TestEqual(TEXT("The Level Id is unchanged"), WorldSettings->GetGameZoneId(), PreviousId);
    TestEqual(TEXT("The binding generation is unchanged"), ZoneAsset->GetGameZoneBindingId(), BindingId);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneGlobalBindingAuditTest,
    "IronicRPG.GameZone.Binding.Lifecycle.GlobalAuditFindsDuplicatesAndOrphans",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneGlobalBindingAuditTest::RunTest(const FString& Parameters)
{
    const FRPGId SharedId(TEXT("z.LifecycleAudit"));
    const FGuid SharedBinding = FGuid::NewGuid();
    TArray<FGameZoneAssetBindingRecord> Assets;
    Assets.Add({{}, FSoftObjectPath(TEXT("/Game/A.A")), SharedId, FName(TEXT("/Game/MapA")), SharedBinding});
    Assets.Add({{}, FSoftObjectPath(TEXT("/Game/B.B")), SharedId, FName(TEXT("/Game/MapA")), FGuid::NewGuid()});

    TArray<FGameZoneLevelClaimRecord> Claims;
    Claims.Add({{}, FName(TEXT("/Game/MapA")), SharedId, SharedBinding});
    Claims.Add({{}, FName(TEXT("/Game/Orphan")), FRPGId(TEXT("z.Orphan")), FGuid::NewGuid()});

    const FGameZoneGlobalBindingAuditResult Result = FGameZoneBindingCoordinator::AuditRecords(Assets, Claims);
    TestEqual(TEXT("Duplicate Zone Id is reported once"), CountGlobalIssues(Result, EGameZoneBindingIssue::DuplicateAssetId), 1);
    TestEqual(TEXT("Duplicate Level target is reported once"), CountGlobalIssues(Result, EGameZoneBindingIssue::DuplicateLevelTarget), 1);
    TestEqual(TEXT("Only the ownerless Level claim is orphaned"), CountGlobalIssues(Result, EGameZoneBindingIssue::OrphanLevelClaim), 1);
    TestEqual(TEXT("The torn duplicate Asset binding is reported"), CountGlobalIssues(Result, EGameZoneBindingIssue::BindingConflict), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneGlobalUniquenessGateTest,
    "IronicRPG.GameZone.Binding.Lifecycle.BindRejectsGlobalDuplicateIdAndTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneGlobalUniquenessGateTest::RunTest(const FString& Parameters)
{
    TUniquePtr<FScopedEditorWorld> OwnerWorldScope = CreateBindingTestWorld(TEXT("UniqueOwner"));
    TUniquePtr<FScopedEditorWorld> CandidateWorldScope = CreateBindingTestWorld(TEXT("UniqueCandidate"));
    UGameZoneAsset* Owner = CreatePackageZoneAsset(TEXT("UniqueOwner"));
    UGameZoneAsset* Candidate = CreatePackageZoneAsset(TEXT("UniqueCandidate"));
    ARPGWorldSettings* OwnerSettings = Cast<ARPGWorldSettings>(OwnerWorldScope->GetWorld()->GetWorldSettings());
    const FRPGId OwnerId(TEXT("z.GlobalUniqueOwner"));
    const FGuid OwnerBindingId = FGuid::NewGuid();
    SetAssetBinding(*Owner, OwnerWorldScope->GetWorld(), OwnerId, OwnerBindingId);
    SetWorldClaim(*OwnerSettings, OwnerId, OwnerBindingId);

    GetPropertyValue<FRPGId>(*Candidate, TEXT("Id")) = OwnerId;
    FGameZoneBindingPropertyChangeRequest Request = MakeLevelRequest(*Candidate);
    GetPropertyValue<TSoftObjectPtr<UWorld>>(*Candidate, TEXT("LevelToLoad")) = CandidateWorldScope->GetWorld();
    FGameZoneBindingMutationResult Result = FGameZoneBindingCoordinator::ApplyPropertyChange(Request);
    TestEqual(TEXT("A globally duplicated Zone Id is rejected"), Result.Issue, EGameZoneBindingIssue::DuplicateAssetId);
    TestTrue(TEXT("Rejected duplicate Id bind restores the empty target"), Candidate->GetLevelToLoad().IsNull());

    GetPropertyValue<FRPGId>(*Candidate, TEXT("Id")) = FRPGId(TEXT("z.GlobalUniqueCandidate"));
    Request = MakeLevelRequest(*Candidate);
    GetPropertyValue<TSoftObjectPtr<UWorld>>(*Candidate, TEXT("LevelToLoad")) = OwnerWorldScope->GetWorld();
    Result = FGameZoneBindingCoordinator::ApplyPropertyChange(Request);
    TestEqual(TEXT("A globally duplicated Level target is rejected"), Result.Issue, EGameZoneBindingIssue::DuplicateLevelTarget);
    TestTrue(TEXT("Rejected duplicate target bind restores the empty target"), Candidate->GetLevelToLoad().IsNull());
    TestEqual(TEXT("The exact owner remains unchanged"), Owner->GetGameZoneBindingId(), OwnerBindingId);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneAssetDuplicateNormalizationTest,
    "IronicRPG.GameZone.Binding.Lifecycle.AssetDuplicateIsUnidentified",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneAssetDuplicateNormalizationTest::RunTest(const FString& Parameters)
{
    TUniquePtr<FScopedEditorWorld> WorldScope = CreateBindingTestWorld(TEXT("AssetDuplicate"));
    UGameZoneAsset* Source = CreatePackageZoneAsset(TEXT("DuplicateSource"));
    const FGuid BindingId = FGuid::NewGuid();
    SetAssetBinding(*Source, WorldScope->GetWorld(), FRPGId(TEXT("z.DuplicateSource")), BindingId);
    GetPropertyValue<int32>(*Source, TEXT("MapDataSchemaVersion")) = 1;
    GetPropertyValue<FGuid>(*Source, TEXT("MapBakeRevision")) = FGuid::NewGuid();
    GetPropertyValue<FSoftObjectPath>(*Source, TEXT("BakedSourceLevel")) = Source->GetLevelToLoad().ToSoftObjectPath();
    GetPropertyValue<FString>(*Source, TEXT("MapBakeInputFingerprint")) = TEXT("input");
    GetPropertyValue<FString>(*Source, TEXT("MapBakeOutputFingerprint")) = TEXT("output");

    UGameZoneAsset* Duplicate = DuplicateObject<UGameZoneAsset>(
        Source,
        CreatePackage(TEXT("/Temp/IronicRPG/ZoneLifecycle_DuplicateTarget")),
        TEXT("ZoneLifecycle_DuplicateTarget"));
    TestNotNull(TEXT("The Zone Asset duplicates"), Duplicate);
    if (!Duplicate)
    {
        return false;
    }
    TestFalse(TEXT("Duplicate Id is cleared"), Duplicate->GetId().IsValid());
    TestTrue(TEXT("Duplicate Level is cleared"), Duplicate->GetLevelToLoad().IsNull());
    TestFalse(TEXT("Duplicate binding generation is cleared"), Duplicate->GetGameZoneBindingId().IsValid());
    TestEqual(TEXT("Duplicate bake schema is reset"), Duplicate->GetMapDataSchemaVersion(), 0);
    TestFalse(TEXT("Duplicate bake revision is reset"), Duplicate->GetMapBakeRevision().IsValid());
    TestTrue(TEXT("Duplicate baked source path is reset"), Duplicate->GetBakedSourceLevel().IsNull());
    TestTrue(TEXT("Duplicate input fingerprint is reset"), Duplicate->GetMapBakeInputFingerprint().IsEmpty());
    TestTrue(TEXT("Duplicate output fingerprint is reset"), Duplicate->GetMapBakeOutputFingerprint().IsEmpty());
    TestEqual(
        TEXT("Duplicate verification returns to legacy/unassigned"),
        Duplicate->GetBindingVerificationStatus(),
        EGameZoneBindingVerificationStatus::LegacyUnverified);
    TestEqual(TEXT("Original Id remains intact"), Source->GetId(), FRPGId(TEXT("z.DuplicateSource")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneLevelDuplicateNormalizationTest,
    "IronicRPG.GameZone.Binding.Lifecycle.LevelDuplicateClearsClaim",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneLevelDuplicateNormalizationTest::RunTest(const FString& Parameters)
{
    TUniquePtr<FScopedEditorWorld> WorldScope = CreateBindingTestWorld(TEXT("LevelDuplicate"));
    ARPGWorldSettings* Settings = Cast<ARPGWorldSettings>(WorldScope->GetWorld()->GetWorldSettings());
    TestNotNull(TEXT("The test Level uses RPG World Settings"), Settings);
    if (!Settings)
    {
        return false;
    }
    SetWorldClaim(*Settings, FRPGId(TEXT("z.LevelDuplicate")), FGuid::NewGuid());
    Settings->PostDuplicate(EDuplicateMode::Normal);
    TestFalse(TEXT("A normal Level duplicate clears its Zone Id"), Settings->GetGameZoneId().IsValid());
    TestFalse(TEXT("A normal Level duplicate clears its binding generation"), Settings->GetGameZoneBindingId().IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneChangeIdTransactionTest,
    "IronicRPG.GameZone.Binding.Lifecycle.ChangeZoneIdIsAtomicAndUndoable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneChangeIdTransactionTest::RunTest(const FString& Parameters)
{
    TUniquePtr<FScopedEditorWorld> WorldScope = CreateBindingTestWorld(TEXT("ChangeId"));
    UWorld* World = WorldScope->GetWorld();
    UGameZoneAsset* Asset = NewObject<UGameZoneAsset>(GetTransientPackage(), NAME_None, RF_Transactional);
    ARPGWorldSettings* Settings = Cast<ARPGWorldSettings>(World->GetWorldSettings());
    const FRPGId OldId(TEXT("z.ChangeIdOld"));
    const FRPGId NewId(TEXT("z.ChangeIdNew"));
    const FGuid BindingId = FGuid::NewGuid();
    SetAssetBinding(*Asset, World, OldId, BindingId);
    SetWorldClaim(*Settings, OldId, BindingId);
    FGameZoneBindingPropertyChangeRequest Request;
    Request.ZoneAsset = Asset;
    Request.Change = EGameZoneBindingPropertyChange::ChangeZoneIdCommand;
    Request.PreviousLevel = Asset->GetLevelToLoad().ToSoftObjectPath();
    Request.PreviousZoneId = OldId;
    Request.PreviousBindingId = BindingId;
    Request.PreviousVerificationStatus = Asset->GetBindingVerificationStatus();
    Request.ProposedZoneId = NewId;

    TestTrue(TEXT("Coordinator accepts the exact pair change"), FGameZoneBindingCoordinator::ApplyPropertyChange(Request).IsSuccess());
    TestEqual(TEXT("Asset receives the new Id"), Asset->GetId(), NewId);
    TestEqual(TEXT("Level receives the same new Id"), Settings->GetGameZoneId(), NewId);
    TestEqual(TEXT("Change Id preserves the binding generation"), Asset->GetGameZoneBindingId(), BindingId);

    TestTrue(TEXT("The Change Id transaction can be undone"), GEditor->UndoTransaction());
    TestEqual(TEXT("Undo restores the Asset Id"), Asset->GetId(), OldId);
    TestEqual(TEXT("Undo restores the Level Id"), Settings->GetGameZoneId(), OldId);
    TestTrue(TEXT("The Change Id transaction can be redone"), GEditor->RedoTransaction());
    TestEqual(TEXT("Redo reapplies the Asset Id"), Asset->GetId(), NewId);
    TestEqual(TEXT("Redo reapplies the Level Id"), Settings->GetGameZoneId(), NewId);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneDeleteSafetyTest,
    "IronicRPG.GameZone.Binding.Lifecycle.DeleteVetoAndForceDeleteCompareAndClear",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneDeleteSafetyTest::RunTest(const FString& Parameters)
{
    TUniquePtr<FScopedEditorWorld> WorldScope = CreateBindingTestWorld(TEXT("Delete"));
    UWorld* World = WorldScope->GetWorld();
    UGameZoneAsset* Asset = CreatePackageZoneAsset(TEXT("Delete"));
    ARPGWorldSettings* Settings = Cast<ARPGWorldSettings>(World->GetWorldSettings());
    const FRPGId ZoneId(TEXT("z.DeleteSafety"));
    const FGuid BindingId = FGuid::NewGuid();
    SetAssetBinding(*Asset, World, ZoneId, BindingId);
    SetWorldClaim(*Settings, ZoneId, BindingId);

    FText Reason;
    TestFalse(TEXT("Ordinary Asset delete is vetoed while bound"), FGameZoneBindingCoordinator::CanDeleteObjects({Asset}, &Reason));
    TestFalse(TEXT("Ordinary Level delete is vetoed while claimed"), FGameZoneBindingCoordinator::CanDeleteObjects({World}, &Reason));

    FGameZoneBindingCoordinator::PrepareForceDeleteObjects({Asset});
    TestFalse(TEXT("Force deleting the Asset clears the exact Level Id"), Settings->GetGameZoneId().IsValid());
    TestFalse(TEXT("Force deleting the Asset clears the exact Level generation"), Settings->GetGameZoneBindingId().IsValid());

    SetWorldClaim(*Settings, ZoneId, BindingId);
    FGameZoneBindingCoordinator::PrepareForceDeleteObjects({World});
    TestTrue(TEXT("Force deleting the Level clears the exact Asset target"), Asset->GetLevelToLoad().IsNull());
    TestFalse(TEXT("Force deleting the Level clears the exact Asset generation"), Asset->GetGameZoneBindingId().IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneRenameCanonicalizationTest,
    "IronicRPG.GameZone.Binding.Lifecycle.LevelRenameCanonicalizesAssetPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneRenameCanonicalizationTest::RunTest(const FString& Parameters)
{
    TUniquePtr<FScopedEditorWorld> WorldScope = CreateBindingTestWorld(TEXT("RenameNew"));
    UWorld* World = WorldScope->GetWorld();
    UGameZoneAsset* Asset = CreatePackageZoneAsset(TEXT("Rename"));
    UGameZoneAsset* ConflictAsset = CreatePackageZoneAsset(TEXT("RenameConflict"));
    ARPGWorldSettings* Settings = Cast<ARPGWorldSettings>(World->GetWorldSettings());
    const FRPGId ZoneId(TEXT("z.RenameCanonical"));
    const FGuid BindingId = FGuid::NewGuid();
    const FName OldPackage(TEXT("/Temp/IronicRPG/BindingRenameOld"));
    SetAssetBinding(*Asset, World, ZoneId, BindingId);
    GetPropertyValue<TSoftObjectPtr<UWorld>>(*Asset, TEXT("LevelToLoad")) =
        TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Temp/IronicRPG/BindingRenameOld.BindingRenameOld")));
    SetAssetBinding(*ConflictAsset, World, ZoneId, BindingId);
    GetPropertyValue<TSoftObjectPtr<UWorld>>(*ConflictAsset, TEXT("LevelToLoad")) =
        TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Temp/IronicRPG/BindingRenameOld.BindingRenameOld")));
    SetWorldClaim(*Settings, ZoneId, BindingId);

    AddExpectedError(TEXT("resolves to 2 Zone Assets"), EAutomationExpectedErrorFlags::Contains, 1);
    TestFalse(
        TEXT("An ambiguous renamed binding does not guess a winner"),
        FGameZoneBindingCoordinator::CanonicalizeRenamedWorld(*World, OldPackage));
    TestEqual(
        TEXT("The first ambiguous Asset keeps the old path"),
        Asset->GetLevelToLoad().ToSoftObjectPath().GetLongPackageName(),
        OldPackage.ToString());
    GetPropertyValue<TSoftObjectPtr<UWorld>>(*ConflictAsset, TEXT("LevelToLoad")).Reset();
    GetPropertyValue<FGuid>(*ConflictAsset, TEXT("GameZoneBindingId")).Invalidate();

    TestTrue(
        TEXT("An exact renamed binding is canonicalized"),
        FGameZoneBindingCoordinator::CanonicalizeRenamedWorld(*World, OldPackage));
    TestEqual(
        TEXT("The Asset now targets the renamed World object"),
        Asset->GetLevelToLoad().Get(),
        World);
    TestEqual(
        TEXT("Rename invalidates source freshness"),
        Asset->GetBindingVerificationStatus(),
        EGameZoneBindingVerificationStatus::SourceStale);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

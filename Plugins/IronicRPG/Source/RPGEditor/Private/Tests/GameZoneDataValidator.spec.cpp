// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "GameZoneDataValidator.h"

#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneDataValidationBindingScopeTest,
    "IronicRPG.GameZone.Validation.BindingIssuesReachEveryOwner",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneDataValidationBindingScopeTest::RunTest(const FString& Parameters)
{
    const FSoftObjectPath FirstAsset(TEXT("/Game/Zones/First.First"));
    const FSoftObjectPath SecondAsset(TEXT("/Game/Zones/Second.Second"));
    const FName SharedLevel(TEXT("/Game/Maps/Shared"));

    FGameZoneGlobalBindingAuditResult BindingAudit;
    FGameZoneGlobalBindingIssue& Duplicate = BindingAudit.Issues.AddDefaulted_GetRef();
    Duplicate.Issue = EGameZoneBindingIssue::DuplicateAssetId;
    Duplicate.AssetPath = FirstAsset;
    Duplicate.RelatedAssetPath = SecondAsset;
    Duplicate.Message = FText::FromString(TEXT("Duplicate Zone Id."));

    FGameZoneGlobalBindingIssue& Orphan = BindingAudit.Issues.AddDefaulted_GetRef();
    Orphan.Issue = EGameZoneBindingIssue::OrphanLevelClaim;
    Orphan.LevelPackage = SharedLevel;
    Orphan.Message = FText::FromString(TEXT("Orphan Level claim."));

    FGameZoneValidationSubject SecondAssetSubject;
    SecondAssetSubject.AssetPath = SecondAsset;
    const FGameZoneReleaseValidationResult SecondAssetResult =
        UGameZoneDataValidator::BuildValidationResult(SecondAssetSubject, BindingAudit);
    TestFalse(TEXT("The second duplicate owner is invalid"), SecondAssetResult.IsValid());
    TestEqual(TEXT("The duplicate is reported once"), SecondAssetResult.Errors.Num(), 1);

    FGameZoneValidationSubject LevelSubject;
    LevelSubject.LevelPackage = SharedLevel;
    const FGameZoneReleaseValidationResult LevelResult =
        UGameZoneDataValidator::BuildValidationResult(LevelSubject, BindingAudit);
    TestFalse(TEXT("The orphan Level is invalid"), LevelResult.IsValid());
    TestEqual(TEXT("The orphan claim is reported once"), LevelResult.Errors.Num(), 1);

    FGameZoneValidationSubject UnrelatedSubject;
    UnrelatedSubject.AssetPath = FSoftObjectPath(TEXT("/Game/Zones/Unrelated.Unrelated"));
    TestTrue(
        TEXT("Unrelated assets do not inherit project issues"),
        UGameZoneDataValidator::BuildValidationResult(UnrelatedSubject, BindingAudit).IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneDataValidationBakeGateTest,
    "IronicRPG.GameZone.Validation.BakeMustBeReleaseCurrent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneDataValidationBakeGateTest::RunTest(const FString& Parameters)
{
    FGameZoneValidationSubject Subject;
    Subject.AssetPath = FSoftObjectPath(TEXT("/Game/Zones/Test.Test"));
    const FGameZoneGlobalBindingAuditResult BindingAudit;

    FGameZoneMapBakeAuditResult CurrentBake;
    CurrentBake.bCanEvaluate = true;
    CurrentBake.bSnapshotMatches = true;
    TestTrue(
        TEXT("A saved current Bake passes release validation"),
        UGameZoneDataValidator::BuildValidationResult(Subject, BindingAudit, &CurrentBake).IsValid());

    FGameZoneMapBakeAuditResult StaleBake = CurrentBake;
    StaleBake.bSnapshotMatches = false;
    StaleBake.Issues.Add(FText::FromString(TEXT("The saved Bake is stale.")));
    const FGameZoneReleaseValidationResult StaleResult =
        UGameZoneDataValidator::BuildValidationResult(Subject, BindingAudit, &StaleBake);
    TestFalse(TEXT("A stale Bake fails release validation"), StaleResult.IsValid());
    TestEqual(TEXT("The stale reason is preserved"), StaleResult.Errors.Num(), 1);

    FGameZoneMapBakeAuditResult UnsavedBake = CurrentBake;
    UnsavedBake.bHasUnsavedLevelChanges = true;
    UnsavedBake.Issues.Add(FText::FromString(TEXT("The source Level is unsaved.")));
    TestFalse(
        TEXT("Unsaved authoring fails release validation"),
        UGameZoneDataValidator::BuildValidationResult(Subject, BindingAudit, &UnsavedBake).IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZoneDataValidationSaveTimingTest,
    "IronicRPG.GameZone.Validation.InteractiveWorldSaveDefersBakeAudit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZoneDataValidationSaveTimingTest::RunTest(const FString& Parameters)
{
    TestFalse(
        TEXT("Interactive World save defers Bake validation until the post-save coordinator converges the Asset"),
        UGameZoneDataValidator::ShouldAuditWorldBake(EDataValidationUsecase::Save, false));
    TestTrue(
        TEXT("Cook keeps the World Bake hard gate even though Cook uses the Save validation use case"),
        UGameZoneDataValidator::ShouldAuditWorldBake(EDataValidationUsecase::Save, true));
    TestTrue(
        TEXT("Manual validation audits the World Bake"),
        UGameZoneDataValidator::ShouldAuditWorldBake(EDataValidationUsecase::Manual, false));
    TestTrue(
        TEXT("Commandlet validation audits the World Bake"),
        UGameZoneDataValidator::ShouldAuditWorldBake(EDataValidationUsecase::Commandlet, false));
    return true;
}

#endif

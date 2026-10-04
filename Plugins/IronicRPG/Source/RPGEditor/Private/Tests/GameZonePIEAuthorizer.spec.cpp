// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "GameZonePIEAuthorizer.h"

#include "Levels/GameZoneAsset.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZonePIEBakeClassificationTest,
    "IronicRPG.GameZone.PIE.BakeAuditClassifiesSavedAndUnsavedProblems",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZonePIEBakeClassificationTest::RunTest(const FString& Parameters)
{
    FGameZoneMapBakeAuditResult Audit;
    Audit.bCanEvaluate = true;
    Audit.IssueFingerprint = TEXT("ProblemA");
    Audit.Issues.Add(FText::FromString(TEXT("The saved Bake is stale.")));

    TArray<FGameZonePIEIssue> Issues = FGameZonePIEAuthorizer::BuildBakeIssues(TEXT("/Game/Zones/Test"), Audit);
    TestEqual(TEXT("A saved stale Bake creates one issue"), Issues.Num(), 1);
    if (Issues.Num() != 1)
    {
        return false;
    }
    TestEqual(TEXT("A clean stale snapshot is a saved-Bake issue"), Issues[0].Kind, EGameZonePIEIssueKind::SavedBakeStale);

    Audit.bHasUnsavedLevelChanges = true;
    Issues = FGameZonePIEAuthorizer::BuildBakeIssues(TEXT("/Game/Zones/Test"), Audit);
    TestEqual(TEXT("Unsaved authoring still creates one aggregate issue"), Issues.Num(), 1);
    if (Issues.Num() != 1)
    {
        return false;
    }
    TestEqual(TEXT("Unsaved source wins over saved-stale classification"), Issues[0].Kind, EGameZonePIEIssueKind::UnsavedAuthoring);

    Audit.bHasUnsavedLevelChanges = false;
    Audit.bSnapshotMatches = true;
    Audit.Issues.Reset();
    Issues = FGameZonePIEAuthorizer::BuildBakeIssues(TEXT("/Game/Zones/Test"), Audit);
    TestTrue(TEXT("A verified saved snapshot produces no PIE issue"), Issues.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZonePIEIssueFingerprintTest,
    "IronicRPG.GameZone.PIE.IssuesDeduplicateAndFingerprintDeterministically",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZonePIEIssueFingerprintTest::RunTest(const FString& Parameters)
{
    const FGameZonePIEIssue First{
        EGameZonePIEIssueKind::Binding,
        TEXT("Binding|A"),
        FText::FromString(TEXT("First issue"))};
    const FGameZonePIEIssue Second{
        EGameZonePIEIssueKind::SavedBakeStale,
        TEXT("SavedStale|B"),
        FText::FromString(TEXT("Second issue"))};
    const FGameZonePIEIssue DuplicateFirst{
        EGameZonePIEIssueKind::Binding,
        TEXT("Binding|A"),
        FText::FromString(TEXT("Duplicate callback"))};

    const FGameZonePIEGateResult Forward = FGameZonePIEAuthorizer::BuildGateResult({First, Second, DuplicateFirst});
    const FGameZonePIEGateResult Reverse = FGameZonePIEAuthorizer::BuildGateResult({Second, First});
    TestEqual(TEXT("Repeated reports of the same issue are deduplicated"), Forward.Issues.Num(), 2);
    TestEqual(
        TEXT("Issue ordering does not change the problem fingerprint"),
        Forward.IssueFingerprint,
        Reverse.IssueFingerprint);

    FGameZonePIEIssue Changed = Second;
    Changed.Identity = TEXT("SavedStale|C");
    const FGameZonePIEGateResult Different = FGameZonePIEAuthorizer::BuildGateResult({First, Changed});
    TestNotEqual(
        TEXT("A changed problem set produces a new fingerprint"),
        Forward.IssueFingerprint,
        Different.IssueFingerprint);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameZonePIEContinueOnceTest,
    "IronicRPG.GameZone.PIE.ContinueOnceRequiresExplicitConsent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameZonePIEContinueOnceTest::RunTest(const FString& Parameters)
{
    FGameZonePIEIssue Issue;
    Issue.Identity = TEXT("SavedStale|A");
    Issue.Message = FText::FromString(TEXT("The saved Bake is stale."));
    const FGameZonePIEGateResult Blocked = FGameZonePIEAuthorizer::BuildGateResult({Issue});
    const FGameZonePIEGateResult Allowed;

    TestFalse(
        TEXT("The safe default cancels PIE"),
        FGameZonePIEAuthorizer::ShouldContinueOnce(Blocked, EAppReturnType::No));
    TestTrue(
        TEXT("Explicit Yes permits this PIE request"),
        FGameZonePIEAuthorizer::ShouldContinueOnce(Blocked, EAppReturnType::Yes));
    TestTrue(
        TEXT("A clean audit does not require a dialog decision"),
        FGameZonePIEAuthorizer::ShouldContinueOnce(Allowed, EAppReturnType::No));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

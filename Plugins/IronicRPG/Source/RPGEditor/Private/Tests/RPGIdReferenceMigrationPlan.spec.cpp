// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RPGIdReferenceMigrationPlan.h"

namespace
{
	FRPGIdReferenceMigrationRequest MakeRequest()
	{
		FRPGIdReferenceMigrationRequest Request;
		Request.OldId = FRPGId(TEXT("i1000"));
		Request.NewId = FRPGId(TEXT("i2000"));
		Request.ExpectedOwner = FSoftObjectPath(TEXT("/Game/DataAssets/Item/DA_Old.DA_Old"));
		return Request;
	}

	FRPGIdReferenceMigrationReleaseState MakeReleaseState(const FRPGIdReferenceMigrationRequest& Request)
	{
		FRPGIdReferenceMigrationReleaseState State;
		State.CurrentIds.Add(Request.OldId.Id);
		State.ReservedIds.Add(Request.OldId.Id);
		return State;
	}

	ERPGIdReferenceMigrationArtifactState WritableArtifact(const FRPGIdReferenceMigrationArtifact& Artifact)
	{
		return ERPGIdReferenceMigrationArtifactState::Writable;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceMigrationCertifiedPlanTest,
	"IronicRPG.RPGId.SaveMigration.ReferencePlan.BuildsCompleteCertifiedPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceMigrationCertifiedPlanTest::RunTest(const FString& Parameters)
{
	const FRPGIdReferenceMigrationRequest Request = MakeRequest();
	TMap<FSoftObjectPath, FName> Owners;
	Owners.Add(Request.ExpectedOwner, Request.OldId.Id);

	FRPGIdReferenceAuditResult Audit;
	Audit.IndexGeneration = 42;
	Audit.Hits = {
		{ TEXT("/Game/DataAssets/Character/DA_Hero.DA_Hero"), TEXT("DefaultEquipment{0}.Value.ItemId") },
		{ TEXT("Blueprint:/Game/BP_Controller.BP_Controller"), TEXT("ClassDefaultObject.TestItem") },
		{ TEXT("Blueprint:/Game/BP_Controller.BP_Controller"), TEXT("Template[DefaultSceneRoot].TestItem") },
		{ TEXT("Blueprint:/Game/BP_Controller.BP_Controller"), TEXT("Graph[EventGraph].Node[Call].Pin[ItemId]") },
		{ TEXT("Config:/Script/RPGCore.CharacterSystemSettings"), TEXT("DefaultPartyMembers[0]") },
		{ TEXT("LegacyConfig:/Script/RPGCore.RPGSettings"), TEXT("PlayableCharacters[0]") }
	};

	const FRPGIdReferenceMigrationPlan Plan = FRPGIdReferenceMigrationPlanner::BuildFromEvidence(
		Request, Owners, MakeReleaseState(Request), Audit, WritableArtifact);

	TestTrue(TEXT("Complete certified evidence is ready"), Plan.IsReady());
	TestEqual(TEXT("Every reference has one edit"), Plan.Edits.Num(), 6);
	TestEqual(TEXT("Blueprint index generation is retained"), Plan.ReferenceIndexGeneration, uint64(42));
	TestEqual(TEXT("Required artifacts are deduplicated"), Plan.RequiredArtifacts.Num(), 4);
	TestTrue(TEXT("Every edit retains a reversible old-to-new substitution"),
		Plan.Edits.ContainsByPredicate([&Request](const FRPGIdReferenceMigrationEdit& Edit)
		{
			return Edit.ExpectedValue != Request.OldId || Edit.ReplacementValue != Request.NewId;
		}) == false);
	TestTrue(TEXT("Native object writer is present"), Plan.Edits.ContainsByPredicate([](const FRPGIdReferenceMigrationEdit& Edit)
	{
		return Edit.Writer == ERPGIdReferenceMigrationWriter::NativeObjectProperty;
	}));
	TestTrue(TEXT("Blueprint default writer is present"), Plan.Edits.ContainsByPredicate([](const FRPGIdReferenceMigrationEdit& Edit)
	{
		return Edit.Writer == ERPGIdReferenceMigrationWriter::BlueprintDefaultProperty;
	}));
	TestTrue(TEXT("Blueprint template writer is present"), Plan.Edits.ContainsByPredicate([](const FRPGIdReferenceMigrationEdit& Edit)
	{
		return Edit.Writer == ERPGIdReferenceMigrationWriter::BlueprintTemplateProperty;
	}));
	TestTrue(TEXT("Blueprint graph writer is present"), Plan.Edits.ContainsByPredicate([](const FRPGIdReferenceMigrationEdit& Edit)
	{
		return Edit.Writer == ERPGIdReferenceMigrationWriter::BlueprintGraphLiteral;
	}));
	TestTrue(TEXT("Typed config writer is present"), Plan.Edits.ContainsByPredicate([](const FRPGIdReferenceMigrationEdit& Edit)
	{
		return Edit.Writer == ERPGIdReferenceMigrationWriter::TypedConfigProperty;
	}));
	TestTrue(TEXT("Legacy config writer is present"), Plan.Edits.ContainsByPredicate([](const FRPGIdReferenceMigrationEdit& Edit)
	{
		return Edit.Writer == ERPGIdReferenceMigrationWriter::LegacyConfigProperty;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceMigrationCoverageTest,
	"IronicRPG.RPGId.SaveMigration.ReferencePlan.RejectsGapsAndUncertifiedEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceMigrationCoverageTest::RunTest(const FString& Parameters)
{
	const FRPGIdReferenceMigrationRequest Request = MakeRequest();
	TMap<FSoftObjectPath, FName> Owners;
	Owners.Add(Request.ExpectedOwner, Request.OldId.Id);
	FRPGIdReferenceAuditResult Audit;
	Audit.Hits.Add({ TEXT("Runtime:ConnectedValue"), TEXT("CurrentItem") });
	Audit.CoverageGaps.Add(FText::FromString(TEXT("Blueprint index changed during inspection.")));

	const FRPGIdReferenceMigrationPlan Plan = FRPGIdReferenceMigrationPlanner::BuildFromEvidence(
		Request, Owners, MakeReleaseState(Request), Audit, WritableArtifact);

	TestFalse(TEXT("Unknown evidence and coverage gaps fail closed"), Plan.IsReady());
	TestTrue(TEXT("Coverage gap remains named"), Plan.Issues.ContainsByPredicate([](const FRPGIdReferenceMigrationIssue& Issue)
	{
		return Issue.Code == ERPGIdReferenceMigrationIssueCode::CoverageGap
			&& Issue.Detail.Contains(TEXT("Blueprint index changed"));
	}));
	TestTrue(TEXT("Unknown source remains unsupported"), Plan.Issues.ContainsByPredicate([](const FRPGIdReferenceMigrationIssue& Issue)
	{
		return Issue.Code == ERPGIdReferenceMigrationIssueCode::UnsupportedReference
			&& Issue.Source == TEXT("Runtime:ConnectedValue");
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceMigrationPreflightTest,
	"IronicRPG.RPGId.SaveMigration.ReferencePlan.RejectsOwnerAndArtifactFailures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceMigrationPreflightTest::RunTest(const FString& Parameters)
{
	const FRPGIdReferenceMigrationRequest Request = MakeRequest();
	TMap<FSoftObjectPath, FName> Owners;
	Owners.Add(Request.ExpectedOwner, Request.OldId.Id);
	Owners.Add(FSoftObjectPath(TEXT("/Game/DataAssets/Item/DA_Duplicate.DA_Duplicate")), Request.OldId.Id);
	Owners.Add(FSoftObjectPath(TEXT("/Game/DataAssets/Item/DA_NewOwner.DA_NewOwner")), Request.NewId.Id);
	FRPGIdReferenceAuditResult Audit;
	Audit.Hits = {
		{ TEXT("/Game/DataAssets/Character/DA_ReadOnly.DA_ReadOnly"), TEXT("ItemId") },
		{ TEXT("Blueprint:/Game/BP_Missing.BP_Missing"), TEXT("Graph[EventGraph].Node[Call].Pin[ItemId]") }
	};

	const FRPGIdReferenceMigrationPlan Plan = FRPGIdReferenceMigrationPlanner::BuildFromEvidence(Request, Owners,
		MakeReleaseState(Request), Audit,
		[](const FRPGIdReferenceMigrationArtifact& Artifact)
		{
			return Artifact.Identifier.Contains(TEXT("DA_ReadOnly"))
				? ERPGIdReferenceMigrationArtifactState::ReadOnly : ERPGIdReferenceMigrationArtifactState::Missing;
		});

	TestFalse(TEXT("Owner and artifact failures block the plan"), Plan.IsReady());
	TestTrue(TEXT("Duplicate old owner is rejected"), Plan.Issues.ContainsByPredicate([](const FRPGIdReferenceMigrationIssue& Issue)
	{
		return Issue.Code == ERPGIdReferenceMigrationIssueCode::OldOwnerMismatch;
	}));
	TestTrue(TEXT("Claimed target is rejected"), Plan.Issues.ContainsByPredicate([](const FRPGIdReferenceMigrationIssue& Issue)
	{
		return Issue.Code == ERPGIdReferenceMigrationIssueCode::NewIdClaimed;
	}));
	TestTrue(TEXT("Read-only artifact requires checkout"), Plan.Issues.ContainsByPredicate([](const FRPGIdReferenceMigrationIssue& Issue)
	{
		return Issue.Code == ERPGIdReferenceMigrationIssueCode::CheckoutRequired;
	}));
	TestTrue(TEXT("Missing artifact is rejected"), Plan.Issues.ContainsByPredicate([](const FRPGIdReferenceMigrationIssue& Issue)
	{
		return Issue.Code == ERPGIdReferenceMigrationIssueCode::ArtifactMissing;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceMigrationReleaseBoundaryTest,
	"IronicRPG.RPGId.SaveMigration.ReferencePlan.RequiresCurrentReleaseSourceAndUnreservedTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceMigrationReleaseBoundaryTest::RunTest(const FString& Parameters)
{
	const FRPGIdReferenceMigrationRequest Request = MakeRequest();
	TMap<FSoftObjectPath, FName> Owners;
	Owners.Add(Request.ExpectedOwner, Request.OldId.Id);
	FRPGIdReferenceMigrationReleaseState ReleaseState;
	ReleaseState.ReservedIds.Add(Request.NewId.Id);

	const FRPGIdReferenceMigrationPlan Plan = FRPGIdReferenceMigrationPlanner::BuildFromEvidence(
		Request, Owners, ReleaseState, FRPGIdReferenceAuditResult(), WritableArtifact);

	TestFalse(TEXT("Non-current source and reserved target fail closed"), Plan.IsReady());
	TestTrue(TEXT("Source must belong to the current Release"), Plan.Issues.ContainsByPredicate([](const FRPGIdReferenceMigrationIssue& Issue)
	{
		return Issue.Code == ERPGIdReferenceMigrationIssueCode::OldIdNotCurrentRelease;
	}));
	TestTrue(TEXT("Target must not be reserved by Release history"), Plan.Issues.ContainsByPredicate([](const FRPGIdReferenceMigrationIssue& Issue)
	{
		return Issue.Code == ERPGIdReferenceMigrationIssueCode::NewIdReserved;
	}));
	return true;
}

#endif

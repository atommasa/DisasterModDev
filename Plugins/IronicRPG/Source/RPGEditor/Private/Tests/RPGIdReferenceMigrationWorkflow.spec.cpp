// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RPGIdReferenceMigrationWorkflow.h"

namespace
{
	FRPGIdReferenceMigrationPlan ReadyPlan()
	{
		FRPGIdReferenceMigrationPlan Plan;
		Plan.OldId = FRPGId(TEXT("i1000"));
		Plan.NewId = FRPGId(TEXT("i2000"));
		Plan.ExpectedOwner = FSoftObjectPath(TEXT("/Game/DataAssets/Item/DA_Old.DA_Old"));
		Plan.Summary = FText::FromString(TEXT("Ready plan"));
		FRPGIdReferenceMigrationEdit Edit;
		Edit.Source = TEXT("/Game/BP_Consumer.BP_Consumer");
		Edit.PropertyPath = TEXT("TestItem");
		Edit.ExpectedValue = Plan.OldId;
		Edit.ReplacementValue = Plan.NewId;
		Edit.Artifact = { ERPGIdReferenceMigrationArtifactKind::Package, TEXT("/Game/BP_Consumer") };
		Plan.Edits.Add(Edit);
		Plan.RequiredArtifacts = { Edit.Artifact };
		return Plan;
	}

	class FWorkflowBackend final : public IRPGIdReferenceMigrationWorkflowBackend
	{
	public:
		virtual FRPGIdReferenceMigrationPlan Preview(const FRPGIdReferenceMigrationRequest& Request) override
		{
			++PreviewCount;
			LastRequest = Request;
			return Plan;
		}

		virtual FRPGIdReferenceMigrationApplyResult Apply(const FRPGIdReferenceMigrationPlan& PreviewPlan) override
		{
			++ApplyCount;
			LastAppliedPlan = PreviewPlan;
			return ApplyResult;
		}

		FRPGIdReferenceMigrationPlan Plan = ReadyPlan();
		FRPGIdReferenceMigrationApplyResult ApplyResult;
		FRPGIdReferenceMigrationRequest LastRequest;
		FRPGIdReferenceMigrationPlan LastAppliedPlan;
		int32 PreviewCount = 0;
		int32 ApplyCount = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceMigrationWorkflowPreviewTest,
	"IronicRPG.RPGId.SaveMigration.ReferenceWorkflow.ReadyPreviewIsExplicitAndReadOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceMigrationWorkflowPreviewTest::RunTest(const FString& Parameters)
{
	TSharedRef<FWorkflowBackend> Backend = MakeShared<FWorkflowBackend>();
	FRPGIdReferenceMigrationWorkflow Workflow(Backend);
	const FRPGIdReferenceMigrationWorkflowResult Result = Workflow.Preview(
		{Backend->Plan.OldId, Backend->Plan.NewId, Backend->Plan.ExpectedOwner});

	TestEqual(TEXT("Preview uses the backend once"), Backend->PreviewCount, 1);
	TestEqual(TEXT("Ready plan is actionable"), Result.Code, ERPGIdReferenceMigrationWorkflowCode::Ready);
	TestTrue(TEXT("Ready preview can apply"), Result.CanApply());
	TestTrue(TEXT("Preview names the reference count"), Result.Context.ToString().Contains(TEXT("1 certified edit")));
	TestTrue(TEXT("Preview names the explicit save and seal boundary"), Result.Context.ToString().Contains(TEXT("does not save or seal")));
	TestEqual(TEXT("Preview does not apply"), Backend->ApplyCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceMigrationWorkflowRoutingTest,
	"IronicRPG.RPGId.SaveMigration.ReferenceWorkflow.NonReleaseChangeFallsBackToClaim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceMigrationWorkflowRoutingTest::RunTest(const FString& Parameters)
{
	TSharedRef<FWorkflowBackend> Backend = MakeShared<FWorkflowBackend>();
	Backend->Plan.Issues.Add({ ERPGIdReferenceMigrationIssueCode::OldIdNotCurrentRelease,
		TEXT("The old Id is not current."), FString(), FString() });
	FRPGIdReferenceMigrationWorkflow Workflow(Backend);
	const FRPGIdReferenceMigrationWorkflowResult Result = Workflow.Preview(
		{Backend->Plan.OldId, Backend->Plan.NewId, Backend->Plan.ExpectedOwner});

	TestEqual(TEXT("Non-release Id is not a migration workflow"), Result.Code,
		ERPGIdReferenceMigrationWorkflowCode::NotApplicable);
	TestFalse(TEXT("Non-release Id cannot apply as migration"), Result.CanApply());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceMigrationWorkflowApplyTest,
	"IronicRPG.RPGId.SaveMigration.ReferenceWorkflow.ApplyUsesExactPreviewAndReportsRedirect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceMigrationWorkflowApplyTest::RunTest(const FString& Parameters)
{
	TSharedRef<FWorkflowBackend> Backend = MakeShared<FWorkflowBackend>();
	Backend->ApplyResult.Code = ERPGIdReferenceMigrationApplyCode::Success;
	Backend->ApplyResult.Message = FText::FromString(TEXT("Applied"));
	Backend->ApplyResult.Redirect = { Backend->Plan.OldId, Backend->Plan.NewId };
	FRPGIdReferenceMigrationWorkflow Workflow(Backend);
	const FRPGIdReferenceMigrationWorkflowResult Preview = Workflow.Preview(
		{Backend->Plan.OldId, Backend->Plan.NewId, Backend->Plan.ExpectedOwner});
	const FRPGIdReferenceMigrationWorkflowResult Applied = Workflow.Apply(Preview);

	TestEqual(TEXT("Apply delegates once"), Backend->ApplyCount, 1);
	TestEqual(TEXT("Exact preview plan is retained"), Backend->LastAppliedPlan.ReferenceIndexGeneration,
		Backend->Plan.ReferenceIndexGeneration);
	TestEqual(TEXT("Successful workflow is applied"), Applied.Code, ERPGIdReferenceMigrationWorkflowCode::Applied);
	TestEqual(TEXT("Redirect source is exposed"), Applied.Redirect.OldId, Backend->Plan.OldId);
	TestEqual(TEXT("Redirect target is exposed"), Applied.Redirect.NewId, Backend->Plan.NewId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceMigrationWorkflowBlockedApplyTest,
	"IronicRPG.RPGId.SaveMigration.ReferenceWorkflow.BlockedPreviewNeverApplies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceMigrationWorkflowBlockedApplyTest::RunTest(const FString& Parameters)
{
	TSharedRef<FWorkflowBackend> Backend = MakeShared<FWorkflowBackend>();
	Backend->Plan.Issues.Add({ ERPGIdReferenceMigrationIssueCode::CheckoutRequired,
		TEXT("Check out /Game/BP_Consumer."), TEXT("/Game/BP_Consumer"), FString() });
	FRPGIdReferenceMigrationWorkflow Workflow(Backend);
	const FRPGIdReferenceMigrationWorkflowResult Preview = Workflow.Preview(
		{Backend->Plan.OldId, Backend->Plan.NewId, Backend->Plan.ExpectedOwner});
	const FRPGIdReferenceMigrationWorkflowResult Applied = Workflow.Apply(Preview);

	TestEqual(TEXT("Blocked current-release plan remains visible"), Preview.Code,
		ERPGIdReferenceMigrationWorkflowCode::Blocked);
	TestTrue(TEXT("Blocked preview identifies checkout"), Preview.Context.ToString().Contains(TEXT("Check out")));
	TestEqual(TEXT("Blocked workflow does not delegate Apply"), Backend->ApplyCount, 0);
	TestEqual(TEXT("Blocked Apply request is named as failure"), Applied.Code,
		ERPGIdReferenceMigrationWorkflowCode::Failed);
	return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

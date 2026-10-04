// Copyright Ironic Studio. All Rights Reserved.

#include "RPGIdReferenceMigrationWorkflow.h"

#define LOCTEXT_NAMESPACE "RPGIdReferenceMigrationWorkflow"

namespace
{
	class FProductionBackend final : public IRPGIdReferenceMigrationWorkflowBackend
	{
	public:
		virtual FRPGIdReferenceMigrationPlan Preview(const FRPGIdReferenceMigrationRequest& Request) override
		{
			return FRPGIdReferenceMigrationPlanner::Build(Request);
		}

		virtual FRPGIdReferenceMigrationApplyResult Apply(const FRPGIdReferenceMigrationPlan& PreviewPlan) override
		{
			return FRPGIdReferenceMigrationApplier::Apply(PreviewPlan);
		}
	};

	bool IsNotApplicable(const FRPGIdReferenceMigrationPlan& Plan)
	{
		return Plan.Issues.ContainsByPredicate([](const FRPGIdReferenceMigrationIssue& Issue)
		{
			return Issue.Code == ERPGIdReferenceMigrationIssueCode::OldIdNotCurrentRelease;
		});
	}

	FText BuildReadyContext(const FRPGIdReferenceMigrationPlan& Plan)
	{
		FString Context = FString::Printf(TEXT("%d certified edit(s) across %d artifact(s).\n"), Plan.Edits.Num(),
			Plan.RequiredArtifacts.Num());
		const int32 DisplayLimit = 16;
		for (int32 Index = 0; Index < FMath::Min(Plan.Edits.Num(), DisplayLimit); ++Index)
		{
			const FRPGIdReferenceMigrationEdit& Edit = Plan.Edits[Index];
			Context += Edit.Source + TEXT(" :: ") + Edit.PropertyPath + TEXT("\n");
		}
		if (Plan.Edits.Num() > DisplayLimit)
		{
			Context += FString::Printf(TEXT("... and %d more certified edit(s).\n"), Plan.Edits.Num() - DisplayLimit);
		}
		Context += TEXT("Apply changes the owner and every listed reference in one Undo transaction; it does not save or seal artifacts.");
		return FText::FromString(Context);
	}

	FText BuildBlockedContext(const FRPGIdReferenceMigrationPlan& Plan)
	{
		FString Context;
		for (const FRPGIdReferenceMigrationIssue& Issue : Plan.Issues)
		{
			Context += Issue.Detail;
			if (!Issue.Source.IsEmpty())
			{
				Context += TEXT("\n") + Issue.Source;
				if (!Issue.PropertyPath.IsEmpty())
				{
					Context += TEXT(" :: ") + Issue.PropertyPath;
				}
			}
			Context += TEXT("\n");
		}
		return FText::FromString(Context.TrimEnd());
	}
}

FRPGIdReferenceMigrationWorkflow::FRPGIdReferenceMigrationWorkflow()
	: Backend(MakeShared<FProductionBackend>())
{
}

FRPGIdReferenceMigrationWorkflow::FRPGIdReferenceMigrationWorkflow(
	TSharedRef<IRPGIdReferenceMigrationWorkflowBackend> InBackend)
	: Backend(MoveTemp(InBackend))
{
}

FRPGIdReferenceMigrationWorkflowResult FRPGIdReferenceMigrationWorkflow::Preview(
	const FRPGIdReferenceMigrationRequest& Request) const
{
	FRPGIdReferenceMigrationWorkflowResult Result;
	Result.Plan = Backend->Preview(Request);
	Result.Message = Result.Plan.Summary;
	if (IsNotApplicable(Result.Plan))
	{
		Result.Code = ERPGIdReferenceMigrationWorkflowCode::NotApplicable;
		return Result;
	}
	if (!Result.Plan.IsReady())
	{
		Result.Code = ERPGIdReferenceMigrationWorkflowCode::Blocked;
		Result.Context = BuildBlockedContext(Result.Plan);
		return Result;
	}
	Result.Code = ERPGIdReferenceMigrationWorkflowCode::Ready;
	Result.Context = BuildReadyContext(Result.Plan);
	return Result;
}

FRPGIdReferenceMigrationWorkflowResult FRPGIdReferenceMigrationWorkflow::Apply(
	const FRPGIdReferenceMigrationWorkflowResult& PreviewResult) const
{
	if (!PreviewResult.CanApply())
	{
		FRPGIdReferenceMigrationWorkflowResult Result = PreviewResult;
		Result.Code = ERPGIdReferenceMigrationWorkflowCode::Failed;
		Result.Message = LOCTEXT("FreshPreviewRequired", "A ready reference migration Preview is required; nothing was modified.");
		return Result;
	}
	const FRPGIdReferenceMigrationApplyResult Applied = Backend->Apply(PreviewResult.Plan);
	FRPGIdReferenceMigrationWorkflowResult Result = PreviewResult;
	Result.Code = Applied.IsSuccess() ? ERPGIdReferenceMigrationWorkflowCode::Applied
		: ERPGIdReferenceMigrationWorkflowCode::Failed;
	Result.Message = Applied.Message;
	Result.Redirect = Applied.Redirect;
	return Result;
}

#undef LOCTEXT_NAMESPACE
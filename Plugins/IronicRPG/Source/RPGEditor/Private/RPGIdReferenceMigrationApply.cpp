// Copyright Ironic Studio. All Rights Reserved.

#include "RPGIdReferenceMigrationApply.h"

#include "RPGIdReferenceMigrationWorkspace.h"

#define LOCTEXT_NAMESPACE "RPGIdReferenceMigrationApply"

namespace
{
	bool ArtifactsEqual(const FRPGIdReferenceMigrationArtifact& A, const FRPGIdReferenceMigrationArtifact& B)
	{
		return A == B;
	}

	bool EditsEqual(const FRPGIdReferenceMigrationEdit& A, const FRPGIdReferenceMigrationEdit& B)
	{
		return A.Writer == B.Writer && A.Source == B.Source && A.PropertyPath == B.PropertyPath
			&& A.ExpectedValue == B.ExpectedValue && A.ReplacementValue == B.ReplacementValue && A.Artifact == B.Artifact;
	}

	bool IssuesEqual(const FRPGIdReferenceMigrationIssue& A, const FRPGIdReferenceMigrationIssue& B)
	{
		return A.Code == B.Code && A.Detail == B.Detail && A.Source == B.Source && A.PropertyPath == B.PropertyPath;
	}

	template <typename ValueType, typename PredicateType>
	bool ArraysEqual(const TArray<ValueType>& A, const TArray<ValueType>& B, PredicateType Predicate)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (!Predicate(A[Index], B[Index]))
			{
				return false;
			}
		}
		return true;
	}

	bool PlansEqual(const FRPGIdReferenceMigrationPlan& A, const FRPGIdReferenceMigrationPlan& B)
	{
		return A.OldId == B.OldId && A.NewId == B.NewId && A.ExpectedOwner == B.ExpectedOwner
			&& A.ReferenceIndexGeneration == B.ReferenceIndexGeneration
			&& ArraysEqual(A.Edits, B.Edits, EditsEqual)
			&& ArraysEqual(A.RequiredArtifacts, B.RequiredArtifacts, ArtifactsEqual)
			&& ArraysEqual(A.Issues, B.Issues, IssuesEqual);
	}

	FRPGIdReferenceMigrationApplyResult Failure(const ERPGIdReferenceMigrationApplyCode Code, const FText& Message)
	{
		FRPGIdReferenceMigrationApplyResult Result;
		Result.Code = Code;
		Result.Message = Message;
		return Result;
	}

	FRPGIdReferenceMigrationApplyResult RollbackFailure(IRPGIdReferenceMigrationApplyWorkspace& Workspace,
		const ERPGIdReferenceMigrationApplyCode Code, const FText& Error)
	{
		FText RollbackError;
		if (!Workspace.Rollback(RollbackError))
		{
			return Failure(ERPGIdReferenceMigrationApplyCode::RollbackFailed,
				FText::Format(LOCTEXT("RollbackFailed", "Migration failed and rollback also failed: {0}"), RollbackError));
		}
		return Failure(Code, Error);
	}
}

FRPGIdReferenceMigrationApplyResult FRPGIdReferenceMigrationApplier::Apply(const FRPGIdReferenceMigrationPlan& PreviewPlan)
{
	if (!PreviewPlan.IsReady())
	{
		return Failure(ERPGIdReferenceMigrationApplyCode::PlanNotReady,
			LOCTEXT("PreviewNotReady", "Only a complete ready Preview can be applied; nothing was modified."));
	}
	const FRPGIdReferenceMigrationRequest Request{PreviewPlan.OldId, PreviewPlan.NewId, PreviewPlan.ExpectedOwner};
	const FRPGIdReferenceMigrationPlan FreshPlan = FRPGIdReferenceMigrationPlanner::Build(Request);
	TUniquePtr<IRPGIdReferenceMigrationApplyWorkspace> Workspace = RPGIdReferenceMigrationPrivate::CreateProductionWorkspace();
	return ApplyPrepared(PreviewPlan, FreshPlan, *Workspace);
}

FRPGIdReferenceMigrationApplyResult FRPGIdReferenceMigrationApplier::ApplyPrepared(
	const FRPGIdReferenceMigrationPlan& PreviewPlan, const FRPGIdReferenceMigrationPlan& FreshPlan,
	IRPGIdReferenceMigrationApplyWorkspace& Workspace)
{
	if (!PreviewPlan.IsReady() || !FreshPlan.IsReady())
	{
		return Failure(ERPGIdReferenceMigrationApplyCode::PlanNotReady,
			LOCTEXT("PlanNotReady", "Reference migration requires two complete ready plans; nothing was modified."));
	}
	if (!PlansEqual(PreviewPlan, FreshPlan))
	{
		return Failure(ERPGIdReferenceMigrationApplyCode::StalePlan,
			LOCTEXT("StalePlan", "The workspace changed since Preview. Preview again before Apply; nothing was modified."));
	}

	FText Error;
	if (!Workspace.Begin(FreshPlan, Error))
	{
		return Failure(ERPGIdReferenceMigrationApplyCode::PreflightFailed, Error);
	}
	for (const FRPGIdReferenceMigrationEdit& Edit : FreshPlan.Edits)
	{
		if (!Workspace.ApplyEdit(Edit, Error))
		{
			return RollbackFailure(Workspace, ERPGIdReferenceMigrationApplyCode::WriterFailed, Error);
		}
	}

	const FRPGIdReferenceMigrationRequest Request{FreshPlan.OldId, FreshPlan.NewId, FreshPlan.ExpectedOwner};
	if (!Workspace.ApplyOwner(Request, Error))
	{
		return RollbackFailure(Workspace, ERPGIdReferenceMigrationApplyCode::OwnerFailed, Error);
	}
	if (!Workspace.VerifyOldIdAbsent(FreshPlan.OldId, Error))
	{
		return RollbackFailure(Workspace, ERPGIdReferenceMigrationApplyCode::ReverseAuditFailed, Error);
	}
	const FRPGIdRedirect Redirect(FreshPlan.OldId, FreshPlan.NewId);
	if (!Workspace.StageRedirect(Redirect, Error))
	{
		return RollbackFailure(Workspace, ERPGIdReferenceMigrationApplyCode::HandoffFailed, Error);
	}
	if (!Workspace.Commit(Error))
	{
		return RollbackFailure(Workspace, ERPGIdReferenceMigrationApplyCode::CommitFailed, Error);
	}

	FRPGIdReferenceMigrationApplyResult Result;
	Result.Code = ERPGIdReferenceMigrationApplyCode::Success;
	Result.Message = LOCTEXT("Success",
		"References and owner were migrated atomically. Packages remain dirty; save them explicitly, then seal the redirect.");
	Result.Redirect = Redirect;
	Result.ModifiedArtifacts = FreshPlan.RequiredArtifacts;
	return Result;
}

#undef LOCTEXT_NAMESPACE

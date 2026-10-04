// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RPGIdMigration.h"
#include "RPGIdReferenceMigrationPlan.h"

enum class ERPGIdReferenceMigrationApplyCode : uint8
{
	Success,
	PlanNotReady,
	StalePlan,
	PreflightFailed,
	WriterFailed,
	OwnerFailed,
	ReverseAuditFailed,
	HandoffFailed,
	CommitFailed,
	RollbackFailed
};

struct FRPGIdReferenceMigrationApplyResult
{
	bool IsSuccess() const { return Code == ERPGIdReferenceMigrationApplyCode::Success; }

	ERPGIdReferenceMigrationApplyCode Code = ERPGIdReferenceMigrationApplyCode::PreflightFailed;
	FText Message;
	FRPGIdRedirect Redirect;
	TArray<FRPGIdReferenceMigrationArtifact> ModifiedArtifacts;
};

/** Internal workspace seam. Begin must fully preflight and may not mutate on failure. */
class IRPGIdReferenceMigrationApplyWorkspace
{
public:
	virtual ~IRPGIdReferenceMigrationApplyWorkspace() = default;
	virtual bool Begin(const FRPGIdReferenceMigrationPlan& Plan, FText& OutError) = 0;
	virtual bool ApplyEdit(const FRPGIdReferenceMigrationEdit& Edit, FText& OutError) = 0;
	virtual bool ApplyOwner(const FRPGIdReferenceMigrationRequest& Request, FText& OutError) = 0;
	virtual bool VerifyOldIdAbsent(const FRPGId& OldId, FText& OutError) = 0;
	virtual bool StageRedirect(const FRPGIdRedirect& Redirect, FText& OutError) = 0;
	virtual bool Commit(FText& OutError) = 0;
	virtual bool Rollback(FText& OutError) = 0;
};

/** Applies one exact fresh plan as an all-or-nothing Editor transaction. It never saves packages. */
class FRPGIdReferenceMigrationApplier
{
public:
	static FRPGIdReferenceMigrationApplyResult Apply(const FRPGIdReferenceMigrationPlan& PreviewPlan);

	static FRPGIdReferenceMigrationApplyResult ApplyPrepared(const FRPGIdReferenceMigrationPlan& PreviewPlan,
		const FRPGIdReferenceMigrationPlan& FreshPlan, IRPGIdReferenceMigrationApplyWorkspace& Workspace);
};

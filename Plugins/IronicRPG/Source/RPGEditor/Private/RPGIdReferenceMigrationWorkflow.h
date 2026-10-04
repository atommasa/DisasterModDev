// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RPGIdReferenceMigrationApply.h"

enum class ERPGIdReferenceMigrationWorkflowCode : uint8
{
	NotApplicable,
	Blocked,
	Ready,
	Applied,
	Failed
};

struct FRPGIdReferenceMigrationWorkflowResult
{
	bool IsApplicable() const { return Code != ERPGIdReferenceMigrationWorkflowCode::NotApplicable; }
	bool CanApply() const { return Code == ERPGIdReferenceMigrationWorkflowCode::Ready; }

	ERPGIdReferenceMigrationWorkflowCode Code = ERPGIdReferenceMigrationWorkflowCode::NotApplicable;
	FText Message;
	FText Context;
	FRPGIdReferenceMigrationPlan Plan;
	FRPGIdRedirect Redirect;
};

/** Adapter seam used by the authoring workflow and its tests. */
class IRPGIdReferenceMigrationWorkflowBackend
{
public:
	virtual ~IRPGIdReferenceMigrationWorkflowBackend() = default;
	virtual FRPGIdReferenceMigrationPlan Preview(const FRPGIdReferenceMigrationRequest& Request) = 0;
	virtual FRPGIdReferenceMigrationApplyResult Apply(const FRPGIdReferenceMigrationPlan& PreviewPlan) = 0;
};

/** Owns Preview/Apply routing and presentation for the authoring UI. It never saves or seals artifacts. */
class FRPGIdReferenceMigrationWorkflow
{
public:
	FRPGIdReferenceMigrationWorkflow();
	explicit FRPGIdReferenceMigrationWorkflow(TSharedRef<IRPGIdReferenceMigrationWorkflowBackend> InBackend);

	FRPGIdReferenceMigrationWorkflowResult Preview(const FRPGIdReferenceMigrationRequest& Request) const;
	FRPGIdReferenceMigrationWorkflowResult Apply(const FRPGIdReferenceMigrationWorkflowResult& PreviewResult) const;

private:
	TSharedRef<IRPGIdReferenceMigrationWorkflowBackend> Backend;
};
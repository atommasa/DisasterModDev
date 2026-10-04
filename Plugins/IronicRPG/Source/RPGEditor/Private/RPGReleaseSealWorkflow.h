// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RPGReleaseSealService.h"

struct FRPGReleaseSealWorkflowResult
{
	bool bSucceeded = false;
	bool bCleanupWarning = false;
	FText Message;
	TArray<FRPGIdRedirect> Redirects;
};

/** Adapter seam shared by the Release Seal UI and workflow tests. */
class IRPGReleaseSealWorkflowBackend
{
public:
	virtual ~IRPGReleaseSealWorkflowBackend() = default;
	virtual bool ReadPending(TArray<FRPGIdRedirect>& OutRedirects, FText& OutError) = 0;
	virtual bool Run(ERPGReleaseSealAction Action, const FString& ReleaseId, const FString& SaveVersion,
		TConstArrayView<FRPGIdRedirect> Redirects, FText& OutMessage) = 0;
	virtual bool Consume(TConstArrayView<FRPGIdRedirect> Redirects, FText& OutError) = 0;
};

/** Owns pending redirect presentation and exact routing into Release Seal. */
class FRPGReleaseSealWorkflow
{
public:
	FRPGReleaseSealWorkflow();
	explicit FRPGReleaseSealWorkflow(TSharedRef<IRPGReleaseSealWorkflowBackend> InBackend);

	FText DescribePending() const;
	FRPGReleaseSealWorkflowResult Execute(ERPGReleaseSealAction Action, const FString& ReleaseId,
		const FString& SaveVersion) const;

private:
	TSharedRef<IRPGReleaseSealWorkflowBackend> Backend;
};

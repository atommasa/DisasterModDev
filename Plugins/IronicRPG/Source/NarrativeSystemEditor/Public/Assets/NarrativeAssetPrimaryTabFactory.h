// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

/**
 * This class is responsible for creating the primary tab for the Narrative Asset Editor.
 */
class NarrativeAssetPrimaryTabFactory : public FWorkflowTabFactory
{
public:
	NarrativeAssetPrimaryTabFactory(TSharedPtr<class NarrativeAssetEditorApp> InApp);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

private:
	TWeakPtr<class NarrativeAssetEditorApp> _App;

};

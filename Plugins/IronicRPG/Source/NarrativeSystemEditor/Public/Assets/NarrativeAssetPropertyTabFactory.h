// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

/**
 * This class is responsible for creating the primary tab for the Narrative Asset Editor.
 */
class NarrativeAssetPropertyTabFactory : public FWorkflowTabFactory
{
public:
	NarrativeAssetPropertyTabFactory(TSharedPtr<class NarrativeAssetEditorApp> InApp);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

private:
	TWeakPtr<class NarrativeAssetEditorApp> _App;

};

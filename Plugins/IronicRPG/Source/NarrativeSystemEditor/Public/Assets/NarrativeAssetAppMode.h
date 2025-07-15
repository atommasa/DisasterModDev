// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintEditorModes.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

struct FNarrativeAssetAppModes
{
	static const FName NarrativeDefaultMode;
	static const FName NarrativeEventGraphMode;

	static FText GetLocalizedMode(FName InMode);
};

/**
 * 
 */
class NarrativeAssetAppMode : public FBlueprintEditorApplicationMode
{
public:
	NarrativeAssetAppMode(TSharedPtr<class NarrativeAssetEditorApp> App);

	virtual void RegisterTabFactories(TSharedPtr<class FTabManager> InTabManager) override;
	virtual void PreDeactivateMode() override;
	virtual void PostActivateMode() override;

private:
	TWeakPtr<class NarrativeAssetEditorApp> _App;
};

//class NarrativeEventGraphAppMode : public FApplicationMode
//{
//public:
//	NarrativeEventGraphAppMode(TSharedPtr<class FBlueprintEditor> InBlueprintEditor, FName InModeName, FText(*GetLocalizedMode)(const FName), const bool bRegisterViewport, const bool bRegisterDefaultsTab);
//
//	virtual void RegisterTabFactories(TSharedPtr<class FTabManager> InTabManager) override;
//	virtual void PreDeactivateMode() override;
//	virtual void PostActivateMode() override;
//
//private:
//	TWeakPtr<class NarrativeAssetEditorApp> _App;
//	FWorkflowAllowedTabSet _Tabs;
//};

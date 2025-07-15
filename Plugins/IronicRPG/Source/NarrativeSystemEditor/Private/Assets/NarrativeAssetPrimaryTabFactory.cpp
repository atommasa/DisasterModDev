// Fill out your copyright notice in the Description page of Project Settings.


#include "Assets/NarrativeAssetPrimaryTabFactory.h"
#include "Assets/NarrativeAssetEditorApp.h"
#include "NarrativeAsset.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "GraphEditor.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Nodes/NarrativeNodeInfo.h"

NarrativeAssetPrimaryTabFactory::NarrativeAssetPrimaryTabFactory(TSharedPtr<class NarrativeAssetEditorApp> InApp)
	: FWorkflowTabFactory(FName("NarrativeAssetPrimaryTab"), InApp)
{
	_App = InApp;
	TabLabel = FText::FromString("Primary");
	ViewMenuDescription = FText::FromString("Displays a primary view.");
	ViewMenuTooltip = FText::FromString("Show the primary view.");
}

TSharedRef<SWidget> NarrativeAssetPrimaryTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<NarrativeAssetEditorApp> App = _App.Pin();
	if (!App.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	SGraphEditor::FGraphEditorEvents GraphEvents;
	GraphEvents.OnSelectionChanged.BindRaw(App.Get(), &NarrativeAssetEditorApp::OnGraphSelectionChanged);

	TSharedPtr<SGraphEditor> GraphEditor =
		SNew(SGraphEditor)
		.IsEditable(true)
		.GraphEvents(GraphEvents)
		.GraphToEdit(App->GetWorkingGraph())
		.AdditionalCommands(App->GetCommandList());

	App->SetWorkingGraphUI(GraphEditor);
	
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.HAlign(HAlign_Fill)
		[
			GraphEditor.ToSharedRef()
		];
}

FText NarrativeAssetPrimaryTabFactory::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const
{
	return FText::FromString(TEXT("A primary view for doing primary things."));
}

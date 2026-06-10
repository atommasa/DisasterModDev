// Copyright Ironic Studio. All Rights Reserved.


#include "Assets/NarrativeAssetPropertyTabFactory.h"
#include "Assets/NarrativeAssetEditorApp.h"
#include "NarrativeAsset.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"

NarrativeAssetPropertyTabFactory::NarrativeAssetPropertyTabFactory(TSharedPtr<class NarrativeAssetEditorApp> InApp)
	: FWorkflowTabFactory(FName("Inspector"), InApp)
{
	_App = InApp;
	TabLabel = FText::FromString("Properties");
	ViewMenuDescription = FText::FromString("Displays the properties view for the current asset.");
	ViewMenuTooltip = FText::FromString("Show the properties view.");
}

TSharedRef<SWidget> NarrativeAssetPropertyTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<NarrativeAssetEditorApp> App = _App.Pin();
	if (!App.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	// Create a details view for the Narrative Asset
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bSearchInitialKeyFocus = true;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.NotifyHook = App.Get();
	DetailsViewArgs.bShowOptions = true;
	DetailsViewArgs.bShowModifiedPropertiesOption = false;
	DetailsViewArgs.bShowScrollBar = false;

	TSharedPtr<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	// Set the object to be displayed in the details view
	DetailsView->SetObject(App->GetClassDefaultsObject());

	DetailsView->OnFinishedChangingProperties().AddLambda(
		[WeakApp = _App](const FPropertyChangedEvent& Event)
		{
			TSharedPtr<NarrativeAssetEditorApp> App = WeakApp.Pin();
			if (!App.IsValid())
			{
				return;
			}

			App->OnFinishedChangingProperties(Event);
		}
	);

	TSharedPtr<IDetailsView> SelectedNodeDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	SelectedNodeDetailsView->SetObject(nullptr);
	App->SetSelectedNodeDetailView(SelectedNodeDetailsView);

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.HAlign(HAlign_Fill)
		[
			DetailsView.ToSharedRef()
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.HAlign(HAlign_Fill)
		[
			SelectedNodeDetailsView.ToSharedRef()
		];
}

FText NarrativeAssetPropertyTabFactory::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const
{
	return FText::FromString(TEXT("A properties view for the current asset."));
}

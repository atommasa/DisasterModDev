// Copyright Ironic Studio. All Rights Reserved.


#include "Assets/EventGraphTabFactory.h"
#include "Assets/NarrativeAssetEditorApp.h"
#include "Kismet2/KismetEditorUtilities.h"

EventGraphTabFactory::EventGraphTabFactory(TSharedPtr<class NarrativeAssetEditorApp> InApp)
    : FWorkflowTabFactory(FName("EventGraphTab"), InApp)
{
    _App = InApp;
    bIsSingleton = true;
    TabLabel = FText::FromString("Event Graph");
    ViewMenuDescription = FText::FromString("Edit blueprint events.");
    ViewMenuTooltip = FText::FromString("Edit the shared blueprint event graph.");
}

TSharedRef<SWidget> EventGraphTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
    TSharedPtr<NarrativeAssetEditorApp> App = _App.Pin();
    if (!App.IsValid())
    {
        return SNullWidget::NullWidget;
    }

    UEdGraph* EventGraph = App->GetOrCreateSharedEventGraph();

    TSharedPtr<SGraphEditor> GraphEditor = SNew(SGraphEditor)
        .AdditionalCommands(App->GetCommandList())
        .GraphToEdit(EventGraph)
        .IsEditable(true)
        .AutoExpandActionMenu(true);

    if (EventGraph)
    {
        FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(EventGraph);
    }
    
    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .HAlign(HAlign_Fill)
        [
            GraphEditor.ToSharedRef()
        ];
}

FText EventGraphTabFactory::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const
{
    return FText::FromString("This is the shared event graph for narrative logic.");
}

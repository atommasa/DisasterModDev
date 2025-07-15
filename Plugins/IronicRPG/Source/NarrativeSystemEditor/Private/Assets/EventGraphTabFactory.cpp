// Fill out your copyright notice in the Description page of Project Settings.


#include "Assets/EventGraphTabFactory.h"
#include "Assets/NarrativeAssetEditorApp.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "NarrativeAsset.h"
#include "SGraphActionMenu.h"

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

    SGraphEditor::FGraphEditorEvents GraphEvents;
    //GraphEvents.OnCreateActionMenu = SGraphEditor::FOnCreateActionMenu::CreateLambda(
    //    [](UEdGraph* Graph, const FVector2D& NodePosition, const TArray<UEdGraphPin*>& DragFromPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed MenuClosedCallback)
    //    {
    //        FGraphContextMenuBuilder ContextMenuBuilder(Graph);
    //        Graph->GetSchema()->GetGraphContextActions(ContextMenuBuilder);

    //        return SNew(SGraphActionMenu, false)
    //            .OnActionSelected(SGraphActionMenu::FOnActionSelected())
    //            .AutoExpandActionMenu(bAutoExpand)
    //            //.ActionMenuBuilder(ContextMenuBuilder)
    //            .OnMenuClosed(MenuClosedCallback);
    //    }
    //);

    TSharedPtr<SGraphEditor> GraphEditor = SNew(SGraphEditor)
        .AdditionalCommands(App->GetCommandList())
        .GraphToEdit(App->GetNarrativeAsset()->UbergraphPages.Last())
        .IsEditable(true)
        .AutoExpandActionMenu(true);

    FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(App->GetNarrativeAsset()->UbergraphPages.Last());
    
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

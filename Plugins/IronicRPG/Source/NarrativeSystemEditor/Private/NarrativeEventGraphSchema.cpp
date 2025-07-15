// Fill out your copyright notice in the Description page of Project Settings.


#include "NarrativeEventGraphSchema.h"

void UNarrativeEventGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	GetGraphNodeActions(ContextMenuBuilder, FString());
	GetCommentAction(ContextMenuBuilder, ContextMenuBuilder.CurrentGraph);
}

void UNarrativeEventGraphSchema::GetGraphNodeActions(FGraphActionMenuBuilder& ActionMenuBuilder, const FString& InCategory) const
{
    // In most cases, base node considered as an abstract node, for test add base node here
    if (const UEdGraphNode* BaseGraphNode = UEdGraphNode::StaticClass()->GetDefaultObject<UEdGraphNode>())
    {
        const TSharedPtr<FNarrativeNewNodeAction_EventGraph> NewNodeAction(new FNarrativeNewNodeAction_EventGraph());
        ActionMenuBuilder.AddAction(NewNodeAction);
    }

    TArray<UClass*> GraphSampleNodes;
    GetDerivedClasses(UEdGraphNode::StaticClass(), GraphSampleNodes);
    for (const UClass* FocusesClass : GraphSampleNodes)
    {
        if (const UEdGraphNode* FocusesGraphNode = FocusesClass->GetDefaultObject<UEdGraphNode>())
        {
            if (InCategory.IsEmpty())
            {
                const TSharedPtr<FNarrativeNewNodeAction_EventGraph> NewNodeAction(new FNarrativeNewNodeAction_EventGraph());
                ActionMenuBuilder.AddAction(NewNodeAction);
            }
        }
    }
    
    // Maybe need add BP extension categories here
}

void UNarrativeEventGraphSchema::GetCommentAction(FGraphActionMenuBuilder& ActionMenuBuilder, const UEdGraph* CurrentGraph) const
{
    /*if (!ActionMenuBuilder.FromPin)
    {
        const bool bIsManyNodesSelected = CurrentGraph ? (->GetNumberOfSelectedNodes() > 0) : false;
        const FText MenuDescription = bIsManyNodesSelected ? LOCTEXT("CreateCommentAction", "Create Comment from Selection") : LOCTEXT("AddCommentAction", "Add Comment...");
        const FText ToolTip = LOCTEXT("CreateCommentToolTip", "Creates a comment.");

        const TSharedPtr<FGraphSampleGraphSchemaAction_NewComment> NewAction(new FGraphSampleGraphSchemaAction_NewComment(FText::GetEmpty(), MenuDescription, ToolTip, 0));
        ActionMenuBuilder.AddAction(NewAction);
    }*/
}


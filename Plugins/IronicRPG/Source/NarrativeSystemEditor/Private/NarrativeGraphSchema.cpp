// Copyright Ironic Studio. All Rights Reserved.

#include "NarrativeGraphSchema.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "Narrative/Dialogue.h"
#include "Nodes/NarrativeBranchNode.h"
#include "Nodes/NarrativeCutsceneNode.h"
#include "Nodes/NarrativeDialogueNode.h"
#include "Nodes/NarrativeDialogueNodeInfo.h"
#include "Nodes/NarrativeGraphNodeBase.h"
#include "Nodes/NarrativeNodeKnot.h"
#include "Nodes/NarrativePlayerOptionsNode.h"
#include "Nodes/NarrativeSetVariablesNode.h"
#include "Nodes/NarrativeStartGraphNode.h"

void UNarrativeGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
    UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(ContextMenuBuilder.CurrentGraph);
    if (!Blueprint || !Blueprint->GeneratedClass)
    {
        return;
    }

    UDialogue* Dialogue = Cast<UDialogue>(Blueprint->GeneratedClass->GetDefaultObject());
    if (!Dialogue)
    {
        return;
    }

    for (const FSpeakerData& Data : Dialogue->Speakers)
    {
        const FText DisplayName = Data.GetSpeakerDisplayName();

        TSharedPtr<FNewNodeAction> NewNodeAction = MakeShareable(new FNewNodeAction(
            FText::FromString(TEXT("Dialogue Nodes")),
            FText::Format(
                NSLOCTEXT("NarrativeEditor", "AddNarrativeNode", "Add dialogue node for {0}"),
                DisplayName
            ),
            FText::FromString(TEXT("Makes a new dialogue node")),
            UNarrativeDialogueNode::StaticClass()
        ));

        NewNodeAction->SpeakerData = Data;
        ContextMenuBuilder.AddAction(NewNodeAction);
    }

    ContextMenuBuilder.AddAction(MakeShareable(new FNewNodeAction(
        FText::FromString(TEXT("Dialogue Nodes")),
        NSLOCTEXT("NarrativeEditor", "AddPlayerOptionsNode", "Add player options node"),
        NSLOCTEXT("NarrativeEditor", "AddPlayerOptionsNodeTooltip", "Makes a new player options node"),
        UNarrativePlayerOptionsNode::StaticClass()
    )));

    ContextMenuBuilder.AddAction(MakeShareable(new FNewNodeAction(
        FText::FromString(TEXT("Presentation Nodes")),
        NSLOCTEXT("NarrativeEditor", "AddCutsceneNode", "Add cutscene node"),
        NSLOCTEXT("NarrativeEditor", "AddCutsceneNodeTooltip", "Makes a new cutscene node"),
        UNarrativeCutsceneNode::StaticClass()
    )));

    ContextMenuBuilder.AddAction(MakeShareable(new FNewNodeAction(
        FText::FromString(TEXT("Flow Nodes")),
        NSLOCTEXT("NarrativeEditor", "AddBranchNode", "Add branch node"),
        NSLOCTEXT("NarrativeEditor", "AddBranchNodeTooltip", "Makes a new branch node"),
        UNarrativeBranchNode::StaticClass()
    )));

    ContextMenuBuilder.AddAction(MakeShareable(new FNewNodeAction(
        FText::FromString(TEXT("Game Logic Nodes")),
        NSLOCTEXT("NarrativeEditor", "AddSetVariablesNode", "Add set variables node"),
        NSLOCTEXT("NarrativeEditor", "AddSetVariablesTooltip", "Makes a new set variables node"),
        UNarrativeSetVariablesNode::StaticClass()
    )));

    ContextMenuBuilder.AddAction(MakeShareable(new FNewNodeAction(
        FText(),
        NSLOCTEXT("NarrativeEditor", "AddRerouteNode", "Add reroute node"),
        NSLOCTEXT("NarrativeEditor", "AddRerouteNodeTooltip", "Makes a new reroute node"),
        UNarrativeNodeKnot::StaticClass()
    )));
}

void UNarrativeGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
    UNarrativeStartGraphNode* StartNode = NewObject<UNarrativeStartGraphNode>(&Graph, NAME_None, RF_Transactional);
    if (!StartNode)
    {
        return;
    }

    StartNode->CreateNewGuid();
    StartNode->NodePosX = 0;
    StartNode->NodePosY = 0;
    StartNode->AllocateDefaultPins();

    Graph.AddNode(StartNode, true, true);
    Graph.Modify();
}

TSubclassOf<URPGGraphNodeKnot> UNarrativeGraphSchema::GetKnotNodeClass() const
{
    return UNarrativeNodeKnot::StaticClass();
}

void FNewNodeAction::PostNodeCreated(UEdGraphNode* NewNode)
{
    UNarrativeGraphNodeBase* NarrativeNode = Cast<UNarrativeGraphNodeBase>(NewNode);
    if (!NarrativeNode)
    {
        return;
    }

    if (UNarrativeDialogueNodeInfo* DialogueNodeInfo = NarrativeNode->GetNodeInfoAs<UNarrativeDialogueNodeInfo>())
    {
        DialogueNodeInfo->Modify();
        DialogueNodeInfo->SpeakerName = SpeakerData.GetSpeakerDisplayName();
    }
}

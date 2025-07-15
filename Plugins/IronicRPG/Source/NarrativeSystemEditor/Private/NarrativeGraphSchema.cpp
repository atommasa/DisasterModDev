// Fill out your copyright notice in the Description page of Project Settings.


#include "NarrativeGraphSchema.h"
#include "NarrativeGraphNode.h"
#include "ToolMenus.h"
#include "GraphEditorActions.h"
#include "EdGraph/EdGraph.h"
#include "Nodes/NarrativeNodeInfo.h"
#include "Nodes/NarrativeDialogueNodeInfo.h"
#include "Nodes/NarrativePlayerNodeInfo.h"
#include "NarrativeAsset.h"
#include "Nodes/NarrativeStartGraphNode.h"
#include "Nodes/NarrativePlayerGraphNode.h"
#include "HAL/PlatformApplicationMisc.h"
#include "NarrativeTextParser.h"
#include "Framework/Commands/GenericCommands.h"
#include "BlueprintNodeSpawner.h"
#include "Nodes/NarrativeCutsceneNode.h"
#include "Nodes/NarrativeCutsceneNodeInfo.h"
#include "NarrativeEditorSubsystem.h"
#include "Characters/CharacterPrimaryAsset.h"

void UNarrativeGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	auto* NarrativeEditorSubsystem = GEditor->GetEditorSubsystem<UNarrativeEditorSubsystem>();
	if (!NarrativeEditorSubsystem)
	{
		return;
	}

	auto Assets = NarrativeEditorSubsystem->TryGetSpeakerAssets();

	// Create a new action for dialog nodes
	for (const auto& Asset : Assets)
	{
		TSharedPtr<FNewNodeAction> NewNodeAction =
			MakeShareable(new FNewNodeAction
			(
				FText::FromString(TEXT("Dialog Nodes")),
				FText::Format(
					NSLOCTEXT("NarrativeEditor", "AddNarrativeNode", "Add narrative node for {0}"),
					Asset.Value->DefaultData.DisplayName
				),
				FText::FromString(TEXT("Makes a new node")),
				0
			));
		
		NewNodeAction->NodeName = Asset.Value->Id.Id;

		ContextMenuBuilder.AddAction(NewNodeAction);
	}

	// Create a new action for player nodes
	TSharedPtr<FNewNodeAction> NewPlayerNodeAction =
		MakeShareable(new FNewNodeAction
		(
			FText::FromString(TEXT("Player Nodes")),
			NSLOCTEXT("NarrativeEditor", "AddPlayerNode", "Add player node"),
			NSLOCTEXT("NarrativeEditor", "AddPlayerNodeTooltip", "Makes a new player node"),
			1
		));

	ContextMenuBuilder.AddAction(NewPlayerNodeAction);

	// Create a new action for cutscene nodes
	TSharedPtr<FNewNodeAction> NewCutsceneNodeAction =
		MakeShareable(new FNewNodeAction
		(
			FText::FromString(TEXT("Cutscene Nodes")),
			NSLOCTEXT("NarrativeEditor", "AddCutsceneNode", "Add cutscene node"),
			NSLOCTEXT("NarrativeEditor", "AddCutsceneNodeTooltip", "Makes a new cutscene node"),
			2
		));

	ContextMenuBuilder.AddAction(NewCutsceneNodeAction);
}

void UNarrativeGraphSchema::GetContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	Super::GetContextMenuActions(Menu, Context);

	if (Context->Node)
	{
		FToolMenuSection& Section = Menu->AddSection("BehaviorTreeGraphSchemaNodeActions", NSLOCTEXT("NarrativeGraphSchema", "ClassActionsMenuHeader", "Node Actions"));
		Section.AddMenuEntry(FGenericCommands::Get().Delete);
		Section.AddMenuEntry(FGenericCommands::Get().Cut);
		Section.AddMenuEntry(FGenericCommands::Get().Copy);
		Section.AddMenuEntry(FGenericCommands::Get().Duplicate);

		Section.AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);
	}

	Super::GetContextMenuActions(Menu, Context);
}

const FPinConnectionResponse UNarrativeGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
    if (A->Direction == B->Direction)
    {
        return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Same direction not allowed."));
    }

	if (A->GetOwningNode() == B->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Same owning node not allowed."));
	}

	if (A->Direction == EGPD_Output)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, TEXT("OK"));
	}

	if (B->Direction == EGPD_Output)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_B, TEXT("OK"));
	}

    return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT("OK"));
}

void UNarrativeGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	UNarrativeStartGraphNode* StartNode = NewObject<UNarrativeStartGraphNode>(&Graph);
	StartNode->CreateNewGuid();
	StartNode->NodePosX = 0;
	StartNode->NodePosY = 0;

	StartNode->CreateNarrativePin(EGPD_Output, TEXT(""));

	Graph.AddNode(StartNode, true, true);
	Graph.Modify();
}

void UNarrativeGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	const FScopedTransaction Transaction(NSLOCTEXT("Narrative", "BreakNodeLinks", "Break Node Links"));

	Super::BreakNodeLinks(TargetNode);
}

void UNarrativeGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const
{
	const FScopedTransaction Transaction(NSLOCTEXT("Narrative", "BreakPinLinks", "Break Pin Links"));

	Super::BreakPinLinks(TargetPin, bSendsNodeNotification);
}

void UNarrativeGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	const FScopedTransaction Transaction(NSLOCTEXT("Narrative", "BreakPinLink", "Break Pin Link"));

	Super::BreakSinglePinLink(SourcePin, TargetPin);
}

UEdGraphNode* FNewNodeAction::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	const FScopedTransaction Transaction(NSLOCTEXT("Narrative", "AddNodes", "Add Narrative Nodes"));
	UNarrativeGraphNodeBase* ResultNode = nullptr;

	switch (Grouping)
	{
	case 0: // Dialog Node
	{
		UNarrativeGraphNode* DialogueGraphNode = NewObject<UNarrativeGraphNode>(ParentGraph);
		DialogueGraphNode->CreateNewGuid();
		DialogueGraphNode->NodePosX = Location.X;
		DialogueGraphNode->NodePosY = Location.Y;

		DialogueGraphNode->SetNodeInfo(NewObject<UNarrativeDialogueNodeInfo>(DialogueGraphNode));

		Cast<UNarrativeDialogueNodeInfo>(DialogueGraphNode->GetNodeInfo())->SpeakerId = NodeName.IsNone() ? FName(TEXT("Unknown Speaker")) : NodeName;

		UEdGraphPin* InputPin = DialogueGraphNode->CreateNarrativePin(EEdGraphPinDirection::EGPD_Input, TEXT(""));
		FString DefaultOption = TEXT("");
		DialogueGraphNode->CreateNarrativePin(EEdGraphPinDirection::EGPD_Output, FName(DefaultOption));

		// If the from pin is not null, connect them
		if (FromPin)
		{
			DialogueGraphNode->GetSchema()->TryCreateConnection(FromPin, InputPin);
		}

		ParentGraph->Modify();
		ParentGraph->AddNode(DialogueGraphNode, true, true);

		ResultNode = DialogueGraphNode;
		break;
	}
	case 1: // Player Node
	{
		UNarrativePlayerGraphNode* PlayerGraphNode = NewObject<UNarrativePlayerGraphNode>(ParentGraph);
		PlayerGraphNode->CreateNewGuid();
		PlayerGraphNode->NodePosX = Location.X;
		PlayerGraphNode->NodePosY = Location.Y;

		PlayerGraphNode->SetNodeInfo(NewObject<UNarrativePlayerNodeInfo>(PlayerGraphNode));

		UEdGraphPin* InputPin = PlayerGraphNode->CreateNarrativePin(EEdGraphPinDirection::EGPD_Input, TEXT(""));
		FString DefaultOption = TEXT("");
		PlayerGraphNode->CreateNarrativePin(EEdGraphPinDirection::EGPD_Output, FName(DefaultOption));
		Cast<UNarrativePlayerNodeInfo>(PlayerGraphNode->GetNodeInfo())->Options.Add(FText::FromString(DefaultOption));

		if (FromPin)
		{
			PlayerGraphNode->GetSchema()->TryCreateConnection(FromPin, InputPin);
		}

		ParentGraph->Modify();
		ParentGraph->AddNode(PlayerGraphNode, true, true);

		ResultNode = PlayerGraphNode;
		break;
	}
	case 2:
	{
		UNarrativeCutsceneNode* CutsceneNode = NewObject<UNarrativeCutsceneNode>(ParentGraph);
		CutsceneNode->CreateNewGuid();
		CutsceneNode->NodePosX = Location.X;
		CutsceneNode->NodePosY = Location.Y;

		CutsceneNode->SetNodeInfo(NewObject<UNarrativeCutsceneNodeInfo>(CutsceneNode));

		UEdGraphPin* InputPin = CutsceneNode->CreateNarrativePin(EEdGraphPinDirection::EGPD_Input, TEXT(""));
		FString DefaultOption = TEXT("");
		CutsceneNode->CreateNarrativePin(EEdGraphPinDirection::EGPD_Output, FName(DefaultOption));

		// If the from pin is not null, connect them
		if (FromPin)
		{
			CutsceneNode->GetSchema()->TryCreateConnection(FromPin, InputPin);
		}

		ParentGraph->Modify();
		ParentGraph->AddNode(CutsceneNode, true, true);

		ResultNode = CutsceneNode;
		break;
	}
	}

	if (ResultNode)
	{
		ResultNode->SetFlags(RF_Transactional);
		ResultNode->Rename(nullptr, ParentGraph, REN_NonTransactional);
		ResultNode->Modify();
	}

	return ResultNode;
}

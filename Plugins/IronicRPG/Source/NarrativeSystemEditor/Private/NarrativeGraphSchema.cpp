// Copyright Ironic Studio. All Rights Reserved.


#include "NarrativeGraphSchema.h"
#include "ToolMenus.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "GraphEditorActions.h"
#include "EdGraph/EdGraph.h"
#include "Nodes/NarrativeDialogueNode.h"
#include "Nodes/NarrativeDialogueNodeInfo.h"
#include "Nodes/NarrativeStartGraphNode.h"
#include "Nodes/NarrativePlayerOptionsNode.h"
#include "Nodes/NarrativeBranchNode.h"
#include "Nodes/NarrativeSetVariablesNode.h"
#include "Nodes/NarrativeCutsceneNode.h"
#include "Nodes/NarrativeNodeKnot.h"
#include "Framework/Commands/GenericCommands.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

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

	// Create a new action for dialog nodes
	for (const FSpeakerData& Data : Dialogue->Speakers)
	{
		FText DisplayName = Data.GetSpeakerDisplayName();

		TSharedPtr<FNewNodeAction> NewNodeAction =
			MakeShareable(new FNewNodeAction
			(
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

	// Create a new action for player options nodes
	TSharedPtr<FNewNodeAction> NewPlayerNodeAction =
		MakeShareable(new FNewNodeAction
		(
			FText::FromString(TEXT("Dialogue Nodes")),
			NSLOCTEXT("NarrativeEditor", "AddPlayerOptionsNode", "Add player options node"),
			NSLOCTEXT("NarrativeEditor", "AddPlayerOptionsNodeTooltip", "Makes a new player options node"),
			UNarrativePlayerOptionsNode::StaticClass()
		));

	ContextMenuBuilder.AddAction(NewPlayerNodeAction);

	// Create a new action for cutscene nodes
	TSharedPtr<FNewNodeAction> NewCutsceneNodeAction =
		MakeShareable(new FNewNodeAction
		(
			FText::FromString(TEXT("Presentation Nodes")),
			NSLOCTEXT("NarrativeEditor", "AddCutsceneNode", "Add cutscene node"),
			NSLOCTEXT("NarrativeEditor", "AddCutsceneNodeTooltip", "Makes a new cutscene node"),
			UNarrativeCutsceneNode::StaticClass()
		));

	ContextMenuBuilder.AddAction(NewCutsceneNodeAction);

	// Create a new action for branch nodes
	TSharedPtr<FNewNodeAction> NewBranchNodeAction =
		MakeShareable(new FNewNodeAction
		(
			FText::FromString(TEXT("Flow Nodes")),
			NSLOCTEXT("NarrativeEditor", "AddBranchNode", "Add branch node"),
			NSLOCTEXT("NarrativeEditor", "AddBranchNodeTooltip", "Makes a new branch node"),
			UNarrativeBranchNode::StaticClass()
		));

	ContextMenuBuilder.AddAction(NewBranchNodeAction);

	// Create a new action for set variables nodes
	TSharedPtr<FNewNodeAction> NewSetVariablesNodeAction =
		MakeShareable(new FNewNodeAction
		(
			FText::FromString(TEXT("Game Logic Nodes")),
			NSLOCTEXT("NarrativeEditor", "AddSetVariablesNode", "Add set variables node"),
			NSLOCTEXT("NarrativeEditor", "AddSetVariablesTooltip", "Makes a new set variables node"),
			UNarrativeSetVariablesNode::StaticClass()
		));

	ContextMenuBuilder.AddAction(NewSetVariablesNodeAction);

	// Create a new action for reroute nodes
	TSharedPtr<FNewNodeAction> NewRerouteNodeAction =
		MakeShareable(new FNewNodeAction
		(
			FText::FromString(TEXT("")),
			NSLOCTEXT("NarrativeEditor", "AddRerouteNode", "Add reroute node"),
			NSLOCTEXT("NarrativeEditor", "AddRerouteNodeTooltip", "Makes a new reroute node"),
			UNarrativeNodeKnot::StaticClass()
		));

	ContextMenuBuilder.AddAction(NewRerouteNodeAction);
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
	if (IsKnotToKnot(A, B))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, TEXT("OK"));
	}

	const UEdGraphPin* ResolvedA = ResolveKnotPinForOtherPinConst(A, B);
	const UEdGraphPin* ResolvedB = ResolveKnotPinForOtherPinConst(B, ResolvedA);

	if (ResolvedA->GetOwningNode() == ResolvedB->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Same owning node not allowed."));
	}

	if (ResolvedA->Direction == ResolvedB->Direction)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Same direction not allowed."));
	}

	if (ResolvedA->Direction == EGPD_Output)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, TEXT("OK"));
	}

	if (ResolvedB->Direction == EGPD_Output)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_B, TEXT("OK"));
	}

    return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT("OK"));
}

bool UNarrativeGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
	if (!A || !B)
	{
		return false;
	}

	if (IsKnotToKnot(A, B))
	{
		UEdGraphPin* KnotPinA = Cast<UNarrativeNodeKnot>(A->GetOwningNode())->GetOutputPin();
		UEdGraphPin* KnotPinB = Cast<UNarrativeNodeKnot>(B->GetOwningNode())->GetInputPin();

		return Super::TryCreateConnection(KnotPinA, KnotPinB);
	}

	UEdGraphPin* ResolvedA = ResolveKnotPinForOtherPin(A, B);
	UEdGraphPin* ResolvedB = ResolveKnotPinForOtherPin(B, ResolvedA);

	if (!ResolvedA || !ResolvedB)
	{
		return false;
	}

	if (ResolvedA == ResolvedB)
	{
		return false;
	}

	return Super::TryCreateConnection(ResolvedA, ResolvedB);
}

void UNarrativeGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	UNarrativeStartGraphNode* StartNode = NewObject<UNarrativeStartGraphNode>(&Graph);
	
	StartNode->CreateNewGuid();
	StartNode->NodePosX = 0;
	StartNode->NodePosY = 0;

	StartNode->AllocateDefaultPins();

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

void UNarrativeGraphSchema::OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const
{
	if (!PinA || !PinB)
	{
		return;
	}

	UEdGraphNode* NodeA = PinA->GetOwningNode();
	UEdGraphNode* NodeB = PinB->GetOwningNode();

	if (!NodeA || !NodeB)
	{
		return;
	}

	UEdGraph* Graph = NodeA->GetGraph();
	if (!Graph || Graph != NodeB->GetGraph())
	{
		return;
	}

	UEdGraphPin* OutputPin = nullptr;
	UEdGraphPin* InputPin = nullptr;

	if (PinA->Direction == EGPD_Output && PinB->Direction == EGPD_Input)
	{
		OutputPin = PinA;
		InputPin = PinB;
	}
	else if (PinB->Direction == EGPD_Output && PinA->Direction == EGPD_Input)
	{
		OutputPin = PinB;
		InputPin = PinA;
	}
	else
	{
		return;
	}

	const FScopedTransaction Transaction(
		NSLOCTEXT("Narrative", "CreateRerouteNode", "Create Reroute Node")
	);

	Graph->Modify();
	OutputPin->Modify();
	InputPin->Modify();
	
	FGraphNodeCreator<UNarrativeNodeKnot> NodeCreator(*Graph);
	UNarrativeNodeKnot* RerouteNode = NodeCreator.CreateNode();

	RerouteNode->NodePosX = GraphPosition.X;
	RerouteNode->NodePosY = GraphPosition.Y;

	NodeCreator.Finalize();

	if (!RerouteNode)
	{
		return;
	}

	UEdGraphPin* RerouteInputPin = RerouteNode->GetInputPin();
	UEdGraphPin* RerouteOutputPin = RerouteNode->GetOutputPin();

	if (!RerouteInputPin || !RerouteOutputPin)
	{
		return;
	}

	RerouteNode->Modify();

	BreakSinglePinLink(OutputPin, InputPin);

	TryCreateConnection(OutputPin, RerouteInputPin);
	TryCreateConnection(RerouteOutputPin, InputPin);

	Graph->NotifyGraphChanged();

	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph))
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
}

FConnectionDrawingPolicy* UNarrativeGraphSchema::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj) const
{
	return new FNarrativeConnectionDrawingPolicy(
		InBackLayerID,
		InFrontLayerID,
		InZoomFactor,
		InClippingRect,
		InDrawElements,
		InGraphObj
	);
}

UEdGraphPin* UNarrativeGraphSchema::ResolveKnotPinForOtherPin(UEdGraphPin* Pin, const UEdGraphPin* OtherPin) const
{
	return const_cast<UEdGraphPin*>(ResolveKnotPinForOtherPinConst(Pin, OtherPin));
}

const UEdGraphPin* UNarrativeGraphSchema::ResolveKnotPinForOtherPinConst(const UEdGraphPin* Pin, const UEdGraphPin* OtherPin) const
{
	if (!Pin || !OtherPin)
	{
		return Pin;
	}

	UNarrativeNodeKnot* KnotNode = Cast<UNarrativeNodeKnot>(Pin->GetOwningNode());
	if (!KnotNode)
	{
		return Pin;
	}

	if (OtherPin->Direction == EGPD_Input)
	{
		return KnotNode->GetOutputPin();
	}

	if (OtherPin->Direction == EGPD_Output)
	{
		return KnotNode->GetInputPin();
	}

	return Pin;
}

bool UNarrativeGraphSchema::IsKnotToKnot(const UEdGraphPin* Pin, const UEdGraphPin* OtherPin) const
{
	return Pin->GetOwningNode()->IsA<UNarrativeNodeKnot>() && OtherPin->GetOwningNode()->IsA<UNarrativeNodeKnot>();
}

UEdGraphNode* FNewNodeAction::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	if (!ParentGraph || !NodeClass)
	{
		return nullptr;
	}

	const FScopedTransaction Transaction(
		NSLOCTEXT("Narrative", "AddNodes", "Add Narrative Node")
	);

	UEdGraphPin* PreviousLinkedToPin = nullptr;
	UNarrativeGraphNodeBase* PreviousLinkedToNode = nullptr;
	if (FromPin && FromPin->LinkedTo.IsValidIndex(0))
	{
		PreviousLinkedToPin = FromPin->LinkedTo[0];
		if (PreviousLinkedToPin)
		{
			PreviousLinkedToNode = Cast<UNarrativeGraphNodeBase>(PreviousLinkedToPin->GetOwningNode());
		}
	}

	ParentGraph->Modify();

	UEdGraphNode* ResultNode = NewObject<UEdGraphNode>(
		ParentGraph,
		NodeClass,
		NAME_None,
		RF_Transactional
	);

	if (!ResultNode)
	{
		return nullptr;
	}

	ResultNode->Modify();

	ResultNode->CreateNewGuid();
	ResultNode->NodePosX = Location.X;
	ResultNode->NodePosY = Location.Y;

	ResultNode->AllocateDefaultPins();

	if (UNarrativeGraphNodeBase* NarrativeNode = Cast<UNarrativeGraphNodeBase>(ResultNode))
	{
		if (UNarrativeDialogueNodeInfo* DialogueNodeInfo = NarrativeNode->GetNodeInfoAs<UNarrativeDialogueNodeInfo>())
		{
			DialogueNodeInfo->Modify();
			DialogueNodeInfo->SpeakerName = SpeakerData.GetSpeakerDisplayName();
		}
	}

	ParentGraph->AddNode(ResultNode, true, bSelectNewNode);

	if (FromPin)
	{
		if (UEdGraphPin** InputPin = ResultNode->Pins.FindByPredicate([](const UEdGraphPin* Pin)
			{
				return Pin->Direction == EGPD_Input;
			}))
		{
			ResultNode->GetSchema()->TryCreateConnection(FromPin, *InputPin);
		}
	}

	if (PreviousLinkedToPin && PreviousLinkedToNode)
	{
		if (UEdGraphPin** OutputPin = ResultNode->Pins.FindByPredicate([](const UEdGraphPin* Pin)
			{
				return Pin->Direction == EGPD_Output;
			}))
		{
			ResultNode->GetSchema()->TryCreateConnection(*OutputPin, PreviousLinkedToPin);
		}
	}

	ParentGraph->NotifyGraphChanged();

	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph))
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		Blueprint->MarkPackageDirty();
	}

	return ResultNode;
}

FNarrativeConnectionDrawingPolicy::FNarrativeConnectionDrawingPolicy(
	int32 InBackLayerID,
	int32 InFrontLayerID,
	float InZoomFactor,
	const FSlateRect& InClippingRect,
	FSlateWindowElementList& InDrawElements,
	UEdGraph* InGraphObj
)
	: FKismetConnectionDrawingPolicy(
		InBackLayerID,
		InFrontLayerID,
		InZoomFactor,
		InClippingRect,
		InDrawElements,
		InGraphObj
	)
{
}

bool FNarrativeConnectionDrawingPolicy::ShouldChangeTangentForRerouteControlPoint(
	const UNarrativeNodeKnot* Node
)
{
	if (!Node)
	{
		return false;
	}

	if (bool* CachedResult = KnotToReversedDirectionMap.Find(Node))
	{
		return *CachedResult;
	}

	bool bPinReversed = false;

	int32 InputPinIndex = 0;
	int32 OutputPinIndex = 0;

	if (Node->ShouldDrawNodeAsControlPointOnly(InputPinIndex, OutputPinIndex))
	{
		const TArray<UEdGraphPin*>& Pins = Node->GetAllPins();

		if (!Pins.IsValidIndex(InputPinIndex) || !Pins.IsValidIndex(OutputPinIndex))
		{
			KnotToReversedDirectionMap.Add(Node, false);
			return false;
		}

		FVector2f AverageLeftPin = FVector2f::ZeroVector;
		FVector2f AverageRightPin = FVector2f::ZeroVector;
		FVector2f CenterPin = FVector2f::ZeroVector;

		// InputPin / OutputPin 視覺上共用同一個位置，取任一個 center 即可。
		const bool bCenterValid = FindPinCenter(Pins[OutputPinIndex], CenterPin);

		const bool bLeftValid = GetAverageConnectedPositionForPin(Pins[InputPinIndex], AverageLeftPin);
		const bool bRightValid = GetAverageConnectedPositionForPin(Pins[OutputPinIndex], AverageRightPin);

		if (bLeftValid && bRightValid)
		{
			bPinReversed = AverageRightPin.X < AverageLeftPin.X;
		}
		else if (bCenterValid)
		{
			if (bLeftValid)
			{
				bPinReversed = CenterPin.X < AverageLeftPin.X;
			}
			else if (bRightValid)
			{
				bPinReversed = AverageRightPin.X < CenterPin.X;
			}
		}
	}

	KnotToReversedDirectionMap.Add(Node, bPinReversed);
	return bPinReversed;
}

bool FNarrativeConnectionDrawingPolicy::GetAverageConnectedPositionForPin(
	UEdGraphPin* InPin,
	FVector2f& OutPos
) const
{
	if (!InPin)
	{
		return false;
	}

	FVector2f Result = FVector2f::ZeroVector;
	int32 ResultCount = 0;

	for (UEdGraphPin* LinkedPin : InPin->LinkedTo)
	{
		if (!LinkedPin)
		{
			continue;
		}

		FVector2f CenterPoint;
		if (FindPinCenter(LinkedPin, CenterPoint))
		{
			Result += CenterPoint;
			++ResultCount;
		}
	}

	if (ResultCount <= 0)
	{
		return false;
	}

	OutPos = Result / static_cast<float>(ResultCount);
	return true;
}

void FNarrativeConnectionDrawingPolicy::DetermineWiringStyle(
	UEdGraphPin* OutputPin,
	UEdGraphPin* InputPin,
	FConnectionParams& Params
)
{
	FKismetConnectionDrawingPolicy::DetermineWiringStyle(OutputPin, InputPin, Params);

	if (!OutputPin || !InputPin)
	{
		return;
	}

	if (OutputPin->Direction == EGPD_Input)
	{
		Swap(OutputPin, InputPin);
	}

	const UNarrativeNodeKnot* OutputNode =
		Cast<UNarrativeNodeKnot>(OutputPin->GetOwningNode());

	const UNarrativeNodeKnot* InputNode =
		Cast<UNarrativeNodeKnot>(InputPin->GetOwningNode());

	if (OutputNode && ShouldChangeTangentForRerouteControlPoint(OutputNode))
	{
		Params.StartDirection = EGPD_Input;
	}

	if (InputNode && ShouldChangeTangentForRerouteControlPoint(InputNode))
	{
		Params.EndDirection = EGPD_Output;
	}

	// 你的 Narrative 線條樣式可放這裡
	Params.WireColor = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);
	Params.WireThickness = 2.0f;
}
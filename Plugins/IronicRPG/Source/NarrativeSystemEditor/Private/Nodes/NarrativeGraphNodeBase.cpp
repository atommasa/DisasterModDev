// Copyright Ironic Studio. All Rights Reserved.


#include "Nodes/NarrativeGraphNodeBase.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "NarrativeEditorUtils.h"

FName UNarrativeGraphNodeBase::PinNane(TEXT("NarrativePin"));

UEdGraphPin* UNarrativeGraphNodeBase::CreateNarrativePin(EEdGraphPinDirection Direction, FName PinName)
{
	FName Category = (Direction == EGPD_Input) ? TEXT("Input") : TEXT("Output");
	FName SubCategory = PinNane;

	UEdGraphPin* NewPin = CreatePin(Direction, Category, PinName);
	NewPin->PinType.PinSubCategory = SubCategory;

	return NewPin;
}

void UNarrativeGraphNodeBase::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	FToolMenuSection& Section = Menu->AddSection(TEXT("SectionName"), FText::FromString(TEXT("Narratvie Node Actions")));

	UNarrativeGraphNodeBase* NodePtr = (UNarrativeGraphNodeBase*)this;

}

void UNarrativeGraphNodeBase::SetNodeInfo(UNarrativeNodeInfo* InNodeInfo)
{
	InNodeInfo->SetFlags(RF_Transactional);
	InNodeInfo->Rename(nullptr, GetGraph(), REN_NonTransactional);
	NodeInfo = InNodeInfo;
}

void UNarrativeGraphNodeBase::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Modify();
	GetGraph()->Modify();

	TArray<FPinConnectionData> NewConnections;
	
	for (UEdGraphPin* Linked : Pin->LinkedTo)
	{
		if (Linked)
		{
			FPinConnectionData Conn;
			Conn.FromPinId = Pin->PinId;
			Conn.ToNodeId = Linked->GetOwningNode()->NodeGuid;
			Conn.ToPinId = Linked->PinId;

			NewConnections.Add(Conn);
		}
	}

	// Save the new connections
	SavedConnections = NewConnections;

	UE_LOG(LogTemp, Display, TEXT("%s: %d"), *Pin->GetName(), Pin->LinkedTo.Num());
}

void UNarrativeGraphNodeBase::SyncPin()
{
	TMap<FGuid, UEdGraphPin*> PinMap;
	TMap<FGuid, UEdGraphNode*> NodeMap;

	for (UEdGraphNode* Node : GetGraph()->Nodes)
	{
		NodeMap.Add(Node->NodeGuid, Node);

		for (UEdGraphPin* Pin : Node->Pins)
		{
			PinMap.Add(Pin->PinId, Pin);
		}
	}

	for (const FPinConnectionData& Conn : SavedConnections)
	{
		if (UEdGraphPin* From = PinMap.FindRef(Conn.FromPinId))
		{
			if (UEdGraphNode* ToNode = NodeMap.FindRef(Conn.ToNodeId))
			{
				if (UEdGraphPin* To = ToNode->FindPinById(Conn.ToPinId))
				{
					if (!From->LinkedTo.Contains(To))
					{
						if (From && To && !From->LinkedTo.Contains(To) && !To->LinkedTo.Contains(From))
						{
							From->Modify();
							To->Modify();
							From->MakeLinkTo(To);
						}
					}
				}
			}
		}
	}

	SavedConnections.Empty();
}

void UNarrativeGraphNodeBase::DestroyNode()
{
	const FScopedTransaction Transaction(NSLOCTEXT("Narrative", "DeleteNode", "Delete Node"));
	Modify();
	GetGraph()->Modify();

	for (UEdGraphPin * Pin : Pins)
	{
		for (UEdGraphPin* Linked : Pin->LinkedTo)
		{
			if (Linked)
			{
				FPinConnectionData Conn;
				Conn.FromPinId = Pin->PinId;
				Conn.ToNodeId = Linked->GetOwningNode()->NodeGuid;
				Conn.ToPinId = Linked->PinId;
				SavedConnections.Add(Conn);
			}
		}
	}

	Super::DestroyNode();
}

UK2Node_CustomEvent* UNarrativeGraphNodeBase::CreateOrFocusNarrativeCustomEvent()
{
	const FName& EventName = GetNodeInfo()->CallableBindingName;
	const FText& EventComment = CreateCallableBindingComment();

	UDialogueBlueprint* DialogueBP = GetNarrativeAsset();
	UEdGraph* EventGraph = FNarrativeEditorUtils::GetOrCreateGraph(DialogueBP, FName(TEXT("DialogueEventGraph"))); // TODO: 之後考慮不寫死

	if (!DialogueBP || !EventGraph)
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : EventGraph->Nodes)
	{
		UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node);
		if (CustomEvent && CustomEvent->CustomFunctionName == EventName)
		{
			CustomEvent->Modify();

			CreateCallableBindingParameterPins(CustomEvent);
			CustomEvent->ReconstructNode();

			EventGraph->NotifyNodeChanged(CustomEvent);

			FBlueprintEditorUtils::MarkBlueprintAsModified(DialogueBP);
			DialogueBP->MarkPackageDirty();

			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(CustomEvent);
			return CustomEvent;
		}
	}

	const FScopedTransaction Transaction(NSLOCTEXT("Narrative", "CreateCustomEvent", "Create Custom Event"));

	DialogueBP->Modify();
	EventGraph->Modify();

	FGraphNodeCreator<UK2Node_CustomEvent> NodeCreator(*EventGraph);
	UK2Node_CustomEvent* CustomEventNode = NodeCreator.CreateNode();

	CustomEventNode->CustomFunctionName = EventName;
	CustomEventNode->NodeComment = EventComment.ToString();
	CustomEventNode->bCommentBubbleVisible = true;
	CustomEventNode->bIsEditable = false;

	FVector2D NewNodePosition = FNarrativeEditorUtils::FindLocationForNewNode(EventGraph);
	CustomEventNode->NodePosX = NewNodePosition.X;
	CustomEventNode->NodePosY = NewNodePosition.Y;

	NodeCreator.Finalize();

	CustomEventNode->Modify();

	CreateCallableBindingParameterPins(CustomEventNode);

	CustomEventNode->ReconstructNode();

	EventGraph->NotifyNodeChanged(CustomEventNode);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(DialogueBP);
	DialogueBP->MarkPackageDirty();

	FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(CustomEventNode);

	return CustomEventNode;
}

UK2Node_FunctionEntry* UNarrativeGraphNodeBase::CreateOrFocusNarrativeFunction()
{
	const FName& FunctionName = GetNodeInfo()->CallableBindingName;
	const FText& FunctionComment = CreateCallableBindingComment();

	UDialogueBlueprint* DialogueBP = GetNarrativeAsset();
	UEdGraph* FunctionGraph = FNarrativeEditorUtils::GetOrCreateFunctionGraph(DialogueBP, FunctionName, GetFunctionAsSignature());

	if (!FunctionGraph)
	{
		return nullptr;
	}

	UK2Node_FunctionEntry* Entry = nullptr;

	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		if (!Entry)
		{
			Entry = Cast<UK2Node_FunctionEntry>(Node);
			break;
		}
	}

	Entry->Modify();

	Entry->NodeComment = FunctionComment.ToString();
	Entry->bCommentBubbleVisible = true;
	Entry->bIsEditable = false;
	Entry->bCanRenameNode = false;

	CreateCallableBindingParameterPins(Entry);
	Entry->ReconstructNode();

	FunctionGraph->NotifyNodeChanged(Entry);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(DialogueBP);
	DialogueBP->MarkPackageDirty();

	FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Entry);

	return Entry;
}

FName UNarrativeGraphNodeBase::CreateCallableBindingName() const
{
	return FName(*FString::Printf(TEXT("On%sEventTriggers_%s"), *GetClass()->GetName(), *NodeGuid.ToString()));
}

FText UNarrativeGraphNodeBase::CreateCallableBindingComment() const
{
	return FText();
}


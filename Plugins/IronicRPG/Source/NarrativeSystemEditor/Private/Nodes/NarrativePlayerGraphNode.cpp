// Fill out your copyright notice in the Description page of Project Settings.


#include "Nodes/NarrativePlayerGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Editor/Transactor.h"
#include "NarrativeSystemEditor.h"

TSharedPtr<SGraphNode> UNarrativePlayerGraphNode::CreateVisualWidget()
{
	return SNew(SNarrativePlayerGraphNode, this);
}


void UNarrativePlayerGraphNode::GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
{
	FToolMenuSection& Section = Menu->AddSection(TEXT("SectionName"), FText::FromString(TEXT("Narratvie Node Actions")));

	UNarrativePlayerGraphNode* NodePtr = (UNarrativePlayerGraphNode*)this;

	Section.AddMenuEntry
	(
		TEXT("AddPinEntry"),
		FText::FromString(TEXT("Add Option")),
		FText::FromString(TEXT("Adds an option to the node.")),
		FSlateIcon(FNarrativeSystemEditorStyleSet::Get().GetStyleSetName(), "NarrativeEditor.NodeAddPinIcon"),
		FUIAction
		(
			FExecuteAction::CreateLambda([NodePtr]()
				{
					NodePtr->AddPin();
				})
		)
	);

	Section.AddMenuEntry
	(
		TEXT("DeletePinEntry"),
		FText::FromString(TEXT("Delete Option")),
		FText::FromString(TEXT("Deletes an option from the node.")),
		FSlateIcon(FNarrativeSystemEditorStyleSet::Get().GetStyleSetName(), "NarrativeEditor.NodeDeletePinIcon"),
		FUIAction
		(
			FExecuteAction::CreateLambda([NodePtr]()
				{
					NodePtr->DeletePin();
				})
		)
	);

	Super::GetNodeContextMenuActions(Menu, Context);
}

UEdGraphPin* UNarrativePlayerGraphNode::CreateNarrativePin(EEdGraphPinDirection Direction, FName PinName)
{
	Modify();

	FName Category = (Direction == EGPD_Input) ? TEXT("Input") : TEXT("Output");
	FName SubCategory = TEXT("NarrativePin");

	UEdGraphPin* NewPin = CreatePin(Direction, Category, PinName);
	NewPin->PinType.PinSubCategory = SubCategory;
	NewPin->Modify();

	return NewPin;
}

void UNarrativePlayerGraphNode::SyncPinWithResponse()
{
	UNarrativePlayerNodeInfo* NodeInfo = Cast<UNarrativePlayerNodeInfo>(GetNodeInfo());
	int32 NumPins = Pins.Num() - 1; // -1 for the input pin
	int32 NumInfoPins = NodeInfo->Options.Num();

	while (NumPins > NumInfoPins)
	{
		RemovePinAt(NumPins - 1, EGPD_Output);
		NumPins--;
	}

	while (NumPins < NumInfoPins)
	{
		UEdGraphPin* NewPin = CreateNarrativePin(EGPD_Output, FName(NodeInfo->Options[NumPins].ToString()));
		NumPins++;
	}

	int32 Index = 1;
	for (const FText& Option : NodeInfo->Options)
	{
		GetPinAt(Index)->Modify();
		GetPinAt(Index)->PinName = FName(Option.ToString().Len() > 9 ? Option.ToString().Left(9) + TEXT("...") : Option.ToString());
		Index++;
	}

	Super::SyncPinWithResponse();
}

void UNarrativePlayerGraphNode::AddPin()
{
	const FScopedTransaction Transaction(NSLOCTEXT("Narrative", "AddOption", "Add Dialogue Option"));
	Modify();
	GetGraph()->Modify();

	if (auto* NodeInfo = Cast<UNarrativePlayerNodeInfo>(GetNodeInfo()))
	{
		NodeInfo->Modify();

		NodeInfo->Options.Add(FText::FromString(TEXT("")));
	}

	SyncPinWithResponse();
	GetGraph()->NotifyGraphChanged();
}

void UNarrativePlayerGraphNode::DeletePin()
{
	const FScopedTransaction Transaction(NSLOCTEXT("Narrative", "DeleteOption", "Delete Dialogue Option"));
	Modify();
	GetGraph()->Modify();

	if (auto* NodeInfo = Cast<UNarrativePlayerNodeInfo>(GetNodeInfo()))
	{
		NodeInfo->Modify();

		if (NodeInfo->Options.Num() > 0)
		{
			NodeInfo->Options.Pop();
		}
	}

	SavedConnections.Empty();

	UEdGraphPin* OutputPin = Pins.Last();
	for (UEdGraphPin* Linked : OutputPin->LinkedTo)
	{
		if (Linked)
		{
			FPinConnectionData Conn;
			Conn.FromPinId = OutputPin->PinId;
			Conn.ToNodeId = Linked->GetOwningNode()->NodeGuid;
			Conn.ToPinId = Linked->PinId;
			SavedConnections.Add(Conn);
		}
	}

	OutputPin->BreakAllPinLinks(true);

	SyncPinWithResponse();
	GetGraph()->NotifyGraphChanged();
}

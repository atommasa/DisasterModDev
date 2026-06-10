// Copyright Ironic Studio. All Rights Reserved.


#include "Nodes/NarrativeStartGraphNode.h"

TSharedPtr<SGraphNode> UNarrativeStartGraphNode::CreateVisualWidget()
{
	return SNew(SNarrativeStartGraphNode, this);
}

void UNarrativeStartGraphNode::AllocateDefaultPins()
{
	CreateNarrativePin(EGPD_Output, TEXT(""));
}

UEdGraphPin* UNarrativeStartGraphNode::CreateNarrativePin(EEdGraphPinDirection Direction, FName PinName)
{
	FName Category = TEXT("Output");
	FName SubCategory = TEXT("StartPin");

	UEdGraphPin* NewPin = CreatePin(Direction, Category, PinName);
	NewPin->PinType.PinSubCategory = SubCategory;

	return NewPin;
}
// Copyright Ironic Studio. All Rights Reserved.


#include "Nodes/NarrativeStartGraphNode.h"

TSharedPtr<SGraphNode> UNarrativeStartGraphNode::CreateVisualWidget()
{
	return SNew(SNarrativeStartGraphNode, this);
}

void UNarrativeStartGraphNode::AllocateDefaultPins()
{
	CreateRPGGraphPin(EGPD_Output, TEXT(""));
}

UEdGraphPin* UNarrativeStartGraphNode::CreateRPGGraphPin(EEdGraphPinDirection Direction, FName InPinName)
{
	FName Category = TEXT("Output");
	FName SubCategory = TEXT("StartPin");

	UEdGraphPin* NewPin = CreatePin(Direction, Category, InPinName);
	NewPin->PinType.PinSubCategory = SubCategory;

	return NewPin;
}
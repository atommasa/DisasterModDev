// Fill out your copyright notice in the Description page of Project Settings.


#include "Nodes/NarrativeStartGraphNode.h"

TSharedPtr<SGraphNode> UNarrativeStartGraphNode::CreateVisualWidget()
{
	return SNew(SNarrativeStartGraphNode, this);
}

UEdGraphPin* UNarrativeStartGraphNode::CreateNarrativePin(EEdGraphPinDirection Direction, FName PinName)
{
	FName Category = TEXT("Output");
	FName SubCategory = TEXT("StartPin");

	UEdGraphPin* NewPin = CreatePin(Direction, Category, PinName);
	NewPin->PinType.PinSubCategory = SubCategory;

	return NewPin;
}
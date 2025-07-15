// Fill out your copyright notice in the Description page of Project Settings.


#include "Nodes/NarrativeCutsceneNode.h"

TSharedPtr<SGraphNode> UNarrativeCutsceneNode::CreateVisualWidget()
{
	return SNew(SNarrativeCutsceneNode, this);
}

UEdGraphPin* UNarrativeCutsceneNode::CreateNarrativePin(EEdGraphPinDirection Direction, FName PinName)
{
	FName Category = (Direction == EGPD_Input) ? TEXT("Input") : TEXT("Output");
	FName SubCategory = TEXT("NarrativePin");

	UEdGraphPin* NewPin = CreatePin(Direction, Category, PinName);
	NewPin->PinType.PinSubCategory = SubCategory;

	return NewPin;
}


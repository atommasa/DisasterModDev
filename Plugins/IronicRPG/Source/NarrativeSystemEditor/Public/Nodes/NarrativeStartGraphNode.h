// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/NarrativeGraphNodeBase.h"
#include "Nodes/NarrativeGraphNodeStyle.h"
#include "NarrativeStartGraphNode.generated.h"

/**
 * This node is the start of the narrative graph
 */
UCLASS()
class NARRATIVESYSTEMEDITOR_API UNarrativeStartGraphNode : public UNarrativeGraphNodeBase
{
	GENERATED_BODY()

public: // UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override { return FText::FromString("Start Node"); }
	virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor::Red; }
	virtual bool CanUserDeleteNode() const override { return false; }
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;

public: // UNarrativeGraphNode interface
	virtual UEdGraphPin* CreateNarrativePin(EEdGraphPinDirection Direction, FName PinName) override;

};

class SNarrativeStartGraphNode : public SNarrativeGraphNodeBase
{
public:
	SLATE_BEGIN_ARGS(SNarrativeStartGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphNode* InNode)
	{
		GraphNode = InNode;
		SetCursor(EMouseCursor::CardinalCross);

		NodeTitle = FText::FromString("Start");
		TitleColor = FColor::Red;
		UpdateGraphNode();
	}

};
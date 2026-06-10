// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/NarrativeGraphNodeBase.h"
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
	virtual bool CanUserDeleteNode() const override { return false; }
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
	virtual void AllocateDefaultPins() override;

public: // UNarrativeGraphNode interface
	virtual ENarrativeNodeType GetNarrativeNodeType() const override { return ENarrativeNodeType::StartNode; }
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

		TitleColor = FColor::Red;

		UpdateGraphNode();
	}

};
// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/NarrativeGraphNodeBase.h"
#include "NarrativeSetVariablesNode.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVESYSTEMEDITOR_API UNarrativeSetVariablesNode : public UNarrativeGraphNodeBase
{
	GENERATED_BODY()

public: // UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override { return FText::FromString("Set Variables Node"); }
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
	virtual void AllocateDefaultPins() override;

public: // UNarrativeGraphNodeBase interface
	virtual ENarrativeNodeType GetNarrativeNodeType() const override { return ENarrativeNodeType::SetVariablesNode; }

};

class SNarrativeSetVariablesNode : public SNarrativeGraphNodeBase
{
public:
	SLATE_BEGIN_ARGS(SNarrativeSetVariablesNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphNode* InNode)
	{
		GraphNode = InNode;
		SetCursor(EMouseCursor::CardinalCross);

		TitleColor = FColor(26, 105, 156);

		BindPropertyChangeToNodeInfo(&SNarrativeSetVariablesNode::OnNodeInfoPropertyChanged);

		UpdateGraphNode();
	}

	virtual TSharedRef<SWidget> CreateNarrativeNodeCenterContent() override;
	virtual void UpdateVariablesListContainer();

protected:
	void OnNodeInfoPropertyChanged(const FPropertyChangedChainEvent& PropertyChangedChainEvent) { UpdateGraphNode(); }

	TSharedPtr<SVerticalBox> VariablesListContainer;

};

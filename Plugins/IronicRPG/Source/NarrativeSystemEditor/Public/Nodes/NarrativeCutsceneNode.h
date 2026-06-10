// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"
#include "LevelSequence.h"
#include "Nodes/NarrativeGraphNodeBase.h"
#include "Nodes/NarrativeCutsceneNodeInfo.h"
#include "NarrativeCutsceneNode.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVESYSTEMEDITOR_API UNarrativeCutsceneNode : public UNarrativeGraphNodeBase
{
	GENERATED_BODY()
	
public: // UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override { return FText::FromString("Cutscene Node"); }
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
	virtual void AllocateDefaultPins() override;

public: // UNarrativeGraphNode interface

};

class SNarrativeCutsceneNode : public SNarrativeGraphNodeBase
{
public:
	SLATE_BEGIN_ARGS(SNarrativeCutsceneNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphNode* InNode)
	{
		GraphNode = InNode;
		SetCursor(EMouseCursor::CardinalCross);

		TitleColor = FColor::Emerald;

		BindPropertyChangeToNodeInfo(&SNarrativeCutsceneNode::OnNodeInfoPropertyChanged);

		UpdateGraphNode();
	}

	virtual TSharedRef<SWidget> CreateNarrativeNodeCenterContent() override;

protected:
	void OnNodeInfoPropertyChanged(const FPropertyChangedChainEvent& PropertyChangedChainEvent) { UpdateGraphNode(); }

};
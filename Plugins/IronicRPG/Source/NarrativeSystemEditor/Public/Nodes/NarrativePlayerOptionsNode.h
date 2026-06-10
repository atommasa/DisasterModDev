// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NarrativeGraphNodeBase.h"
#include "Nodes/NarrativePlayerOptionsNodeInfo.h"
#include "SGraphNode.h"
#include "NarrativePlayerOptionsNode.generated.h"

/**
 * This node is used to represent a player node in the narrative graph.
 */
UCLASS()
class NARRATIVESYSTEMEDITOR_API UNarrativePlayerOptionsNode : public UNarrativeGraphNodeBase
{
	GENERATED_BODY()

public: // UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override { return FText::FromString("Player Options Node"); }
	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
	virtual void AllocateDefaultPins() override;

public: // UNarrativeGraphNodeBase interface
	virtual ENarrativeNodeType GetNarrativeNodeType() const override { return ENarrativeNodeType::PlayerOptionsNode; }
	virtual void SyncPin() override;
	virtual ECallableBindingType GetCallableBindingType() const override { return ECallableBindingType::CBT_Event; }

private: // UNarrativeGraphNodeBase interface
	virtual FText CreateCallableBindingComment() const override;
	virtual void CreateCallableBindingParameterPins(UK2Node_EditablePinBase* InNode) override;

public:
	void AddOption();
	void DeleteOption();
};

class SNarrativePlayerOptionsNode : public SNarrativeGraphNodeBase
{
public:
	SLATE_BEGIN_ARGS(SNarrativePlayerOptionsNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphNode* InNode)
	{
		GraphNode = InNode;
		SetCursor(EMouseCursor::CardinalCross);
		
		TitleColor = FColor::Orange;

		UpdateGraphNode();
	}

	virtual TSharedRef<SWidget> CreateNarrativeNodeCenterContent() override;

};
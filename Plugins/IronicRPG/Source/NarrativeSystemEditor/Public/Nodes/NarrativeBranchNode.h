// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/NarrativeGraphNodeBase.h"
#include "NarrativeBranchNode.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVESYSTEMEDITOR_API UNarrativeBranchNode : public UNarrativeGraphNodeBase
{
	GENERATED_BODY()

public: // UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override { return FText::FromString("Branch Node"); }
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
	virtual void AllocateDefaultPins() override;

public: // UNarrativeGraphNodeBase interface
	virtual ENarrativeNodeType GetNarrativeNodeType() const override { return ENarrativeNodeType::BranchNode; }
	virtual ECallableBindingType GetCallableBindingType() const override { return ECallableBindingType::CBT_Function; }
	virtual bool CanCreateCallableBinding() const override;
	virtual UFunction* GetFunctionAsSignature() const override;

private: // UNarrativeGraphNodeBase interface
	virtual FName CreateCallableBindingName() const override;
	virtual FText CreateCallableBindingComment() const override;

};

class SNarrativeBranchNode : public SNarrativeGraphNodeBase
{
public:
	SLATE_BEGIN_ARGS(SNarrativeBranchNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphNode* InNode)
	{
		GraphNode = InNode;
		SetCursor(EMouseCursor::CardinalCross);

		TitleColor = FColor::Turquoise;

		BindPropertyChangeToNodeInfo(&SNarrativeBranchNode::OnNodeInfoPropertyChanged);

		UpdateGraphNode();
	}

	virtual TSharedRef<SWidget> CreateNarrativeNodeCenterContent() override;

protected:
	void OnNodeInfoPropertyChanged(const FPropertyChangedChainEvent& PropertyChangedChainEvent) { UpdateGraphNode(); }

};

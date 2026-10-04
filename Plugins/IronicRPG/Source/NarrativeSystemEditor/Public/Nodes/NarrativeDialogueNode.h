// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/NarrativeGraphNodeBase.h"
#include "Nodes/NarrativeDialogueNodeInfo.h"
#include "NarrativeAsset.h"
#include "NarrativeDialogueNode.generated.h"

/**
 * This node is used to represent a dialogue node in the narrative graph.
 */
UCLASS()
class NARRATIVESYSTEMEDITOR_API UNarrativeDialogueNode : public UNarrativeGraphNodeBase
{
    GENERATED_BODY()

public: // UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool CanUserDeleteNode() const override { return true; }
	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
	virtual void AllocateDefaultPins() override;

public: // UNarrativeGraphNodeBase interface
	virtual ECallableBindingType GetCallableBindingType() const override { return ECallableBindingType::CBT_Event; }
	
private: // UNarrativeDialogueNode interface
	virtual FText CreateCallableBindingComment() const override;
	virtual void CreateCallableBindingParameterPins(UK2Node_EditablePinBase* InNode) override;

};

/*
* This class is used to create a custom graph node for the Narrative System.
*/
class SNarrativeDialogueNode : public SNarrativeGraphNodeBase
{
public:
	SLATE_BEGIN_ARGS(SNarrativeDialogueNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphNode* InNode)
	{
		GraphNode = InNode;
		SetCursor(EMouseCursor::CardinalCross);

		TitleColor = FColor::Purple;

		UpdateGraphNode();
	}

	virtual TSharedRef<SWidget> CreateNodeTitleWidget() override;

	virtual TSharedRef<SWidget> CreateNodeNodeCenterContent() override;

	virtual TSharedRef<SWidget> CreateTitleComboButtonMenuContent();

	virtual TSharedRef<SWidget> CreatePreviewTextBlock();

protected:
	virtual FText GetEditableDialogueText() const;

protected:
	float DialogueTextBoxWidth = 200.0f;
	float DialogueTextSize = 10.0f;

	virtual float GetWrapTextPixel() const { return DialogueTextBoxWidth - DialogueTextSize * 2.0f; }

protected:
	TSharedPtr<SWidget> PreviewTextBlock;

};

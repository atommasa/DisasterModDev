// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/NarrativeGraphNodeBase.h"
#include "Nodes/NarrativeDialogueNodeInfo.h"
#include "Nodes/NarrativeGraphNodeStyle.h"
#include "Nodes/NarrativeNodeInfo.h"
#include "NarrativeAsset.h"
#include "NarrativeGraphNode.generated.h"
// TODO: rename UDialogueGraphNode
/**
 * This node is used to represent a dialogue node in the narrative graph.
 */
UCLASS()
class NARRATIVESYSTEMEDITOR_API UNarrativeGraphNode : public UNarrativeGraphNodeBase
{
    GENERATED_BODY()

public: // UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual bool CanUserDeleteNode() const override { return true; }
	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;

	FText GetNodeDialogueText() const;

public: // UNarrativeGraphNode interface
	virtual UEdGraphPin* CreateNarrativePin(EEdGraphPinDirection Direction, FName PinName) override;

};

/*
* This class is used to create a custom graph node for the Narrative System.
*/
class SNarrativeGraphNode : public SNarrativeGraphNodeBase
{
public:
	SLATE_BEGIN_ARGS(SNarrativeGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphNode* InNode)
	{
		GraphNode = InNode;
		SetCursor(EMouseCursor::CardinalCross);

		NodeTitle = InNode->GetNodeTitle(ENodeTitleType::FullTitle);
		TitleColor = FColor::Purple;
		UpdateGraphNode();
	}

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;

	virtual TSharedRef<SWidget> CreateNarrativeTitleWidget() override;

	virtual TSharedRef<SWidget> CreateNarrativeNodeCenterContent() override;

	virtual TSharedRef<SWidget> CreateTitleComboButtonMenuContent();

	virtual TSharedRef<SWidget> CreatePreviewRichTextBlock();

protected:
	UFUNCTION()
	virtual FText GetEditableDialogueText() const;

	UFUNCTION()
	virtual void OnDialogueTextCommitted(const FText& NewText, ETextCommit::Type CommitType);

protected:
	float DialogueTextBoxWidth = 200.0f;
	float DialogueTextSize = 10.0f;

	virtual float GetWrapTextPixel() const { return DialogueTextBoxWidth - DialogueTextSize * 2.0f; }

};

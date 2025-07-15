// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"

/**
 * 
 */
class SNarrativeGraphNodeBase : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SNarrativeGraphNodeBase) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphNode* InNode)
	{
		GraphNode = InNode;
		SetCursor(EMouseCursor::CardinalCross);
		UpdateGraphNode();
	}

	virtual void UpdateGraphNode() override;

protected:
	virtual TSharedRef<SWidget> CreateNarrativeTitleWidget();

	virtual TSharedRef<SWidget> CreateNarrativeTitleRightWidget();

	virtual TSharedRef<SWidget> CreateNarrativeNodeContentArea();

	virtual TSharedRef<SWidget> CreateNarrativeNodeCenterContent();

	virtual FLinearColor GetTitleColor() const { return TitleColor; }

	virtual FLinearColor GetBackgroundColor() const { return BackgroundColor; }

protected:
	FText NodeTitle;
	int32 TitleCharMax = 20;
	FLinearColor TitleColor = FColor::Silver;


	FLinearColor BackgroundColor = FLinearColor(0.1f, 0.1f, 0.1f);
};

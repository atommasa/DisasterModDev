// Copyright Ironic Studio. All Rights Reserved.


#include "Nodes/NarrativeCutsceneNode.h"

TSharedPtr<SGraphNode> UNarrativeCutsceneNode::CreateVisualWidget()
{
	return SNew(SNarrativeCutsceneNode, this);
}

void UNarrativeCutsceneNode::AllocateDefaultPins()
{
	SetNodeInfoObject(NewObject<UNarrativeCutsceneNodeInfo>(this));

	CreateRPGGraphPin(EEdGraphPinDirection::EGPD_Input, TEXT(""));
	CreateRPGGraphPin(EEdGraphPinDirection::EGPD_Output, TEXT(""));
}

TSharedRef<SWidget> SNarrativeCutsceneNode::CreateNodeNodeCenterContent()
{
	UNarrativeCutsceneNode* CutsceneNode = Cast<UNarrativeCutsceneNode>(GraphNode);
	if (!CutsceneNode)
	{
		return SNullWidget::NullWidget;
	}

	UNarrativeCutsceneNodeInfo* CutsceneNodeInfo = CutsceneNode->GetNodeInfoAs<UNarrativeCutsceneNodeInfo>();
	if (!CutsceneNodeInfo)
	{
		return SNullWidget::NullWidget;
	}

	if (!CutsceneNodeInfo->Cutscene)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.FillContentHeight(50)
			[
				SNew(STextBlock)
					.Text(FText::FromString(TEXT("NO CUTSCENE TO PLAY")))
					.TextStyle(FAppStyle::Get(), "NormalText")
					.Font(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 10, "Bold"))
					.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.05f, 0.05f, 1.0f)))
			];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Center)
		.FillContentHeight(50)
		[
			SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("Play %s"), *CutsceneNodeInfo->Cutscene->GetName())))
				.TextStyle(FAppStyle::Get(), "NormalText")
				.Font(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 10, "Bold"))
				.ColorAndOpacity(FSlateColor(FLinearColor::Gray))
		];
}

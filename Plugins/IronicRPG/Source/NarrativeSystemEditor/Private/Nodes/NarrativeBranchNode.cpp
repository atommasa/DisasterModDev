// Copyright Ironic Studio. All Rights Reserved.


#include "Nodes/NarrativeBranchNode.h"
#include "Nodes/NarrativeBranchNodeInfo.h"
#include "NarrativeEditorUtils.h"
#include "K2Node_FunctionResult.h"

TSharedPtr<SGraphNode> UNarrativeBranchNode::CreateVisualWidget()
{
	return SNew(SNarrativeBranchNode, this);
}

void UNarrativeBranchNode::AllocateDefaultPins()
{
	SetNodeInfoObject(NewObject<UNarrativeBranchNodeInfo>(this));
	
	CreateRPGGraphPin(EEdGraphPinDirection::EGPD_Input, TEXT(""));
	CreateRPGGraphPin(EEdGraphPinDirection::EGPD_Output, TEXT("True"));
	CreateRPGGraphPin(EEdGraphPinDirection::EGPD_Output, TEXT("False"));
}

bool UNarrativeBranchNode::CanCreateCallableBinding() const
{
	UNarrativeBranchNodeInfo* BranchNodeInfo = GetNodeInfoAs<UNarrativeBranchNodeInfo>();
	if (!BranchNodeInfo)
	{
		return false;
	}

	return BranchNodeInfo->ImplementationType == EConditionImplementationType::CIT_ConditionFunction && Super::CanCreateCallableBinding();
}

UFunction* UNarrativeBranchNode::GetFunctionAsSignature() const
{
	return GetNarrativeNodeInfo()->FindFunctionChecked(GET_FUNCTION_NAME_CHECKED(UNarrativeBranchNodeInfo, IsConditionMet));
}

FName UNarrativeBranchNode::CreateCallableBindingName() const
{
	return FName(*FString::Printf(TEXT("IsConditionMet_%s"), *NodeGuid.ToString()));
}

FText UNarrativeBranchNode::CreateCallableBindingComment() const
{
	return FText::FromString(TEXT("Define your own condition logic in this function."));
}

TSharedRef<SWidget> SNarrativeBranchNode::CreateNodeNodeCenterContent()
{
	UNarrativeBranchNode* BranchNode = Cast<UNarrativeBranchNode>(GraphNode);
	if (!BranchNode)
	{
		return SNullWidget::NullWidget;
	}

	UNarrativeBranchNodeInfo* BranchNodeInfo = BranchNode->GetNodeInfoAs<UNarrativeBranchNodeInfo>();
	if (!BranchNodeInfo)
	{
		return SNullWidget::NullWidget;
	}

	if (!BranchNodeInfo->Condition)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.FillContentHeight(50)
			[
				SNew(STextBlock)
					.Text(FText::FromString(TEXT("NO CONDITION")))
					.TextStyle(FAppStyle::Get(), "NormalText")
					.Font(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 12, "Bold"))
					.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.05f, 0.05f, 1.0f)))
			];
	}

	FText BranchNodeText = BranchNodeInfo->ImplementationType == EConditionImplementationType::CIT_ConditionObject ?
		BranchNodeInfo->Condition->GetBranchNodeText(BranchNodeInfo->bInverse) :
		FText::FromString(TEXT("Custom Blueprint Function"));

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0, 5, 0, 5))
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("CONDITION")))
				.TextStyle(FAppStyle::Get(), "NormalText")
				.Font(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 12, "Bold"))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.75f, 0.05f, 1.0f)))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.FillContentHeight(50)
		[
			SNew(STextBlock)
				.Text(BranchNodeText)
				.Font(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 10, "Bold"))
		];
}

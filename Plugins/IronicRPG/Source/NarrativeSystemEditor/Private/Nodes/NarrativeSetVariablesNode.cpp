// Copyright Ironic Studio. All Rights Reserved.


#include "Nodes/NarrativeSetVariablesNode.h"
#include "Nodes/NarrativeSetVariablesNodeInfo.h"

TSharedPtr<SGraphNode> UNarrativeSetVariablesNode::CreateVisualWidget()
{
	return SNew(SNarrativeSetVariablesNode, this);
}

void UNarrativeSetVariablesNode::AllocateDefaultPins()
{
	SetNodeInfo(NewObject<UNarrativeSetVariablesNodeInfo>(this));

	CreateNarrativePin(EEdGraphPinDirection::EGPD_Input, TEXT(""));
	CreateNarrativePin(EEdGraphPinDirection::EGPD_Output, TEXT(""));
}

TSharedRef<SWidget> SNarrativeSetVariablesNode::CreateNarrativeNodeCenterContent()
{
	UNarrativeSetVariablesNode* SetVariablesNode = Cast<UNarrativeSetVariablesNode>(GraphNode);
	if (!SetVariablesNode)
	{
		return SNullWidget::NullWidget;
	}

	UNarrativeSetVariablesNodeInfo* SetVariablesNodeInfo = SetVariablesNode->GetNodeInfoAs<UNarrativeSetVariablesNodeInfo>();
	if (!SetVariablesNodeInfo)
	{
		return SNullWidget::NullWidget;
	}

	if (SetVariablesNodeInfo->VariableAssignments.IsEmpty())
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.FillContentHeight(50)
			[
				SNew(STextBlock)
					.Text(FText::FromString(TEXT("NO VARIABLE TO SET")))
					.TextStyle(FAppStyle::Get(), "NormalText")
					.Font(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 12, "Bold"))
					.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.05f, 0.05f, 1.0f)))
			];
	}

	TSharedRef<SWidget> WidgetRef = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0, 5, 0, 5))
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("VARIABLES")))
				.TextStyle(FAppStyle::Get(), "NormalText")
				.Font(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 12, "Bold"))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.75f, 0.05f, 1.0f)))
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		.FillContentHeight(50)
		[
			SAssignNew(VariablesListContainer, SVerticalBox)
		];

	UpdateVariablesListContainer();

	return WidgetRef;
}

void SNarrativeSetVariablesNode::UpdateVariablesListContainer()
{
	if (!VariablesListContainer)
	{
		return;
	}

	VariablesListContainer->ClearChildren();

	UNarrativeSetVariablesNode* SetVariablesNode = Cast<UNarrativeSetVariablesNode>(GraphNode);
	if (!SetVariablesNode)
	{
		return;
	}

	UNarrativeSetVariablesNodeInfo* SetVariablesNodeInfo =
		SetVariablesNode->GetNodeInfoAs<UNarrativeSetVariablesNodeInfo>();

	if (!SetVariablesNodeInfo)
	{
		return;
	}

	for (const FNarrativeVariableAssignment& Assignment : SetVariablesNodeInfo->VariableAssignments)
	{
		const FName VariableName = Assignment.VariableName;

		if (VariableName.IsNone())
		{
			continue;
		}

		FString ValueString = Assignment.Value.IsValid()
			? Assignment.ToString()
			: TEXT("None");

		if (ValueString.Len() > 40)
		{
			// Truncate long value strings for display purposes
			const int32 CharsToShow = 17;
			const int32 StartChars = CharsToShow / 2;
			const int32 EndChars = CharsToShow - StartChars;
			const FString StartStr = ValueString.Left(StartChars);
			const FString EndStr = ValueString.Right(EndChars);
			ValueString = FString::Printf(TEXT("%s...%s"), *StartStr, *EndStr);
		}

		const FText VariableText = FText::FromString(
			FString::Printf(
				TEXT("Set %s as %s"),
				*VariableName.ToString(),
				ValueString.IsEmpty() ? TEXT("None") : *ValueString
			)
		);

		VariablesListContainer->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0, 2))
			[
				SNew(STextBlock)
					.Text(VariableText)
					.TextStyle(FAppStyle::Get(), "NormalText")
					.Font(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 10))
					.ColorAndOpacity(FSlateColor(FLinearColor::White))
			];
	}
}

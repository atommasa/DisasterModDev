// Fill out your copyright notice in the Description page of Project Settings.


#include "Nodes/NarrativeGraphNodeStyle.h"

void SNarrativeGraphNodeBase::UpdateGraphNode()
{
	SGraphNode::UpdateGraphNode();

	GetOrAddSlot(ENodeZone::Center)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body"))
						.BorderBackgroundColor(GetBackgroundColor())
						.Padding(0)
						[
							SNew(SVerticalBox)

								// Node title area
								+ SVerticalBox::Slot()
								.AutoHeight()
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Fill)
								[
									CreateNarrativeTitleWidget()
								]

								// Node content area
								+ SVerticalBox::Slot()
								.AutoHeight()
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Top)
								[
									CreateNarrativeNodeContentArea()
								]
						]
				]
		];
}

TSharedRef<SWidget> SNarrativeGraphNodeBase::CreateNarrativeTitleWidget()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Graph.Node.TitleBackground"))
		.BorderBackgroundColor(GetTitleColor().Desaturate(0.2f))
		.Padding(FMargin(0, 2))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
				.Text_Lambda([&]() -> FText
					{
						FString TitleString = NodeTitle.ToString();
						if (TitleString.Len() > TitleCharMax)
						{
							TitleString = TitleString.Left(TitleCharMax) + TEXT("...");
						}

						return FText::FromString(TitleString);
					})
				.Font(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 12, "Bold"))
		];
}

TSharedRef<SWidget> SNarrativeGraphNodeBase::CreateNarrativeTitleRightWidget()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill);
}

TSharedRef<SWidget> SNarrativeGraphNodeBase::CreateNarrativeNodeContentArea()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(0, 3))
		[
			SNew(SHorizontalBox)

				// Left pins
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Left)
				[
					SNew(SBox)
						.WidthOverride(50)
						[
							LeftNodeBox.ToSharedRef()
						]
				]

				// Center content
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.FillWidth(1.0f)
				.Padding(FMargin(0, 2))
				[
					CreateNarrativeNodeCenterContent()
				]

				// Right pins
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Right)
				[
					SNew(SBox)
						.WidthOverride(50)
						[
							RightNodeBox.ToSharedRef()
						]
				]
		];
}

TSharedRef<SWidget> SNarrativeGraphNodeBase::CreateNarrativeNodeCenterContent()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill);
}

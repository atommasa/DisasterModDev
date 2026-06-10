// Copyright Ironic Studio. All Rights Reserved.


#include "Nodes/NarrativeGraphNodeStyle.h"
#include "Nodes/NarrativeGraphNodeBase.h"

EEdGraphPinDirection FNarrativeGraphDragState::DragSourceDirection = EEdGraphPinDirection::EGPD_MAX;

void SNarrativeGraphNodeBase::UpdateGraphNode()
{
	SGraphNode::UpdateGraphNode();

	if (GraphNode)
	{
		NodeTitle = GraphNode->GetNodeTitle(ENodeTitleType::FullTitle);
	}

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

UNarrativeNodeInfo* SNarrativeGraphNodeBase::GetNarrativeNodeInfo() const
{
	return GraphNode ? Cast<UNarrativeGraphNodeBase>(GraphNode)->GetNodeInfo() : nullptr;
}

FReply SNarrativeGraphNodeBase::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	UNarrativeGraphNodeBase* NodePtr = Cast<UNarrativeGraphNodeBase>(GraphNode);
	if (!NodePtr)
	{
		return FReply::Unhandled();
	}

	UNarrativeNodeInfo* NodeInfo = NodePtr->GetNodeInfo();
	if (!NodeInfo)
	{
		return FReply::Unhandled();
	}

	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	if (NodePtr->CanCreateCallableBinding())
	{
		NodeInfo->Modify();

		NodeInfo->CallableBindingName = NodePtr->CreateCallableBindingName();

		switch (NodePtr->GetCallableBindingType())
		{
		case ECallableBindingType::CBT_Event:
			NodePtr->CreateOrFocusNarrativeCustomEvent();
			break;
		case ECallableBindingType::CBT_Function:
			NodePtr->CreateOrFocusNarrativeFunction();
			break;
		default:
			break;
		}
	}

	return FReply::Handled();
}

TSharedRef<SWidget> SNarrativeGraphNodeBase::CreateNarrativeTitleWidget()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Graph.Node.TitleBackground"))
		.BorderBackgroundColor(GetTitleColor().Desaturate(0.2f))
		.Padding(FMargin(15, 2))
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
						.MinDesiredWidth(50)
						[
							LeftNodeBox.ToSharedRef()
						]
				]

				// Center content
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.FillWidth(1.0f)
				.Padding(FMargin(0, 7))
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
						.MinDesiredWidth(50)
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

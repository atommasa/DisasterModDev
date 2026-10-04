// Copyright Ironic Studio. All Rights Reserved.

#include "Nodes/RPGGraphNodeStyle.h"
#include "Nodes/RPGGraphNodeBase.h"

EEdGraphPinDirection FRPGGraphDragState::DragSourceDirection = EGPD_MAX;

void SRPGGraphNodeBase::UpdateGraphNode()
{
	SGraphNode::UpdateGraphNode();

	BindNodeInfoChanged();

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
									CreateNodeTitleWidget()
								]

								// Node content area
								+ SVerticalBox::Slot()
								.AutoHeight()
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Top)
								[
									CreateNodeNodeContentArea()
								]
						]
				]
		];
}

FReply SRPGGraphNodeBase::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	URPGGraphNodeBase* NodePtr = Cast<URPGGraphNodeBase>(GraphNode);
	if (!NodePtr)
	{
		return FReply::Unhandled();
	}

	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	if (NodePtr->CanCreateCallableBinding())
	{
		const FName& CallableBindingName = NodePtr->CreateCallableBindingName();

		switch (NodePtr->GetCallableBindingType())
		{
		case ECallableBindingType::CBT_Event:
			NodePtr->CreateOrFocusCustomEvent(CallableBindingName);
			break;
		case ECallableBindingType::CBT_Function:
			NodePtr->CreateOrFocusCustomFunction(CallableBindingName);
			break;
		default:
			break;
		}
	}

	return FReply::Handled();
}

TSharedRef<SWidget> SRPGGraphNodeBase::CreateNodeTitleWidget()
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

TSharedRef<SWidget> SRPGGraphNodeBase::CreateNodeTitleRightWidget()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill);
}

TSharedRef<SWidget> SRPGGraphNodeBase::CreateNodeNodeContentArea()
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
					CreateNodeNodeCenterContent()
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

TSharedRef<SWidget> SRPGGraphNodeBase::CreateNodeNodeCenterContent()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill);
}

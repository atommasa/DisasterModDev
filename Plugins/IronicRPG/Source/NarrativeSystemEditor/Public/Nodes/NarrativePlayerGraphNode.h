// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NarrativeGraphNodeBase.h"
#include "Nodes/NarrativePlayerNodeInfo.h"
#include "SGraphNode.h"
#include "NarrativePlayerGraphNode.generated.h"

/**
 * This node is used to represent a player node in the narrative graph.
 */
UCLASS()
class NARRATIVESYSTEMEDITOR_API UNarrativePlayerGraphNode : public UNarrativeGraphNodeBase
{
	GENERATED_BODY()

public: // UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override { return FText::FromString("Player Node"); }
	virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor::FLinearColor(0.9f, 0.1f, 0.0f, 1.0f); }
	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;

public: // UNarrativeGraphNode interface
	virtual UEdGraphPin* CreateNarrativePin(EEdGraphPinDirection Direction, FName PinName) override;
	virtual void SyncPinWithResponse() override;

public:
	virtual void AddPin() override;
	virtual void DeletePin() override;
};

class SNarrativePlayerGraphNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SNarrativePlayerGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UNarrativePlayerGraphNode* InNode)
	{
		GraphNode = Cast<UEdGraphNode>(InNode);
		UpdateGraphNode();
	}

	virtual void UpdateGraphNode() override
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
							.BorderBackgroundColor_Lambda([this]() -> FSlateColor
								{
									return GraphNode->GetNodeTitleColor();
								})
							.Padding(0)
							[
								SNew(SVerticalBox)

									// Node content area
									+ SVerticalBox::Slot()
									.AutoHeight()
									.HAlign(HAlign_Fill)
									.VAlign(VAlign_Top)
									[
										SNew(SBorder)
											.BorderImage(FAppStyle::GetBrush("NoBorder"))
											.HAlign(HAlign_Fill)
											.VAlign(VAlign_Fill)
											.Padding(FMargin(0, 3))
											[
												SNew(SHorizontalBox)

													// Left pins
													+ SHorizontalBox::Slot()
													.AutoWidth()
													.VAlign(VAlign_Center)
													.HAlign(HAlign_Left)
													[
														SNew(SBox)
															.WidthOverride(120)
															[
																LeftNodeBox.ToSharedRef()
															]
													]

													+ SHorizontalBox::Slot()
													.VAlign(VAlign_Center)
													.HAlign(HAlign_Center)
													.FillWidth(1.0f)
													.Padding(FMargin(0, 3))
													[
														SNew(SVerticalBox)
															+ SVerticalBox::Slot()
															.AutoHeight()
															[
																SNew(STextBlock)
																	.Text(FText::FromString(TEXT("Player Options")))
																	.Font(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 16, "Bold"))
															]

													]

													// Right pins
													+ SHorizontalBox::Slot()
													.AutoWidth()
													.VAlign(VAlign_Center)
													.HAlign(HAlign_Right)
													[
														SNew(SBox)
															.WidthOverride(120)
															[
																RightNodeBox.ToSharedRef()
															]
													]
											]
									]
							]
					]
			];
	}
};
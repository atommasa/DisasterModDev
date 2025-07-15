// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"
#include "LevelSequence.h"
#include "Nodes/NarrativeGraphNodeBase.h"
#include "Nodes/NarrativeCutsceneNodeInfo.h"
#include "NarrativeCutsceneNode.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVESYSTEMEDITOR_API UNarrativeCutsceneNode : public UNarrativeGraphNodeBase
{
	GENERATED_BODY()
	
public: // UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override { return FText::FromString("Player Node"); }
	virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor::FLinearColor(0.9f, 0.5f, 0.0f, 1.0f); }
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;

public: // UNarrativeGraphNode interface
	virtual UEdGraphPin* CreateNarrativePin(EEdGraphPinDirection Direction, FName PinName) override;

};

class SNarrativeCutsceneNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SNarrativeCutsceneNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UNarrativeCutsceneNode* InNode)
	{
		GraphNode = InNode;
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
															.WidthOverride(50)
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
															.Padding(5)
															[
																SNew(STextBlock)
																	.Text_Lambda([this]() -> FText
																		{
																			if (UNarrativeCutsceneNode* Node = Cast<UNarrativeCutsceneNode>(GraphNode))
																			{
																				if (UNarrativeCutsceneNodeInfo* NodeInfo = Cast<UNarrativeCutsceneNodeInfo>(Node->GetNodeInfo()))
																				{
																					if (NodeInfo->Cutscene)
																					{
																						return FText::FromString(NodeInfo->Cutscene->GetName());
																					}
																				}
																			}

																			return FText::FromString("No cutscene");
																		})
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
															.WidthOverride(50)
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
// Copyright Ironic Studio. All Rights Reserved.


#include "Nodes/NarrativeDialogueNode.h"
#include "EdGraph/EdGraphPin.h"
#include "ToolMenu.h"
#include "Nodes/NarrativeDialogueNodeInfo.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Decorators/RPGTextDecoratorInstance.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "NarrativeEditorUtils.h"

FText UNarrativeDialogueNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	UNarrativeDialogueNodeInfo* DialogueNodeInfo = GetNodeInfoAs<UNarrativeDialogueNodeInfo>();
	if (!DialogueNodeInfo)
	{
		return FText::FromString(TEXT("Unknown Speaker"));
	}

	FString SpeakerName = DialogueNodeInfo->SpeakerName.ToString();

	if (!DialogueNodeInfo->OverriddenName.IsEmpty())
	{
		SpeakerName = DialogueNodeInfo->OverriddenName.ToString() + FString::Printf(TEXT(" (%s)"), *SpeakerName);
	}

	return FText::FromString(TEXT("Speaker : ") + SpeakerName);
}

void UNarrativeDialogueNode::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	FToolMenuSection& Section = Menu->AddSection(TEXT("SectionName"), FText::FromString(TEXT("Narratvie Node Actions")));

	UNarrativeDialogueNode* NodePtr = (UNarrativeDialogueNode*)this;

	Super::GetNodeContextMenuActions(Menu, Context);
}

TSharedPtr<SGraphNode> UNarrativeDialogueNode::CreateVisualWidget()
{
	return SNew(SNarrativeDialogueNode, this);
}

void UNarrativeDialogueNode::AllocateDefaultPins()
{
	SetNodeInfoObject(NewObject<UNarrativeDialogueNodeInfo>(this));

	CreateRPGGraphPin(EEdGraphPinDirection::EGPD_Input, TEXT(""));
	CreateRPGGraphPin(EEdGraphPinDirection::EGPD_Output, TEXT(""));
}

FText UNarrativeDialogueNode::CreateCallableBindingComment() const
{
	return FText::FromString(TEXT("This event will be called when the NarrativeDialogueNode starts / finishes"));
}

void UNarrativeDialogueNode::CreateCallableBindingParameterPins(UK2Node_EditablePinBase* InNode)
{
	FNarrativeEditorUtils::CreateUserDefinedPinFor(
		InNode,
		TEXT("DialogueEventType"),
		FEdGraphPinType(
			UEdGraphSchema_K2::PC_Byte,
			NAME_None,
			StaticEnum<EDialogueEventType>(),
			EPinContainerType::None,
			false,
			FEdGraphTerminalType()
		),
		EGPD_Output
	);
}

TSharedRef<SWidget> SNarrativeDialogueNode::CreateNodeTitleWidget()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Graph.Node.TitleBackground"))
		.BorderBackgroundColor(GetTitleColor().Desaturate(0.2f))
		.Padding(0)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)

				// Title Combo Button
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoHeight()
				[
					SNew(SComboButton)
						.ButtonStyle(FAppStyle::Get(), "FlatButton")
						.ContentPadding(0)
						.ForegroundColor(FSlateColor::UseForeground())
						.OnGetMenuContent(this, &SNarrativeDialogueNode::CreateTitleComboButtonMenuContent)
						.ButtonContent()
						[
							SNew(STextBlock)
								.Text(GraphNode->GetNodeTitle(ENodeTitleType::EditableTitle))
								.Font(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 12, "Bold"))
						]
				]

		];
}

TSharedRef<SWidget> SNarrativeDialogueNode::CreateNodeNodeCenterContent()
{
	UNarrativeDialogueNode* DialogueNode = Cast<UNarrativeDialogueNode>(GraphNode);
	if (!DialogueNode)
	{
		return SNullWidget::NullWidget;
	}

	UNarrativeDialogueNodeInfo* DialogueNodeInfo = DialogueNode->GetNodeInfoAs<UNarrativeDialogueNodeInfo>();
	if (!DialogueNode)
	{
		return SNullWidget::NullWidget;
	}

	if (DialogueNodeInfo->DialogueLine.DialogueText.IsEmpty())
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.FillContentHeight(50)
			.AutoHeight()
			[
				SNew(STextBlock)
					.Text(FText::FromString(TEXT("NO TEXT")))
					.TextStyle(FAppStyle::Get(), "NormalText")
					.Font(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 12, "Bold"))
					.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.05f, 0.05f, 1.0f)))
			];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0, 5, 0, 5))
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("TEXT CONTENT")))
				.TextStyle(FAppStyle::Get(), "NormalText")
				.Font(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 12, "Bold"))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.75f, 0.05f, 1.0f)))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.FillContentHeight(50)
		[
			CreatePreviewTextBlock()
		];
}

TSharedRef<SWidget> SNarrativeDialogueNode::CreateTitleComboButtonMenuContent()
{
	TSharedRef<SVerticalBox> ButtonList = SNew(SVerticalBox);

	if (UNarrativeDialogueNode* Node = Cast<UNarrativeDialogueNode>(GraphNode))
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Node);
		UDialogue* Dialogue = Cast<UDialogue>(Blueprint->GeneratedClass->GetDefaultObject());
		if (!Dialogue)
		{
			return SNew(SBox);
		}

		for (const FSpeakerData& Data : Dialogue->Speakers)
		{
			FText SpeakerName = Data.GetSpeakerDisplayName();

			ButtonList->AddSlot()
				.AutoHeight()
				.Padding(2)
				[
					SNew(SButton)
						.Text(SpeakerName)
						.OnClicked_Lambda([=]() -> FReply
							{
								const FScopedTransaction Transaction(NSLOCTEXT("Narrative", "ChangeNodeSpeaker", "Change Node Speaker"));
								Node->Modify();
								Node->GetGraph()->Modify();

								if (UNarrativeDialogueNodeInfo* DialogueNodeInfo = Cast<UNarrativeDialogueNodeInfo>(Node->GetNarrativeNodeInfo()))
								{
									DialogueNodeInfo->Modify();
									DialogueNodeInfo->SpeakerName = SpeakerName;
								}

								Node->GetGraph()->NotifyNodeChanged(Node);
								return FReply::Handled();
							})
				];
		}
	}

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		.Padding(5)
		[
			ButtonList
		];
}

TSharedRef<SWidget> SNarrativeDialogueNode::CreatePreviewTextBlock()
{
	return SAssignNew(PreviewTextBlock, SRichTextBlock)
		.Text(this, &SNarrativeDialogueNode::GetEditableDialogueText)
		.TextStyle(FAppStyle::Get(), "NormalText")
		.AutoWrapText(true)
		.Decorators({ MakeShareable(new FRPGTextStyleDecorator()) });
}

FText SNarrativeDialogueNode::GetEditableDialogueText() const
{
	if (const UNarrativeDialogueNode* DialogueNode = Cast<UNarrativeDialogueNode>(GraphNode))
	{
		if (const UNarrativeDialogueNodeInfo* Info = Cast<UNarrativeDialogueNodeInfo>(DialogueNode->GetNarrativeNodeInfo()))
		{
			return Info->DialogueLine.DialogueText;
		}
	}

	return FText::GetEmpty();
}

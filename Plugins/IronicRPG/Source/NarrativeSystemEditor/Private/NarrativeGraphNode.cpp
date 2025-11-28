// Fill out your copyright notice in the Description page of Project Settings.


#include "NarrativeGraphNode.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "EdGraph/EdGraphPin.h"
#include "ToolMenu.h"
#include "Nodes/NarrativeDialogueNodeInfo.h"
#include "NarrativeAsset.h"
#include "Assets/NarrativeAssetEditorApp.h"
#include "NarrativeAsset.h"
#include "NarrativeEditorSubsystem.h"
#include "Characters/CharacterAsset.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Components/RichTextBlock.h"
#include "Decorators/RPGTextDecoratorInstance.h"

FText UNarrativeGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	UNarrativeDialogueNodeInfo* DialogueNodeInfo = Cast<UNarrativeDialogueNodeInfo>(GetNodeInfo());
	if (!DialogueNodeInfo)
	{
		return FText::FromString(TEXT("Unknown Speaker"));
	}

	auto* NarrativeEditorSubsystem = GEditor->GetEditorSubsystem<UNarrativeEditorSubsystem>();
	if (!NarrativeEditorSubsystem)
	{
		return FText::FromString(TEXT("Unknown Speaker"));
	}

	auto Assets = NarrativeEditorSubsystem->TryGetSpeakerAssets();

	FRPGId SpeakerId = DialogueNodeInfo->SpeakerId;
	if (!Assets.Contains(SpeakerId))
	{
		return FText::FromString(TEXT("Unknown Speaker"));
	}

	FString Title = TEXT("Speaker : ") + Assets[SpeakerId]->GetDisplayName().DefaultName.ToString();

	return FText::FromString(Title);
}

FLinearColor UNarrativeGraphNode::GetNodeTitleColor() const
{
	return FLinearColor::Yellow;
}

void UNarrativeGraphNode::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	FToolMenuSection& Section = Menu->AddSection(TEXT("SectionName"), FText::FromString(TEXT("Narratvie Node Actions")));

	UNarrativeGraphNode* NodePtr = (UNarrativeGraphNode*)this;

	/*Section.AddMenuEntry
	(
		
	);*/

	Super::GetNodeContextMenuActions(Menu, Context);
}

TSharedPtr<SGraphNode> UNarrativeGraphNode::CreateVisualWidget()
{
	return SNew(SNarrativeGraphNode, this);
}

FText UNarrativeGraphNode::GetNodeDialogueText() const
{
	UNarrativeDialogueNodeInfo* DialogueNodeInfo = Cast<UNarrativeDialogueNodeInfo>(GetNodeInfo());
	if (!DialogueNodeInfo)
	{
		return FText::FromString(TEXT(""));
	}
	
	FText Dialogue = DialogueNodeInfo->Dialogue;

	if (Dialogue.ToString().Len() > 15)
	{
		return FText::FromString(Dialogue.ToString().Left(15) + TEXT("..."));
	}

	return Dialogue;
}

UEdGraphPin* UNarrativeGraphNode::CreateNarrativePin(EEdGraphPinDirection Direction, FName PinName)
{
	FName Category = (Direction == EGPD_Input) ? TEXT("Input") : TEXT("Output");
	FName SubCategory = TEXT("NarrativePin");

	UEdGraphPin* NewPin = CreatePin(Direction, Category, PinName);
	NewPin->PinType.PinSubCategory = SubCategory;
	
	return NewPin;
}

FReply SNarrativeGraphNode::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (UNarrativeGraphNodeBase* NarrativeNode = Cast<UNarrativeGraphNodeBase>(GraphNode))
		{
			if (TSharedPtr<NarrativeAssetEditorApp> EditorApp = NarrativeNode->GetNarrativeAsset()->EditorApp.Pin())
			{
				UBlueprint* Blueprint = EditorApp->GetBlueprintObj();
				if (Blueprint && Blueprint->UbergraphPages.Num() > 0)
				{
					EditorApp->OpenDocument(Blueprint->UbergraphPages[0], FDocumentTracker::OpenNewDocument);
				}
			}
		}
	}

	return FReply::Handled();
}

TSharedRef<SWidget> SNarrativeGraphNode::CreateNarrativeTitleWidget()
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
						.OnGetMenuContent(this, &SNarrativeGraphNode::CreateTitleComboButtonMenuContent)
						.ButtonContent()
						[
							SNew(STextBlock)
								.Text(GraphNode->GetNodeTitle(ENodeTitleType::EditableTitle))
								.Font(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 12, "Bold"))
						]
				]

		];
}

TSharedRef<SWidget> SNarrativeGraphNode::CreateNarrativeNodeCenterContent()
{
	return SNew(SBox)
		.WidthOverride(DialogueTextBoxWidth)
		[
			SNew(SVerticalBox)

				// Editable Dialogue Text Block
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.AutoHeight()
				.Padding(FMargin(0, 2))
				[
					SNew(SMultiLineEditableTextBox)
						.Text(this, &SNarrativeGraphNode::GetEditableDialogueText)
						.OnTextCommitted(this, &SNarrativeGraphNode::OnDialogueTextCommitted)
						.WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
						.AutoWrapText(false)
						.WrapTextAt(GetWrapTextPixel())
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", DialogueTextSize))
				]

				// Preview Rich Text Block
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.AutoHeight()
				.Padding(FMargin(0, 2))
				[
					CreatePreviewRichTextBlock()
				]
		];
}

TSharedRef<SWidget> SNarrativeGraphNode::CreateTitleComboButtonMenuContent()
{
	TSharedRef<SVerticalBox> ButtonList = SNew(SVerticalBox);

	if (UNarrativeGraphNode* Node = Cast<UNarrativeGraphNode>(GraphNode))
	{
		auto* NarrativeEditorSubsystem = GEditor->GetEditorSubsystem<UNarrativeEditorSubsystem>();
		if (!NarrativeEditorSubsystem)
		{
			return SNew(SBorder);
		}

		auto Assets = NarrativeEditorSubsystem->TryGetSpeakerAssets();

		for (const auto& Asset : Assets)
		{
			FText SpeakerName = Asset.Value->GetDisplayName().DefaultName;
			FRPGId SpeakerId = Asset.Value->GetId();

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

								if (UNarrativeDialogueNodeInfo* DialogueNodeInfo = Cast<UNarrativeDialogueNodeInfo>(Node->GetNodeInfo()))
								{
									DialogueNodeInfo->Modify();
									DialogueNodeInfo->SpeakerId = SpeakerId;
								}

								Node->GetGraph()->NotifyGraphChanged();
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

TSharedRef<SWidget> SNarrativeGraphNode::CreatePreviewRichTextBlock()
{
	return SNew(SRichTextBlock)
		.Text(this, &SNarrativeGraphNode::GetEditableDialogueText)
		.TextStyle(FAppStyle::Get(), "NormalText")
		.AutoWrapText(true)
		.Decorators({ MakeShareable(new FRPGTextStyleDecorator()) });
}

FText SNarrativeGraphNode::GetEditableDialogueText() const
{
	if (const UNarrativeGraphNode* DialogueNode = Cast<UNarrativeGraphNode>(GraphNode))
	{
		if (const UNarrativeDialogueNodeInfo* Info = Cast<UNarrativeDialogueNodeInfo>(DialogueNode->GetNodeInfo()))
		{
			return Info->Dialogue;
		}
	}

	return FText::GetEmpty();
}

void SNarrativeGraphNode::OnDialogueTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	if (UNarrativeGraphNode* DialogueNode = Cast<UNarrativeGraphNode>(GraphNode))
	{
		if (UNarrativeDialogueNodeInfo* Info = Cast<UNarrativeDialogueNodeInfo>(DialogueNode->GetNodeInfo()))
		{
			const FScopedTransaction Transaction(NSLOCTEXT("Narrative", "EditDialogueText", "Edit Dialogue Text"));
			DialogueNode->Modify();
			Info->Modify();

			Info->Dialogue = NewText;
			DialogueNode->GetGraph()->NotifyGraphChanged();
		}
	}
}

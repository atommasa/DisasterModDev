// Copyright Ironic Studio. All Rights Reserved.


#include "Nodes/NarrativePlayerOptionsNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Editor/Transactor.h"
#include "NarrativeSystemEditor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "NarrativeEditorUtils.h"

TSharedPtr<SGraphNode> UNarrativePlayerOptionsNode::CreateVisualWidget()
{
	return SNew(SNarrativePlayerOptionsNode, this);
}

void UNarrativePlayerOptionsNode::AllocateDefaultPins()
{
	SetNodeInfo(NewObject<UNarrativePlayerOptionsNodeInfo>(this));

	CreateNarrativePin(EEdGraphPinDirection::EGPD_Input, TEXT(""));
	if (UNarrativePlayerOptionsNodeInfo* PlayerOptionsNodeInfo = GetNodeInfoAs<UNarrativePlayerOptionsNodeInfo>())
	{
		PlayerOptionsNodeInfo->Options.Add({ });
	}

	SyncPin();
}

void UNarrativePlayerOptionsNode::GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
{
	FToolMenuSection& Section = Menu->AddSection(TEXT("SectionName"), FText::FromString(TEXT("Narratvie Node Actions")));

	UNarrativePlayerOptionsNode* NodePtr = const_cast<UNarrativePlayerOptionsNode*>(this);

	Section.AddMenuEntry
	(
		TEXT("AddPinEntry"),
		FText::FromString(TEXT("Add Option")),
		FText::FromString(TEXT("Adds an option to the node.")),
		FSlateIcon(FNarrativeSystemEditorStyleSet::Get().GetStyleSetName(), "NarrativeEditor.NodeAddPinIcon"),
		FUIAction
		(
			FExecuteAction::CreateLambda([NodePtr]()
				{
					if (NodePtr)
					{
						NodePtr->AddOption();
					}
				})
		)
	);

	Section.AddMenuEntry
	(
		TEXT("DeletePinEntry"),
		FText::FromString(TEXT("Delete Option")),
		FText::FromString(TEXT("Deletes an option from the node.")),
		FSlateIcon(FNarrativeSystemEditorStyleSet::Get().GetStyleSetName(), "NarrativeEditor.NodeDeletePinIcon"),
		FUIAction
		(
			FExecuteAction::CreateLambda([NodePtr]()
				{
					if (NodePtr)
					{
						NodePtr->DeleteOption();
					}
				})
		)
	);

	Super::GetNodeContextMenuActions(Menu, Context);
}

void UNarrativePlayerOptionsNode::SyncPin()
{
	UNarrativePlayerOptionsNodeInfo* Info = Cast<UNarrativePlayerOptionsNodeInfo>(GetNodeInfo());
	if (!Info)
	{
		return;
	}

	Modify();

	TArray<UEdGraphPin*> OutputPins;
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output)
		{
			OutputPins.Add(Pin);
		}
	}

	while (OutputPins.Num() > Info->Options.Num())
	{
		UEdGraphPin* PinToRemove = OutputPins.Pop();
		PinToRemove->BreakAllPinLinks();
		Pins.Remove(PinToRemove);
		PinToRemove->MarkAsGarbage();
	}

	while (OutputPins.Num() < Info->Options.Num())
	{
		const int32 Index = OutputPins.Num();

		UEdGraphPin* NewPin = CreateNarrativePin(EGPD_Output, FName(*FString::FromInt(Index)));

		OutputPins.Add(NewPin);
	}

	for (int32 Index = 0; Index < Info->Options.Num(); ++Index)
	{
		OutputPins[Index]->PinFriendlyName = Info->Options[Index];
	}

	if (UEdGraph* Graph = GetGraph())
	{
		Graph->NotifyNodeChanged(this);
	}
}

FText UNarrativePlayerOptionsNode::CreateCallableBindingComment() const
{
	return FText::FromString(TEXT("This event will be called when the PlayerOptionsNode starts / commits / finishes"));
}

void UNarrativePlayerOptionsNode::CreateCallableBindingParameterPins(UK2Node_EditablePinBase* InNode)
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

	FNarrativeEditorUtils::CreateUserDefinedPinFor(
		InNode,
		TEXT("PlayerSelectedIndex"),
		FEdGraphPinType(
			UEdGraphSchema_K2::PC_Int,
			NAME_None,
			nullptr,
			EPinContainerType::None,
			false,
			FEdGraphTerminalType()
		),
		EGPD_Output
	);

	FNarrativeEditorUtils::CreateUserDefinedPinFor(
		InNode,
		TEXT("PlayerSelectedOption"),
		FEdGraphPinType(
			UEdGraphSchema_K2::PC_Text,
			NAME_None,
			nullptr,
			EPinContainerType::None,
			false,
			FEdGraphTerminalType()
		),
		EGPD_Output
	);
}

void UNarrativePlayerOptionsNode::AddOption()
{
	const FScopedTransaction Transaction(NSLOCTEXT("Narrative", "AddOption", "Add Dialogue Option"));
	Modify();
	GetGraph()->Modify();
	
	if (UNarrativePlayerOptionsNodeInfo* PlayerOptionsNodeInfo = Cast<UNarrativePlayerOptionsNodeInfo>(GetNodeInfo()))
	{
		PlayerOptionsNodeInfo->Modify();

		PlayerOptionsNodeInfo->Options.Add(FText::FromString(TEXT("")));
	}

	SyncPin();
	GetGraph()->NotifyNodeChanged(this);
}

void UNarrativePlayerOptionsNode::DeleteOption()
{
	const FScopedTransaction Transaction(NSLOCTEXT("Narrative", "DeleteOption", "Delete Dialogue Option"));
	Modify();
	GetGraph()->Modify();

	if (UNarrativePlayerOptionsNodeInfo* PlayerOptionsNodeInfo = Cast<UNarrativePlayerOptionsNodeInfo>(GetNodeInfo()))
	{
		PlayerOptionsNodeInfo->Modify();

		// At least one option should be kept
		if (PlayerOptionsNodeInfo->Options.Num() > 1)
		{
			PlayerOptionsNodeInfo->Options.Pop();
		}
	}

	SavedConnections.Empty();

	UEdGraphPin* OutputPin = Pins.Last();
	for (UEdGraphPin* Linked : OutputPin->LinkedTo)
	{
		if (Linked)
		{
			FPinConnectionData Conn;
			Conn.FromPinId = OutputPin->PinId;
			Conn.ToNodeId = Linked->GetOwningNode()->NodeGuid;
			Conn.ToPinId = Linked->PinId;
			SavedConnections.Add(Conn);
		}
	}

	OutputPin->BreakAllPinLinks(true);

	SyncPin();
	GetGraph()->NotifyNodeChanged(this);
}

TSharedRef<SWidget> SNarrativePlayerOptionsNode::CreateNarrativeNodeCenterContent()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Player Options")))
			.Font(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 16, "Bold"))
		];
}
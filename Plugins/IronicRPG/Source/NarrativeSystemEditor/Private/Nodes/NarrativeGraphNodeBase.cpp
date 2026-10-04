// Copyright Ironic Studio. All Rights Reserved.


#include "Nodes/NarrativeGraphNodeBase.h"

FName UNarrativeGraphNodeBase::PinNane(TEXT("NarrativePin"));

UEdGraphPin* UNarrativeGraphNodeBase::CreateRPGGraphPin(EEdGraphPinDirection Direction, FName InPinName)
{
	FName Category = (Direction == EGPD_Input) ? TEXT("Input") : TEXT("Output");
	FName SubCategory = PinNane;

	UEdGraphPin* NewPin = CreatePin(Direction, Category, InPinName);
	NewPin->PinType.PinSubCategory = SubCategory;

	return NewPin;
}

void UNarrativeGraphNodeBase::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	FToolMenuSection& Section = Menu->AddSection(TEXT("SectionName"), FText::FromString(TEXT("Narratvie Node Actions")));

	UNarrativeGraphNodeBase* NodePtr = (UNarrativeGraphNodeBase*)this;

}

void UNarrativeGraphNodeBase::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Modify();
	GetGraph()->Modify();

	TArray<FPinConnectionData> NewConnections;
	
	for (UEdGraphPin* Linked : Pin->LinkedTo)
	{
		if (Linked)
		{
			FPinConnectionData Conn;
			Conn.FromPinId = Pin->PinId;
			Conn.ToNodeId = Linked->GetOwningNode()->NodeGuid;
			Conn.ToPinId = Linked->PinId;

			NewConnections.Add(Conn);
		}
	}

	// Save the new connections
	SavedConnections = NewConnections;
}

FName UNarrativeGraphNodeBase::CreateCallableBindingName() const
{
	FName CallableBindingName(*FString::Printf(TEXT("On%sEventTriggers_%s"), *GetClass()->GetName(), *NodeGuid.ToString()));

	GetNarrativeNodeInfo()->Modify();
	GetNarrativeNodeInfo()->CallableBindingName = CallableBindingName;

	return CallableBindingName;
}

void UNarrativeGraphNodeBase::DestroyNode()
{
	const FScopedTransaction Transaction(NSLOCTEXT("Narrative", "DeleteNode", "Delete Node"));
	Modify();
	GetGraph()->Modify();

	for (UEdGraphPin * Pin : Pins)
	{
		for (UEdGraphPin* Linked : Pin->LinkedTo)
		{
			if (Linked)
			{
				FPinConnectionData Conn;
				Conn.FromPinId = Pin->PinId;
				Conn.ToNodeId = Linked->GetOwningNode()->NodeGuid;
				Conn.ToPinId = Linked->PinId;
				SavedConnections.Add(Conn);
			}
		}
	}

	Super::DestroyNode();
}


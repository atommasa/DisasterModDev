// Fill out your copyright notice in the Description page of Project Settings.


#include "Nodes/NarrativeGraphNodeBase.h"

void UNarrativeGraphNodeBase::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	FToolMenuSection& Section = Menu->AddSection(TEXT("SectionName"), FText::FromString(TEXT("Narratvie Node Actions")));

	UNarrativeGraphNodeBase* NodePtr = (UNarrativeGraphNodeBase*)this;

}

void UNarrativeGraphNodeBase::SetNodeInfo(UNarrativeNodeInfo* InNodeInfo)
{
	InNodeInfo->SetFlags(RF_Transactional);
	InNodeInfo->Rename(nullptr, GetGraph(), REN_NonTransactional);
	_NodeInfo = InNodeInfo;
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

	UE_LOG(LogTemp, Display, TEXT("%s: %d"), *Pin->GetName(), Pin->LinkedTo.Num());
}

void UNarrativeGraphNodeBase::SyncPinWithResponse()
{
	TMap<FGuid, UEdGraphPin*> PinMap;
	TMap<FGuid, UEdGraphNode*> NodeMap;

	for (UEdGraphNode* Node : GetGraph()->Nodes)
	{
		NodeMap.Add(Node->NodeGuid, Node);

		for (UEdGraphPin* Pin : Node->Pins)
		{
			PinMap.Add(Pin->PinId, Pin);
		}
	}

	for (const FPinConnectionData& Conn : SavedConnections)
	{
		if (UEdGraphPin* From = PinMap.FindRef(Conn.FromPinId))
		{
			if (UEdGraphNode* ToNode = NodeMap.FindRef(Conn.ToNodeId))
			{
				if (UEdGraphPin* To = ToNode->FindPinById(Conn.ToPinId))
				{
					if (!From->LinkedTo.Contains(To))
					{
						if (From && To && !From->LinkedTo.Contains(To) && !To->LinkedTo.Contains(From))
						{
							From->Modify();
							To->Modify();
							From->MakeLinkTo(To);
						}
					}
				}
			}
		}
	}

	SavedConnections.Empty();
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

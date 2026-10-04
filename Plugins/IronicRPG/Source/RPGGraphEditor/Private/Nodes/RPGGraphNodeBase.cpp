// Copyright Ironic Studio. All Rights Reserved.

#include "Nodes/RPGGraphNodeBase.h"
#include "ScopedTransaction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "RPGGraphEditorUtils.h"

FName URPGGraphNodeBase::PinName(TEXT("RPGGraphPin"));

UEdGraphPin* URPGGraphNodeBase::CreateRPGGraphPin(EEdGraphPinDirection Direction, FName InPinName)
{
    const FName Category = (Direction == EGPD_Input) ? TEXT("Input") : TEXT("Output");

    UEdGraphPin* NewPin = CreatePin(Direction, Category, InPinName);
    if (NewPin)
    {
        NewPin->PinType.PinSubCategory = PinName;
    }

    return NewPin;
}

void URPGGraphNodeBase::SetNodeInfoObject(UObject* InNodeInfo)
{
    if (!InNodeInfo)
    {
        NodeInfoObject = nullptr;
        return;
    }

    InNodeInfo->SetFlags(RF_Transactional);
    InNodeInfo->Rename(nullptr, GetGraph(), REN_NonTransactional);
    NodeInfoObject = InNodeInfo;
}

void URPGGraphNodeBase::PinConnectionListChanged(UEdGraphPin* Pin)
{
    Modify();
    if (UEdGraph* Graph = GetGraph())
    {
        Graph->Modify();
    }

    TArray<FPinConnectionData> NewConnections;

    if (Pin)
    {
        for (UEdGraphPin* Linked : Pin->LinkedTo)
        {
            if (!Linked || !Linked->GetOwningNode())
            {
                continue;
            }

            FPinConnectionData Connection;
            Connection.FromPinId = Pin->PinId;
            Connection.ToNodeId = Linked->GetOwningNode()->NodeGuid;
            Connection.ToPinId = Linked->PinId;

            NewConnections.Add(Connection);
        }
    }

    SavedConnections = NewConnections;
}

void URPGGraphNodeBase::DestroyNode()
{
    const FScopedTransaction Transaction(NSLOCTEXT("RPGGraphEditor", "DeleteNode", "Delete Node"));

    Modify();
    if (UEdGraph* Graph = GetGraph())
    {
        Graph->Modify();
    }

    SavedConnections.Reset();

    for (UEdGraphPin* Pin : Pins)
    {
        if (!Pin)
        {
            continue;
        }

        for (UEdGraphPin* Linked : Pin->LinkedTo)
        {
            if (!Linked || !Linked->GetOwningNode())
            {
                continue;
            }

            FPinConnectionData Connection;
            Connection.FromPinId = Pin->PinId;
            Connection.ToNodeId = Linked->GetOwningNode()->NodeGuid;
            Connection.ToPinId = Linked->PinId;
            SavedConnections.Add(Connection);
        }
    }

    Super::DestroyNode();
}

void URPGGraphNodeBase::SyncPin()
{
    UEdGraph* Graph = GetGraph();
    if (!Graph)
    {
        return;
    }

    TMap<FGuid, UEdGraphPin*> PinMap;
    TMap<FGuid, UEdGraphNode*> NodeMap;

    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node)
        {
            continue;
        }

        NodeMap.Add(Node->NodeGuid, Node);

        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin)
            {
                PinMap.Add(Pin->PinId, Pin);
            }
        }
    }

    for (const FPinConnectionData& Connection : SavedConnections)
    {
        UEdGraphPin* FromPin = PinMap.FindRef(Connection.FromPinId);
        UEdGraphNode* ToNode = NodeMap.FindRef(Connection.ToNodeId);

        if (!FromPin || !ToNode)
        {
            continue;
        }

        UEdGraphPin* ToPin = ToNode->FindPinById(Connection.ToPinId);
        if (!ToPin)
        {
            continue;
        }

        if (!FromPin->LinkedTo.Contains(ToPin) && !ToPin->LinkedTo.Contains(FromPin))
        {
            FromPin->Modify();
            ToPin->Modify();
            FromPin->MakeLinkTo(ToPin);
        }
    }

    SavedConnections.Empty();
}

UK2Node_CustomEvent* URPGGraphNodeBase::CreateOrFocusCustomEvent(FName EventName)
{
    const FText& EventComment = CreateCallableBindingComment();

    UBlueprint* Blueprint = GetBlueprintAsset();
    UEdGraph* EventGraph = FRPGGraphEditorUtils::GetOrCreateUbergraphPage(Blueprint, GetCustomEventGraphName());

    if (!Blueprint || !EventGraph)
    {
        return nullptr;
    }

    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node);
        if (CustomEvent && CustomEvent->CustomFunctionName == EventName)
        {
            CustomEvent->Modify();

            CreateCallableBindingParameterPins(CustomEvent);
            CustomEvent->ReconstructNode();

            EventGraph->NotifyNodeChanged(CustomEvent);

            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            Blueprint->MarkPackageDirty();

            FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(CustomEvent);
            return CustomEvent;
        }
    }

    const FScopedTransaction Transaction(NSLOCTEXT("Narrative", "CreateCustomEvent", "Create Custom Event"));

    Blueprint->Modify();
    EventGraph->Modify();

    FGraphNodeCreator<UK2Node_CustomEvent> NodeCreator(*EventGraph);
    UK2Node_CustomEvent* CustomEventNode = NodeCreator.CreateNode();

    CustomEventNode->CustomFunctionName = EventName;
    CustomEventNode->NodeComment = EventComment.ToString();
    CustomEventNode->bCommentBubbleVisible = true;
    CustomEventNode->bIsEditable = false;

    FVector2D NewNodePosition = FRPGGraphEditorUtils::FindLocationForNewNode(EventGraph);
    CustomEventNode->NodePosX = NewNodePosition.X;
    CustomEventNode->NodePosY = NewNodePosition.Y;

    NodeCreator.Finalize();

    CustomEventNode->Modify();

    CreateCallableBindingParameterPins(CustomEventNode);

    CustomEventNode->ReconstructNode();

    EventGraph->NotifyNodeChanged(CustomEventNode);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();

    FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(CustomEventNode);

    return CustomEventNode;
}

UK2Node_FunctionEntry* URPGGraphNodeBase::CreateOrFocusCustomFunction(FName FunctionName)
{
    const FText& FunctionComment = CreateCallableBindingComment();

    UBlueprint* Blueprint = GetBlueprintAsset();
    UEdGraph* FunctionGraph = FRPGGraphEditorUtils::GetOrCreateFunctionGraph(Blueprint, FunctionName, GetFunctionAsSignature());

    if (!FunctionGraph)
    {
        return nullptr;
    }

    UK2Node_FunctionEntry* Entry = nullptr;

    for (UEdGraphNode* Node : FunctionGraph->Nodes)
    {
        if (!Entry)
        {
            Entry = Cast<UK2Node_FunctionEntry>(Node);
            break;
        }
    }

    Entry->Modify();

    Entry->NodeComment = FunctionComment.ToString();
    Entry->bCommentBubbleVisible = true;
    Entry->bIsEditable = false;
    Entry->bCanRenameNode = false;

    CreateCallableBindingParameterPins(Entry);
    Entry->ReconstructNode();

    FunctionGraph->NotifyNodeChanged(Entry);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Blueprint->MarkPackageDirty();

    FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Entry);

    return Entry;
}

FName URPGGraphNodeBase::CreateCallableBindingName() const
{
    return NAME_None;
}

FText URPGGraphNodeBase::CreateCallableBindingComment() const
{
    return FText();
}
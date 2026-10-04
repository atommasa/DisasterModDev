// Copyright Ironic Studio. All Rights Reserved.

#include "RPGGraphEditorUtils.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_EditablePinBase.h"
#include "K2Node_Knot.h"
#include "ScopedTransaction.h"
#include "Nodes/RPGGraphNodeKnot.h"

UEdGraph* FRPGGraphEditorUtils::GetGraphByName(UBlueprint* InBlueprint, FName GraphName)
{
    if (!InBlueprint)
    {
        return nullptr;
    }

    TArray<UEdGraph*> AllGraphs;
    InBlueprint->GetAllGraphs(AllGraphs);

    for (UEdGraph* Graph : AllGraphs)
    {
        if (!Graph)
        {
            continue;
        }

        if (Graph->GetFName() == GraphName)
        {
            return Graph;
        }
    }

    return nullptr;
}

UEdGraph* FRPGGraphEditorUtils::GetOrCreateUbergraphPage(UBlueprint* InBlueprint, FName GraphName)
{
    if (!InBlueprint || GraphName.IsNone())
    {
        return nullptr;
    }

    for (UEdGraph* Graph : InBlueprint->UbergraphPages)
    {
        if (Graph && Graph->GetFName() == GraphName)
        {
            return Graph;
        }
    }

    InBlueprint->Modify();

    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
        InBlueprint,
        GraphName,
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass()
    );

    if (!NewGraph)
    {
        return nullptr;
    }

    NewGraph->Modify();
    NewGraph->SetFlags(RF_Transactional);
    NewGraph->ClearFlags(RF_Transient);

    FBlueprintEditorUtils::AddUbergraphPage(InBlueprint, NewGraph);
    InBlueprint->LastEditedDocuments.AddUnique(NewGraph);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(InBlueprint);
    InBlueprint->BroadcastChanged();
    InBlueprint->MarkPackageDirty();

    return NewGraph;
}

UEdGraph* FRPGGraphEditorUtils::GetOrCreateFunctionGraph(UBlueprint* InBlueprint, FName FunctionName, UFunction* SignatureFunction)
{
    if (!InBlueprint || FunctionName.IsNone())
    {
        return nullptr;
    }

    if (UEdGraph* ExistingGraph = FindFunctionGraphByName(InBlueprint, FunctionName))
    {
        return ExistingGraph;
    }

    const FScopedTransaction Transaction(
        NSLOCTEXT("RPGGraphEditor", "CreateFunctionGraph", "Create Function Graph")
    );

    InBlueprint->Modify();

    UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(
        InBlueprint,
        FunctionName,
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass()
    );

    if (!FunctionGraph)
    {
        return nullptr;
    }

    FunctionGraph->Modify();
    FunctionGraph->SetFlags(RF_Transactional);
    FunctionGraph->ClearFlags(RF_Transient);

    if (SignatureFunction)
    {
        FBlueprintEditorUtils::AddFunctionGraph(
            InBlueprint,
            FunctionGraph,
            true,
            SignatureFunction
        );
    }
    else
    {
        FBlueprintEditorUtils::AddFunctionGraph<UFunction>(
            InBlueprint,
            FunctionGraph,
            true,
            nullptr
        );
    }

    InBlueprint->UbergraphPages.Remove(FunctionGraph);
    FunctionGraph->NotifyGraphChanged();

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(InBlueprint);
    InBlueprint->BroadcastChanged();
    InBlueprint->MarkPackageDirty();

    return FunctionGraph;
}

UEdGraph* FRPGGraphEditorUtils::FindFunctionGraphByName(UBlueprint* InBlueprint, FName FunctionName)
{
    if (!InBlueprint || FunctionName.IsNone())
    {
        return nullptr;
    }

    for (UEdGraph* Graph : InBlueprint->FunctionGraphs)
    {
        if (Graph && Graph->GetFName() == FunctionName)
        {
            return Graph;
        }
    }

    return nullptr;
}

FVector2D FRPGGraphEditorUtils::FindLocationForNewNode(const UEdGraph* Graph, const FVector2D& DefaultLocation)
{
    if (!Graph || Graph->Nodes.Num() == 0)
    {
        return DefaultLocation;
    }

    int32 MaxY = TNumericLimits<int32>::Lowest();
    int32 MinX = TNumericLimits<int32>::Max();
    bool bFoundNode = false;

    for (const UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node)
        {
            continue;
        }

        bFoundNode = true;
        MinX = FMath::Min(MinX, Node->NodePosX);
        MaxY = FMath::Max(MaxY, Node->NodePosY);
    }

    if (!bFoundNode)
    {
        return DefaultLocation;
    }

    return FVector2D(MinX, MaxY + 250.0f);
}

UEdGraphPin* FRPGGraphEditorUtils::CreateUserDefinedPinFor(UK2Node_EditablePinBase* Node, FName PinName, const FEdGraphPinType& PinType, EEdGraphPinDirection Direction)
{
    if (!Node || !CanCreateUserDefinedPinFor(Node, PinName))
    {
        return nullptr;
    }

    return Node->CreateUserDefinedPin(PinName, PinType, Direction);
}

bool FRPGGraphEditorUtils::CanCreateUserDefinedPinFor(UK2Node_EditablePinBase* Node, FName PinName)
{
    if (!Node || PinName.IsNone())
    {
        return false;
    }

    return !Node->UserDefinedPins.ContainsByPredicate([&PinName](const TSharedPtr<FUserPinInfo>& UserPin)
    {
        return UserPin.IsValid() && UserPin->PinName == PinName;
    });
}

void FRPGGraphEditorUtils::GetRealLinkedNodes(UEdGraphPin* SourcePin, TArray<UEdGraphNode*>& OutRealLinkedNodes)
{
    if (!SourcePin)
    {
        return;
    }

    for (UEdGraphPin* LinkedPin : SourcePin->LinkedTo)
    {
        if (!LinkedPin)
        {
            continue;
        }

        UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
        if (!LinkedNode)
        {
            continue;
        }

        if (LinkedNode->IsA<UK2Node_Knot>())
        {
            for (UEdGraphPin* KnotPin : LinkedNode->Pins)
            {
                if (KnotPin != LinkedPin)
                {
                    GetRealLinkedNodes(KnotPin, OutRealLinkedNodes);
                }
            }
        }
        else
        {
            OutRealLinkedNodes.AddUnique(LinkedNode);
        }
    }
}

void FRPGGraphEditorUtils::ResolveLinkedNodesIgnoringKnot(UEdGraphPin* LinkedPin, TArray<UEdGraphNode*>& OutNodes)
{
    TSet<UEdGraphNode*> VisitedKnotNodes;

    ResolveLinkedNodesIgnoringKnot(
        LinkedPin,
        OutNodes,
        VisitedKnotNodes
    );
}

void FRPGGraphEditorUtils::ResolveLinkedNodesIgnoringKnot(UEdGraphPin* LinkedPin, TArray<UEdGraphNode*>& OutNodes, TSet<UEdGraphNode*>& VisitedKnotNodes)
{
    if (!LinkedPin)
    {
        return;
    }

    UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
    if (!LinkedNode)
    {
        return;
    }

    URPGGraphNodeKnot* KnotNode = Cast<URPGGraphNodeKnot>(LinkedNode);
    if (!KnotNode)
    {
        OutNodes.AddUnique(LinkedNode);
        return;
    }

    if (VisitedKnotNodes.Contains(KnotNode))
    {
        return;
    }

    VisitedKnotNodes.Add(KnotNode);

    UEdGraphPin* NextPin = KnotNode->GetOutputPin();

    if (!NextPin)
    {
        return;
    }

    for (UEdGraphPin* NextLinkedPin : NextPin->LinkedTo)
    {
        ResolveLinkedNodesIgnoringKnot(
            NextLinkedPin,
            OutNodes,
            VisitedKnotNodes
        );
    }
}

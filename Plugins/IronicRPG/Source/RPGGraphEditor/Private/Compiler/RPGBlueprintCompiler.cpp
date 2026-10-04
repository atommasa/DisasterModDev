// Copyright Ironic Studio. All Rights Reserved.


#include "Compiler/RPGBlueprintCompiler.h"
#include "RPGGraphEditorUtils.h"

bool FRPGGraphTerminationValidator::Validate(UEdGraph* InGraph, UEdGraphNode* StartNode)
{
    NextNodeMap.Reset();
    ReachableNodes.Reset();

    IndexCounter = 0;
    NodeIndexMap.Reset();
    LowLinkMap.Reset();
    Stack.Reset();
    NodesOnStack.Reset();

    bValid = true;

    if (!InGraph || !StartNode)
    {
        MessageLog.Error(TEXT("Dialogue graph validation failed: invalid graph or start node."));
        return false;
    }

    BuildReachableGraph(StartNode);

    if (ReachableNodes.IsEmpty())
    {
        MessageLog.Error(TEXT("Dialogue graph validation failed: no reachable nodes."));
        return false;
    }

    for (UEdGraphNode* Node : ReachableNodes)
    {
        if (!NodeIndexMap.Contains(Node))
        {
            StrongConnect(Node);
        }

        if (OnNodeVisited)
        {
            OnNodeVisited(Node);
        }
    }

    return bValid;
}

void FRPGGraphTerminationValidator::BuildReachableGraph(UEdGraphNode* StartNode)
{
    TArray<UEdGraphNode*> PendingNodes;
    PendingNodes.Add(StartNode);

    while (!PendingNodes.IsEmpty())
    {
        UEdGraphNode* CurrentNode = PendingNodes.Pop();

        if (!CurrentNode || ReachableNodes.Contains(CurrentNode))
        {
            continue;
        }

        ReachableNodes.Add(CurrentNode);

        TArray<UEdGraphNode*> NextNodes;
        GetNextNodes(CurrentNode, NextNodes);

        NextNodeMap.Add(CurrentNode, NextNodes);

        for (UEdGraphNode* NextNode : NextNodes)
        {
            if (NextNode && !ReachableNodes.Contains(NextNode))
            {
                PendingNodes.Add(NextNode);
            }
        }
    }
}

void FRPGGraphTerminationValidator::GetNextNodes(UEdGraphNode* Node, TArray<UEdGraphNode*>& OutNextNodes) const
{
    OutNextNodes.Reset();

    if (!Node)
    {
        return;
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (!Pin || Pin->Direction != EGPD_Output)
        {
            continue;
        }

        for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
        {
            FRPGGraphEditorUtils::ResolveLinkedNodesIgnoringKnot(LinkedPin, OutNextNodes);
        }
    }
}

void FRPGGraphTerminationValidator::StrongConnect(UEdGraphNode* Node)
{
    if (!Node)
    {
        return;
    }

    NodeIndexMap.Add(Node, IndexCounter);
    LowLinkMap.Add(Node, IndexCounter);
    ++IndexCounter;

    Stack.Add(Node);
    NodesOnStack.Add(Node);

    const TArray<UEdGraphNode*>* NextNodes = NextNodeMap.Find(Node);
    if (NextNodes)
    {
        for (UEdGraphNode* NextNode : *NextNodes)
        {
            if (!NextNode || !ReachableNodes.Contains(NextNode))
            {
                continue;
            }

            if (!NodeIndexMap.Contains(NextNode))
            {
                StrongConnect(NextNode);

                const int32 NodeLowLink = LowLinkMap.FindChecked(Node);
                const int32 NextLowLink = LowLinkMap.FindChecked(NextNode);

                LowLinkMap.Add(Node, FMath::Min(NodeLowLink, NextLowLink));
            }
            else if (NodesOnStack.Contains(NextNode))
            {
                const int32 NodeLowLink = LowLinkMap.FindChecked(Node);
                const int32 NextIndex = NodeIndexMap.FindChecked(NextNode);

                LowLinkMap.Add(Node, FMath::Min(NodeLowLink, NextIndex));
            }
        }
    }

    if (LowLinkMap.FindChecked(Node) != NodeIndexMap.FindChecked(Node))
    {
        return;
    }

    TSet<UEdGraphNode*> Component;

    while (!Stack.IsEmpty())
    {
        UEdGraphNode* StackNode = Stack.Pop();
        NodesOnStack.Remove(StackNode);

        if (StackNode)
        {
            Component.Add(StackNode);
        }

        if (StackNode == Node)
        {
            break;
        }
    }

    if (IsCycleComponent(Component))
    {
        if (!HandleCycleComponent(Component))
        {
            bValid = false;
        }
    }
}

bool FRPGGraphTerminationValidator::IsCycleComponent(const TSet<UEdGraphNode*>& Component) const
{
    if (Component.Num() > 1)
    {
        return true;
    }

    if (Component.Num() != 1)
    {
        return false;
    }

    UEdGraphNode* OnlyNode = nullptr;

    for (UEdGraphNode* Node : Component)
    {
        OnlyNode = Node;
        break;
    }

    if (!OnlyNode)
    {
        return false;
    }

    const TArray<UEdGraphNode*>* NextNodes = NextNodeMap.Find(OnlyNode);
    if (!NextNodes)
    {
        return false;
    }

    return NextNodes->Contains(OnlyNode);
}

bool FRPGGraphTerminationValidator::HandleCycleComponent(const TSet<UEdGraphNode*>& Component)
{
    bool bResult = false;

    if (CyclePolicy)
    {
        bResult = CyclePolicy(Component);
    }

    FString ComponentText;

    for (UEdGraphNode* Node : Component)
    {
        if (!Node)
        {
            continue;
        }

        if (!ComponentText.IsEmpty())
        {
            ComponentText += TEXT(", ");
        }

        ComponentText += Node->GetName();
    }

    if (!bResult)
    {
        MessageLog.Error(
            *FString::Printf(
                TEXT("Dialogue graph contains a possible infinite loop: %s"),
                *ComponentText
            )
        );

        for (UEdGraphNode* Node : Component)
        {
            if (Node)
            {
                MessageLog.Error(TEXT("Loop node: @@"), Node);
            }
        }
    }

    return bResult;
}

void FRPGBlueprintCompilerContext::SpawnNewClass(const FString& NewClassName)
{
    if (Blueprint)
    {
        NewClass = NewObject<UBlueprintGeneratedClass>(
            Blueprint->GetOutermost(),
            GeneratedClassClass,
            *NewClassName,
            RF_Public | RF_Transactional
        );

        NewClass->ClassGeneratedBy = Blueprint;
    }
    else
    {
        FKismetCompilerContext::SpawnNewClass(NewClassName);
    }
}

bool FRPGBlueprintCompilerContext::IsNodePure(const UEdGraphNode* Node) const
{
    if (const UK2Node* K2Node = Cast<const UK2Node>(Node))
    {
        return K2Node->IsNodePure();
    }

    return true;
}

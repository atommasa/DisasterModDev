// Copyright Ironic Studio. All Rights Reserved.

#include "RPGGraphSchema.h"

#include "EdGraphSchema_K2.h"
#include "ToolMenus.h"
#include "GraphEditorActions.h"
#include "Framework/Commands/GenericCommands.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Nodes/RPGGraphNodeKnot.h"

FLinearColor URPGGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
    return GetDefault<UEdGraphSchema_K2>()->GetPinTypeColor(PinType);
}

void URPGGraphSchema::GetContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
    Super::GetContextMenuActions(Menu, Context);

    if (Context && Context->Node)
    {
        FToolMenuSection& Section = Menu->AddSection("RPGGraphSchemaNodeActions", NSLOCTEXT("RPGGraphSchema", "NodeActionsMenuHeader", "Node Actions"));
        Section.AddMenuEntry(FGenericCommands::Get().Delete);
        Section.AddMenuEntry(FGenericCommands::Get().Cut);
        Section.AddMenuEntry(FGenericCommands::Get().Copy);
        Section.AddMenuEntry(FGenericCommands::Get().Duplicate);
        Section.AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);
    }
}

const FPinConnectionResponse URPGGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
    if (!A || !B)
    {
        return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Invalid pin."));
    }

    if (IsKnotToKnot(A, B))
    {
        return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, TEXT("OK"));
    }

    const UEdGraphPin* ResolvedA = ResolveKnotPinForOtherPinConst(A, B);
    const UEdGraphPin* ResolvedB = ResolveKnotPinForOtherPinConst(B, ResolvedA);

    if (!ResolvedA || !ResolvedB)
    {
        return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Invalid resolved pin."));
    }

    if (ResolvedA->GetOwningNode() == ResolvedB->GetOwningNode())
    {
        return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Same owning node not allowed."));
    }

    if (ResolvedA->Direction == ResolvedB->Direction)
    {
        return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Same direction not allowed."));
    }

    if (ResolvedA->Direction == EGPD_Output)
    {
        return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, TEXT("OK"));
    }

    if (ResolvedB->Direction == EGPD_Output)
    {
        return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_B, TEXT("OK"));
    }

    return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT("OK"));
}

bool URPGGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
    if (!A || !B)
    {
        return false;
    }

    bool bResult = false;

    if (IsKnotToKnot(A, B))
    {
        URPGGraphNodeKnot* KnotNodeA = Cast<URPGGraphNodeKnot>(A->GetOwningNode());
        URPGGraphNodeKnot* KnotNodeB = Cast<URPGGraphNodeKnot>(B->GetOwningNode());
        if (!KnotNodeA || !KnotNodeB)
        {
            return false;
        }

        bResult = Super::TryCreateConnection(KnotNodeA->GetOutputPin(), KnotNodeB->GetInputPin());
    }
    else
    {
        UEdGraphPin* ResolvedA = ResolveKnotPinForOtherPin(A, B);
        UEdGraphPin* ResolvedB = ResolveKnotPinForOtherPin(B, ResolvedA);

        if (!ResolvedA || !ResolvedB || ResolvedA == ResolvedB)
        {
            return false;
        }

        bResult = Super::TryCreateConnection(ResolvedA, ResolvedB);
    }

    if (bResult)
    {
        if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(A->GetOwningNode()))
        {
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            Blueprint->MarkPackageDirty();
        }
    }

    return bResult;
}

void URPGGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
    const FScopedTransaction Transaction(NSLOCTEXT("RPGGraphEditor", "BreakNodeLinks", "Break Node Links"));

    if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(&TargetNode))
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        Blueprint->MarkPackageDirty();
    }

    Super::BreakNodeLinks(TargetNode);
}

void URPGGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const
{
    const FScopedTransaction Transaction(NSLOCTEXT("RPGGraphEditor", "BreakPinLinks", "Break Pin Links"));

    if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(TargetPin.GetOwningNode()))
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        Blueprint->MarkPackageDirty();
    }

    Super::BreakPinLinks(TargetPin, bSendsNodeNotification);
}

void URPGGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
    const FScopedTransaction Transaction(NSLOCTEXT("RPGGraphEditor", "BreakPinLink", "Break Pin Link"));

    if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(SourcePin->GetOwningNode()))
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        Blueprint->MarkPackageDirty();
    }

    Super::BreakSinglePinLink(SourcePin, TargetPin);
}

void URPGGraphSchema::OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const
{
    if (!PinA || !PinB)
    {
        return;
    }

    UEdGraphNode* NodeA = PinA->GetOwningNode();
    UEdGraphNode* NodeB = PinB->GetOwningNode();
    if (!NodeA || !NodeB)
    {
        return;
    }

    UEdGraph* Graph = NodeA->GetGraph();
    if (!Graph || Graph != NodeB->GetGraph())
    {
        return;
    }

    UEdGraphPin* OutputPin = nullptr;
    UEdGraphPin* InputPin = nullptr;

    if (PinA->Direction == EGPD_Output && PinB->Direction == EGPD_Input)
    {
        OutputPin = PinA;
        InputPin = PinB;
    }
    else if (PinB->Direction == EGPD_Output && PinA->Direction == EGPD_Input)
    {
        OutputPin = PinB;
        InputPin = PinA;
    }
    else
    {
        return;
    }

    const FScopedTransaction Transaction(NSLOCTEXT("RPGGraphEditor", "CreateRerouteNode", "Create Reroute Node"));

    Graph->Modify();
    OutputPin->Modify();
    InputPin->Modify();

    URPGGraphNodeKnot* RerouteNode = NewObject<URPGGraphNodeKnot>(
        Graph,
        GetKnotNodeClass(),
        NAME_None,
        RF_Transactional
    );

    if (!RerouteNode)
    {
        return;
    }

    RerouteNode->CreateNewGuid();
    RerouteNode->NodePosX = GraphPosition.X;
    RerouteNode->NodePosY = GraphPosition.Y;
    RerouteNode->AllocateDefaultPins();
    Graph->AddNode(RerouteNode, true, true);

    UEdGraphPin* RerouteInputPin = RerouteNode->GetInputPin();
    UEdGraphPin* RerouteOutputPin = RerouteNode->GetOutputPin();
    if (!RerouteInputPin || !RerouteOutputPin)
    {
        return;
    }

    RerouteNode->Modify();

    BreakSinglePinLink(OutputPin, InputPin);
    TryCreateConnection(OutputPin, RerouteInputPin);
    TryCreateConnection(RerouteOutputPin, InputPin);

    Graph->NotifyGraphChanged();

    if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph))
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        Blueprint->MarkPackageDirty();
    }
}

FConnectionDrawingPolicy* URPGGraphSchema::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj) const
{
    return new FRPGConnectionDrawingPolicy(
        InBackLayerID,
        InFrontLayerID,
        InZoomFactor,
        InClippingRect,
        InDrawElements,
        InGraphObj
    );
}

TSubclassOf<URPGGraphNodeKnot> URPGGraphSchema::GetKnotNodeClass() const
{
    return URPGGraphNodeKnot::StaticClass();
}

UEdGraphPin* URPGGraphSchema::ResolveKnotPinForOtherPin(UEdGraphPin* Pin, const UEdGraphPin* OtherPin) const
{
    return const_cast<UEdGraphPin*>(ResolveKnotPinForOtherPinConst(Pin, OtherPin));
}

const UEdGraphPin* URPGGraphSchema::ResolveKnotPinForOtherPinConst(const UEdGraphPin* Pin, const UEdGraphPin* OtherPin) const
{
    if (!Pin || !OtherPin)
    {
        return Pin;
    }

    const URPGGraphNodeKnot* KnotNode = Cast<URPGGraphNodeKnot>(Pin->GetOwningNode());
    if (!KnotNode)
    {
        return Pin;
    }

    if (OtherPin->Direction == EGPD_Input)
    {
        return KnotNode->GetOutputPin();
    }

    if (OtherPin->Direction == EGPD_Output)
    {
        return KnotNode->GetInputPin();
    }

    return Pin;
}

bool URPGGraphSchema::IsKnotToKnot(const UEdGraphPin* Pin, const UEdGraphPin* OtherPin) const
{
    return Pin && OtherPin &&
        Pin->GetOwningNode() && OtherPin->GetOwningNode() &&
        Pin->GetOwningNode()->IsA<URPGGraphNodeKnot>() &&
        OtherPin->GetOwningNode()->IsA<URPGGraphNodeKnot>();
}

UEdGraphNode* FRPGNewNodeAction::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
    if (!ParentGraph || !NodeClass)
    {
        return nullptr;
    }

    const FScopedTransaction Transaction(NSLOCTEXT("RPGGraphEditor", "AddNode", "Add Node"));

    UEdGraphPin* PreviousLinkedToPin = nullptr;
    if (FromPin && FromPin->LinkedTo.IsValidIndex(0))
    {
        PreviousLinkedToPin = FromPin->LinkedTo[0];
    }

    ParentGraph->Modify();

    UEdGraphNode* ResultNode = NewObject<UEdGraphNode>(ParentGraph, NodeClass, NAME_None, RF_Transactional);
    if (!ResultNode)
    {
        return nullptr;
    }

    ResultNode->Modify();
    ResultNode->CreateNewGuid();
    ResultNode->NodePosX = Location.X;
    ResultNode->NodePosY = Location.Y;
    ResultNode->AllocateDefaultPins();

    PostNodeCreated(ResultNode);

    ParentGraph->AddNode(ResultNode, true, bSelectNewNode);

    if (FromPin)
    {
        if (UEdGraphPin** InputPin = ResultNode->Pins.FindByPredicate([](const UEdGraphPin* Pin)
            {
                return Pin && Pin->Direction == EGPD_Input;
            }))
        {
            ResultNode->GetSchema()->TryCreateConnection(FromPin, *InputPin);
        }
    }

    if (PreviousLinkedToPin)
    {
        if (UEdGraphPin** OutputPin = ResultNode->Pins.FindByPredicate([](const UEdGraphPin* Pin)
            {
                return Pin && Pin->Direction == EGPD_Output;
            }))
        {
            ResultNode->GetSchema()->TryCreateConnection(*OutputPin, PreviousLinkedToPin);
        }
    }

    ParentGraph->NotifyGraphChanged();

    if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph))
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        Blueprint->MarkPackageDirty();
    }

    return ResultNode;
}

FRPGConnectionDrawingPolicy::FRPGConnectionDrawingPolicy(
    int32 InBackLayerID,
    int32 InFrontLayerID,
    float InZoomFactor,
    const FSlateRect& InClippingRect,
    FSlateWindowElementList& InDrawElements,
    UEdGraph* InGraphObj
)
    : FKismetConnectionDrawingPolicy(
        InBackLayerID,
        InFrontLayerID,
        InZoomFactor,
        InClippingRect,
        InDrawElements,
        InGraphObj
    )
{
}

bool FRPGConnectionDrawingPolicy::ShouldChangeTangentForRerouteControlPoint(const URPGGraphNodeKnot* Node)
{
    if (!Node)
    {
        return false;
    }

    if (bool* CachedResult = KnotToReversedDirectionMap.Find(Node))
    {
        return *CachedResult;
    }

    bool bPinReversed = false;
    int32 InputPinIndex = 0;
    int32 OutputPinIndex = 0;

    if (Node->ShouldDrawNodeAsControlPointOnly(InputPinIndex, OutputPinIndex))
    {
        const TArray<UEdGraphPin*>& Pins = Node->GetAllPins();

        if (!Pins.IsValidIndex(InputPinIndex) || !Pins.IsValidIndex(OutputPinIndex))
        {
            KnotToReversedDirectionMap.Add(Node, false);
            return false;
        }

        FVector2f AverageLeftPin = FVector2f::ZeroVector;
        FVector2f AverageRightPin = FVector2f::ZeroVector;
        FVector2f CenterPin = FVector2f::ZeroVector;

        const bool bCenterValid = FindPinCenter(Pins[OutputPinIndex], CenterPin);
        const bool bLeftValid = GetAverageConnectedPositionForPin(Pins[InputPinIndex], AverageLeftPin);
        const bool bRightValid = GetAverageConnectedPositionForPin(Pins[OutputPinIndex], AverageRightPin);

        if (bLeftValid && bRightValid)
        {
            bPinReversed = AverageRightPin.X < AverageLeftPin.X;
        }
        else if (bCenterValid)
        {
            if (bLeftValid)
            {
                bPinReversed = CenterPin.X < AverageLeftPin.X;
            }
            else if (bRightValid)
            {
                bPinReversed = AverageRightPin.X < CenterPin.X;
            }
        }
    }

    KnotToReversedDirectionMap.Add(Node, bPinReversed);
    return bPinReversed;
}

bool FRPGConnectionDrawingPolicy::GetAverageConnectedPositionForPin(UEdGraphPin* InPin, FVector2f& OutPos) const
{
    if (!InPin)
    {
        return false;
    }

    FVector2f Result = FVector2f::ZeroVector;
    int32 ResultCount = 0;

    for (UEdGraphPin* LinkedPin : InPin->LinkedTo)
    {
        if (!LinkedPin)
        {
            continue;
        }

        FVector2f CenterPoint;
        if (FindPinCenter(LinkedPin, CenterPoint))
        {
            Result += CenterPoint;
            ++ResultCount;
        }
    }

    if (ResultCount <= 0)
    {
        return false;
    }

    OutPos = Result / static_cast<float>(ResultCount);
    return true;
}

void FRPGConnectionDrawingPolicy::DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, FConnectionParams& Params)
{
    FKismetConnectionDrawingPolicy::DetermineWiringStyle(OutputPin, InputPin, Params);

    if (!OutputPin || !InputPin)
    {
        return;
    }

    if (OutputPin->Direction == EGPD_Input)
    {
        Swap(OutputPin, InputPin);
    }

    const URPGGraphNodeKnot* OutputNode = Cast<URPGGraphNodeKnot>(OutputPin->GetOwningNode());
    const URPGGraphNodeKnot* InputNode = Cast<URPGGraphNodeKnot>(InputPin->GetOwningNode());

    if (OutputNode && ShouldChangeTangentForRerouteControlPoint(OutputNode))
    {
        Params.StartDirection = EGPD_Input;
    }

    if (InputNode && ShouldChangeTangentForRerouteControlPoint(InputNode))
    {
        Params.EndDirection = EGPD_Output;
    }

    Params.WireColor = GetDefaultWireColor(OutputPin, InputPin);
    Params.WireThickness = GetDefaultWireThickness(OutputPin, InputPin);
}

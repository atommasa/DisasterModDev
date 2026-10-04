// Copyright Ironic Studio. All Rights Reserved.

#include "Nodes/RPGGraphNodeKnot.h"
#include "Nodes/RPGGraphNodeBase.h"
#include "Styling/AppStyle.h"

const FName URPGGraphNodeKnot::InputPinName(TEXT("InputPin"));
const FName URPGGraphNodeKnot::OutputPinName(TEXT("OutputPin"));

void URPGGraphNodeKnot::AllocateDefaultPins()
{
    UEdGraphPin* InputPin = CreatePin(EGPD_Input, TEXT("Input"), InputPinName);
    UEdGraphPin* OutputPin = CreatePin(EGPD_Output, TEXT("Output"), OutputPinName);

    if (InputPin)
    {
        InputPin->PinType.PinSubCategory = URPGGraphNodeBase::PinName;
    }

    if (OutputPin)
    {
        OutputPin->PinType.PinSubCategory = URPGGraphNodeBase::PinName;
    }
}

TSharedPtr<SGraphNode> URPGGraphNodeKnot::CreateVisualWidget()
{
    return SNew(SRPGNodeKnot, this);
}

bool URPGGraphNodeKnot::HasAnyConnections() const
{
    const UEdGraphPin* InputPin = GetInputPin();
    const UEdGraphPin* OutputPin = GetOutputPin();

    return (InputPin && InputPin->LinkedTo.Num() > 0) || (OutputPin && OutputPin->LinkedTo.Num() > 0);
}

void SRPGNodeKnot::UpdateGraphNode()
{
    InputPins.Empty();
    OutputPins.Empty();

    InputPinWidget.Reset();
    OutputPinWidget.Reset();
    PinOverlay.Reset();

    ContentScale.Bind(this, &SGraphNode::GetContentScale);

    GetOrAddSlot(ENodeZone::Center)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(SBox)
                .WidthOverride(25.0f)
                .HeightOverride(25.0f)
                [
                    SAssignNew(PinOverlay, SOverlay)
                ]
        ];

    CreatePinWidgets();
    RefreshPinOverlay();
}

void SRPGNodeKnot::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
    PinToAdd->SetOwner(SharedThis(this));
    PinToAdd->SetShowLabel(false);

    if (PinToAdd->GetDirection() == EGPD_Input)
    {
        InputPins.Add(PinToAdd);
        InputPinWidget = PinToAdd;
    }
    else
    {
        OutputPins.Add(PinToAdd);
        OutputPinWidget = PinToAdd;
    }
}

void SRPGNodeKnot::RefreshPinOverlay()
{
    if (!PinOverlay.IsValid() || bOverlayReorderedForDrag)
    {
        return;
    }

    bOverlayReorderedForDrag = true;
    PinOverlay->ClearChildren();

    const URPGGraphNodeKnot* KnotNode = Cast<URPGGraphNodeKnot>(GraphNode);
    if (!KnotNode)
    {
        return;
    }

    const bool bDraggingFromOutput = CurrentDragDirection == EGPD_Output;
    const bool bDraggingFromInput = CurrentDragDirection == EGPD_Input;

    const UEdGraphPin* KnotInputPin = KnotNode->GetInputPin();
    const bool bInputConnected = KnotInputPin && KnotInputPin->LinkedTo.Num() > 0;

    if (bDraggingFromOutput)
    {
        AddPinToOverlay(OutputPinWidget);
        AddPinToOverlay(InputPinWidget);
    }
    else if (bDraggingFromInput)
    {
        AddPinToOverlay(InputPinWidget);
        AddPinToOverlay(OutputPinWidget);
    }
    else if (bInputConnected)
    {
        AddPinToOverlay(InputPinWidget);
        AddPinToOverlay(OutputPinWidget);
    }
    else
    {
        AddPinToOverlay(OutputPinWidget);
        AddPinToOverlay(InputPinWidget);
    }
}

void SRPGNodeKnot::AddPinToOverlay(TSharedPtr<SGraphPin> PinWidget)
{
    if (!PinWidget.IsValid())
    {
        return;
    }

    PinOverlay->AddSlot()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(SBox)
                .WidthOverride(20.0f)
                .HeightOverride(20.0f)
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                [
                    PinWidget.ToSharedRef()
                ]
        ];
}

void SRPGNodeKnot::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
    CurrentDragDirection = FRPGGraphDragState::GetDragSourceDirection();
    bOverlayReorderedForDrag = false;
    RefreshPinOverlay();
}

FReply SRPGNodeKnot::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
    if (!bOverlayReorderedForDrag)
    {
        CurrentDragDirection = FRPGGraphDragState::GetDragSourceDirection();
        RefreshPinOverlay();
    }

    return FReply::Unhandled();
}

FReply SRPGNodeKnot::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
    FReply Reply = SGraphNode::OnDrop(MyGeometry, DragDropEvent);

    CurrentDragDirection = EGPD_MAX;
    bOverlayReorderedForDrag = false;
    RefreshPinOverlay();

    return Reply;
}

const FSlateBrush* SRPGNodeKnotPin::GetKnotPinBrush() const
{
    const URPGGraphNodeKnot* NodeKnot = GraphPinObj ? Cast<URPGGraphNodeKnot>(GraphPinObj->GetOwningNode()) : nullptr;
    if (!NodeKnot)
    {
        return nullptr;
    }

    return NodeKnot->HasAnyConnections()
        ? FAppStyle::GetBrush(TEXT("Graph.Pin.Connected_VarA"))
        : FAppStyle::GetBrush(TEXT("Graph.Pin.Disconnected_VarA"));
}

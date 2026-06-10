// Copyright Ironic Studio. All Rights Reserved.


#include "Nodes/NarrativeNodeKnot.h"
#include "Nodes/NarrativeGraphNodeBase.h"

const FName UNarrativeNodeKnot::InputPinName(TEXT("InputPin"));
const FName UNarrativeNodeKnot::OutputPinName(TEXT("OutputPin"));

void UNarrativeNodeKnot::AllocateDefaultPins()
{
	UEdGraphPin* InputPin = CreatePin(
		EGPD_Input,
		FName(TEXT("Input")),
		InputPinName
	);

	UEdGraphPin* OutputPin = CreatePin(
		EGPD_Output,
		FName(TEXT("Output")),
		OutputPinName
	);

	if (InputPin)
	{
		InputPin->PinType.PinSubCategory = UNarrativeGraphNodeBase::PinNane;
	}

	if (OutputPin)
	{
		OutputPin->PinType.PinSubCategory = UNarrativeGraphNodeBase::PinNane;
	}
}

TSharedPtr<SGraphNode> UNarrativeNodeKnot::CreateVisualWidget()
{
	return SNew(SNarrativeNodeKnot, this);
}

bool UNarrativeNodeKnot::HasAnyConnections() const
{
	UEdGraphPin* InputPin = GetInputPin();
	UEdGraphPin* OutputPin = GetOutputPin();

	if (InputPin && OutputPin)
	{
		return InputPin->LinkedTo.Num() > 0 || OutputPin->LinkedTo.Num() > 0;
	}

	return false;
}

void SNarrativeNodeKnot::UpdateGraphNode()
{
	InputPins.Empty();
	OutputPins.Empty();

	InputPinWidget.Reset();
	OutputPinWidget.Reset();
	PinOverlay.Reset();

	this->ContentScale.Bind(this, &SGraphNode::GetContentScale);

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

void SNarrativeNodeKnot::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
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

void SNarrativeNodeKnot::RefreshPinOverlay()
{
	if (!PinOverlay.IsValid())
	{
		return;
	}

	if (bOverlayReorderedForDrag)
	{
		return;
	}

	bOverlayReorderedForDrag = true;

	PinOverlay->ClearChildren();

	UNarrativeNodeKnot* KnotNode = Cast<UNarrativeNodeKnot>(GraphNode);
	if (!KnotNode)
	{
		return;
	}

	const bool bDraggingFromOutput = CurrentDragDirection == EGPD_Output;

	const bool bDraggingFromInput = CurrentDragDirection == EGPD_Input;

	UEdGraphPin* KnotInputPin = KnotNode->GetInputPin();
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

void SNarrativeNodeKnot::AddPinToOverlay(TSharedPtr<SGraphPin> PinWidget)
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

void SNarrativeNodeKnot::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	CurrentDragDirection = FNarrativeGraphDragState::GetDragSourceDirection();
	bOverlayReorderedForDrag = false;

	RefreshPinOverlay();
}

FReply SNarrativeNodeKnot::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (!bOverlayReorderedForDrag)
	{
		CurrentDragDirection = FNarrativeGraphDragState::GetDragSourceDirection();
		RefreshPinOverlay();
	}

	return FReply::Unhandled();
}

FReply SNarrativeNodeKnot::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	FReply Reply = SGraphNode::OnDrop(MyGeometry, DragDropEvent);

	CurrentDragDirection = EGPD_MAX;
	bOverlayReorderedForDrag = false;

	RefreshPinOverlay();

	return Reply;
}

const FSlateBrush* SNarrativeNodeKnotPin::GetKnotPinBrush() const
{
	if (!GraphPinObj)
	{
		return nullptr;
	}

	UNarrativeNodeKnot* NodeKnot = Cast<UNarrativeNodeKnot>(GraphPinObj->GetOwningNode());
	if (!NodeKnot)
	{
		return nullptr;
	}

	return NodeKnot->HasAnyConnections() ? FAppStyle::GetBrush(TEXT("Graph.Pin.Connected_VarA")) : FAppStyle::GetBrush(TEXT("Graph.Pin.Disconnected_VarA"));
}

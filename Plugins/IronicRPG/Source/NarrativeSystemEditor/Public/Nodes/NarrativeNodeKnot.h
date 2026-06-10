// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Nodes/NarrativeGraphNodeStyle.h"
#include "NarrativeNodeKnot.generated.h"

class FKismetCompilerContext;
class FBlueprintActionDatabaseRegistrar;
class INameValidatorInterface;

UCLASS()
class NARRATIVESYSTEMEDITOR_API UNarrativeNodeKnot : public UEdGraphNode
{
	GENERATED_BODY()

public:
	static const FName InputPinName;
	static const FName OutputPinName;

	virtual void AllocateDefaultPins() override;

	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override { return FText::GetEmpty(); }
	virtual bool CanUserDeleteNode() const override { return true; }
	virtual bool CanDuplicateNode() const override { return true; }
	virtual bool ShouldDrawNodeAsControlPointOnly(int32& OutInputPinIndex, int32& OutOutputPinIndex) const override { OutInputPinIndex = 0; OutOutputPinIndex = 1; return true; }

	UEdGraphPin* GetInputPin() const { return FindPin(InputPinName, EGPD_Input); }
	UEdGraphPin* GetOutputPin() const { return FindPin(OutputPinName, EGPD_Output); }

	bool HasAnyConnections() const;
};

class SNarrativeNodeKnot : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SNarrativeNodeKnot) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UNarrativeNodeKnot* InNode)
	{
		GraphNode = InNode;
		SetCursor(EMouseCursor::CardinalCross);
		UpdateGraphNode();
	}

	virtual void UpdateGraphNode() override;

	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;

private:
	void RefreshPinOverlay();
	void AddPinToOverlay(TSharedPtr<SGraphPin> PinWidget);

	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

private:
	TSharedPtr<SOverlay> PinOverlay;
	TSharedPtr<SGraphPin> InputPinWidget;
	TSharedPtr<SGraphPin> OutputPinWidget;

	EEdGraphPinDirection CurrentDragDirection = EGPD_MAX;
	bool bOverlayReorderedForDrag = false;
};

class SNarrativeNodeKnotPin : public SNarrativeGraphPinBase
{
public:
	SLATE_BEGIN_ARGS(SNarrativeNodeKnotPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin)
	{
		SNarrativeGraphPinBase::Construct(SNarrativeGraphPinBase::FArguments(), InPin);

		ChildSlot
			[
				SNew(SBox)
					.WidthOverride(16.0f)
					.HeightOverride(16.0f)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SScaleBox)
							.Stretch(EStretch::ScaleToFit)
							[
								SNew(SImage)
									.Image(this, &SNarrativeNodeKnotPin::GetKnotPinBrush)
									.ColorAndOpacity(this, &SNarrativeNodeKnotPin::GetPinColor)
							]
					]
			];
	}

protected:
	const FSlateBrush* GetKnotPinBrush() const;
	virtual FSlateColor GetPinColor() const override { return FSlateColor(FLinearColor::White); }
	virtual TSharedRef<SWidget> GetDefaultValueWidget() override { return SNullWidget::NullWidget; }

};
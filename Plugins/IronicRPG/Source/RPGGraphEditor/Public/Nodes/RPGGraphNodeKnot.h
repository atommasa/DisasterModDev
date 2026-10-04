// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Nodes/RPGGraphNodeStyle.h"
#include "RPGGraphNodeKnot.generated.h"

UCLASS()
class RPGGRAPHEDITOR_API URPGGraphNodeKnot : public UEdGraphNode
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
    virtual bool ShouldDrawNodeAsControlPointOnly(int32& OutInputPinIndex, int32& OutOutputPinIndex) const override
    {
        OutInputPinIndex = 0;
        OutOutputPinIndex = 1;
        return true;
    }

    virtual bool ShouldOverridePinNames() const override { return true; }
    virtual FText GetPinNameOverride(const UEdGraphPin& Pin) const override { return FText::GetEmpty(); }

    UEdGraphPin* GetInputPin() const { return FindPin(InputPinName, EGPD_Input); }
    UEdGraphPin* GetOutputPin() const { return FindPin(OutputPinName, EGPD_Output); }

    bool HasAnyConnections() const;
};

class RPGGRAPHEDITOR_API SRPGNodeKnot : public SGraphNode
{
public:
    SLATE_BEGIN_ARGS(SRPGNodeKnot) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, URPGGraphNodeKnot* InNode)
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

class RPGGRAPHEDITOR_API SRPGNodeKnotPin : public SRPGGraphPinBase
{
public:
    SLATE_BEGIN_ARGS(SRPGNodeKnotPin) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UEdGraphPin* InPin)
    {
        SRPGGraphPinBase::Construct(SRPGGraphPinBase::FArguments(), InPin);

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
                                .Image(this, &SRPGNodeKnotPin::GetKnotPinBrush)
                                .ColorAndOpacity(this, &SRPGNodeKnotPin::GetPinColor)
                        ]
                ]
        ];
    }

protected:
    const FSlateBrush* GetKnotPinBrush() const;
    virtual FSlateColor GetPinColor() const override { return FSlateColor(FLinearColor::White); }
    virtual TSharedRef<SWidget> GetDefaultValueWidget() override { return SNullWidget::NullWidget; }
};

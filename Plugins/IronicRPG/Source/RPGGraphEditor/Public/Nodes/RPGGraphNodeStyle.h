// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphPin.h"

class RPGGRAPHEDITOR_API FRPGGraphDragState
{
public:
    static void SetDragSourceDirection(EEdGraphPinDirection InDirection) { DragSourceDirection = InDirection; }
    static EEdGraphPinDirection GetDragSourceDirection() { return DragSourceDirection; }
    static void Clear() { DragSourceDirection = EGPD_MAX; }

private:
    static EEdGraphPinDirection DragSourceDirection;
};

class RPGGRAPHEDITOR_API SRPGGraphPinBase : public SGraphPin
{
public:
    SLATE_BEGIN_ARGS(SRPGGraphPinBase) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UEdGraphPin* InPin)
    {
        GraphPinObj = InPin;
        check(GraphPinObj);

        SetCursor(EMouseCursor::Default);
        SGraphPin::Construct(SGraphPin::FArguments(), InPin);
    }

    virtual TSharedRef<FDragDropOperation> SpawnPinDragEvent(
        const TSharedRef<SGraphPanel>& InGraphPanel,
        const TArray<TSharedRef<SGraphPin>>& InStartingPins
    ) override
    {
        if (GraphPinObj)
        {
            FRPGGraphDragState::SetDragSourceDirection(GraphPinObj->Direction);
        }

        return SGraphPin::SpawnPinDragEvent(InGraphPanel, InStartingPins);
    }

    virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
    {
        FReply Reply = SGraphPin::OnMouseButtonUp(MyGeometry, MouseEvent);

        if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
        {
            FRPGGraphDragState::Clear();
        }

        return Reply;
    }
};

class RPGGRAPHEDITOR_API SRPGGraphNodeBase : public SGraphNode
{
public:
    SLATE_BEGIN_ARGS(SRPGGraphNodeBase) {}
    SLATE_END_ARGS()

    virtual ~SRPGGraphNodeBase() override { UnbindNodeInfoChanged(); }

    void Construct(const FArguments& InArgs, UEdGraphNode* InNode) {}

    virtual void UpdateGraphNode() override;

protected:
    virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;

    virtual TSharedRef<SWidget> CreateNodeTitleWidget();

    virtual TSharedRef<SWidget> CreateNodeTitleRightWidget();

    virtual TSharedRef<SWidget> CreateNodeNodeContentArea();

    virtual TSharedRef<SWidget> CreateNodeNodeCenterContent();

    virtual FLinearColor GetTitleColor() const { return TitleColor; }

    virtual FLinearColor GetBackgroundColor() const { return BackgroundColor; }

    virtual void HandleNodeInfoPropertyChanged() { UpdateGraphNode(); }
    virtual void BindNodeInfoChanged() {}
    virtual void UnbindNodeInfoChanged() {}

protected:
    FText NodeTitle;
    int32 TitleCharMax = 20;
    FLinearColor TitleColor = FColor::Silver;

    FLinearColor BackgroundColor = FLinearColor(0.1f, 0.1f, 0.1f);

};
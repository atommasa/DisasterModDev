// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphNode.h"
#include "SGraphPin.h"
#include "Nodes/NarrativeNodeInfo.h"

class FNarrativeGraphDragState
{
public:
	static EEdGraphPinDirection DragSourceDirection;

	static void SetDragSourceDirection(EEdGraphPinDirection InPin) { DragSourceDirection = InPin; }

	static EEdGraphPinDirection GetDragSourceDirection() { return DragSourceDirection; }

	static void Clear() { DragSourceDirection = EEdGraphPinDirection::EGPD_MAX; }
};

/**
 * 
 */
class SNarrativeGraphNodeBase : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SNarrativeGraphNodeBase) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphNode* InNode)
	{
		
	}

	virtual void UpdateGraphNode() override;

public:
	UNarrativeNodeInfo* GetNarrativeNodeInfo() const;

protected:
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;

	virtual TSharedRef<SWidget> CreateNarrativeTitleWidget();

	virtual TSharedRef<SWidget> CreateNarrativeTitleRightWidget();

	virtual TSharedRef<SWidget> CreateNarrativeNodeContentArea();

	virtual TSharedRef<SWidget> CreateNarrativeNodeCenterContent();

	virtual FLinearColor GetTitleColor() const { return TitleColor; }

	virtual FLinearColor GetBackgroundColor() const { return BackgroundColor; }

protected:
	// Helper function to bind a property change callback to the node info's OnNodeInfoPropertyChanged delegate
    template<typename WidgetType>
    void BindPropertyChangeToNodeInfo(void (WidgetType::* Callback)(const FPropertyChangedChainEvent&))
    {
        static_assert(
            TIsDerivedFrom<WidgetType, SNarrativeGraphNodeBase>::IsDerived,
            "WidgetType must derive from SNarrativeGraphNodeBase."
            );

        UNarrativeNodeInfo* NodeInfo = GetNarrativeNodeInfo();
        if (!NodeInfo)
        {
            return;
        }

        if (!NodeInfo->OnNodeInfoPropertyChanged.IsBoundToObject(this))
		{
			WidgetType* TypedThis = static_cast<WidgetType*>(this);

			NodeInfo->OnNodeInfoPropertyChanged.AddSP(
				TypedThis,
				Callback
			);
		}
    }

protected:
	FText NodeTitle;
	int32 TitleCharMax = 20;
	FLinearColor TitleColor = FColor::Silver;

	FLinearColor BackgroundColor = FLinearColor(0.1f, 0.1f, 0.1f);

};

class SNarrativeGraphPinBase : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SNarrativeGraphPinBase) {}
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
			FNarrativeGraphDragState::SetDragSourceDirection(GraphPinObj->Direction);
		}

		return SGraphPin::SpawnPinDragEvent(InGraphPanel, InStartingPins);
	}

	virtual FReply OnDrop(
		const FGeometry& MyGeometry,
		const FDragDropEvent& DragDropEvent
	) override
	{
		FReply Reply = SGraphPin::OnDrop(MyGeometry, DragDropEvent);

		return Reply;
	}

	virtual FReply OnMouseButtonUp(
		const FGeometry& MyGeometry,
		const FPointerEvent& MouseEvent
	) override
	{
		FReply Reply = SGraphPin::OnMouseButtonUp(MyGeometry, MouseEvent);

		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			FNarrativeGraphDragState::Clear();
		}

		return Reply;
	}
};
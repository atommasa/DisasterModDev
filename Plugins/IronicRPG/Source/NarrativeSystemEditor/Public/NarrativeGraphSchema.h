// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"
#include "BlueprintConnectionDrawingPolicy.h"
#include "Narrative/SpeakerData.h"
#include "NarrativeGraphSchema.generated.h"

class UNarrativeNodeKnot;

/**
 * This class defines the schema for the Narrative Graph.
 */
UCLASS()
class NARRATIVESYSTEMEDITOR_API UNarrativeGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:
	virtual EGraphType GetGraphType(const UEdGraph* TestEdGraph) const override { return GT_Ubergraph; }
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override { return GetDefault<UEdGraphSchema_K2>()->GetPinTypeColor(PinType); }

	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
	virtual void GetContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;
	virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;

	virtual void BreakNodeLinks(UEdGraphNode& TargetNode) const override;
	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const override;
	virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const override;
	virtual void OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const override;

	virtual FConnectionDrawingPolicy* CreateConnectionDrawingPolicy(
		int32 InBackLayerID,
		int32 InFrontLayerID,
		float InZoomFactor,
		const FSlateRect& InClippingRect,
		FSlateWindowElementList& InDrawElements,
		UEdGraph* InGraphObj
	) const override;

	UEdGraphPin* ResolveKnotPinForOtherPin(UEdGraphPin* Pin, const UEdGraphPin* OtherPin) const;
	const UEdGraphPin* ResolveKnotPinForOtherPinConst(const UEdGraphPin* Pin, const UEdGraphPin* OtherPin) const;
	bool IsKnotToKnot(const UEdGraphPin* Pin, const UEdGraphPin* OtherPin) const;

};

/**
 * This struct defines a new node action for the Narrative Graph.
 */
USTRUCT()
struct FNewNodeAction : public FEdGraphSchemaAction_K2Struct
{
	GENERATED_BODY()

public:
	FNewNodeAction() {}
	FNewNodeAction(FText InNodeCategory, FText InMenuDesc, FText InToolTip, TSubclassOf<class UEdGraphNode> InNodeClass)
		: FEdGraphSchemaAction_K2Struct(InNodeCategory, InMenuDesc, InToolTip, 0) 
		, NodeClass(InNodeClass)
	{}

	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;

	TSubclassOf<class UEdGraphNode> NodeClass;

	FSpeakerData SpeakerData;
};

class FNarrativeConnectionDrawingPolicy : public FKismetConnectionDrawingPolicy
{
public:
	FNarrativeConnectionDrawingPolicy(
		int32 InBackLayerID,
		int32 InFrontLayerID,
		float InZoomFactor,
		const FSlateRect& InClippingRect,
		FSlateWindowElementList& InDrawElements,
		UEdGraph* InGraphObj
	);

	virtual void DetermineWiringStyle(
		UEdGraphPin* OutputPin,
		UEdGraphPin* InputPin,
		FConnectionParams& Params
	) override;

private:
	bool ShouldChangeTangentForRerouteControlPoint(const UNarrativeNodeKnot* Node);
	bool GetAverageConnectedPositionForPin(UEdGraphPin* InPin, FVector2f& OutPos) const;

private:
	TMap<const UNarrativeNodeKnot*, bool> KnotToReversedDirectionMap;
};
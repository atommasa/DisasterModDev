// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "BlueprintConnectionDrawingPolicy.h"
#include "RPGGraphSchema.generated.h"

class URPGGraphNodeKnot;

UCLASS(Abstract)
class RPGGRAPHEDITOR_API URPGGraphSchema : public UEdGraphSchema
{
    GENERATED_BODY()

public:
    virtual EGraphType GetGraphType(const UEdGraph* TestEdGraph) const override { return GT_Ubergraph; }
    virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;

    virtual void GetContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
    virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
    virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;

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

    virtual TSubclassOf<URPGGraphNodeKnot> GetKnotNodeClass() const;

    UEdGraphPin* ResolveKnotPinForOtherPin(UEdGraphPin* Pin, const UEdGraphPin* OtherPin) const;
    const UEdGraphPin* ResolveKnotPinForOtherPinConst(const UEdGraphPin* Pin, const UEdGraphPin* OtherPin) const;
    bool IsKnotToKnot(const UEdGraphPin* Pin, const UEdGraphPin* OtherPin) const;
};

USTRUCT()
struct RPGGRAPHEDITOR_API FRPGNewNodeAction : public FEdGraphSchemaAction_K2Struct
{
    GENERATED_BODY()

public:
    FRPGNewNodeAction() {}
    FRPGNewNodeAction(FText InNodeCategory, FText InMenuDesc, FText InToolTip, TSubclassOf<UEdGraphNode> InNodeClass)
        : FEdGraphSchemaAction_K2Struct(InNodeCategory, InMenuDesc, InToolTip, 0)
        , NodeClass(InNodeClass)
    {}

    virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;

protected:
    virtual void PostNodeCreated(UEdGraphNode* NewNode) {}

public:
    UPROPERTY()
    TSubclassOf<UEdGraphNode> NodeClass;
};

class RPGGRAPHEDITOR_API FRPGConnectionDrawingPolicy : public FKismetConnectionDrawingPolicy
{
public:
    FRPGConnectionDrawingPolicy(
        int32 InBackLayerID,
        int32 InFrontLayerID,
        float InZoomFactor,
        const FSlateRect& InClippingRect,
        FSlateWindowElementList& InDrawElements,
        UEdGraph* InGraphObj
    );

    virtual void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, FConnectionParams& Params) override;

protected:
    virtual FLinearColor GetDefaultWireColor(UEdGraphPin* OutputPin, UEdGraphPin* InputPin) const { return FLinearColor(0.85f, 0.85f, 0.85f, 1.0f); }
    virtual float GetDefaultWireThickness(UEdGraphPin* OutputPin, UEdGraphPin* InputPin) const { return 2.0f; }

private:
    bool ShouldChangeTangentForRerouteControlPoint(const URPGGraphNodeKnot* Node);
    bool GetAverageConnectedPositionForPin(UEdGraphPin* InPin, FVector2f& OutPos) const;

private:
    TMap<const URPGGraphNodeKnot*, bool> KnotToReversedDirectionMap;
};

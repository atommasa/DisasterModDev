// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RPGGraphSchema.h"
#include "Narrative/SpeakerData.h"
#include "NarrativeGraphSchema.generated.h"

class UNarrativeNodeKnot;

/**
 * Schema for the Dialogue/Narrative graph.
 * Generic connection/reroute behavior is inherited from RPGGraphEditor.
 */
UCLASS()
class NARRATIVESYSTEMEDITOR_API UNarrativeGraphSchema : public URPGGraphSchema
{
    GENERATED_BODY()

public:
    virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
    virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;
    virtual TSubclassOf<URPGGraphNodeKnot> GetKnotNodeClass() const override;
};

/**
 * Narrative-specific new-node action. The generic creation/auto-wiring behavior
 * comes from FRPGNewNodeAction; this class only injects speaker data for dialogue nodes.
 */
USTRUCT()
struct FNewNodeAction : public FRPGNewNodeAction
{
    GENERATED_BODY()

public:
    FNewNodeAction() {}
    FNewNodeAction(FText InNodeCategory, FText InMenuDesc, FText InToolTip, TSubclassOf<UEdGraphNode> InNodeClass)
        : FRPGNewNodeAction(InNodeCategory, InMenuDesc, InToolTip, InNodeClass)
    {}

protected:
    virtual void PostNodeCreated(UEdGraphNode* NewNode) override;

public:
    FSpeakerData SpeakerData;
};

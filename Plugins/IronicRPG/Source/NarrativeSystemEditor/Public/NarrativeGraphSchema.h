// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"
#include "NarrativeGraphSchema.generated.h"

/**
 * This class defines the schema for the Narrative Graph.
 */
UCLASS()
class NARRATIVESYSTEMEDITOR_API UNarrativeGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:
	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
	virtual void GetContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;

	virtual void BreakNodeLinks(UEdGraphNode& TargetNode) const override;
	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const override;
	virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const override;
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
	FNewNodeAction(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction_K2Struct(InNodeCategory, InMenuDesc, InToolTip, InGrouping) {}

	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;

	FName NodeName;
};

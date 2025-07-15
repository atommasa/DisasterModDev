// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EdGraphSchema_K2.h"
#include "NarrativeEventGraphSchema.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVESYSTEMEDITOR_API UNarrativeEventGraphSchema : public UEdGraphSchema_K2
{
	GENERATED_BODY()

public:
    virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const;
	virtual void GetCommentAction(FGraphActionMenuBuilder& ActionMenuBuilder, const UEdGraph* CurrentGraph) const;

protected:
	void GetGraphNodeActions(FGraphActionMenuBuilder& ActionMenuBuilder, const FString& InCategory) const;
};

USTRUCT()
struct FNarrativeNewNodeAction_EventGraph : public FEdGraphSchemaAction_K2Struct
{
	GENERATED_BODY()

public:
	FNarrativeNewNodeAction_EventGraph() {}
	FNarrativeNewNodeAction_EventGraph(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction_K2Struct(InNodeCategory, InMenuDesc, InToolTip, InGrouping) {}

	//virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true);

	FName NodeName;
};
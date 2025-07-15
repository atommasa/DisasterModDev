// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "NarrativeAsset.h"
#include "NarrativeGraphNodeBase.generated.h"

/**
 * This is a structure that records pin connections.
 */
USTRUCT()
struct FPinConnectionData
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid FromPinId;

	UPROPERTY()
	FGuid ToNodeId;

	UPROPERTY()
	FGuid ToPinId;

};

/**
 * This is the base class for all nodes in the narrative graph.
 */
UCLASS()
class NARRATIVESYSTEMEDITOR_API UNarrativeGraphNodeBase : public UEdGraphNode
{
	GENERATED_BODY()

public:
	virtual UEdGraphPin* CreateNarrativePin(EEdGraphPinDirection Direction, FName PinName) { /* Must be overridden */ return nullptr; }
	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;

	void SetNodeInfo(class UNarrativeNodeInfo* InNodeInfo);
	class UNarrativeNodeInfo* GetNodeInfo() const { return _NodeInfo; }

	class UDialogueBlueprint* GetNarrativeAsset() const { return Cast<UDialogueBlueprint>(GetGraph()->GetOuter()); }

	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;

public: // UNarrativeGraphNode interface
	virtual void SyncPinWithResponse();

public: // Menu Entry
	virtual void DestroyNode() override;

	virtual void AddPin() {};
	virtual void DeletePin() {};

protected:
	UPROPERTY()
	class UNarrativeNodeInfo* _NodeInfo = nullptr;

	// Try to record redo and undo pin connection
	UPROPERTY()
	TArray<FPinConnectionData> SavedConnections;

};

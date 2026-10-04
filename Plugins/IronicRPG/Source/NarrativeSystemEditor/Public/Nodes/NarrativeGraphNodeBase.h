// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/RPGGraphNodeBase.h"
#include "NarrativeAsset.h"
#include "Nodes/NarrativeGraphNodeStyle.h"
#include "NarrativeGraphNodeBase.generated.h"

/**
 * This is the base class for all nodes in the narrative graph.
 */
UCLASS()
class NARRATIVESYSTEMEDITOR_API UNarrativeGraphNodeBase : public URPGGraphNodeBase
{
	GENERATED_BODY()

	friend class SNarrativeGraphNodeBase;

public: // URPGGraphNodeBase interfaces
	virtual UEdGraphPin* CreateRPGGraphPin(EEdGraphPinDirection Direction, FName InPinName) override;
	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;

	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;

	virtual FName GetCustomEventGraphName() const override { return UDialogueBlueprint::DialogueEventGraphName; }

	virtual FName CreateCallableBindingName() const override;

public: // Menu Entry
	virtual void DestroyNode() override;

public:
	class UNarrativeNodeInfo* GetNarrativeNodeInfo() const { return GetNodeInfoAs<class UNarrativeNodeInfo>(); }
	class UDialogueBlueprint* GetNarrativeAsset() const { return Cast<UDialogueBlueprint>(GetBlueprintAsset()); }

public:
	static FName PinNane;

};

// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "NarrativeAsset.h"
#include "Nodes/NarrativeGraphNodeStyle.h"
#include "NarrativeGraphNodeBase.generated.h"

namespace ECallableBindingType
{
	enum Type
	{
		CBT_None,

		// Call an event
		CBT_Event,

		// Call a BP function
		CBT_Function,
	};
}

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

	friend class SNarrativeGraphNodeBase;

public:
	typedef ECallableBindingType::Type ECallableBindingType;

public:
	virtual UEdGraphPin* CreateNarrativePin(EEdGraphPinDirection Direction, FName PinName);
	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;

	void SetNodeInfo(class UNarrativeNodeInfo* InNodeInfo);
	class UNarrativeNodeInfo* GetNodeInfo() const { return NodeInfo; }

	class UDialogueBlueprint* GetNarrativeAsset() const { return Cast<UDialogueBlueprint>(GetGraph()->GetOuter()); }

	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;

	template<typename T>
	T* GetNodeInfoAs() const
	{
		return Cast<T>(NodeInfo);
	}

public: // Menu Entry
	virtual void DestroyNode() override;

public:
	virtual ENarrativeNodeType GetNarrativeNodeType() const { return ENarrativeNodeType::UnknownNode; }
	virtual void SyncPin();

public:
	virtual ECallableBindingType GetCallableBindingType() const { return ECallableBindingType::CBT_None; }
	virtual bool CanCreateCallableBinding() const { return  GetCallableBindingType() != ECallableBindingType::CBT_None; }

	virtual UFunction* GetFunctionAsSignature() const { return nullptr; }

	class UK2Node_CustomEvent* CreateOrFocusNarrativeCustomEvent();
	class UK2Node_FunctionEntry* CreateOrFocusNarrativeFunction();

protected:
	virtual FName CreateCallableBindingName() const;
	virtual FText CreateCallableBindingComment() const;

	virtual void CreateCallableBindingParameterPins(class UK2Node_EditablePinBase* InNode) { }

public:
	static FName PinNane;

protected:
	UPROPERTY()
	class UNarrativeNodeInfo* NodeInfo = nullptr;

	// Try to record redo and undo pin connection
	UPROPERTY()
	TArray<FPinConnectionData> SavedConnections;

};

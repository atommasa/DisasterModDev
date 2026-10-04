// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "RPGGraphNodeBase.generated.h"

UENUM()
enum class ECallableBindingType
{
    CBT_None,

    // Call an event
    CBT_Event,

    // Call a BP function
    CBT_Function,
};

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

UCLASS(Abstract)
class RPGGRAPHEDITOR_API URPGGraphNodeBase : public UEdGraphNode
{
    GENERATED_BODY()

public:
    virtual UEdGraphPin* CreateRPGGraphPin(EEdGraphPinDirection Direction, FName InPinName);

    virtual void SetNodeInfoObject(UObject* InNodeInfo);

    template<typename T>
    T* GetNodeInfoAs() const
    {
        return Cast<T>(NodeInfoObject);
    }

    virtual UBlueprint* GetBlueprintAsset() const { return Cast<UBlueprint>(GetGraph()->GetOuter()); }

    virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
    virtual void DestroyNode() override;

    virtual void SyncPin();

public:
    virtual ECallableBindingType GetCallableBindingType() const { return ECallableBindingType::CBT_None; }
    virtual bool CanCreateCallableBinding() const { return  GetCallableBindingType() != ECallableBindingType::CBT_None; }

    virtual FName GetCustomEventGraphName() const { return NAME_None; }
    virtual UFunction* GetFunctionAsSignature() const { return nullptr; }

    virtual class UK2Node_CustomEvent* CreateOrFocusCustomEvent(FName EventName);
    virtual class UK2Node_FunctionEntry* CreateOrFocusCustomFunction(FName FunctionName);

    virtual FName CreateCallableBindingName() const;
    virtual FText CreateCallableBindingComment() const;

    virtual void CreateCallableBindingParameterPins(class UK2Node_EditablePinBase* InNode) {}

public:
    static FName PinName;

protected:
    UPROPERTY()
    TObjectPtr<UObject> NodeInfoObject = nullptr;

    UPROPERTY()
    TArray<FPinConnectionData> SavedConnections;
};

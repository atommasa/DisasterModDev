// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h" 	
#include "UObject/NameTypes.h"
#include "Nodes/NarrativeNodeInfo.h"
#include "Nodes/NarrativeNodeType.h"
#include "NarrativeRuntimeGraph.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNodeStarted, class UNarrativeNodeInfo*, NodeInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNodeFinished, class UNarrativeNodeInfo*, NodeInfo);

UCLASS()
class NARRATIVESYSTEMRUNTIME_API UNarrativeRuntimePin : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FName PinName;

    UPROPERTY()
    FGuid PinId;

    UPROPERTY()
    UNarrativeRuntimePin* Connection = nullptr;

    UPROPERTY()
    UNarrativeRuntimeNode* Parent = nullptr;
};

UCLASS()
class NARRATIVESYSTEMRUNTIME_API UNarrativeRuntimeNode : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FGuid NodeGuid;

    UPROPERTY()
	ENarrativeNodeType NodeType = ENarrativeNodeType::UnknownNode;

    UPROPERTY()
    UNarrativeRuntimePin* InputPin;

    UPROPERTY()
    TArray<UNarrativeRuntimePin*> OutputPins;

    UPROPERTY()
    FVector2D Position;

	UPROPERTY()
	UNarrativeNodeInfo* NodeInfo = nullptr;

    // Called when the node is started
    UPROPERTY(BlueprintAssignable, Category = "Narrative")
    FOnNodeStarted OnNodeStarted;

    // Called when the node is finished
    UPROPERTY(BlueprintAssignable, Category = "Narrative")
    FOnNodeFinished OnNodeFinished;

};

UCLASS()
class NARRATIVESYSTEMRUNTIME_API UNarrativeRuntimeGraph : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY()
    TArray<UNarrativeRuntimeNode*> Nodes;

};

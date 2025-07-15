// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Kismet/GameplayStatics.h"
#include "NarrativeNodeInfo.generated.h"

/**
 * This class is used to store information about a narrative node.
 */
UCLASS(BlueprintType)
class NARRATIVESYSTEMRUNTIME_API UNarrativeNodeInfo : public UObject
{
	GENERATED_BODY()
	
public: // Node Execution
	virtual bool ExecuteNode() { return true; }
	virtual bool StopNode() { return true; }
};
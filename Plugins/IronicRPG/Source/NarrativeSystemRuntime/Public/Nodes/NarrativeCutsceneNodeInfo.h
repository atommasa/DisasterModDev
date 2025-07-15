// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/NarrativeNodeInfo.h"
#include "NarrativeCutsceneNodeInfo.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVESYSTEMRUNTIME_API UNarrativeCutsceneNodeInfo : public UNarrativeNodeInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class ULevelSequence* Cutscene;
	
public: // Node Execution
	virtual bool ExecuteNode() override;
};

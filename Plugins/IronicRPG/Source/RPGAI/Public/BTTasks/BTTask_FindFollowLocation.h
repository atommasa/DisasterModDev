// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_FindFollowLocation.generated.h"

/**
 * 
 */
UCLASS()
class RPGAI_API UBTTask_FindFollowLocation : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
	
public:
	explicit UBTTask_FindFollowLocation(const FObjectInitializer& ObjectInitializer);

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	UPROPERTY()
	TWeakObjectPtr<class AFollowLocationTracker> CurrentFollowTracker;
};

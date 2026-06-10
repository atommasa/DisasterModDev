// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "Navigation/PathFollowingComponent.h"
#include "BTTask_MoveToLocation.generated.h"

/**
 * 
 */
UCLASS()
class RPGAI_API UBTTask_MoveToLocation : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
	
public:
	explicit UBTTask_MoveToLocation(const FObjectInitializer& ObjectInitializer);

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	void MoveThroughNavLink(const UPathFollowingComponent* PFComp, const FVector& TargetLocation);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "AI")
	UBehaviorTreeComponent* BTComp;

	UPROPERTY(BlueprintReadOnly, Category = "AI")
	AAIController* AIController;

	UPROPERTY(BlueprintReadOnly, Category = "AI")
	class UAITask_MoveTo* MoveToTask;

	UFUNCTION()
	void OnMoveTaskFinished(TEnumAsByte<EPathFollowingResult::Type> Result, AAIController* Controller);
};

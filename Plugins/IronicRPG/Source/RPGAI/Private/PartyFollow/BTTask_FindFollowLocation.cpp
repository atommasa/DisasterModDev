// Copyright Ironic Studio. All Rights Reserved.


#include "PartyFollow/BTTask_FindFollowLocation.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "PartyFollow/PartyFollowComponent.h"

UBTTask_FindFollowLocation::UBTTask_FindFollowLocation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = "Find Follow Location";

}

EBTNodeResult::Type UBTTask_FindFollowLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Failed;
	}

	APawn* OwnerPawn = AIController->GetPawn();
	if (!OwnerPawn)
	{
		return EBTNodeResult::Failed;
	}

	if (UPartyFollowComponent* FollowComp = OwnerPawn->FindComponentByClass<UPartyFollowComponent>())
	{
		FPartyFollowTarget FollowTarget;
		
		if (FollowComp->GetFollowTarget(FollowTarget))
		{
			UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
			if (!Blackboard)
			{
				return EBTNodeResult::Failed;
			}

			if (FollowTarget.Mode == EPartyFollowTargetMode::Hold)
			{
				// Waiting, Settled, and a leader handoff deliberately suspend the old path.
				// Keeping the previous request would make a follower finish an
				// unnecessary correction after the leader has already stopped.
				AIController->StopMovement();
			}

			Blackboard->SetValueAsVector(
				GetSelectedBlackboardKey(),
				FollowTarget.Location);
			
			return EBTNodeResult::Succeeded;
		}
	}

	return EBTNodeResult::Failed;
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTasks/BTTask_FindFollowLocation.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "RPGAISubsystem.h"
#include "FollowLocationTracker.h"

UBTTask_FindFollowLocation::UBTTask_FindFollowLocation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = "Find Follow Location";

}

EBTNodeResult::Type UBTTask_FindFollowLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (!CurrentFollowTracker.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			URPGAISubsystem* AISubsystem = World->GetSubsystem<URPGAISubsystem>();
			if (AISubsystem)
			{
				CurrentFollowTracker = AISubsystem->FollowLocationTracker;
			}
		}
	}

	if (auto* AIController = OwnerComp.GetAIOwner())
	{
		if (CurrentFollowTracker.IsValid())
		{
			FVector TargetLocation = CurrentFollowTracker->FindNearestLocationAndOccupy(AIController);

			// Set the target location in the blackboard
			OwnerComp.GetBlackboardComponent()->SetValueAsVector(GetSelectedBlackboardKey(), TargetLocation);
		}

		// Finish with success
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}


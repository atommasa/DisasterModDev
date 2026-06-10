// Copyright Ironic Studio. All Rights Reserved.


#include "BTTasks/BTTask_MoveToLocation.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "AIController.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Tasks/AITask_MoveTo.h"
#include "NavMesh/NavMeshPath.h"

UBTTask_MoveToLocation::UBTTask_MoveToLocation(const FObjectInitializer& ObjectInitializer)
	: Super{ ObjectInitializer }
{
	NodeName = "Move To Location";

	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_MoveToLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	BTComp = &OwnerComp;
	AIController = OwnerComp.GetAIOwner();
	if (!BTComp || !AIController)
	{
		return EBTNodeResult::Failed;
	}

	// Get the target location from the blackboard
	FVector TargetLocation = OwnerComp.GetBlackboardComponent()->GetValueAsVector(GetSelectedBlackboardKey());

	// Check if the AIController has a valid path and is currently navigating a link
	UPathFollowingComponent* PFComp = AIController->GetPathFollowingComponent();
	if (PFComp && PFComp->HasValidPath() && PFComp->IsCurrentSegmentNavigationLink())
	{
		// If the AI is navigating a link, we need to handle it differently
		MoveThroughNavLink(PFComp, TargetLocation);

		return EBTNodeResult::InProgress;
	}
	else
	{
		UAIBlueprintHelperLibrary::SimpleMoveToLocation(AIController, TargetLocation);

		FinishLatentTask(*BTComp, EBTNodeResult::Succeeded);
		return EBTNodeResult::Succeeded;
	}
}

void UBTTask_MoveToLocation::MoveThroughNavLink(const UPathFollowingComponent* PFComp, const FVector& TargetLocation)
{
	int32 Index = INDEX_NONE;
	const FNavPathSharedPtr Path = PFComp->GetPath();
	if (Path.IsValid())
	{
		// Find nearest NavLink start point index in the path
		const TArray<FNavPathPoint>& PathPoints = Path->GetPathPoints();
		for (int32 i = PFComp->GetCurrentPathIndex(); i < PathPoints.Num(); ++i)
		{
			if (FNavMeshNodeFlags(PathPoints[i].Flags).IsNavLink())
			{
				Index = i;

				break;
			}
		}

		// Find current NavLink end point index (i.e. the next index of start point) in the path
		if (Index != INDEX_NONE && PathPoints.IsValidIndex(++Index))
		{
			// Create a move task to handle the navigation link
			MoveToTask = UAITask_MoveTo::AIMoveTo(
				AIController,
				PathPoints[Index].Location,
				nullptr,
				-1.0f,
				EAIOptionFlag::Default,
				EAIOptionFlag::Default,
				false,
				true,
				false
			);

			if (MoveToTask)
			{
				MoveToTask->OnMoveTaskFinished.AddUObject(this, &UBTTask_MoveToLocation::OnMoveTaskFinished);
				MoveToTask->ReadyForActivation();
			}

			DrawDebugSphere(AIController->GetWorld(), PathPoints[Index].Location, 10.0f, 12, FColor::Red, false, 3.0f, 0, 1.0f);
		}
	}
}

void UBTTask_MoveToLocation::OnMoveTaskFinished(TEnumAsByte<EPathFollowingResult::Type> Result, AAIController* Controller)
{
	if (!BTComp || !Controller)
	{
		return;
	}

	EBTNodeResult::Type TaskResult = (Result == EPathFollowingResult::Success)
		? EBTNodeResult::Succeeded
		: EBTNodeResult::Failed;

	FinishLatentTask(*BTComp, TaskResult);

	if (MoveToTask)
	{
		MoveToTask->OnMoveTaskFinished.RemoveAll(this);
		MoveToTask->EndTask();
		MoveToTask = nullptr;
	}
}
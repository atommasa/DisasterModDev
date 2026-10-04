// Copyright Ironic Studio. All Rights Reserved.

#include "Tests/InteractionTestTypes.h"

#include "Components/InteractableComponent.h"

void UInteractionTestReceiver::HandleRequested(AActor* Interactor, UInteractableComponent* Interactable, FGameplayTag ActionTag, FGuid RequestId)
{
	++RequestCount;
	LastRequestId = RequestId;
	LastActionTag = ActionTag;
	LastInteractable = Interactable;
}

void UInteractionTestReceiver::HandleCompleted(FGuid RequestId, const FInteractionResult& Result)
{
	++CompletionCount;
	LastRequestId = RequestId;
	LastResult = Result;
}

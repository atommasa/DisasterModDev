// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractionTypes.h"
#include "UObject/Object.h"
#include "InteractionTestTypes.generated.h"

class UInteractableComponent;

UCLASS()
class UInteractionTestReceiver : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleRequested(AActor* Interactor, UInteractableComponent* Interactable, FGameplayTag ActionTag, FGuid RequestId);

	UFUNCTION()
	void HandleCompleted(FGuid RequestId, const FInteractionResult& Result);

public:
	int32 RequestCount = 0;
	int32 CompletionCount = 0;
	FGuid LastRequestId;
	FGameplayTag LastActionTag;
	FInteractionResult LastResult;
	TWeakObjectPtr<UInteractableComponent> LastInteractable;
};

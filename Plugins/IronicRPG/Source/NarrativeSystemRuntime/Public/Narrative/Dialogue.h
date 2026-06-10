// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Narrative/SpeakerData.h"
#include "Dialogue.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable)
class NARRATIVESYSTEMRUNTIME_API UDialogue : public UObject
{
    GENERATED_BODY()

public:
	// If there is no player actor, the system will use the current player controller to get the player actor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
	TSubclassOf<AActor> PlayerActorClass;

	// The speakers participating in this dialogue.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speakers")
    TArray<FSpeakerData> Speakers;

};

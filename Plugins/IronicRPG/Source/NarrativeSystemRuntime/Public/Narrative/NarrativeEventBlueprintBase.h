// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Narrative/SpeakerData.h"
#include "NarrativeEventBlueprintBase.generated.h"
// TODO: Rename Dialogue

/**
 * 
 */

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
    TMap<FName, FSpeakerData> Speakers;

};

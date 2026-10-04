// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Narrative/SpeakerData.h"
#include "Narrative/DialogueDataTypes.h"
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

public:
	UFUNCTION(BlueprintNativeEvent)
	FText GetDialogueVariable(const UNarrativeNodeInfo* NodeInfo, const FDialogueLine& DialogueLine, const FName& VariableName);
	virtual FText GetDialogueVariable_Implementation(const UNarrativeNodeInfo* NodeInfo, const FDialogueLine& DialogueLine, const FName& VariableName) { return FText(); }

#if WITH_EDITOR
public:

#endif // WITH_EDITOR
};

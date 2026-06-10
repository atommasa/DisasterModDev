// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "NarrativeAsset.h"
#include "NarrativeEventBlueprint.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVESYSTEMRUNTIME_API UNarrativeEventBlueprint : public UBlueprint
{
	GENERATED_BODY()

public:

	UPROPERTY()
	UDialogueBlueprint* NarrativeAsset;
	
};

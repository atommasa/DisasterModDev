// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RPGMapInteractionContext.generated.h"

/** Identifies the selected Marker Action requested by an interactive widget. */
USTRUCT(BlueprintType)
struct RPGGAMEPLAY_API FMapMarkerActionButtonContext
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite)
    FGameplayTag ActionTag;
};

/** Identifies the Marker candidate chosen by an interactive widget. */
USTRUCT(BlueprintType)
struct RPGGAMEPLAY_API FMapMarkerCandidateButtonContext
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite)
    FGuid PointId;
};

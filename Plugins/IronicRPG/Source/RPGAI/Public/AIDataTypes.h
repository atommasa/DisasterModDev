// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIDataTypes.generated.h"

UENUM(BlueprintType)
enum class EPratrolOverrideType : uint8
{
	Override UMETA(DisplayName = "Override"),
	Ignore UMETA(DisplayName = "Ignore"),
};

/**
 * Struct to hold patrol data for AI actors, or can be used for override patrol settings by splines.
 */
USTRUCT(BlueprintType)
struct RPGAI_API FPatrolData
{
	GENERATED_BODY()

	// Speed at which the AI should patrol.
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float PatrolSpeed = 200.f;

	// How long it should stay at the patrol point before moving to the next one.
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float WaitTime = 2.f;

	// The patrol route actor containing the spline path for patrolling.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, meta=(EditCondition = "bIsActorData", HideEditConditionToggle, EditConditionHides))
	TObjectPtr<class APatrolRoute> PatrolRoute = nullptr;

	// Whether the patrol should loop back to the start after reaching the end.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, meta=(EditCondition = "bIsActorData", HideEditConditionToggle, EditConditionHides))
	bool bLoop = true;

	// Whether the patrol should reverse direction upon reaching the end of the route.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, meta=(EditCondition = "bIsActorData && !bLoop", EditConditionHides))
	bool bReverseAtEnd = false;

	// Is this struct used for actor patrol data?
	UPROPERTY()
	bool bIsActorData = true;

};
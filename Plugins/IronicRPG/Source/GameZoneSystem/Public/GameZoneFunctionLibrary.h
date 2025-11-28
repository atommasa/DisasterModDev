// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameZoneFunctionLibrary.generated.h"

/**
 * This function library is a collection of static functions which can be called in any blueprint.
 */
UCLASS()
class GAMEZONESYSTEM_API UGameZoneFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Gameplay", meta=(WorldContext = "WorldContextObject"))
	static void TransitionToLevel(const UObject* WorldContextObject, FName LevelName, bool bAbsolute = true, FString Options = FString(TEXT("")));
};

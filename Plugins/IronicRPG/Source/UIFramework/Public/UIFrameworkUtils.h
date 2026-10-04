// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UIFrameworkUtils.generated.h"

class UUIControlComponent;

/**
 * 
 */
UCLASS()
class UIFRAMEWORK_API UUIFrameworkUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintPure, Category = "RPG|UI", meta=(WorldContext = "WorldContextObject"))
	static UUIControlComponent* GetUIControlComponent(const UObject* WorldContextObject);

};

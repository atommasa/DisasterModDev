// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "AbilityCostCalculation.generated.h"

/**
 * 
 */
UCLASS()
class RPGCORE_API UAbilityCostCalculation : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()
	
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

};

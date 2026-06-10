// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "Abilities/AbilityDataTypes.h"
#include "RPGExecution_AbilityFormula.generated.h"

/**
 * 
 */
UCLASS()
class RPGCORE_API URPGExecution_AbilityFormula : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()
	
public:
    virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;

protected:
    virtual float EvaluateFormula(const FAbilityFormula& Formula, UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC, float AbilityLevel) const;
	virtual float RoundingFinalValue(float Value, const FAbilityModifierFormula& ModifierFormula, UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC) const;
	virtual EMagnitudeRoundingMode GetRoundingPolicy(const FGameplayTag& EffectTypeTag, UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC) const;

};

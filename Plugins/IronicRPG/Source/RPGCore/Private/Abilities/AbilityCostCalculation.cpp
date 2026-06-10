// Copyright Ironic Studio. All Rights Reserved.


#include "Abilities/AbilityCostCalculation.h"
#include "Abilities/RPGGameplayAbility.h"

#include "Characters/Attributes/RPGAttributeSet.h"

void UAbilityCostCalculation::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	//const URPGGameplayAbility* Ability = Cast<URPGGameplayAbility>(ExecutionParams.GetOwningSpec().GetContext().GetAbilityInstance_NotReplicated());
	//if (!Ability)
	//{
	//	return;
	//}
	//
	//for (const FAbilityCostEntry& CostEntry : Ability->AbilityCostSpec.CostEntries)
	//{
	//	float CostValue = CostEntry.Magnitude.GetValueAtLevel(ExecutionParams.GetOwningSpec().GetLevel());
	//	float CurrentValue = ExecutionParams.GetSourceAbilitySystemComponent()->GetNumericAttribute(CostEntry.Attribute);

	//	switch (CostEntry.CalcType)
	//	{
	//		case EMagnitudeCalcType::Flat:
	//			// CostValue is already flat, do nothing
	//			break;

	//		case EMagnitudeCalcType::PercentOfMax:
	//		{
	//			float MaxValue = 0.0f;
	//			URPGAttributeSet::HasMaxClampAttribute(
	//				ExecutionParams.GetSourceAbilitySystemComponent()->GetOwner(),
	//				CostEntry.Attribute,
	//				MaxValue
	//			);

	//			CostValue = MaxValue * CostValue; // max * percentage
	//			break;
	//		}

	//		case EMagnitudeCalcType::PercentOfCurrent:
	//		{
	//			CostValue = CurrentValue * CostValue; // current * percentage
	//			break;
	//		}
	//	}
	//	
	//	if (CostValue >= CurrentValue && CostEntry.bCanEndure)
	//	{
	//		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
	//			CostEntry.Attribute,
	//			EGameplayModOp::Override,
	//			1.f // Set to minimum of 1 if enduring
	//		));

	//		continue;
	//	}

	//	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
	//		CostEntry.Attribute,
	//		EGameplayModOp::Additive,
	//		-CostValue // Costs are typically represented as negative values
	//	));
	//}
}

// Copyright Ironic Studio. All Rights Reserved.


#include "Abilities/Executions/RPGExecution_AbilityFormula.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/AbilityAsset.h"
#include "Abilities/RPGGameplayAbility.h"

#include "Misc/RPGTeamAgentInterface.h"

#include "RPGSettings.h"

void URPGExecution_AbilityFormula::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
    const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

    UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
    UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();

    if (!SourceASC || !TargetASC)
    {
        return;
    }

    const UAbilityAsset* AbilityAsset = Cast<UAbilityAsset>(Spec.GetContext().GetSourceObject());

    if (!AbilityAsset)
    {
        return;
    }

    const int32 EffectSpecIndex = FMath::RoundToInt(
        Spec.GetSetByCallerMagnitude(
            Ability_Data_EffectSpecIndex,
            false,
            -1.0f
        )
    );

    const TArray<FAbilityEffectSpec>& EffectSpecs = AbilityAsset->GetEffectSpecs();

    if (!EffectSpecs.IsValidIndex(EffectSpecIndex))
    {
        return;
    }

    const FAbilityEffectSpec& EffectSpec = EffectSpecs[EffectSpecIndex];

    const float AbilityLevel = Spec.GetLevel();

    for (const FAbilityModifierFormula& ModifierFormula : EffectSpec.ModifierFormulas)
    {
        if (!ModifierFormula.ModifiedAttribute.IsValid())
        {
            continue;
        }

        const float RawValue = EvaluateFormula(
            ModifierFormula.Formula,
            SourceASC,
            TargetASC,
            AbilityLevel
        );

		const float RoundedValue = RoundingFinalValue(
            RawValue,
			ModifierFormula,
			SourceASC,
			TargetASC
		);

        OutExecutionOutput.AddOutputModifier(
            FGameplayModifierEvaluatedData(
                ModifierFormula.ModifiedAttribute,
                ModifierFormula.ModifierOp,
                RoundedValue
            )
        );
    }
}

float URPGExecution_AbilityFormula::EvaluateFormula(const FAbilityFormula& Formula, UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC, float AbilityLevel) const
{
    float TotalValue = 0.0f;

    for (const FAbilityFormulaTerm& Term : Formula.Terms)
    {
        if (!Term.Attribute.IsValid())
        {
            continue;
        }

        UAbilitySystemComponent* ProviderASC = nullptr;

        switch (Term.Provider)
        {
        case EAttributeProvider::Source:
            ProviderASC = SourceASC;
            break;

        case EAttributeProvider::Target:
            ProviderASC = TargetASC;
            break;

        default:
            break;
        }

        if (!ProviderASC)
        {
            continue;
        }

        const float AttributeValue = ProviderASC->GetNumericAttribute(Term.Attribute);

        const float Coefficient = Term.Coefficient.GetValueAtLevel(AbilityLevel);

        TotalValue += AttributeValue * Coefficient + Term.FlatBonus;
    }

    const float BaseCoefficient = Formula.BaseCoefficient.GetValueAtLevel(AbilityLevel);

    return TotalValue * BaseCoefficient + Formula.BaseBonus;
}

float URPGExecution_AbilityFormula::RoundingFinalValue(float Value, const FAbilityModifierFormula& ModifierFormula, UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC) const
{
	const float AbsValue = FMath::Abs(Value);
    const float Sign = FMath::Sign(Value);
    const EMagnitudeRoundingMode RoundingPolicy = GetRoundingPolicy(ModifierFormula.EffectTypeTag, SourceASC, TargetASC);

	float RoundedValue = AbsValue;

    switch (RoundingPolicy)
    {
    case EMagnitudeRoundingMode::RoundNearest:
        RoundedValue = FMath::RoundToFloat(AbsValue);
        break;

    case EMagnitudeRoundingMode::RoundUp:
        RoundedValue = FMath::CeilToFloat(AbsValue);
        break;

    case EMagnitudeRoundingMode::RoundDown:
        RoundedValue = FMath::FloorToFloat(AbsValue);
        break;

	case EMagnitudeRoundingMode::RoundHalfToEven:
        RoundedValue = FMath::RoundHalfToEven(AbsValue);
		break;

    case EMagnitudeRoundingMode::None:
    default:
        break;
    }

    return RoundedValue * Sign;
}

EMagnitudeRoundingMode URPGExecution_AbilityFormula::GetRoundingPolicy(const FGameplayTag& EffectTypeTag, UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC) const
{
	const URPGSettings* Settings = GetDefault<URPGSettings>();

	const IRPGTeamAgentInterface* SourceTeamAgent = Cast<IRPGTeamAgentInterface>(SourceASC->GetOwner());
	const IRPGTeamAgentInterface* TargetTeamAgent = Cast<IRPGTeamAgentInterface>(TargetASC->GetOwner());

    if (!SourceTeamAgent || !TargetTeamAgent)
    {
        return EMagnitudeRoundingMode::None;
	}

	if (Settings)
	{
		if (const EMagnitudeRoundingMode* RoundingPolicy = Settings->RoundingModes.Find(EffectTypeTag))
		{
			return *RoundingPolicy;
		}
	}

	return EMagnitudeRoundingMode::None;
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Attributes/CombatAttributeSet.h"
#include "GameplayEffectExtension.h"

void UCombatAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetMaxHealthAttribute())
    {
        MaxValueChanged(Health, MaxHealth, NewValue, GetHealthAttribute());
    }
    else if (Attribute == GetMaxSpiritEnergyAttribute())
    {
        MaxValueChanged(SpiritEnergy, MaxSpiritEnergy, NewValue, GetSpiritEnergyAttribute());
    }
}

void UCombatAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetHealthAttribute())
    {
        // Make sure Health does not drop below 0
        SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));

        // If Health is 0, character is dead
        if (GetHealth() <= 0.0f)
        {
            
        }

    }
    else if (Data.EvaluatedData.Attribute == GetSpiritEnergyAttribute())
    {
        // Make sure SpiritEnergy does not drop below 0
        SetSpiritEnergy(FMath::Clamp(GetSpiritEnergy(), 0.0f, GetMaxSpiritEnergy()));
    }
    else if (Data.EvaluatedData.Attribute == GetAttackAttribute())
    {
        // Make sure Attack does not drop below 0
        SetAttack(FMath::Max(GetAttack(), 0.0f));
    }
    else if (Data.EvaluatedData.Attribute == GetDefenseAttribute())
    {
        // Make sure Defense does not drop below 0
        SetDefense(FMath::Max(GetDefense(), 0.0f));
    }
    else if (Data.EvaluatedData.Attribute == GetCriticalRateAttribute())
    {
        // Make sure CriticalRate does not drop below 0
        SetCriticalRate(FMath::Max(GetCriticalRate(), 0.0f));
    }
    else if (Data.EvaluatedData.Attribute == GetCriticalDamageAttribute())
    {
        // Make sure CriticalDamage does not drop below 0
        SetCriticalDamage(FMath::Max(GetCriticalDamage(), 0.0f));
    }
    else if (Data.EvaluatedData.Attribute == GetCriticalResistanceAttribute())
    {
        // Make sure CriticalResistance does not drop below 0
        SetCriticalResistance(FMath::Max(GetCriticalResistance(), 0.0f));
    }
    else if (Data.EvaluatedData.Attribute == GetShieldAttribute())
    {
        // Make sure Shield does not drop below 0
        SetShield(FMath::Max(GetShield(), 0.0f));
    }
}

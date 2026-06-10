// Copyright Ironic Studio. All Rights Reserved.


#include "Characters/Attributes/CombatAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"

void UCombatAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
    Super::PostAttributeChange(Attribute, OldValue, NewValue);

    if (Attribute == GetMaxHealthAttribute())
    {
        MaxValueChanged(GetHealthAttribute(), OldValue, NewValue);
    }
    else if (Attribute == GetMaxSpiritEnergyAttribute())
    {
        MaxValueChanged(GetSpiritEnergyAttribute(), OldValue, NewValue);
    }
}

void UCombatAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetHealthAttribute())
    {
		float OldHealth = GetHealth() - Data.EvaluatedData.Magnitude; // Calculate old health before the change

        // Make sure Health does not drop below 0
        float NewHealth = FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth());

        SetHealth(NewHealth);

        FAttributeEventContext EventContext = FAttributeEventContext::MakeContext(Data, OldHealth, NewHealth);

        if (Data.EvaluatedData.ModifierOp == EGameplayModOp::Additive)
        {
            // If damage was applied
            if (Data.EvaluatedData.Magnitude < 0.0f)
            {
				OnTakeDamage.Broadcast(EventContext);
            }
            // If healing was applied
            else if (Data.EvaluatedData.Magnitude > 0.0f)
            {
                OnHeal.Broadcast(EventContext);
            }
        }

        // If Health is 0, broadcast the OnOutOfHealth event and return early to avoid triggering hit reactions or other effects
        if (GetHealth() <= 0.0f)
        {
            OnOutOfHealth.Broadcast(EventContext);
        }
		// If Health was 0 and is now above 0, broadcast the OnRevive event
        else if (OldHealth <= 0.0f)
        {
            OnRevive.Broadcast(EventContext);
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

void UCombatAttributeSet::BindAttributeChangedDelegates(UAbilitySystemComponent* AbilitySystemComponent)
{
    if (!AbilitySystemComponent)
    {
        return;
	}

    BIND_ATTRIBUTE_CHANGE_DELEGATE(AbilitySystemComponent, UCombatAttributeSet, Health);
	BIND_ATTRIBUTE_CHANGE_DELEGATE(AbilitySystemComponent, UCombatAttributeSet, MaxHealth);
	BIND_ATTRIBUTE_CHANGE_DELEGATE(AbilitySystemComponent, UCombatAttributeSet, SpiritEnergy);
	BIND_ATTRIBUTE_CHANGE_DELEGATE(AbilitySystemComponent, UCombatAttributeSet, MaxSpiritEnergy);
	BIND_ATTRIBUTE_CHANGE_DELEGATE(AbilitySystemComponent, UCombatAttributeSet, Attack);
	BIND_ATTRIBUTE_CHANGE_DELEGATE(AbilitySystemComponent, UCombatAttributeSet, Defense);
	BIND_ATTRIBUTE_CHANGE_DELEGATE(AbilitySystemComponent, UCombatAttributeSet, CriticalRate);
	BIND_ATTRIBUTE_CHANGE_DELEGATE(AbilitySystemComponent, UCombatAttributeSet, CriticalDamage);
	BIND_ATTRIBUTE_CHANGE_DELEGATE(AbilitySystemComponent, UCombatAttributeSet, CriticalResistance);
    BIND_ATTRIBUTE_CHANGE_DELEGATE(AbilitySystemComponent, UCombatAttributeSet, Shield);
}

TSet<FGameplayAttribute> UCombatAttributeSet::GetSaveableAttributes() const
{
    static const TSet<FGameplayAttribute> SaveableAttributes = {
        GetHealthAttribute(),
        GetMaxHealthAttribute(),
        GetSpiritEnergyAttribute(),
        GetMaxSpiritEnergyAttribute(),
        GetAttackAttribute(),
        GetDefenseAttribute(),
        GetCriticalRateAttribute(),
        GetCriticalDamageAttribute(),
        GetCriticalResistanceAttribute()
	};

	return SaveableAttributes;
}

const FGameplayAttribute UCombatAttributeSet::GetMaxClampAttribute(const FGameplayAttribute& Attribute) const
{
    if (Attribute == GetHealthAttribute())
    {
        return GetMaxHealthAttribute();
    }
    else if (Attribute == GetSpiritEnergyAttribute())
    {
        return GetMaxSpiritEnergyAttribute();
    }

	return nullptr;
}


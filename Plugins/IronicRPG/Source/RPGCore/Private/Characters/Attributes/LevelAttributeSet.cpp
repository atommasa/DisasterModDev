// Copyright Ironic Studio. All Rights Reserved.


#include "Characters/Attributes/LevelAttributeSet.h"
#include "GameplayEffectExtension.h"

void ULevelAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
    Super::PostAttributeChange(Attribute, OldValue, NewValue);
}

void ULevelAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    
}

void ULevelAttributeSet::BindAttributeChangedDelegates(UAbilitySystemComponent* AbilitySystemComponent)
{
    if (!AbilitySystemComponent)
    {
        return;
    }

	BIND_ATTRIBUTE_CHANGE_DELEGATE(AbilitySystemComponent, ULevelAttributeSet, Level);
	BIND_ATTRIBUTE_CHANGE_DELEGATE(AbilitySystemComponent, ULevelAttributeSet, CurrentExp);
	BIND_ATTRIBUTE_CHANGE_DELEGATE(AbilitySystemComponent, ULevelAttributeSet, ExpToNextLevel);
}

TSet<FGameplayAttribute> ULevelAttributeSet::GetSaveableAttributes() const
{
    static const TSet<FGameplayAttribute> SaveableAttributes = {
        GetLevelAttribute(),
        GetCurrentExpAttribute(),
        GetExpToNextLevelAttribute()
	};

	return SaveableAttributes;
}

const FGameplayAttribute ULevelAttributeSet::GetMaxClampAttribute(const FGameplayAttribute& Attribute) const
{
    if (Attribute == GetCurrentExpAttribute())
    {
        return GetExpToNextLevelAttribute();
    }

	return nullptr;
}

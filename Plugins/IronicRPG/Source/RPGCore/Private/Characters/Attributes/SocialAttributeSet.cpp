// Copyright Ironic Studio. All Rights Reserved.


#include "Characters/Attributes/SocialAttributeSet.h"
#include "GameplayEffectExtension.h"

void USocialAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
    Super::PostAttributeChange(Attribute, OldValue, NewValue);

    
}

void USocialAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    
}

void USocialAttributeSet::BindAttributeChangedDelegates(UAbilitySystemComponent* AbilitySystemComponent)
{
    if (!AbilitySystemComponent)
    {
        return;
	}

	BIND_ATTRIBUTE_CHANGE_DELEGATE(AbilitySystemComponent, USocialAttributeSet, Favorability);
}

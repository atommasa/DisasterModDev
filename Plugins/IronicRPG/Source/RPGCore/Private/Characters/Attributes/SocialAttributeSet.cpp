// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Attributes/SocialAttributeSet.h"
#include "GameplayEffectExtension.h"

void USocialAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    
}

void USocialAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Attributes/LevelAttributeSet.h"
#include "GameplayEffectExtension.h"

void ULevelAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

}

void ULevelAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    
}

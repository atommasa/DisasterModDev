// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Attributes/RPGAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Characters/BaseCharacter.h"

void URPGAttributeSet::MaxValueChanged(FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty)
{
    UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
    const float CurrentMaxValue = MaxAttribute.GetCurrentValue();

    if (!FMath::IsNearlyEqual(CurrentMaxValue, NewMaxValue) && ASC)
    {
        const float CurrentValue = AffectedAttribute.GetCurrentValue();
        float NewDelta = (CurrentMaxValue > 0.0f) ? (CurrentValue * NewMaxValue / CurrentMaxValue) - CurrentValue : NewMaxValue;
        ASC->ApplyModToAttributeUnsafe(AffectedAttributeProperty, EGameplayModOp::Additive, NewDelta);
    }
}

void URPGAttributeSet::SaveAttributesTo(FCharacterSaveData& OutSaveData)
{
    for (TFieldIterator<FProperty> It(GetClass()); It; ++It)
    {
        FStructProperty* StructProperty = CastField<FStructProperty>(*It);
        if (StructProperty && StructProperty->HasMetaData(ATTRIBUTE_METATAG_SaveGame) && StructProperty->Struct == FGameplayAttributeData::StaticStruct())
        {
            const FGameplayAttributeData* AttributeData = StructProperty->ContainerPtrToValuePtr<FGameplayAttributeData>(this);
            const FGameplayAttribute Attribute(StructProperty);
            OutSaveData.Attributes.Add(Attribute, AttributeData->GetCurrentValue());
        }
    }
}

void URPGAttributeSet::LoadAttributesFrom(const FCharacterSaveData& InSaveData)
{
    for (TFieldIterator<FProperty> It(GetClass()); It; ++It)
    {
        FStructProperty* StructProperty = CastField<FStructProperty>(*It);
        if (StructProperty && StructProperty->HasMetaData(ATTRIBUTE_METATAG_SaveGame) && StructProperty->Struct == FGameplayAttributeData::StaticStruct())
        {
            FGameplayAttributeData* AttributeData = StructProperty->ContainerPtrToValuePtr<FGameplayAttributeData>(this);
            const FGameplayAttribute Attribute(StructProperty);

            if (const float* SavedValuePtr = InSaveData.Attributes.Find(Attribute))
            {
                AttributeData->SetBaseValue(*SavedValuePtr);
                AttributeData->SetCurrentValue(*SavedValuePtr);
            }
        }
    }
}

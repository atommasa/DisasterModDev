// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Attributes/CharacterAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Characters/BaseCharacter.h"
#include "SaveGame/RPGSaveGame.h"

void UCharacterAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
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

void UCharacterAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
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

void UCharacterAttributeSet::MaxValueChanged(FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty)
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

void UCharacterAttributeSet::SaveAttributesTo(FCharacterInstanceData& OutSaveData)
{
    for (TFieldIterator<FProperty> It(GetClass()); It; ++It)
    {
        FStructProperty* StructProperty = CastField<FStructProperty>(*It);
        if (StructProperty && StructProperty->HasMetaData(FName(TEXT("SaveGame"))) && StructProperty->Struct == FGameplayAttributeData::StaticStruct())
        {
            const FGameplayAttributeData* AttributeData = StructProperty->ContainerPtrToValuePtr<FGameplayAttributeData>(this);
            const FString AttributeName = StructProperty->GetName();
            OutSaveData.Attributes.Add(AttributeName, AttributeData->GetCurrentValue());
        }
    }
}

void UCharacterAttributeSet::LoadAttributesFrom(const FCharacterInstanceData& InSaveData)
{
    for (TFieldIterator<FProperty> It(GetClass()); It; ++It)
    {
        FStructProperty* StructProperty = CastField<FStructProperty>(*It);
        if (StructProperty && StructProperty->HasMetaData(FName(TEXT("SaveGame"))) && StructProperty->Struct == FGameplayAttributeData::StaticStruct())
        {
            FGameplayAttributeData* AttributeData = StructProperty->ContainerPtrToValuePtr<FGameplayAttributeData>(this);
            const FString AttributeName = StructProperty->GetName();

            if (const float* SavedValue = InSaveData.Attributes.Find(AttributeName))
            {
                AttributeData->SetBaseValue(*SavedValue);
                AttributeData->SetCurrentValue(*SavedValue);
            }
        }
    }
}

// Copyright Ironic Studio. All Rights Reserved.


#include "Characters/Attributes/RPGAttributeSet.h"
#include "GameplayEffectExtension.h"

void URPGAttributeSet::MaxValueChanged(const FGameplayAttribute& CurrentAttribute, float OldMaxValue, float NewMaxValue)
{
    if (bIsInitializing)
    {
        return;
    }

    UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
    if (!ASC || OldMaxValue <= 0.0f)
    {
        return;
    }

    const float CurrentValue = ASC->GetNumericAttribute(CurrentAttribute);
    const float NewCurrentValue = CurrentValue * NewMaxValue / OldMaxValue;
    const float Delta = NewCurrentValue - CurrentValue;

    UE_LOG(LogTemp, Display,
        TEXT("[Before Apply] %s CurrentValue=%f OldMax=%f NewMax=%f NewCurrent=%f Delta=%f"),
        *CurrentAttribute.GetName(),
        CurrentValue,
        OldMaxValue,
        NewMaxValue,
        NewCurrentValue,
        Delta
    );

    ASC->ApplyModToAttributeUnsafe(
        CurrentAttribute,
        EGameplayModOp::Additive,
        Delta
    );

    const float AfterValue = ASC->GetNumericAttribute(CurrentAttribute);

    UE_LOG(LogTemp, Display,
        TEXT("[After Apply] %s AfterValue=%f"),
        *CurrentAttribute.GetName(),
        AfterValue
    );
}

void URPGAttributeSet::GetSaveableAttributes(TSet<FGameplayAttribute>& OutSet) const
{
    OutSet.Reset();

    for (TFieldIterator<FProperty> PropIt(GetClass(), EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;

        if (!Property->HasAnyPropertyFlags(CPF_SaveGame))
        {
            continue;
        }

        if (!FGameplayAttribute::IsGameplayAttributeDataProperty(Property))
        {
            continue;
        }

        OutSet.Add(FGameplayAttribute(Property));
    }
}

void URPGAttributeSet::SaveAttributesTo(FCharacterSaveData& OutSaveData) const
{
    TSet<FGameplayAttribute> Saveables;
    GetSaveableAttributes(Saveables);

    for (const FGameplayAttribute& Attribute : Saveables)
    {
        OutSaveData.Attributes.Add(Attribute, Attribute.GetNumericValue(this));
    }
}

void URPGAttributeSet::LoadAttributesFrom(const FCharacterSaveData& InSaveData)
{
    bIsInitializing = true;

    TSet<FGameplayAttribute> Saveables;
    GetSaveableAttributes(Saveables);

    for (const FGameplayAttribute& Attribute : Saveables)
    {
        if (!Attribute.IsValid())
        {
            continue;
        }

        const float* SavedValuePtr = InSaveData.Attributes.Find(Attribute);
        if (!SavedValuePtr)
        {
            continue;
        }

        FProperty* Property = Attribute.GetUProperty();
        FStructProperty* StructProperty = CastField<FStructProperty>(Property);

        if (!StructProperty || StructProperty->Struct != FGameplayAttributeData::StaticStruct())
        {
            continue;
        }

        FGameplayAttributeData* AttributeData = StructProperty->ContainerPtrToValuePtr<FGameplayAttributeData>(this);

        if (!AttributeData)
        {
            continue;
        }

        const float SavedValue = *SavedValuePtr;

        AttributeData->SetBaseValue(SavedValue);
        AttributeData->SetCurrentValue(SavedValue);

        UE_LOG(LogTemp, Display,
            TEXT("[LoadAttributesFrom] %s Base=%f Current=%f"),
            *Attribute.GetName(),
            AttributeData->GetBaseValue(),
            AttributeData->GetCurrentValue()
        );
    }

    bIsInitializing = false;
}

bool URPGAttributeSet::HasMaxClampAttribute(const UAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute, float& OutMaxValue)
{
    if (!ASC)
    {
		return false;
    }

	OutMaxValue = 0.0f;

    UClass* AttributeSetClass = Attribute.GetAttributeSetClass();

    const URPGAttributeSet* AttributeSet = Cast<URPGAttributeSet>(ASC->GetAttributeSet(AttributeSetClass));
    if (AttributeSet)
    {
        const FGameplayAttribute& MaxAttribute = GetMaxClampAttributeFor(Attribute);
        OutMaxValue = MaxAttribute.GetNumericValue(AttributeSet);
        return true;
    }

	return false;
}

FGameplayAttribute URPGAttributeSet::GetMaxClampAttributeFor(const FGameplayAttribute& Attribute)
{
    UClass* AttributeSetClass = Attribute.GetAttributeSetClass();

    return AttributeSetClass->GetDefaultObject<URPGAttributeSet>()->GetMaxClampAttribute(Attribute);
}

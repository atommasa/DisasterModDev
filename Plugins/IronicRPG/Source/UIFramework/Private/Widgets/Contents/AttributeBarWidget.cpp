// Copyright Ironic Studio. All Rights Reserved.


#include "Widgets/Contents/AttributeBarWidget.h"
#include "AbilitySystemBlueprintLibrary.h"

void UAttributeBarWidget::SetDisplayAttribute(AActor* InActor, FGameplayAttribute InCurrentAttribute, FGameplayAttribute InMaxAttribute)
{
    if (!InActor)
    {
        return;
    }

    UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InActor);

    SetAbilitySystemComponent(ASC, InCurrentAttribute, InMaxAttribute);
}

void UAttributeBarWidget::SetAbilitySystemComponent(UAbilitySystemComponent* InASC, FGameplayAttribute InCurrentAttribute, FGameplayAttribute InMaxAttribute)
{
    UnbindAttributeDelegates();

    AbilitySystemComponent = InASC;
    CurrentAttribute = InCurrentAttribute;
    MaxAttribute = InMaxAttribute;

    BindAttributeDelegates();
    RefreshAttributeBar();
}

void UAttributeBarWidget::NativeDestruct()
{
    UnbindAttributeDelegates();

    Super::NativeDestruct();
}

void UAttributeBarWidget::BindAttributeDelegates()
{
    if (!AbilitySystemComponent)
    {
        return;
    }

    if (CurrentAttribute.IsValid())
    {
        CurrentAttributeChangedHandle =
            AbilitySystemComponent
            ->GetGameplayAttributeValueChangeDelegate(CurrentAttribute)
            .AddUObject(this, &UAttributeBarWidget::HandleCurrentAttributeChanged);
    }

    if (MaxAttribute.IsValid() && MaxAttribute != CurrentAttribute)
    {
        MaxAttributeChangedHandle =
            AbilitySystemComponent
            ->GetGameplayAttributeValueChangeDelegate(MaxAttribute)
            .AddUObject(this, &UAttributeBarWidget::HandleMaxAttributeChanged);
    }
}

void UAttributeBarWidget::UnbindAttributeDelegates()
{
    if (!AbilitySystemComponent)
    {
        CurrentAttributeChangedHandle.Reset();
        MaxAttributeChangedHandle.Reset();
        return;
    }

    if (CurrentAttributeChangedHandle.IsValid() && CurrentAttribute.IsValid())
    {
        AbilitySystemComponent
            ->GetGameplayAttributeValueChangeDelegate(CurrentAttribute)
            .Remove(CurrentAttributeChangedHandle);
    }

    if (MaxAttributeChangedHandle.IsValid() && MaxAttribute.IsValid())
    {
        AbilitySystemComponent
            ->GetGameplayAttributeValueChangeDelegate(MaxAttribute)
            .Remove(MaxAttributeChangedHandle);
    }

    CurrentAttributeChangedHandle.Reset();
    MaxAttributeChangedHandle.Reset();
}

void UAttributeBarWidget::HandleCurrentAttributeChanged(const FOnAttributeChangeData& Data)
{
    CurrentValue = Data.NewValue;
    RefreshAttributeBar();
}

void UAttributeBarWidget::HandleMaxAttributeChanged(const FOnAttributeChangeData& Data)
{
    MaxValue = Data.NewValue;
    RefreshAttributeBar();
}

void UAttributeBarWidget::RefreshAttributeBar()
{
    if (!AbilitySystemComponent)
    {
        return;
    }

    if (CurrentAttribute.IsValid())
    {
        CurrentValue = AbilitySystemComponent->GetNumericAttribute(CurrentAttribute);
    }

    if (MaxAttribute.IsValid())
    {
        MaxValue = AbilitySystemComponent->GetNumericAttribute(MaxAttribute);
    }

    Percent = MaxValue > 0.0f ? FMath::Clamp(CurrentValue / MaxValue, 0.0f, 1.0f) : 0.0f;

    OnAttributeBarChanged.Broadcast(CurrentValue, MaxValue, Percent);
    BP_OnAttributeBarChanged(CurrentValue, MaxValue, Percent);
}

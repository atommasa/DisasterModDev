// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AbilitySystemComponent.h"
#include "AttributeBarWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOnAttributeBarChanged,
    float, CurrentValue,
    float, MaxValue,
    float, Percent
);

/**
 * 
 */
UCLASS()
class UIFRAMEWORK_API UAttributeBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Attribute Bar")
    void SetDisplayAttribute(
        AActor* InActor,
        FGameplayAttribute InCurrentAttribute,
        FGameplayAttribute InMaxAttribute
    );

    UFUNCTION(BlueprintCallable, Category = "Attribute Bar")
    void SetAbilitySystemComponent(
        UAbilitySystemComponent* InASC,
        FGameplayAttribute InCurrentAttribute,
        FGameplayAttribute InMaxAttribute
    );

    UPROPERTY(BlueprintAssignable, Category = "Attribute Bar")
    FOnAttributeBarChanged OnAttributeBarChanged;

protected:
    virtual void NativeDestruct() override;

    UPROPERTY(BlueprintReadOnly, Category = "Attribute Bar")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute Bar")
    FGameplayAttribute CurrentAttribute;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute Bar")
    FGameplayAttribute MaxAttribute;

    UPROPERTY(BlueprintReadOnly, Category = "Attribute Bar")
    float CurrentValue = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Attribute Bar")
    float MaxValue = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Attribute Bar")
    float Percent = 0.0f;

    UFUNCTION(BlueprintImplementableEvent, Category = "Attribute Bar")
    void BP_OnAttributeBarChanged(float NewCurrentValue, float NewMaxValue, float NewPercent);

private:
    FDelegateHandle CurrentAttributeChangedHandle;
    FDelegateHandle MaxAttributeChangedHandle;

    void BindAttributeDelegates();
    void UnbindAttributeDelegates();

    void HandleCurrentAttributeChanged(const FOnAttributeChangeData& Data);
    void HandleMaxAttributeChanged(const FOnAttributeChangeData& Data);

    void RefreshAttributeBar();
};

// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/Attributes/RPGAttributeSet.h"
#include "SocialAttributeSet.generated.h"

/**
 *
 */
UCLASS()
class RPGCORE_API USocialAttributeSet : public URPGAttributeSet
{
	GENERATED_BODY()

protected:
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	virtual void BindAttributeChangedDelegates(UAbilitySystemComponent* AbilitySystemComponent) override;
	virtual TSet<FGameplayAttribute> GetSaveableAttributes() const override;

public:
	UPROPERTY(BlueprintReadOnly, Category = "Attributes | Favorability")
	FGameplayAttributeData Favorability = 0.0f;
	ATTRIBUTE_ACCESSORS(USocialAttributeSet, Favorability);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Attributes | Favorability")
	FOnAttributeChangedSignature OnFavorabilityChanged;

};


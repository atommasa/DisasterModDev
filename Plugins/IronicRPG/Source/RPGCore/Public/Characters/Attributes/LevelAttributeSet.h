// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/Attributes/RPGAttributeSet.h"
#include "LevelAttributeSet.generated.h"

/**
 * 
 */
UCLASS()
class RPGCORE_API ULevelAttributeSet : public URPGAttributeSet
{
	GENERATED_BODY()

protected:
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	virtual void BindAttributeChangedDelegates(UAbilitySystemComponent* AbilitySystemComponent) override;
	virtual TSet<FGameplayAttribute> GetSaveableAttributes() const override;
	virtual const FGameplayAttribute GetMaxClampAttribute(const FGameplayAttribute& Attribute) const override;

public: // Level Attributes
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Level")
	FGameplayAttributeData Level = 1.0f;
	ATTRIBUTE_ACCESSORS(ULevelAttributeSet, Level);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Attributes|Level")
	FOnAttributeChangedSignature OnLevelChanged;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Level")
	FGameplayAttributeData CurrentExp = 1.0f;
	ATTRIBUTE_ACCESSORS(ULevelAttributeSet, CurrentExp);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Attributes|Level")
	FOnAttributeChangedSignature OnCurrentExpChanged;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Level")
	FGameplayAttributeData ExpToNextLevel = 1.0f;
	ATTRIBUTE_ACCESSORS(ULevelAttributeSet, ExpToNextLevel);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Attributes|Level")
	FOnAttributeChangedSignature OnExpToNextLevelChanged;

};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "RPGAttributeSet.generated.h"

#define ATTRIBUTE_METATAG_SaveGame "SaveGame"
#define ATTRIBUTE_METATAG_AttributeClampMax "AttributeClampMax"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 *
 */
UCLASS(abstract)
class RPGCORE_API URPGAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

protected:
	void MaxValueChanged(FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, float NewValue, const FGameplayAttribute& AffectedAttributeProperty);

public: // Save Game Functions
	UFUNCTION(BlueprintCallable, Category = "Save Game")
	virtual void SaveAttributesTo(FCharacterSaveData& OutSaveData);

	UFUNCTION(BlueprintCallable, Category = "Save Game")
	virtual void LoadAttributesFrom(const FCharacterSaveData& InSaveData);
};
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "CharacterAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * 
 */
UCLASS()
class RPGCORE_API UCharacterAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	void MaxValueChanged(FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, float NewValue, const FGameplayAttribute& AffectedAttributeProperty);

public: // Level Attributes
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Level", meta = (SaveGame = true))
	FGameplayAttributeData Level = 1.0f;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Level)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Level", meta = (SaveGame = true))
	FGameplayAttributeData CurrentExp = 1.0f;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, CurrentExp)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Level", meta = (SaveGame = true))
	FGameplayAttributeData ExpToNextLevel = 1.0f;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, ExpToNextLevel)

public: // Combat Attributes
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Health", meta = (SaveGame = true))
	FGameplayAttributeData Health = 1.0f;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Health", meta = (SaveGame = true))
	FGameplayAttributeData MaxHealth = 1.0f;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|SpiritEnergy", meta = (SaveGame = true))
	FGameplayAttributeData SpiritEnergy = 1.0f;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, SpiritEnergy)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|SpiritEnergy", meta = (SaveGame = true))
	FGameplayAttributeData MaxSpiritEnergy = 1.0f;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, MaxSpiritEnergy)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Attack", meta = (SaveGame = true))
	FGameplayAttributeData Attack = 1.0f;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Attack)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Defense", meta = (SaveGame = true))
	FGameplayAttributeData Defense = 1.0f;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Defense)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|CritialDamage", meta = (SaveGame = true))
	FGameplayAttributeData CriticalRate = 1.0f;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, CriticalRate)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|CritialDamage", meta = (SaveGame = true))
	FGameplayAttributeData CriticalDamage = 1.0f;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, CriticalDamage)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|CritialDamage", meta = (SaveGame = true))
	FGameplayAttributeData CriticalResistance = 1.0f;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, CriticalResistance)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Shield")
	FGameplayAttributeData Shield = 1.0f;
	ATTRIBUTE_ACCESSORS(UCharacterAttributeSet, Shield)

public: // Save Game Functions
	UFUNCTION(BlueprintCallable, Category = "Save Game")
	virtual void SaveAttributesTo(FCharacterInstanceData& OutSaveData);

	UFUNCTION(BlueprintCallable, Category = "Save Game")
	virtual void LoadAttributesFrom(const FCharacterInstanceData& InSaveData);

};

/**
 *
 */
UCLASS()
class RPGCORE_API UPlayerCharacterAttributeSet : public UCharacterAttributeSet
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Attributes | Favorability", meta = (SaveGame = true))
	FGameplayAttributeData Favorability = 1.0f;
	ATTRIBUTE_ACCESSORS(UPlayerCharacterAttributeSet, Favorability)

};

/**
 *
 */
UCLASS()
class RPGCORE_API UEnemyCharacterAttributeSet : public UCharacterAttributeSet
{
	GENERATED_BODY()



};

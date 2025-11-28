// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/Attributes/RPGAttributeSet.h"
#include "CombatAttributeSet.generated.h"

/**
 * 
 */
UCLASS()
class RPGCORE_API UCombatAttributeSet : public URPGAttributeSet
{
	GENERATED_BODY()

protected:
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

public: // Combat Attributes
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Health", meta=(SaveGame, AttributeClampMax = "MaxHealth"))
	FGameplayAttributeData Health = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Health", meta=(SaveGame))
	FGameplayAttributeData MaxHealth = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|SpiritEnergy", meta=(SaveGame, AttributeClampMax = "MaxSpiritEnergy"))
	FGameplayAttributeData SpiritEnergy = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, SpiritEnergy)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|SpiritEnergy", meta=(SaveGame))
	FGameplayAttributeData MaxSpiritEnergy = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, MaxSpiritEnergy)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Attack", meta=(SaveGame))
	FGameplayAttributeData Attack = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Attack)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Defense", meta=(SaveGame))
	FGameplayAttributeData Defense = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Defense)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|CritialDamage", meta=(SaveGame))
	FGameplayAttributeData CriticalRate = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, CriticalRate)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|CritialDamage", meta=(SaveGame))
	FGameplayAttributeData CriticalDamage = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, CriticalDamage)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|CritialDamage", meta=(SaveGame))
	FGameplayAttributeData CriticalResistance = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, CriticalResistance)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Shield")
	FGameplayAttributeData Shield = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Shield)

};

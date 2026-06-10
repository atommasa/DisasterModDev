// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/Attributes/RPGAttributeSet.h"
#include "CombatAttributeSet.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnOutOfHealth, const FAttributeEventContext&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRevive, const FAttributeEventContext&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTakeDamage, const FAttributeEventContext&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHeal, const FAttributeEventContext&);

/**
 * The CombatAttributeSet class defines combat-related attributes for RPG characters, such as Health, Spirit Energy, Attack, Defense, and Critical Damage.
 */
UCLASS()
class RPGCORE_API UCombatAttributeSet : public URPGAttributeSet
{
	GENERATED_BODY()

protected:
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	virtual void BindAttributeChangedDelegates(UAbilitySystemComponent* AbilitySystemComponent) override;
	virtual TSet<FGameplayAttribute> GetSaveableAttributes() const override;
	virtual const FGameplayAttribute GetMaxClampAttribute(const FGameplayAttribute& Attribute) const override;

public: // Combat Attributes
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Health")
	FGameplayAttributeData Health = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Health);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Attributes|Health")
	FOnAttributeChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Health")
	FGameplayAttributeData MaxHealth = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, MaxHealth);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Attributes|Health")
	FOnAttributeChangedSignature OnMaxHealthChanged;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|SpiritEnergy")
	FGameplayAttributeData SpiritEnergy = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, SpiritEnergy);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Attributes|SpiritEnergy")
	FOnAttributeChangedSignature OnSpiritEnergyChanged;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|SpiritEnergy")
	FGameplayAttributeData MaxSpiritEnergy = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, MaxSpiritEnergy);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Attributes|SpiritEnergy")
	FOnAttributeChangedSignature OnMaxSpiritEnergyChanged;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Attack")
	FGameplayAttributeData Attack = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Attack);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Attributes|Attack")
	FOnAttributeChangedSignature OnAttackChanged;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Defense")
	FGameplayAttributeData Defense = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Defense);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Attributes|Defense")
	FOnAttributeChangedSignature OnDefenseChanged;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|CritialDamage")
	FGameplayAttributeData CriticalRate = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, CriticalRate);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Attributes|CritialDamage")
	FOnAttributeChangedSignature OnCriticalRateChanged;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|CritialDamage")
	FGameplayAttributeData CriticalDamage = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, CriticalDamage);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Attributes|CritialDamage")
	FOnAttributeChangedSignature OnCriticalDamageChanged;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|CritialDamage")
	FGameplayAttributeData CriticalResistance = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, CriticalResistance);

	UPROPERTY(BlueprintAssignable, Category = "Attributes|CritialDamage")
	FOnAttributeChangedSignature OnCriticalResistanceChanged;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Shield")
	FGameplayAttributeData Shield = 1.0f;
	ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Shield);

	UPROPERTY(BlueprintAssignable, Category = "Attributes|Shield")
	FOnAttributeChangedSignature OnShieldChanged;

public:
	// Delegate for when Health reaches zero or below (it does not necessarily mean the character is dead, as there could be effects that prevent death or trigger at zero health)
	FOnOutOfHealth OnOutOfHealth;

	// Delegate for when the character is revived (e.g., Health goes from 0 to above 0)
	FOnRevive OnRevive;

	// Delegate for when damage is taken
	FOnTakeDamage OnTakeDamage;

	// Delegate for when healing is applied
	FOnHeal OnHeal;
};

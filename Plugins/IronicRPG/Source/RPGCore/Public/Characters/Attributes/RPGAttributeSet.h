// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "Characters/CharacterDataTypes.h"
#include "RPGAttributeSet.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttributeChangedSignature, float, NewMaxValue);

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

#define BIND_ATTRIBUTE_CHANGE_DELEGATE(AbilitySystemComponent, AttributeSetClass, PropertyName) \
AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSetClass::Get##PropertyName##Attribute()).AddLambda([this](const FOnAttributeChangeData& Data) {On##PropertyName##Changed.Broadcast(Data.NewValue);});

USTRUCT(BlueprintType)
struct FAttributeEventContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> SourceActor = nullptr;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(BlueprintReadOnly)
	float OldValue = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float NewValue = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float Delta = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float Magnitude = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	FGameplayTagContainer SourceTags;

	UPROPERTY(BlueprintReadOnly)
	FGameplayTagContainer TargetTags;

	FGameplayEventData ToGameplayEventData() const
	{
		FGameplayEventData EventData;
		EventData.Instigator = SourceActor;
		EventData.Target = TargetActor;
		EventData.EventMagnitude = Magnitude;
		EventData.InstigatorTags = SourceTags;
		EventData.TargetTags = TargetTags;
		return EventData;
	}

	static FAttributeEventContext MakeContext(const FGameplayEffectModCallbackData& Data, const float& OldValue, const float& NewValue)
	{
		AActor* TargetActor = Data.Target.GetAvatarActor();
		const FGameplayEffectSpec& Spec = Data.EffectSpec;
		const FGameplayEffectContextHandle& Context = Spec.GetEffectContext();

		FAttributeEventContext EventContext;
		EventContext.SourceActor = Context.GetInstigator();
		EventContext.TargetActor = TargetActor;
		EventContext.OldValue = OldValue;
		EventContext.NewValue = NewValue;
		EventContext.Delta = NewValue - OldValue;
		EventContext.Magnitude = Data.EvaluatedData.Magnitude;
		Context.GetOwnedGameplayTags(EventContext.SourceTags, EventContext.TargetTags);

		return EventContext;
	}
};

/**
 *
 */
UCLASS(abstract)
class RPGCORE_API URPGAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

protected:
	void MaxValueChanged(const FGameplayAttribute& CurrentAttribute, float OldMaxValue, float NewMaxValue);

public: // AttributeSet Overrides
	virtual void BindAttributeChangedDelegates(UAbilitySystemComponent* AbilitySystemComponent) {}
	virtual const FGameplayAttribute GetMaxClampAttribute(const FGameplayAttribute& Attribute) const { return nullptr; }

	void GetSaveableAttributes(TSet<FGameplayAttribute>& OutSet) const;

public: // Save Game Functions
	// Save data structure for character attributes
	UFUNCTION(BlueprintCallable, Category = "Save Game")
	virtual void SaveAttributesTo(FCharacterSaveData& OutSaveData) const;

	// Load data structure for character attributes
	UFUNCTION(BlueprintCallable, Category = "Save Game")
	virtual void LoadAttributesFrom(const FCharacterSaveData& InSaveData);



private:
	bool bIsInitializing = false;

public: // Attribute Utility Functions
	// Check if an attribute has a max clamp defined via metadata
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	static bool HasMaxClampAttribute(const UAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute, float& OutMaxValue);
	
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	static FGameplayAttribute GetMaxClampAttributeFor(const FGameplayAttribute& Attribute);

};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "DataTypes/RPGId.h"
#include "CharacterAbilitySystemComponent.generated.h"

/**
 * This class extends the UAbilitySystemComponent to provide additional functionality specific to character abilities in the RPG game.
 */
UCLASS()
class RPGCORE_API URPGAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	URPGAbilitySystemComponent(const FObjectInitializer& ObjectInitializer);

public:
	UFUNCTION(BlueprintCallable, Category = "Character")
	class ABaseCharacter* GetBaseCharacterOwner() const;

public:
	// Learns a new ability for the character using the provided AbilityId
	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual void LearnAbility(const FRPGId& AbilityId, int32 Level = 1);

	// Learns a new ability for the character using the provided AbilityAsset
	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual void LearnAbilityByAsset(class UAbilityAsset* AbilityAsset, int32 Level = 1);

	// Unlearns an ability for the character using the provided AbilityId
	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual void UnlearnAbility(const FRPGId& AbilityId);

	// Checks if the character has learned the ability with the provided AbilityId
	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual bool HasLearnedAbility(const FRPGId& AbilityId) const;

	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual bool HasLearnedAbilityWithTags(const FGameplayTagContainer& AbilityTags) const;

protected:
	virtual UGameplayAbility* CreateNewInstanceOfAbility(FGameplayAbilitySpec& Spec, const UGameplayAbility* Ability) override;

};

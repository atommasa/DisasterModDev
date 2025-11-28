// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/AbilityDataTypes.h"
#include "RPGGameplayAbility.generated.h"

class UAbilityAsset;

/**
 * The RPGGameplayAbility class extends the UGameplayAbility to provide additional functionality specific to RPG abilities.
 */
UCLASS()
class RPGCORE_API URPGGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	// Initializes the ability from an ability asset
	void InitAbilityFrom(const UAbilityAsset* InAbilityAsset);

public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

public:
	// 
	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	// 
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	virtual UGameplayEffect* GetCostGameplayEffect() const override;

public:
	UFUNCTION(BlueprintCallable, Category = "Animations")
	int32 GetRequiredAnimMontageCount() const { return RequiredAnimMontageCount; }

	UFUNCTION(BlueprintCallable, Category = "Animations")
	FAnimMontageData GetAnimMontageDataByIndex(int32 Index) const;

protected:
	// Number of animation montages that require handling in this ability
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animations", meta=(ClampMin = 0))
	int32 RequiredAnimMontageCount = 0;

	// Animation montage data associated with this ability
	UPROPERTY(BlueprintReadOnly, Category = "Animations")
	TArray<FAnimMontageData> AnimMontageData;

	static const FName AbilityMontageTaskName;
	
protected:
	// The ability cost specification loaded from the asset
	UPROPERTY(BlueprintReadOnly, Category = "Costs")
	FAbilityCostSpec AbilityCostSpec;

protected:
	// Whether the ability allows movement while casting (loaded from asset)
	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	bool bCanMoveWhileCasting = false;

};

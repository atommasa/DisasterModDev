// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/AbilityDataTypes.h"
#include "Abilities/AbilityAsset.h"
#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"
#include "RPGGameplayAbility.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Cooldown)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Data_EffectSpecIndex)

/**
 * The RPGGameplayAbility class extends the UGameplayAbility to provide additional functionality specific to RPG abilities.
 */
UCLASS()
class RPGCORE_API URPGGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

	friend class UAbilityAsset;

public:
	// Initializes the ability from an ability asset
	void InitAbilityFrom(const UAbilityAsset* InAbilityAsset);

	void PreLoadRequiredAnimations() const;

public:
	URPGGameplayAbility(const FObjectInitializer& ObjectInitializer);

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

public:
	// 
	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	// 
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	virtual UGameplayEffect* GetCooldownGameplayEffect() const override;

	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	virtual UGameplayEffect* GetCostGameplayEffect() const override;

protected:
	virtual void ApplyCostEntries(UAbilitySystemComponent* ASC, UGameplayEffect* CostEffect, float Level) const;

	virtual void ApplyEffectToTarget(int32 EffectSpecIndex, FAbilityEffectSpec& EffectSpec, const FGameplayAbilityTargetDataHandle& TargetDataHandle) const;

	void StartResolveTargets(EEffectApplyPolicy ApplyPolicy, const FGameplayTag* EffectTagPtr = nullptr);

	UFUNCTION()
	virtual void OnStartEventReceived(FGameplayEventData Payload);

	UFUNCTION()
	virtual void OnEndEventReceived(FGameplayEventData Payload);

	UFUNCTION()
	virtual void OnTargetResolved(UAbilityTargetResolver* Resolver, const FGameplayAbilityTargetDataHandle& TargetDataHandle);

public:
	UFUNCTION(BlueprintCallable, Category = "Ability")
	const UAbilityAsset* GetAbilityAsset() const { return AbilityAsset; }

	UFUNCTION(BlueprintCallable, Category = "Ability")
	FRPGId GetAbilityId() const { return AbilityAsset->GetId(); }

	UFUNCTION(BlueprintCallable, Category = "Animations")
	int32 GetRequiredMontageCount() const { return RequiredMontageCount; }

protected:
	// The unique identifier for this ability
	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	TObjectPtr<const UAbilityAsset> AbilityAsset = nullptr;

	// The number of required animation montages for this ability
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animations", meta=(ClampMin = 0))
	int32 RequiredMontageCount;

	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	TArray<FAbilityEffectSpec> EffectSpecs;
	
	// The ability cost specification loaded from the asset
	UPROPERTY(BlueprintReadOnly, Category = "Cost")
	FAbilityCostSpec AbilityCostSpec;

	// Whether the ability allows movement while casting (loaded from asset)
	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	bool bCanMoveWhileCasting = false;
	
	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	mutable TObjectPtr<UGameplayEffect> CooldownGameplayEffect = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	mutable TObjectPtr<UGameplayEffect> CostGameplayEffect = nullptr;

	mutable bool bHasApplyCostEntries = false;

};

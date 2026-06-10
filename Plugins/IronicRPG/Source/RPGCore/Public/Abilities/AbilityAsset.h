// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Assets/RPGPrimaryAsset.h"
#include "DataTypes/RPGId.h"
#include "Abilities/GameplayAbility.h"
#include "AbilityDataTypes.h"
#include "Misc/SynchronousTableMaster.h"
#include "AbilityAsset.generated.h"

class URPGGameplayAbility;

/**
 * The Ability Asset class represents a skill ability in the RPG game, including its properties and default data.
 */
UCLASS()
class RPGCORE_API UAbilityAsset : public URPGPrimaryAsset
{
	GENERATED_BODY()
	DEFINE_ASSET_TYPE(Ability, a)
	DEFINE_ASSET_BUNDLES("Ability")

public:
	UAbilityAsset(const FObjectInitializer& ObjectInitializer);
	~UAbilityAsset();

protected:
	// -----------------------------
	//	Gameplay Ability Properties
	// -----------------------------

	// The class of the gameplay ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta=(AssetBundles = "Ability"))
	TSoftClassPtr<URPGGameplayAbility> AbilityClass;
	ASSET_PROP_GETTER(TSoftClassPtr<URPGGameplayAbility>, AbilityClass)

	// 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	TArray<FAbilityEffectSpec> EffectSpecs;
	ASSET_PROP_GETTER(TArray<FAbilityEffectSpec>, EffectSpecs)

	// The default cost specification for the ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cost")
	FAbilityCostSpec DefaultCostSpec;
	ASSET_PROP_GETTER(FAbilityCostSpec, DefaultCostSpec)

	// The animation tags associated with the ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation", EditFixedSize)
	TArray<FGameplayTagContainer> RequireAnimTagContainers;
	ASSET_PROP_GETTER(TArray<FGameplayTagContainer>, RequireAnimTagContainers)

	// The characters that are allowed to use this ability or learn it.
	// If empty, all characters can use it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta=(IdType = "Character"))
	TSet<FRPGId> AllowedCharacters;
	ASSET_PROP_GETTER(TSet<FRPGId>, AllowedCharacters)

	// Can move while casting this ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	bool bCanMoveWhileCasting = false;
	ASSET_PROP_GETTER(bool, bCanMoveWhileCasting)

	// Can be interrupted while casting this ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	bool bCanBeInterrupted = true;
	ASSET_PROP_GETTER(bool, bCanBeInterrupted)

	// -----------------------------
	//	Tag Properties
	// -----------------------------

	// The gameplay tags associated with this ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer AbilityTags;
	ASSET_PROP_GETTER(FGameplayTagContainer, AbilityTags)

	// The gameplay tags that will cancel other abilities with these tags when this ability is activated.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer CancelWithTags;
	ASSET_PROP_GETTER(FGameplayTagContainer, CancelWithTags)

	// The gameplay tags that will block other abilities with these tags when this ability is active.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer BlockWithTags;
	ASSET_PROP_GETTER(FGameplayTagContainer, BlockWithTags)

	// The gameplay tags required to activate this ability, extending for the gameplay ability's own activation tags.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer ActivationRequiredTags;
	ASSET_PROP_GETTER(FGameplayTagContainer, ActivationRequiredTags)

	// The gameplay tags that will block the activation of this ability, extending for the gameplay ability's own blocking tags.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer ActivationBlockedTags;
	ASSET_PROP_GETTER(FGameplayTagContainer, ActivationBlockedTags)

	// The gameplay tags applied to the owner when this ability is active.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer ActivationOwnedTags;
	ASSET_PROP_GETTER(FGameplayTagContainer, ActivationOwnedTags)

	// The gameplay tags required on the source to activate this ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer SourceRequiredTags;
	ASSET_PROP_GETTER(FGameplayTagContainer, SourceRequiredTags)

	//  The gameplay tags blocked on the source to activate this ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer SourceBlockedTags;
	ASSET_PROP_GETTER(FGameplayTagContainer, SourceBlockedTags)

	// The gameplay tags required on the target to activate this ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer TargetRequiredTags;
	ASSET_PROP_GETTER(FGameplayTagContainer, TargetRequiredTags)

	// The gameplay tags blocked on the target to activate this ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer TargetBlockedTags;
	ASSET_PROP_GETTER(FGameplayTagContainer, TargetBlockedTags)

	// -----------------------------
	//	UI Properties
	// -----------------------------

	// The icon representing the ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI", meta=(AssetBundles = "UI"))
	TSoftObjectPtr<UTexture2D> Icon;
	ASSET_PROP_GETTER(TSoftObjectPtr<UTexture2D>, Icon)

#if WITH_EDITOR
protected:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

private:
	void OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event);
#endif // WITH_EDITOR
};
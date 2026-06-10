// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "DataTypes/RPGId.h"
#include "Abilities/AbilityDataTypes.h"
#include "Engine/StreamableManager.h"
#include "CharacterAbilitySystemComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLearnedAbility, const FAbilityData&, AbilityData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLearnedAbilities, const TArray<FAbilityData>&, LearnedAbilities);

/**
 * This class extends the UAbilitySystemComponent to provide additional functionality specific to character abilities in the RPG game.
 */
UCLASS()
class RPGCORE_API URPGAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	URPGAbilitySystemComponent(const FObjectInitializer& ObjectInitializer);

	virtual void AbilityLocalInputPressed(int32 InputID) override;
	virtual void AbilityLocalInputReleased(int32 InputID) override;

public:
	// Returns the owning ABaseCharacter of this Ability System Component
	UFUNCTION(BlueprintCallable, Category = "Character")
	class ABaseCharacter* GetBaseCharacterOwner() const;

	// Get all learned abilities
	UFUNCTION(BlueprintCallable, Category = "Ability")
	const TMap<FRPGId, FAbilityData>& GetLearnedAbilities() const { return LearnedAbilities; }

	// Learns a new ability for the character using the provided AbilityId
	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual void LearnAbility(const FAbilityData& AbilityData);
	virtual void LearnAbility(const FAbilityData& AbilityData, TFunction<void(const FAbilityData&)> OnLearnedCallback);

	// Learns multiple new abilities for the character using the provided array of AbilityData
	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual void LearnAbilities(const TArray<FAbilityData>& AbilitiesData);
	virtual void LearnAbilities(const TArray<FAbilityData>& AbilitiesData, TFunction<void(const TArray<FAbilityData>&)> OnLearnedCallback);

	// Unlearns an ability for the character using the provided AbilityId
	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual void UnlearnAbility(UPARAM(meta=(IdType = "Ability")) const FRPGId& AbilityId);

	// Unlearns all abilities learned by the character
	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual void UnlearnAllAbilities();

	// Checks if the character has learned the ability with the provided AbilityId
	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual bool HasLearnedAbility(const FRPGId& AbilityId) const;

	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual bool HasLearnedAbilityWithTags(const FGameplayTagContainer& AbilityTags) const;

	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual bool TryActivateAbilityById(const FRPGId& AbilityId, bool bAllowRemoteActivation = true);

	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual void EquipAbilityById(const FRPGId& AbilityId, int32 InputId);

	UFUNCTION(BlueprintCallable, Category = "Ability")
	virtual void UnequipAbilityByInputId(int32 InputId);

protected:
	void OnLearnedAbility(class UAbilityAsset* AbilityAsset, const FAbilityData& AbilityData);

public:
	UPROPERTY(BlueprintAssignable, Category = "Ability")
	FOnLearnedAbility OnLearnedAbilityDelegate;

	UPROPERTY(BlueprintAssignable, Category = "Ability")
	FOnLearnedAbilities OnLearnedAbilitiesDelegate;

private:
	// The state of an ability being learned asynchronously
	enum class EAbilityLearnState : uint8
	{
		Pending,
		Learned,
		Failed,
	};

	// Record for an ability that is being learned asynchronously
	struct FAbilityLearnRecord
	{
		FAbilityData AbilityData;

		TSharedPtr<FStreamableHandle> Handle = nullptr;

		EAbilityLearnState LearnState = EAbilityLearnState::Pending;

		FAbilityLearnRecord(const FAbilityData& InAbilityData, const TSharedPtr<FStreamableHandle>& Handle)
			: AbilityData(InAbilityData), Handle(Handle), LearnState(EAbilityLearnState::Pending)
		{
		}
	};

	// The abilities that are currently being learned asynchronously
	TMap<FRPGId, FAbilityLearnRecord> PendingLearnedAbilities;

protected:
	virtual UGameplayAbility* CreateNewInstanceOfAbility(FGameplayAbilitySpec& Spec, const UGameplayAbility* Ability) override;

protected:
	// The map of learned abilities, keyed by their unique FRPGId
	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	TMap<FRPGId, FAbilityData> LearnedAbilities;

	// The map of currently equipped abilities, keyed by their input ID (e.g. 0 for primary action, 1 for secondary action, etc.)
	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	TMap<int32, FRPGId> EquippedAbilities;

};

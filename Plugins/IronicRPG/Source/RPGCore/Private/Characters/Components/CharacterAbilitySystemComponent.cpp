// Copyright Ironic Studio. All Rights Reserved.


#include "Characters/Components/CharacterAbilitySystemComponent.h"

#include "Characters/BaseCharacter.h"

#include "Abilities/RPGGameplayAbility.h"
#include "Abilities/AbilityAsset.h"
#include "Assets/RPGAssetLibrary.h"

URPGAbilitySystemComponent::URPGAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

void URPGAbilitySystemComponent::AbilityLocalInputPressed(int32 InputID)
{
	// Consume the input if this InputID is overloaded with GenericConfirm/Cancel and the GenericConfim/Cancel callback is bound
	if (IsGenericConfirmInputBound(InputID))
	{
		LocalInputConfirm();
		return;
	}

	if (IsGenericCancelInputBound(InputID))
	{
		LocalInputCancel();
		return;
	}

	const FRPGId& InputAbilityId = EquippedAbilities.FindRef(InputID);
	FGameplayAbilitySpec* FoundSpec = FindAbilitySpecFromHandle(LearnedAbilities.FindRef(InputAbilityId).AbilitySpecHandle);

	ABILITYLIST_SCOPE_LOCK();
	if (FoundSpec && FoundSpec->InputID == InputID && FoundSpec->Ability)
	{
		FoundSpec->InputPressed = true;
		if (FoundSpec->IsActive())
		{
			if (FoundSpec->Ability->bReplicateInputDirectly && IsOwnerActorAuthoritative() == false)
			{
				ServerSetInputPressed(FoundSpec->Handle);
			}

			AbilitySpecInputPressed(*FoundSpec);

			TArray<UGameplayAbility*> Instances = FoundSpec->GetAbilityInstances();
			const FGameplayAbilityActivationInfo& ActivationInfo = Instances.IsEmpty() ? FoundSpec->ActivationInfo : Instances.Last()->GetCurrentActivationInfoRef();

			// Invoke the InputPressed event. This is not replicated here. If someone is listening, they may replicate the InputPressed event to the server.
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, FoundSpec->Handle, ActivationInfo.GetActivationPredictionKey());
		}
		else
		{
			// Ability is not active, so try to activate it
			TryActivateAbility(FoundSpec->Handle);
		}
	}
}

void URPGAbilitySystemComponent::AbilityLocalInputReleased(int32 InputID)
{
	Super::AbilityLocalInputReleased(InputID);
}

ABaseCharacter* URPGAbilitySystemComponent::GetBaseCharacterOwner() const
{
	return Cast<ABaseCharacter>(GetOwnerActor());
}

void URPGAbilitySystemComponent::LearnAbility(const FAbilityData& AbilityData)
{
	LearnAbility(AbilityData, [WeakThis = MakeWeakObjectPtr(this)](const FAbilityData& AbilityData)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnLearnedAbilityDelegate.Broadcast(AbilityData);
			}
		});
}

void URPGAbilitySystemComponent::LearnAbility(const FAbilityData& AbilityData, TFunction<void(const FAbilityData&)> OnLearnedCallback)
{
	if (HasLearnedAbility(AbilityData.AbilityId))
	{
		UE_LOG(LogTemp, Display, TEXT("URPGAbilitySystemComponent::LearnAbility - Ability already learned: %s"), *AbilityData.AbilityId.ToString());
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("URPGAbilitySystemComponent::LearnAbility - %s is learning ability: %s"),
		*GetBaseCharacterOwner()->GetName(), *AbilityData.AbilityId.ToString());

	// Asynchronously load the AbilityAsset
	TSharedPtr<FStreamableHandle> Handle = URPGAssetLibrary::GetAssetByRPGIdAsync(AbilityData.AbilityId, { "Ability", "UI" },
		[WeakThis = MakeWeakObjectPtr(this), AbilityData, OnLearnedCallback](URPGPrimaryAsset* Asset)
		{
			if (!WeakThis.IsValid())
			{
				OnLearnedCallback(FAbilityData());
				return;
			}

			WeakThis->OnLearnedAbility(Cast<UAbilityAsset>(Asset), AbilityData);

			OnLearnedCallback(AbilityData);
		});
}

void URPGAbilitySystemComponent::LearnAbilities(const TArray<FAbilityData>& AbilitiesData)
{
	LearnAbilities(AbilitiesData, [WeakThis = MakeWeakObjectPtr(this)](const TArray<FAbilityData>& AbilitiesData)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnLearnedAbilitiesDelegate.Broadcast(AbilitiesData);
			}
		});
}

void URPGAbilitySystemComponent::LearnAbilities(const TArray<FAbilityData>& AbilitiesData, TFunction<void(const TArray<FAbilityData>&)> OnLearnedCallback)
{
	TMap<FRPGId, FAbilityData> AbilitiesToLearn;
	for (const FAbilityData& AbilityData : AbilitiesData)
	{
		if (HasLearnedAbility(AbilityData.AbilityId))
		{
			UE_LOG(LogTemp, Display, TEXT("URPGAbilitySystemComponent::LearnAbilities - Ability already learned: %s"), *AbilityData.AbilityId.ToString());
			continue;
		}

		AbilitiesToLearn.Add(AbilityData.AbilityId, AbilityData);

		UE_LOG(LogTemp, Display, TEXT("URPGAbilitySystemComponent::LearnAbilities - %s is learning ability: %s"),
			*GetBaseCharacterOwner()->GetName(), *AbilityData.AbilityId.ToString());
	}

	TArray<FRPGId> AbilityIdsToLoad;
	AbilitiesToLearn.GetKeys(AbilityIdsToLoad);

	TSharedPtr<FStreamableHandle> Handle = URPGAssetLibrary::GetAssetArrayByRPGIdsAsync(AbilityIdsToLoad, {"Ability", "UI"}, 
		[WeakThis = MakeWeakObjectPtr(this), AbilitiesToLearn, OnLearnedCallback](TArray<URPGPrimaryAsset*> Assets)
		{
			if (!WeakThis.IsValid())
			{
				OnLearnedCallback({ });
				return;
			}

			for (URPGPrimaryAsset* Asset : Assets)
			{
				WeakThis->OnLearnedAbility(Cast<UAbilityAsset>(Asset), AbilitiesToLearn[Asset->GetId()]);
			}

			TArray<FAbilityData> LearnedAbilitiesData;
			AbilitiesToLearn.GenerateValueArray(LearnedAbilitiesData);
			OnLearnedCallback(LearnedAbilitiesData);
		});
}

void URPGAbilitySystemComponent::UnlearnAbility(const FRPGId& AbilityId)
{
	const FGameplayAbilitySpecHandle& SpecHandle = LearnedAbilities.FindRef(AbilityId).AbilitySpecHandle;
	if (SpecHandle.IsValid())
	{
		// Remove the ability from the ability system component
		ClearAbility(SpecHandle);

		// Remove from learned abilities list
		LearnedAbilities.Remove(AbilityId);

		// Remove from pending learned abilities list
		PendingLearnedAbilities.Remove(AbilityId);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("URPGAbilitySystemComponent::UnlearnAbility - Ability not found to unlearn: %s"), *AbilityId.ToString());
	}
}

void URPGAbilitySystemComponent::UnlearnAllAbilities()
{
	ClearAllAbilities();

	// Clear learned abilities list
	LearnedAbilities.Empty();
}

bool URPGAbilitySystemComponent::HasLearnedAbility(const FRPGId& AbilityId) const
{
	return LearnedAbilities.Contains(AbilityId);
}

bool URPGAbilitySystemComponent::HasLearnedAbilityWithTags(const FGameplayTagContainer& AbilityTags) const
{
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		const UGameplayAbility* Ability = Spec.Ability;
		if (Ability && Ability->GetAssetTags().HasAny(AbilityTags))
		{
			return true;
		}
	}

	return false;
}

bool URPGAbilitySystemComponent::TryActivateAbilityById(const FRPGId& AbilityId, bool bAllowRemoteActivation)
{
	const FGameplayAbilitySpecHandle& SpecHandle = LearnedAbilities.FindRef(AbilityId).AbilitySpecHandle;
	if (SpecHandle.IsValid())
	{
		return TryActivateAbility(SpecHandle, bAllowRemoteActivation);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("URPGAbilitySystemComponent::TryActivateAbilityById - Ability not found to activate: %s"), *AbilityId.ToString());
		return false;
	}
}

void URPGAbilitySystemComponent::EquipAbilityById(const FRPGId& AbilityId, int32 InputId)
{
	if (FGameplayAbilitySpec* FoundSpec = FindAbilitySpecFromHandle(LearnedAbilities.FindRef(AbilityId).AbilitySpecHandle))
	{
		// If the ability is already bound to a different input, unbind it first
		if (FGameplayAbilitySpec* OldSpec = FindAbilitySpecFromInputID(InputId))
		{
			OldSpec->InputID = INDEX_NONE;
		}

		FoundSpec->InputID = InputId;

		// Mark the ability spec as dirty to ensure it replicates to clients
		MarkAbilitySpecDirty(*FoundSpec);

		// Update the EquippedAbilities map
		EquippedAbilities.Add(InputId, AbilityId);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("URPGAbilitySystemComponent::EquipAbilityById - Ability not found to equip: %s"), *AbilityId.ToString());
	}
}

void URPGAbilitySystemComponent::UnequipAbilityByInputId(int32 InputId)
{
	if (FGameplayAbilitySpec* FoundSpec = FindAbilitySpecFromInputID(InputId))
	{
		FoundSpec->InputID = INDEX_NONE;

		// Mark the ability spec as dirty to ensure it replicates to clients
		MarkAbilitySpecDirty(*FoundSpec);

		// Remove from the EquippedAbilities map
		EquippedAbilities.Remove(InputId);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("URPGAbilitySystemComponent::UnequipAbilityByInputId - No ability found equipped to InputId: %d"), InputId);
	}
}

void URPGAbilitySystemComponent::OnLearnedAbility(UAbilityAsset* AbilityAsset, const FAbilityData& AbilityData)
{
	if (!IsOwnerActorAuthoritative())
	{
		return;
	}

	if (!AbilityAsset)
	{
		UE_LOG(LogTemp, Display, TEXT("URPGAbilitySystemComponent::LearnAbility - %s learned a null ability asset: %s"),
			*GetBaseCharacterOwner()->GetName(), *AbilityData.AbilityId.ToString());
		return;
	}

	TSet<FRPGId> AllowedCharacters = AbilityAsset->GetAllowedCharacters();
	if (!AllowedCharacters.IsEmpty())
	{
		if (!AllowedCharacters.Contains(GetBaseCharacterOwner()->GetId()))
		{
			UE_LOG(LogTemp, Warning, TEXT("URPGAbilitySystemComponent::LearnAbility - Character not allowed to learn ability: %s"), *AbilityAsset->GetId().ToString());
			return;
		}
	}

	TSoftClassPtr<URPGGameplayAbility> AbilityClass = AbilityAsset->GetAbilityClass();
	if (!AbilityClass.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("URPGAbilitySystemComponent::LearnAbility - Invalid AbilityClass in AbilityAsset: %s"), *AbilityAsset->GetId().ToString());
		return;
	}

	FAbilityData NewAbilityData = AbilityData;

	if (NewAbilityData.Level <= 0)
	{
		NewAbilityData.Level = 1;

		UE_LOG(LogTemp, Warning, TEXT("URPGAbilitySystemComponent::LearnAbility - Level must be greater than 0. Setting to 1."));
	}

	if (HasLearnedAbility(AbilityAsset->GetId()))
	{
		UE_LOG(LogTemp, Display, TEXT("URPGAbilitySystemComponent::LearnAbility - Ability already learned: %s"), *AbilityAsset->GetId().ToString());
		return;
	}

	NewAbilityData.AbilityAsset = AbilityAsset;

	FGameplayAbilitySpec AbilitySpec(
		AbilityClass.Get(),		// Ability class
		NewAbilityData.Level,	// Ability level
		INDEX_NONE,				// Input ID (not used here, can be set when equipping)
		AbilityAsset			// Source object
	);

	// Add the ability to the ability system component
	NewAbilityData.AbilitySpecHandle = GiveAbility(AbilitySpec);

	LearnedAbilities.Add(AbilityAsset->GetId(), NewAbilityData);

	PendingLearnedAbilities.Remove(AbilityAsset->GetId());

	OnLearnedAbilityDelegate.Broadcast(NewAbilityData);

	UE_LOG(LogTemp, Display, TEXT("URPGAbilitySystemComponent::LearnAbility - %s learned ability: %s at level %d"),
		*GetBaseCharacterOwner()->GetName(), *AbilityAsset->GetId().ToString(), NewAbilityData.Level);
}

UGameplayAbility* URPGAbilitySystemComponent::CreateNewInstanceOfAbility(FGameplayAbilitySpec& Spec, const UGameplayAbility* Ability)
{
	URPGGameplayAbility* NewAbility = Cast<URPGGameplayAbility>(Super::CreateNewInstanceOfAbility(Spec, Ability));
	if (NewAbility)
	{
		NewAbility->InitAbilityFrom(Cast<UAbilityAsset>(Spec.SourceObject));
	}

	return NewAbility;
}

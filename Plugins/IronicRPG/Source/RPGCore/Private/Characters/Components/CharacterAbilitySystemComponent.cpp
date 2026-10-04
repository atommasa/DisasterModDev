// Copyright Ironic Studio. All Rights Reserved.


#include "Characters/Components/CharacterAbilitySystemComponent.h"

#include "Characters/BaseCharacter.h"

#include "Abilities/RPGGameplayAbility.h"
#include "Abilities/AbilityAsset.h"
#include "Assets/RPGAssetLibrary.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Input, "Ability.Input", "This tag is used for input to activate the ability.")

URPGAbilitySystemComponent::URPGAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

void URPGAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	const EInputRouteResult RouteResult = RouteInputTag(InputTag);

	if (RouteResult == EInputRouteResult::Consumed || RouteResult == EInputRouteResult::Blocked)
	{
		return;
	}

	const FRPGId* InputAbilityId = EquippedAbilities.Find(InputTag);
	if (!InputAbilityId)
	{
		return;
	}

	const FAbilityData* AbilityData = LearnedAbilities.Find(*InputAbilityId);
	if (!AbilityData)
	{
		return;
	}

	ABILITYLIST_SCOPE_LOCK();

	FGameplayAbilitySpec* FoundSpec = FindAbilitySpecFromHandle(AbilityData->AbilitySpecHandle);

	if (!FoundSpec || !FoundSpec->Ability)
	{
		return;
	}

	FoundSpec->InputPressed = true;

	if (FoundSpec->IsActive())
	{
		AbilitySpecInputPressed(*FoundSpec);

		return;
	}

	TryActivateAbility(FoundSpec->Handle);
}

void URPGAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}
	
	const FRPGId* InputAbilityId = EquippedAbilities.Find(InputTag);
	if (!InputAbilityId)
	{
		return;
	}

	const FAbilityData* AbilityData = LearnedAbilities.Find(*InputAbilityId);
	if (!AbilityData)
	{
		return;
	}

	ABILITYLIST_SCOPE_LOCK();

	FGameplayAbilitySpec* FoundSpec = FindAbilitySpecFromHandle(AbilityData->AbilitySpecHandle);

	if (!FoundSpec || !FoundSpec->Ability)
	{
		return;
	}

	FoundSpec->InputPressed = false;

	if (!FoundSpec->IsActive())
	{
		return;
	}

	if (FoundSpec->Ability->bReplicateInputDirectly && !IsOwnerActorAuthoritative())
	{
		ServerSetInputReleased(FoundSpec->Handle);
	}

	AbilitySpecInputReleased(*FoundSpec);

	const TArray<UGameplayAbility*> Instances = FoundSpec->GetAbilityInstances();

	const FGameplayAbilityActivationInfo& ActivationInfo =
		Instances.IsEmpty()
		? FoundSpec->ActivationInfo
		: Instances.Last()->GetCurrentActivationInfoRef();

	InvokeReplicatedEvent(
		EAbilityGenericReplicatedEvent::InputReleased,
		FoundSpec->Handle,
		ActivationInfo.GetActivationPredictionKey()
	);
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
	TSharedPtr<FStreamableHandle> Handle = URPGAssetLibrary::LoadAssetByRPGIdAsync(AbilityData.AbilityId, { "Ability", "UI" },
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

	TSharedPtr<FStreamableHandle> Handle = URPGAssetLibrary::LoadAssetArrayByRPGIdsAsync(AbilityIdsToLoad, {"Ability", "UI"}, 
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

void URPGAbilitySystemComponent::EquipAbilityById(const FRPGId& AbilityId, const FGameplayTag& InputTag)
{
	if (FGameplayAbilitySpec* FoundSpec = FindAbilitySpecFromHandle(LearnedAbilities.FindRef(AbilityId).AbilitySpecHandle))
	{
		// Remove all input tags
		const FGameplayTagContainer& InputChildTags = FoundSpec->DynamicAbilityTags.Filter(FGameplayTagContainer(Ability_Input));
		FoundSpec->DynamicAbilityTags.RemoveTags(InputChildTags);

		// Add input tag
		FoundSpec->DynamicAbilityTags.AddTag(InputTag);

		// Mark the ability spec as dirty to ensure it replicates to clients
		MarkAbilitySpecDirty(*FoundSpec);

		// Update the EquippedAbilities map
		EquippedAbilities.Add(InputTag, AbilityId);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("URPGAbilitySystemComponent::EquipAbilityById - Ability not found to equip: %s"), *AbilityId.ToString());
	}
}

void URPGAbilitySystemComponent::UnequipAbilityByInputId(const FGameplayTag& InputTag)
{
	const FRPGId& AbilityId = EquippedAbilities.FindRef(InputTag);

	if (FGameplayAbilitySpec* FoundSpec = FindAbilitySpecFromHandle(LearnedAbilities.FindRef(AbilityId).AbilitySpecHandle))
	{
		// Remove all input tags
		const FGameplayTagContainer& InputChildTags = FoundSpec->DynamicAbilityTags.Filter(FGameplayTagContainer(Ability_Input));
		FoundSpec->DynamicAbilityTags.RemoveTags(InputChildTags);

		// Mark the ability spec as dirty to ensure it replicates to clients
		MarkAbilitySpecDirty(*FoundSpec);

		// Remove from the EquippedAbilities map
		EquippedAbilities.Remove(InputTag);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("URPGAbilitySystemComponent::UnequipAbilityByInputId - No ability found equipped to InputId: %s"), *InputTag.ToString());
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

FDelegateHandle URPGAbilitySystemComponent::RegisterInputTagListener(UObject* Owner, FInputTagListener Listener, int32 Priority)
{
	check(Owner);

	const FDelegateHandle NewHandle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);

	FInputTagListenerEntry NewEntry;
	NewEntry.Handle = NewHandle;
	NewEntry.Owner = Owner;
	NewEntry.Priority = Priority;
	NewEntry.Listener = MoveTemp(Listener);

	InputTagListeners.Add(MoveTemp(NewEntry));

	InputTagListeners.Sort(
		[](const FInputTagListenerEntry& A,
			const FInputTagListenerEntry& B)
		{
			return A.Priority > B.Priority;
		});

	return NewHandle;
}

void URPGAbilitySystemComponent::UnregisterInputTagListener(FDelegateHandle Handle)
{
	InputTagListeners.RemoveAll(
		[Handle](const FInputTagListenerEntry& Entry)
		{
			return Entry.Handle == Handle;
		});
}

EInputRouteResult URPGAbilitySystemComponent::RouteInputTag(const FGameplayTag & InputTag)
{
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[InputRouter] Route %s, ListenerCount=%d"),
		*InputTag.ToString(),
		InputTagListeners.Num()
	);

	InputTagListeners.RemoveAll([](const FInputTagListenerEntry& Entry)
		{
			return !Entry.Owner.IsValid() || !Entry.Listener.IsBound();
		});

	TArray<FInputTagListener> Listeners;
	Listeners.Reserve(InputTagListeners.Num());

	for (const FInputTagListenerEntry& Entry : InputTagListeners)
	{
		Listeners.Add(Entry.Listener);
	}

	for (const FInputTagListener& Listener : Listeners)
	{
		if (!Listener.IsBound())
		{
			continue;
		}

		const EInputRouteResult Result = Listener.Execute(InputTag);

		if (Result != EInputRouteResult::PassThrough)
		{
			return Result;
		}
	}

	return EInputRouteResult::PassThrough;
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

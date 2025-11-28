// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Components/CharacterAbilitySystemComponent.h"

#include "Characters/BaseCharacter.h"

#include "Abilities/RPGGameplayAbility.h"
#include "Abilities/AbilityAsset.h"
#include "Assets/RPGAssetLibrary.h"

URPGAbilitySystemComponent::URPGAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

ABaseCharacter* URPGAbilitySystemComponent::GetBaseCharacterOwner() const
{
	return Cast<ABaseCharacter>(GetOwnerActor());
}

void URPGAbilitySystemComponent::LearnAbility(const FRPGId& AbilityId, int32 Level)
{
	if (!AbilityId.IsValid() || AbilityId.GetIdType() != UAbilityAsset::GetAssetTypeStatic())
	{
		UE_LOG(LogTemp, Warning, TEXT("URPGAbilitySystemComponent::LearnAbility - Invalid AbilityId or wrong type"));
		return;
	}

	if (HasLearnedAbility(AbilityId))
	{
		UE_LOG(LogTemp, Display, TEXT("URPGAbilitySystemComponent::LearnAbility - Ability already learned: %s"), *AbilityId.ToString());
		return;
	}

	// Asynchronously load the AbilityAsset
	URPGAssetLibrary::GetAssetByRPGIdAsync(AbilityId, { "Ability" }, [this, AbilityId, Level](URPGPrimaryAsset* Asset)
		{
			LearnAbilityByAsset(Cast<UAbilityAsset>(Asset), Level);
		});
}

void URPGAbilitySystemComponent::LearnAbilityByAsset(UAbilityAsset* AbilityAsset, int32 Level)
{
	if (!AbilityAsset)
	{
		UE_LOG(LogTemp, Display, TEXT("URPGAbilitySystemComponent::LearnAbility - AbilityAsset is null"));
		return;
	}

	if (HasLearnedAbility(AbilityAsset->GetId()))
	{
		UE_LOG(LogTemp, Display, TEXT("URPGAbilitySystemComponent::LearnAbility - Ability already learned: %s"), *AbilityAsset->GetId().ToString());
		return;
	}

	TSet<FRPGId> AllowedCharacters = AbilityAsset->GetAllowedCharacters();
	if (!AllowedCharacters.IsEmpty() && !AllowedCharacters.Contains(AbilityAsset->GetId()))
	{
		UE_LOG(LogTemp, Warning, TEXT("URPGAbilitySystemComponent::LearnAbility - Character not allowed to learn ability: %s"), *AbilityAsset->GetId().ToString());
		return;
	}

	TSoftClassPtr<URPGGameplayAbility> AbilityClass = AbilityAsset->GetAbilityClass();
	if (!AbilityClass || !AbilityClass->IsChildOf(URPGGameplayAbility::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("URPGAbilitySystemComponent::LearnAbility - Invalid AbilityClass in AbilityAsset: %s"), *AbilityAsset->GetId().ToString());
		return;
	}

	if (Level <= 0)
	{
		Level = 1;

		UE_LOG(LogTemp, Warning, TEXT("URPGAbilitySystemComponent::LearnAbility - Level must be greater than 0. Setting to 1."));
	}

	AbilityClass.LoadAsync(FLoadSoftObjectPathAsyncDelegate::CreateLambda([this, Level, AbilityAsset](const FSoftObjectPath& LoadedPath, UObject* LoadedObj)
		{
			UClass* LoadedClass = Cast<UClass>(LoadedObj);

			if (!LoadedClass)
			{
				UE_LOG(LogTemp, Warning, TEXT("URPGAbilitySystemComponent::LearnAbility - Failed to load AbilityClass for AbilityAsset: %s"), *AbilityAsset->GetId().ToString());
				return;
			}

			FGameplayAbilitySpec AbilitySpec(
				LoadedClass, // Ability class
				Level,		 // Ability level
				INDEX_NONE,	 // Input ID
				AbilityAsset // Source object
			);

			// Add the ability to the ability system component
			GiveAbility(AbilitySpec);
		}));
}

void URPGAbilitySystemComponent::UnlearnAbility(const FRPGId& AbilityId)
{
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		UAbilityAsset* SourceObject = Cast<UAbilityAsset>(Spec.SourceObject);
		if (SourceObject && SourceObject->GetId() == AbilityId)
		{
			// Remove the ability from the ability system component
			ClearAbility(Spec.Handle);
			
			return;
		}
	}
}

bool URPGAbilitySystemComponent::HasLearnedAbility(const FRPGId& AbilityId) const
{
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		UAbilityAsset* SourceObject = Cast<UAbilityAsset>(Spec.SourceObject);
		if (SourceObject && SourceObject->GetId() == AbilityId)
		{
			return true;
		}
	}

	return false;
}

bool URPGAbilitySystemComponent::HasLearnedAbilityWithTags(const FGameplayTagContainer& AbilityTags) const
{
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		const UGameplayAbility* Ability = Spec.Ability;
		if (Ability && Ability->AbilityTags.HasAny(AbilityTags))
		{
			return true;
		}
	}

	return false;
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

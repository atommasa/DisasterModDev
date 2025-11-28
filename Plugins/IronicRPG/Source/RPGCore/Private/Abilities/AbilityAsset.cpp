// Copyright Ironic Studio. All Rights Reserved.


#include "Abilities/AbilityAsset.h"

#include "Abilities/RPGGameplayAbility.h"

UAbilityAsset::UAbilityAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &UAbilityAsset::OnObjectPropertyChanged);
#endif // WITH_EDITOR
}

UAbilityAsset::~UAbilityAsset()
{
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void UAbilityAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.MemberProperty->GetFName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAbilityAsset, AbilityClass))
	{
		if (!AbilityClass.IsValid())
		{
			AbilityClass.LoadSynchronous();
		}

		if (AbilityClass.IsValid())
		{
			// Check if the AbilityClass is a valid URPGGameplayAbility subclass
			if (URPGGameplayAbility* AbilityCDO = AbilityClass->GetDefaultObject<URPGGameplayAbility>())
			{
				AbilityMontageData.SetNum(AbilityCDO->GetRequiredAnimMontageCount());
			}
		}
		else
		{
			AbilityMontageData.Empty();
		}
	}
}

void UAbilityAsset::OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
{
	if (!AbilityClass)
	{
		return;
	}

	if (!AbilityClass.IsValid())
	{
		AbilityClass.LoadSynchronous();
	}

	// Check if the changed object is the Ability CDO
	if (URPGGameplayAbility* AbilityCDO = AbilityClass->GetDefaultObject<URPGGameplayAbility>())
	{
		if (AbilityClass.IsValid() && Object == AbilityCDO)
		{
			// Update the AbilityMontageData array size based on the AbilityCDO's RequiredAnimMontageCount
			AbilityMontageData.SetNum(AbilityCDO->GetRequiredAnimMontageCount());

			MarkPackageDirty();
		}
	}
}
#endif // WITH_EDITOR

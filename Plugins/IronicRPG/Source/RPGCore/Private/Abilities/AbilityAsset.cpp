// Copyright Ironic Studio. All Rights Reserved.


#include "Abilities/AbilityAsset.h"

#include "Abilities/RPGGameplayAbility.h"

UAbilityAsset::UAbilityAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectTransacted.AddUObject(this, &UAbilityAsset::OnObjectTransacted);
#endif // WITH_EDITOR
}

UAbilityAsset::~UAbilityAsset()
{
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectTransacted.RemoveAll(this);
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void UAbilityAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	const FName PropertyName = PropertyChangedEvent.MemberProperty->GetFName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAbilityAsset, AbilityClass))
	{
		if (URPGGameplayAbility* AbilityCDO = AbilityClass->GetDefaultObject<URPGGameplayAbility>())
		{
			// Update the RequireAnimTagContainers array size based on the AbilityCDO's RequiredAnimMontageCount
			RequireAnimTagContainers.SetNum(AbilityCDO->GetRequiredMontageCount());
		}
		else
		{
			RequireAnimTagContainers.Empty();
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UAbilityAsset, DefaultCostSpec))
	{
		for (FAbilityCostEntry& CostEntry : DefaultCostSpec.CostEntries)
		{
			switch (CostEntry.CalcType)
			{
			case EMagnitudeCalcType::Flat:
				CostEntry.Magnitude.Value = FMath::Max(0.f, CostEntry.Magnitude.Value);
				break;
			case EMagnitudeCalcType::PercentOfMax:
			case EMagnitudeCalcType::PercentOfCurrent:
				CostEntry.Magnitude.Value = FMath::Clamp(CostEntry.Magnitude.Value, 0.f, 1.f);
				break;
			}
		}
	}
}

void UAbilityAsset::OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event)
{
	if (!Event.GetChangedProperties().Contains(GET_MEMBER_NAME_CHECKED(URPGGameplayAbility, RequiredMontageCount)))
	{
		return;
	}

	if (!AbilityClass)
	{
		return;
	}

	if (!AbilityClass.IsValid())
	{
		AbilityClass.LoadSynchronous();
	}

	// Check if the AbilityClass's CDO is the same as the Ability passed in
	if (URPGGameplayAbility* AbilityCDO = AbilityClass->GetDefaultObject<URPGGameplayAbility>())
	{
		if (Object == AbilityCDO)
		{
			// Update the RequireAnimTagContainers array size based on the AbilityCDO's RequiredAnimMontageCount
			RequireAnimTagContainers.SetNum(AbilityCDO->GetRequiredMontageCount());

			MarkPackageDirty();
		}
	}
}
#endif // WITH_EDITOR

// Copyright Ironic Studio. All Rights Reserved.


#include "Abilities/RPGGameplayAbility.h"
#include "Abilities/AbilityAsset.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

#include "Characters/BaseCharacter.h"
#include "Characters/Components/RPGCharacterMovementComponent.h"

const FName URPGGameplayAbility::AbilityMontageTaskName = TEXT("AbilityMontageTask");

void URPGGameplayAbility::InitAbilityFrom(const UAbilityAsset* InAbilityAsset)
{
	if (!InAbilityAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("URPGGameplayAbility initialized with a null asset!"));
		return;
	}

	// Initialize animation montage data from asset
	AnimMontageData = InAbilityAsset->GetAbilityMontageData();

	// Initialize ability cost spec from asset
	AbilityCostSpec = InAbilityAsset->GetDefaultCostSpec();

	// Initialize movement and cancelable properties from asset
	bCanMoveWhileCasting = InAbilityAsset->GetbCanMoveWhileCasting();
	bIsCancelable = InAbilityAsset->GetbCanBeInterrupted();

	// Initialize ability tags from asset
	AbilityTags.AppendTags(InAbilityAsset->GetAbilityTags());
	CancelAbilitiesWithTag.AppendTags(InAbilityAsset->GetCancelWithTags());
	BlockAbilitiesWithTag.AppendTags(InAbilityAsset->GetBlockWithTags());
	ActivationOwnedTags.AppendTags(InAbilityAsset->GetActivationOwnedTags());
	ActivationRequiredTags.AppendTags(InAbilityAsset->GetActivationRequiredTags());
	ActivationBlockedTags.AppendTags(InAbilityAsset->GetActivationBlockedTags());
	SourceRequiredTags.AppendTags(InAbilityAsset->GetSourceRequiredTags());
	SourceBlockedTags.AppendTags(InAbilityAsset->GetSourceBlockedTags());
	TargetRequiredTags.AppendTags(InAbilityAsset->GetTargetRequiredTags());
	TargetBlockedTags.AppendTags(InAbilityAsset->GetTargetBlockedTags());


}

void URPGGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (AbilityCostSpec.CostPolicy == ECostPolicy::OnActivation && !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}

	if (!bCanMoveWhileCasting)
	{
		if (ABaseCharacter* Character = Cast<ABaseCharacter>(ActorInfo->AvatarActor.Get()))
		{
			Character->GetCharacterMovement()->SetMovementMode(MOVE_None);
		}
	}
	
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void URPGGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!bCanMoveWhileCasting)
	{
		// TODO: 應該要恢復原本的移動模式，而不是直接設成走路模式
		if (ABaseCharacter* Character = Cast<ABaseCharacter>(ActorInfo->AvatarActor.Get()))
		{
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

const FGameplayTagContainer* URPGGameplayAbility::GetCooldownTags() const
{
	FGameplayTagContainer* CooldownTags = const_cast<FGameplayTagContainer*>(Super::GetCooldownTags());
	if (!CooldownTags)
	{
		return nullptr;
	}

	CooldownTags->AppendTags(AbilityCostSpec.CooldownTags);

	return CooldownTags;
}

void URPGGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UGameplayEffect* CooldownEffect = GetCooldownGameplayEffect();
	if (CooldownEffect)
	{
		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
			CooldownEffect->GetClass(),
			GetAbilityLevel(Handle, ActorInfo)
		);

		if (SpecHandle.IsValid())
		{
			SpecHandle.Data.Get()->DynamicGrantedTags.AppendTags(AbilityCostSpec.CooldownTags);

			const float Duration = AbilityCostSpec.CooldownDuration.GetValueAtLevel(GetAbilityLevel(Handle, ActorInfo));
			SpecHandle.Data.Get()->SetSetByCallerMagnitude(
				FGameplayTag::RequestGameplayTag(FName("Data.Cooldown")),
				Duration
			);

			// Apply the cooldown effect to the owner
			ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
		}
	}
}

UGameplayEffect* URPGGameplayAbility::GetCostGameplayEffect() const
{
	return Super::GetCostGameplayEffect();
}

FAnimMontageData URPGGameplayAbility::GetAnimMontageDataByIndex(int32 Index) const
{
	if (!AnimMontageData.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Display, TEXT("URPGGameplayAbility::GetAnimMontageDataByIndex - Invalid index: %d (Max: %d)"), Index, RequiredAnimMontageCount);

		return FAnimMontageData();
	}
	
	return AnimMontageData[Index];
}



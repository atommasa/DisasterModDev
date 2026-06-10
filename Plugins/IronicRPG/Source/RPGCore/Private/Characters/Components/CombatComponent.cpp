// Copyright Ironic Studio. All Rights Reserved.


#include "Characters/Components/CombatComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

#include "Characters/Attributes/RPGAttributeSet.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

void UCombatComponent::RequestDeath(const FAttributeEventContext& DeathEventContext)
{
    if (bDeathRequested)
    {
        return;
	}

    if (IsCharacterDead())
    {
        return;
    }

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    bDeathRequested = true;

    FGameplayEventData EventData = DeathEventContext.ToGameplayEventData();
    EventData.EventTag = DeathEventTag;

    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
        Owner,
        DeathEventTag,
        EventData
    );

    return;
}

void UCombatComponent::FinishDeath()
{
    bDeathRequested = false;
	
    // OnDeathFinished.Broadcast();
}

bool UCombatComponent::IsCharacterDead() const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return false;
    }

    const UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner);
    if (!AbilitySystemComponent)
    {
        return false;
    }

    return AbilitySystemComponent->HasMatchingGameplayTag(DeadTag) || AbilitySystemComponent->HasMatchingGameplayTag(DyingTag);
}

bool UCombatComponent::IsCharacterStateDying() const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return false;
    }

    const UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner);
    if (!AbilitySystemComponent)
    {
        return false;
    }

    return AbilitySystemComponent->HasMatchingGameplayTag(DyingTag) && !AbilitySystemComponent->HasMatchingGameplayTag(DeadTag);
}

bool UCombatComponent::IsCharacterStateDead() const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return false;
    }

    const UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner);
    if (!AbilitySystemComponent)
    {
        return false;
    }

    return AbilitySystemComponent->HasMatchingGameplayTag(DeadTag) && !AbilitySystemComponent->HasMatchingGameplayTag(DyingTag);
}

void UCombatComponent::RequestRevive(const FAttributeEventContext& ReviveEventContext)
{
    if (bReviveRequested)
    {
        return;
    }

    if (IsCharacterReviving())
    {
        return;
    }

    if (!IsCharacterStateDead())
    {
        return;
    }

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    bReviveRequested = true;

    FGameplayEventData EventData = ReviveEventContext.ToGameplayEventData();
    EventData.EventTag = ReviveEventTag;

    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
        Owner,
        ReviveEventTag,
        EventData
	);
}

void UCombatComponent::FinishRevive()
{
    bReviveRequested = false;
	// OnReviveFinished.Broadcast();
}

bool UCombatComponent::IsCharacterReviving() const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return false;
    }

    const UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner);
    if (!AbilitySystemComponent)
    {
        return false;
    }

    return AbilitySystemComponent->HasMatchingGameplayTag(RevivingTag);
}

void UCombatComponent::InitializePostDeathHitReaction()
{
    if (!bAllowPostDeathHitReaction || bPostDeathHitReactActive)
    {
        return;
    }

    // PostDeathHitReactCount = 0;
	bPostDeathHitReactActive = true;
}

void UCombatComponent::EndPostDeathHitReaction()
{
    bPostDeathHitReactActive = false;
}

bool UCombatComponent::CanReceivePostDeathHitReaction() const
{
	return bAllowPostDeathHitReaction &&
        bPostDeathHitReactActive &&
        // PostDeathHitReactCount < MaxPostDeathHitReactCount &&
        IsCharacterStateDying();
}

void UCombatComponent::RequestHitReaction(const FAttributeEventContext& HitEventContext)
{
    if (CanReceivePostDeathHitReaction())
    {
		// PostDeathHitReactCount++;
    }
    /*else if (bPostDeathHitReactActive)
    {
        return;
    }*/
    else if (IsCharacterDead())
    {
        return;
    }

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

	bHitReactionRequested = true;

    FGameplayEventData EventData = HitEventContext.ToGameplayEventData();
    EventData.EventTag = HitReactionEventTag;

    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
        Owner,
        HitReactionEventTag,
        EventData
    );
}

void UCombatComponent::FinishHitReaction()
{
	bHitReactionRequested = false;
}

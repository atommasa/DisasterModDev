// Copyright Ironic Studio. All Rights Reserved.


#include "Abilities/AnimNotify_TriggerEffect.h"
#include "AbilitySystemBlueprintLibrary.h"

#include "Characters/BaseCharacter.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_TriggerEffect_Start, "Ability.HitCheck.Start", "This tag is used when the ability hit check begin.")
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_TriggerEffect_End, "Ability.HitCheck.End", "This tag is used when the ability hit check end.")

void UAnimNotifyState_TriggerEffect::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	if (ABaseCharacter* Owner = Cast<ABaseCharacter>(MeshComp->GetOwner()))
	{
		FGameplayEventData Payload;
		Payload.EventTag = Ability_TriggerEffect_Start;
		Payload.OptionalObject = this;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, Ability_TriggerEffect_Start, Payload);
	}
}

void UAnimNotifyState_TriggerEffect::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (ABaseCharacter* Owner = Cast<ABaseCharacter>(MeshComp->GetOwner()))
	{
		FGameplayEventData Payload;
		Payload.EventTag = Ability_TriggerEffect_End;
		Payload.OptionalObject = this;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, Ability_TriggerEffect_End, Payload);
	}
}

void UAnimNotify_TriggerEffect::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (ABaseCharacter* Owner = Cast<ABaseCharacter>(MeshComp->GetOwner()))
	{
		FGameplayEventData Payload;
		Payload.EventTag = Ability_TriggerEffect_Start;
		Payload.OptionalObject = this;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, Ability_TriggerEffect_Start, Payload);
	}
}

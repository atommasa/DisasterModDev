// Copyright Ironic Studio. All Rights Reserved.


#include "Misc/AnimNotify_SendGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"

void UAnimNotifyState_SendGameplayEvent::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	if (AActor* Owner = MeshComp->GetOwner())
	{
		FGameplayEventData Payload;
		Payload.EventTag = BeginEventTag;
		Payload.OptionalObject = this;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, BeginEventTag, Payload);
	}
}

void UAnimNotifyState_SendGameplayEvent::NotifyEnd(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation, const FAnimNotifyEventReference & EventReference)
{
	if (AActor* Owner = MeshComp->GetOwner())
	{
		FGameplayEventData Payload;
		Payload.EventTag = EndEventTag;
		Payload.OptionalObject = this;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EndEventTag, Payload);
	}
}

void UAnimNotify_SendGameplayEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (AActor* Owner = MeshComp->GetOwner())
	{
		FGameplayEventData Payload;
		Payload.EventTag = EventTag;
		Payload.OptionalObject = this;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, Payload);
	}
}

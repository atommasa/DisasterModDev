// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "AnimNotify_TriggerEffect.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_TriggerEffect_Start)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_TriggerEffect_End)

/**
 * This notify state is used to trigger effects. It will trigger the effect with the specified tag when it begins, and end the effect when it ends.
 * Can be used for both melee and ranged attacks. For melee attacks, the effect can be used to apply damage to enemies in the hitbox. For ranged attacks, the effect can be used to spawn projectiles or apply buffs/debuffs to the character.
 */
UCLASS()
class RPGCORE_API UAnimNotifyState_TriggerEffect : public UAnimNotifyState
{
	GENERATED_BODY()

protected:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag EffectTag;

};

/**
 * This notify is used to trigger instant effects. It will trigger the effect with the specified tag when it is triggered.
 */
UCLASS()
class RPGCORE_API UAnimNotify_TriggerEffect : public UAnimNotify
{
	GENERATED_BODY()

protected:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag EffectTag;

};
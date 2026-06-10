// Copyright Ironic Studio. All Rights Reserved.


#include "Misc/AnimNotify_SavePoseSnapshot.h"
#include "Characters/BaseCharacter.h"

void UAnimNotify_SavePoseSnapshot_Character::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp)
	{
		return;
	}

	if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
	{
		AnimInstance->SavePoseSnapshot(PoseSnapshotName);
		UE_LOG(LogTemp, Warning, TEXT("Save Death Pose AnimInstance: %s"), *GetNameSafe(AnimInstance));
	}
}

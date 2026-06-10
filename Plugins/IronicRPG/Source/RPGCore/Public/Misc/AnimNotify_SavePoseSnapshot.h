// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SavePoseSnapshot.generated.h"

/**
 * This notify saves a pose snapshot of the character at the moment it is triggered.
 * Recommended to set the montage's "Blend Out Trigger Time" to a value greater than 0 to ensure the snapshot is taken at the right moment,
 * as the montage will blend out immediately after triggering this notify.
 * And do not add this notify at the very end of the montage, otherwise the snapshot may take the pose after the montage has already ended, which may not be what you want.
 */
UCLASS()
class RPGCORE_API UAnimNotify_SavePoseSnapshot_Character : public UAnimNotify
{
	GENERATED_BODY()

protected:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose Snapshot")
	FName PoseSnapshotName;
	
};

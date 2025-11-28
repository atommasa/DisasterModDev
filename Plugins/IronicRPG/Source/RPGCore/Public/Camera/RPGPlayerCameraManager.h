// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "RPGPlayerCameraManager.generated.h"

/**
 * 
 */
UCLASS()
class RPGCORE_API ARPGPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()
	
protected:
    virtual void UpdateCamera(float DeltaTime) override;

public:
    using FPathFunction = TFunction<FVector(FVector, FVector, FVector, float)>;
    using FOnFinished = TFunction<void(FVector, FRotator)>;

    virtual void ActorCameraViewTransition(
        AActor* NewTargetActor,
        float Duration = 0.f,
        FPathFunction PathFunction = nullptr,
        FOnFinished OnFinished = nullptr,
        bool bKeepViewDirection = true
    );

private:
    void CaculateDesiredEndPosition();

private:
    bool bTransitionActive = false;
    float TransitionDuration = 0.f;
    float TransitionElapsed = 0.f;

    TWeakObjectPtr<AActor> TargetActor = nullptr;

    FVector StartPos;
    FRotator StartRot;
    FVector EndPos;
    FRotator EndRot;
    FVector ControlPos;

    FPathFunction PathFunc;
    FOnFinished FinishCallback;
};

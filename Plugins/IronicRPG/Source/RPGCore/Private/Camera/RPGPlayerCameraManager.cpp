// Copyright Ironic Studio. All Rights Reserved.


#include "Camera/RPGPlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Camera/RPGSpringArmComponent.h"
#include "Kismet/KismetSystemLibrary.h"

void ARPGPlayerCameraManager::UpdateCamera(float DeltaTime)
{
    if (!bTransitionActive)
    {
        Super::UpdateCamera(DeltaTime);

        return;
    }

    TransitionElapsed += DeltaTime;
    float Alpha = FMath::Clamp(TransitionElapsed / TransitionDuration, 0.f, 1.f);

    // Adjust EndPos to OldPawn (But we recommend that OldPawn should stop or move slowly)
    CaculateDesiredEndPosition();

    FVector NewPos;

    if (PathFunc)
    {
        // Use the PathFunction passed in from outside
        NewPos = PathFunc(StartPos, EndPos, ControlPos,  Alpha);
    }
    else
    {
        // Unsaveable: Quadratic Bezier Curve
        FVector Lerp1 = FMath::Lerp(StartPos, ControlPos, Alpha);
        FVector Lerp2 = FMath::Lerp(ControlPos, EndPos, Alpha);
        NewPos = FMath::Lerp(Lerp1, Lerp2, Alpha);
    }

    FRotator NewRot = FMath::Lerp(StartRot, EndRot, Alpha);

    // Set POV
    ViewTarget.POV.Location = NewPos;
    ViewTarget.POV.Rotation = NewRot;
    FillCameraCache(ViewTarget.POV);
    
    if (Alpha >= 1.f)
    {
        bTransitionActive = false;
        TargetActor.Reset();

        if (FinishCallback)
        {
            FinishCallback(EndPos, EndRot);
        }
    }
}

void ARPGPlayerCameraManager::ActorCameraViewTransition(AActor* NewTargetActor, float Duration, FPathFunction PathFunction, FOnFinished OnFinished, bool bKeepViewDirection)
{
    if (bTransitionActive)
    {
        return;
    }

    if (!NewTargetActor)
    {
        return;
    }

    TargetActor = NewTargetActor;

    if (URPGSpringArmComponent* SpringArm = PCOwner->GetPawn()->FindComponentByClass<URPGSpringArmComponent>())
    {
        const float LastTargetArmLength = SpringArm->TargetArmLength;
        if (URPGSpringArmComponent* NewSpringArm = TargetActor->FindComponentByClass<URPGSpringArmComponent>())
        {
            NewSpringArm->TargetArmLength = LastTargetArmLength;
        }
    }

    StartPos = GetCameraLocation();
    StartRot = GetCameraRotation();

    if (bKeepViewDirection)
    {
        CaculateDesiredEndPosition();
    }
    else
    {
        if (const UCameraComponent* TargetCamera = TargetActor->FindComponentByClass<UCameraComponent>())
        {
            EndPos = TargetCamera->GetComponentLocation();
            EndRot = TargetCamera->GetComponentRotation();
        }
        else
        {
            EndPos = TargetActor->GetActorLocation();
            EndRot = TargetActor->GetActorRotation();
        }
    }

    ControlPos = (StartPos + EndPos) * 0.5f + FVector(0, 0, 200.f);

    TransitionDuration = FMath::Max(Duration, KINDA_SMALL_NUMBER);
    TransitionElapsed = 0.f;
    bTransitionActive = true;

    PathFunc = PathFunction;
    FinishCallback = OnFinished;
}

void ARPGPlayerCameraManager::CaculateDesiredEndPosition()
{
    APawn* OldPawn = GetOwningPlayerController()->GetPawn();
    if (!OldPawn)
    {
        return;
    }

    // We need to adjust EndPos so that it does not overlap with other objects
    if (URPGSpringArmComponent* SpringArm = TargetActor->FindComponentByClass<URPGSpringArmComponent>())
    {
        EndPos = (StartPos - OldPawn->GetActorLocation()).GetSafeNormal() * SpringArm->TargetArmLength + TargetActor->GetActorLocation();
        EndRot = StartRot;

        FHitResult HitResult;

        FVector RootPos = SpringArm->GetComponentLocation();
        bool bHit = UKismetSystemLibrary::SphereTraceSingle(
            GetWorld(),
            RootPos,
            EndPos,
            SpringArm->ProbeSize,
            ETraceTypeQuery::TraceTypeQuery1,
            false,
            {},
            EDrawDebugTrace::None,
            HitResult,
            true,
            FColor::Red,
            FColor::Green,
            0.0f
        );

        // Caculate the position that the camera will be if it is colliding
        if (bHit)
        {
            EndPos = HitResult.ImpactPoint + HitResult.ImpactNormal * SpringArm->ProbeSize;

            SpringArm->CurrentEndLocation = EndPos;
        }
    }
}

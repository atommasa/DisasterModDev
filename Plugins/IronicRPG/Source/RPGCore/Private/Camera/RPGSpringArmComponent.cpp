// Copyright Ironic Studio. All Rights Reserved.


#include "Camera/RPGSpringArmComponent.h"
#include "Kismet/KismetSystemLibrary.h"

void URPGSpringArmComponent::UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag, bool bDoRotationLag, float DeltaTime)
{
    Super::UpdateDesiredArmLocation(false, bDoLocationLag, bDoRotationLag, DeltaTime);

    FVector DesiredLoc = GetSocketLocation(SocketName);

    if (CurrentEndLocation.IsZero())
    {
        CurrentEndLocation = DesiredLoc;
    }

    if (bDoTrace)
    {
        FVector StartLoc = GetComponentLocation() + TargetOffset;
        FVector EndLoc = DesiredLoc;
        FVector ArmDir = (EndLoc - StartLoc).GetSafeNormal();

        FHitResult HitResult;
        bool bHit = UKismetSystemLibrary::SphereTraceSingle(
            GetWorld(),
            StartLoc,
            EndLoc,
            ProbeSize,
            ETraceTypeQuery::TraceTypeQuery1,
            false,
            {},
            EDrawDebugTrace::None,
            HitResult,
            true
        );

        FVector TargetLoc = DesiredLoc;
        if (bHit)
        {
            CurrentEndLocation = HitResult.ImpactPoint + HitResult.ImpactNormal * ProbeSize;
        }
        else
        {
            float CurrentLength = FVector::Dist(StartLoc, CurrentEndLocation);
            float TargetLength = FVector::Dist(StartLoc, TargetLoc);

            CurrentLength = FMath::FInterpTo(CurrentLength, TargetLength, DeltaTime, CollisionSpeed);

            CurrentEndLocation = StartLoc + ArmDir * CurrentLength;
        }

        FTransform WorldCamTM(GetComponentRotation(), CurrentEndLocation);
        FTransform RelCamTM = WorldCamTM.GetRelativeTransform(GetComponentTransform());

        RelativeSocketLocation = RelCamTM.GetLocation();
        UpdateChildTransforms();
    }
}

void URPGSpringArmComponent::CameraZoom(float Scale)
{
    TargetArmLength = FMath::Clamp(TargetArmLength + Scale * ZoomRate, MinZoomLength, MaxZoomLength);
}


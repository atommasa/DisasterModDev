// Copyright Ironic Studio. All Rights Reserved.


#include "Navigation/AltitudeNavLinksProxy.h"
#include "Characters/BaseCharacter.h"
#include "Characters/Components/RPGCharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"

UWorld* UAltitudeNavLinksProxy::GetWorld() const
{
	return GetOuter()->GetWorld();
}

bool UAltitudeNavLinksProxy::OnLinkMoveStarted(UObject* PathComp, const FVector& DestPoint)
{
	Super::OnLinkMoveStarted(PathComp, DestPoint);

	UPathFollowingComponent* PathFollowingComp = Cast<UPathFollowingComponent>(PathComp);
	if (PathFollowingComp)
	{
		AActor* PathOwner = PathFollowingComp->GetOwner();
		const AController* ControllerOwner = Cast<AController>(PathOwner);
		if (ControllerOwner)
		{
			PathOwner = ControllerOwner->GetPawn();
		}

		StartLinkMovement(PathOwner, DestPoint);
	}

	return true;
}

void UAltitudeNavLinksProxy::StartLinkMovement(AActor* Agent, const FVector Destination)
{
	auto* Character = Cast<ABaseCharacter>(Agent);
	if (!Character)
	{
		return;
	}

	FVector CurrentLocation = Character->GetActorLocation();
	FVector Velocity = FVector::ZeroVector;

	bool bUpward = CurrentLocation.Z < Destination.Z;

	bool bSuccess = UGameplayStatics::SuggestProjectileVelocity_CustomArc(
		Character,
		Velocity,
		CurrentLocation,
		Destination,
		Character->GetCharacterMovement()->GetGravityZ(),
		bUpward ? 0.2f : 0.5f
	);

	if (!bSuccess || Velocity.IsZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to calculate jump velocity."));
		return;
	}

	FRotator JumpRotation = (Destination - CurrentLocation).GetSafeNormal2D().Rotation();
	Character->SetActorRotation(JumpRotation);
	
	Character->LaunchCharacter(Velocity, true, true);
}

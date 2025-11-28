// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Components/RPGCharacterMovementComponent.h"
#include "Characters/BaseCharacter.h"

URPGCharacterMovementComponent::URPGCharacterMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	RotationRate = FRotator(0.0f, 180.0f, 0.0f);
	bUseControllerDesiredRotation = false;
	bOrientRotationToMovement = true;

	GravityScale = 3.0f;

	JumpZVelocity = 1000.0f;
	AirControl = 0.5f;
	FallingLateralFriction = 2.0f;

	bRunPhysicsWithNoController = true;
}

void URPGCharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Set the RPGCharacterOwner to the owner of this component
	RPGCharacterOwner = Cast<ABaseCharacter>(GetOwner());

	MaxWalkSpeedBeforeSprint = MaxWalkSpeed;
}

void URPGCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	if (!RPGCharacterOwner->IsAIControlled())
	{
		SmoothRotateToTargetDirection(DeltaSeconds);
		UpdateJumpRotateGraceTime(DeltaSeconds);
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void URPGCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

}

void URPGCharacterMovementComponent::CharacterMove(const FVector& Direction, float Scale)
{
	if (!RPGCharacterOwner || Scale == 0.0f || Direction.IsNearlyZero())
	{
		return;
	}

	// Normalized direction to avoid scaling issues
	FVector NormalizedDir = Direction.GetSafeNormal();

	TargetDirection = (NormalizedDir * Scale).GetSafeNormal();

	// Add input vector scaled by the normalized direction and scale factor
	AddInputVector(NormalizedDir * Scale);
}

void URPGCharacterMovementComponent::CharacterJump()
{
	if (!RPGCharacterOwner || IsFalling())
	{
		return;
	}

	// Set remaining grace time for jump rotation
	JumpRotateGraceTimeRemaining = JumpRotateGraceDuration;

	// Trigger the jump action on the RPGCharacterOwner
	RPGCharacterOwner->Jump();
}

void URPGCharacterMovementComponent::CharacterSprint()
{
	if (!RPGCharacterOwner || IsFalling())
	{
		return;
	}

	if (IsWalking())
	{
		MaxWalkSpeed = MaxSprintSpeed;
	}
}

void URPGCharacterMovementComponent::CharacterDash()
{
	if (!RPGCharacterOwner || IsFalling())
	{
		return;
	}
	
	FRotator CharacterRotation = RPGCharacterOwner->GetActorRotation();
	FVector DashDirection = CharacterRotation.Vector().GetSafeNormal();
	if (!DashDirection.IsNearlyZero())
	{
		FVector DashVelocity = DashDirection * DashSpeed;
		RPGCharacterOwner->LaunchCharacter(DashVelocity, true, true);
		DashCount--;
	}
}

void URPGCharacterMovementComponent::SmoothRotateToTargetDirection(float DeltaTime)
{
	if (!RPGCharacterOwner || TargetDirection.IsNearlyZero())
	{
		return;
	}

	// Check if the character is falling and if the grace time has expired
	if (IsFalling() && JumpRotateGraceTimeRemaining <= 0.0f)
	{
		return;
	}

	// Calculate the desired rotation based on the target direction
	FRotator DesiredRotation = TargetDirection.Rotation();

	// Smoothly interpolate the character's rotation towards the desired rotation
	FRotator CurrentRotation = RPGCharacterOwner->GetActorRotation();
	FRotator NewRotation = FMath::RInterpTo(CurrentRotation, DesiredRotation, DeltaTime, 10.0f);

	RPGCharacterOwner->SetActorRotation(NewRotation);
}

void URPGCharacterMovementComponent::UpdateJumpRotateGraceTime(float DeltaTime)
{
	if (JumpRotateGraceTimeRemaining > 0.0f)
	{
		JumpRotateGraceTimeRemaining -= DeltaTime;
	}
}

void URPGCharacterMovementComponent::ResetToWalkingSpeed()
{
	if (!RPGCharacterOwner)
	{
		return;
	}

	// Reset the max walk speed to the original value
	MaxWalkSpeed = MaxWalkSpeedBeforeSprint;
}

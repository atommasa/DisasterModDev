// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RPGCharacterMovementComponent.generated.h"

/**
 * URPGCharacterMovementComponent is a custom character movement component for RPG characters.
 */
UCLASS(meta=(BlueprintSpawnableComponent))
class RPGCORE_API URPGCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
	
public:
	URPGCharacterMovementComponent();

	UPROPERTY(BlueprintReadOnly, Category = "Character Movement")
	class ABaseCharacter* RPGCharacterOwner = nullptr;

protected:
	virtual void InitializeComponent() override;

	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	UFUNCTION(BlueprintCallable, Category = "Character Movement")
	virtual void CharacterMove(const FVector& Direction, float Scale);

	UFUNCTION(BlueprintCallable, Category = "Character Movement")
	virtual void CharacterJump();

	UFUNCTION(BlueprintCallable, Category = "Character Movement")
	virtual void CharacterSprint();

	UFUNCTION(BlueprintCallable, Category = "Character Movement")
	virtual void CharacterDash();

public:
	UFUNCTION(BlueprintPure, Category = "Character Movement")
	float GetSpeed() const { return Velocity.Length(); }

	UFUNCTION(BlueprintPure, Category = "Character Movement")
	FVector GetVelocityDirection() const{ return Velocity.IsNearlyZero() ? FVector::ZeroVector : Velocity.GetSafeNormal(); }

public:
	void SmoothRotateToTargetDirection(float DeltaTime);
	void UpdateJumpRotateGraceTime(float DeltaTime);
	void ResetToWalkingSpeed();

	UPROPERTY(BlueprintReadOnly, Category = "Character Movement")
	FVector TargetDirection = FVector::ZeroVector;

protected:
	// Jumping adjustable time (e.g. turn within 0.2 seconds)
	UPROPERTY(EditDefaultsOnly, Category = "Character Movement: Jumping / Falling")
	float JumpRotateGraceDuration = 0.2f;

	float JumpRotateGraceTimeRemaining = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Sprint", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float MaxSprintSpeed = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Sprint", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float DashSpeed = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Sprint")
	int32 DashCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Sprint")
	float DashCooldown = 1.0f;

	float MaxWalkSpeedBeforeSprint = 0.0f;

};

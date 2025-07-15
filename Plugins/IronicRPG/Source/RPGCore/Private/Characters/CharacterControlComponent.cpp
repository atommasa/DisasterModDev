// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/CharacterControlComponent.h"
#include "Characters/BaseCharacter.h"
#include "Characters/Components/RPGCharacterMovementComponent.h"

#include "EnhancedInputSubsystems.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"

void UCharacterControlComponent::InitializeComponent()
{
	Super::InitializeComponent();

	
}

void UCharacterControlComponent::BeginPlay()
{
	Super::BeginPlay();

	Controller = Cast<APlayerController>(GetOwner());
	if (!Controller)
	{
		return;
	}

	ControlledCharacter = Cast<ABaseCharacter>(Controller->GetPawn());
	if (!ControlledCharacter)
	{
		return;
	}

	CharacterMovement = Cast<URPGCharacterMovementComponent>(ControlledCharacter->GetCharacterMovement());
	if (!CharacterMovement)
	{
		// UE_LOG(LogTemp, Warning, TEXT("CharacterControlComponent: No valid movement component found for character %s"), *ControlledCharacter->GetName());
		return;
	}

	// Find the input subsystem, and add the input mapping context
	InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer());
	if (InputSubsystem)
	{
		if (InputMapping)
		{
			InputSubsystem->AddMappingContext(InputMapping, 0);
		}
	}

	// Bind input actions
	if (UEnhancedInputComponent* InputComponent = CastChecked<UEnhancedInputComponent>(Controller->InputComponent))
	{
		if (LookAction)
		{
			InputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &UCharacterControlComponent::Look);
		}
		if (MoveAction)
		{
			InputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &UCharacterControlComponent::Move);
			InputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &UCharacterControlComponent::EndMove);
		}
		if (JumpAction)
		{
			InputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &UCharacterControlComponent::Jump);
		}
		if (SprintAction)
		{
			InputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &UCharacterControlComponent::Sprint);
		}
	}
}

void UCharacterControlComponent::Look(const FInputActionValue& Value)
{
	if (!ControlledCharacter)
	{
		return;
	}

	// Assuming Value is a vector with X and Y components for look direction
	FVector2D LookInput = Value.Get<FVector2D>();
	ControlledCharacter->AddControllerYawInput(LookInput.X);
	ControlledCharacter->AddControllerPitchInput(-LookInput.Y);
}

void UCharacterControlComponent::Move(const FInputActionValue& Value)
{
	if (!Controller || !ControlledCharacter || !CharacterMovement)
	{
		return;
	}

	FVector2D MovementVector = Value.Get<FVector2D>();

	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.0f, Rotation.Yaw, 0.0f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	const FVector MoveDirection = (ForwardDirection * MovementVector.Y) + (RightDirection * MovementVector.X);

	CharacterMovement->CharacterMove(MoveDirection, MovementVector.Length());
}

void UCharacterControlComponent::EndMove(const FInputActionValue& Value)
{
	if (!CharacterMovement)
	{
		return;
	}

	// Reset the character movement to walking speed when movement ends
	CharacterMovement->ResetToWalkingSpeed();
}

void UCharacterControlComponent::Jump()
{
	if (!CharacterMovement)
	{
		return;
	}

	CharacterMovement->CharacterJump();
}

void UCharacterControlComponent::Sprint(const FInputActionValue& Value)
{
	if (!CharacterMovement)
	{
		return;
	}

	CharacterMovement->CharacterSprint();
}

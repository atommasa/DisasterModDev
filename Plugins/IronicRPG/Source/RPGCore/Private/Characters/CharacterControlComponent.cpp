// Copyright Ironic Studio. All Rights Reserved.


#include "Characters/CharacterControlComponent.h"
#include "Characters/BaseCharacter.h"
#include "Controllers/RPGPlayerController.h"
#include "Characters/Components/RPGCharacterMovementComponent.h"
#include "Characters/Components/CharacterAbilitySystemComponent.h"

#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"

#include "Camera/RPGSpringArmComponent.h"

UCharacterControlComponent::UCharacterControlComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AllowedControlMode = ERPGControlMode::Gameplay;
}

void UCharacterControlComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UCharacterControlComponent::BeginPlay()
{
	Super::BeginPlay();

	// Listen for controlled character changes
	Controller->OnControlledCharacterChanged.AddUniqueDynamic(this, &UCharacterControlComponent::OnControlledCharacterChanged);
	
	// Initial update of the controlled character
	UpdateControlledCharacter();

	EnableAllInputs();

	// Bind input actions
	if (UEnhancedInputComponent* InputComponent = CastChecked<UEnhancedInputComponent>(Controller->InputComponent))
	{
		if (LookAction)
		{
			InputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &UCharacterControlComponent::Look);
		}
		if (ZoomAction)
		{
			InputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &UCharacterControlComponent::Zoom);
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

void UCharacterControlComponent::UpdateControlledCharacter()
{
	if (!Controller)
	{
		return;
	}

	ControlledCharacter = Cast<ABaseCharacter>(Controller->GetPawn());
	if (ControlledCharacter)
	{
		CharacterMovement = Cast<URPGCharacterMovementComponent>(ControlledCharacter->GetCharacterMovement());
		if (CharacterMovement)
		{
			CharacterMovement->TargetDirection = FVector::ZeroVector;
		}
	}
}

void UCharacterControlComponent::Look_Implementation(const FInputActionValue& Value)
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

void UCharacterControlComponent::Zoom_Implementation(const FInputActionValue& Value)
{
	if (ControlledCharacter)
	{
		if (URPGSpringArmComponent* SpringArm = ControlledCharacter->FindComponentByClass<URPGSpringArmComponent>())
		{
			SpringArm->CameraZoom(Value.Get<float>());
		}
	}
}

void UCharacterControlComponent::Move_Implementation(const FInputActionValue& Value)
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

void UCharacterControlComponent::EndMove_Implementation(const FInputActionValue& Value)
{
	if (!CharacterMovement)
	{
		return;
	}

	// Reset the character movement to walking speed when movement ends
	CharacterMovement->ResetToWalkingSpeed();
}

void UCharacterControlComponent::Jump_Implementation()
{
	if (!ControlledCharacter || !ControlledCharacter->AbilitySystemComponent)
	{
		return;
	}

	ControlledCharacter->AbilitySystemComponent->TryActivateAbilityById(ControlledCharacter->GetJumpAbilityId());
}

void UCharacterControlComponent::Sprint_Implementation(const FInputActionValue& Value)
{
	if (!CharacterMovement)
	{
		return;
	}

	CharacterMovement->CharacterSprint();
}

void UCharacterControlComponent::OnControlledCharacterChanged(const ABaseCharacter* NewCharacter, const ABaseCharacter* OldCharacter)
{
	// We need to update the current character of this component first
	UpdateControlledCharacter();

	// Recover contexts' availability
	EnableAllInputs();
}

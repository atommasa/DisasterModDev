// Copyright Ironic Studio. All Rights Reserved.


#include "Controllers/BaseControlComponent.h"

void UBaseControlComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// Try to get the owner controller
	Controller = Cast<ARPGPlayerController>(GetOwner());
	
	check(Controller);

	// Find the input subsystem
	InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer());

	check(InputSubsystem);

	Controller->OnControlModeChanged.AddUniqueDynamic(this, &UBaseControlComponent::OnControlModeChanged);
}

void UBaseControlComponent::OnControlModeChanged(ERPGControlMode NewControlMode)
{
	NewControlMode == AllowedControlMode ? EnableAllInputs() : DisableAllInputs();
}

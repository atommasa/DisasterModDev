// Copyright Ironic Studio. All Rights Reserved.


#include "Controllers/RPGPlayerController.h"
#include "Characters/BaseCharacter.h"

#include "Controllers/BaseControlComponent.h"

DEFINE_LOG_CATEGORY(LogPC);

ARPGPlayerController::ARPGPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

void ARPGPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
}

void ARPGPlayerController::PossessCharacter(APawn* NewCharacter, ERPGControlMode ControlMode)
{
	if (NewCharacter)
	{
		Possess(NewCharacter);
		SetControlMode(ControlMode);
	}
}

void ARPGPlayerController::OnPossess(APawn* InPawn)
{
	// Get the old and new characters (the old character must get before calling Super::OnPossess)
	ABaseCharacter* OldCharacter = Cast<ABaseCharacter>(GetPawn());
	ABaseCharacter* NewCharacter = Cast<ABaseCharacter>(InPawn);
	if (NewCharacter)
	{
		NewCharacter->SetAIControl(false);
	}

	Super::OnPossess(InPawn);

	if (OldCharacter)
	{
		OldCharacter->SetAIControl(true);
	}

	// Broadcast that the controlled character has changed
	OnControlledCharacterChanged.Broadcast();
}

void ARPGPlayerController::OnUnPossess()
{
	Super::OnUnPossess();
}

void ARPGPlayerController::SetControlMode(ERPGControlMode NewControlMode)
{
	OnControlModeChanged.Broadcast(NewControlMode);
}

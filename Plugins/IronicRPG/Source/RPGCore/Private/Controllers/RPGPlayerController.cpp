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
	
	SetControlMode(CurrentControlMode);

}

void ARPGPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnControlModeChanged.Clear();

	Super::EndPlay(EndPlayReason);
}

void ARPGPlayerController::PossessCharacter(APawn* NewCharacter)
{
	PossessCharacterWithMode(NewCharacter, CurrentControlMode);
}

void ARPGPlayerController::PossessCharacterWithMode(APawn* NewCharacter, ERPGControlMode ControlMode)
{
	if (!IsValid(NewCharacter))
	{
		UE_LOG(LogPC, Warning, TEXT("PossessCharacterWithMode was called with an invalid pawn."));
		return;
	}

	// Re-possessing the pawn we already own is not a no-op for this controller's
	// OnPossess implementation: OldCharacter and NewCharacter become the same
	// actor, which would otherwise re-enable its AI controller and let it take
	// the pawn away from this PlayerController. This commonly happens when the
	// current party member is reused by SpawnNewPartyMembers.
	if (GetPawn() != NewCharacter)
	{
		Possess(NewCharacter);
	}
	else
	{
		UE_LOG(LogPC, Verbose, TEXT("PossessCharacterWithMode skipped; already possessing %s."), *GetNameSafe(NewCharacter));
	}
	
	if (ControlMode != CurrentControlMode)
	{
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

	// When possession is requested for the pawn already owned by this
	// PlayerController, OldCharacter and NewCharacter are the same actor.
	// Never restore AI control in that case: the AI controller would possess
	// the current player pawn and leave this controller with no controlled pawn.
	if (IsValid(OldCharacter) && OldCharacter != NewCharacter)
	{
		OldCharacter->SetAIControl(true);
	}

	// Broadcast that the controlled character has changed
	OnControlledCharacterChanged.Broadcast(NewCharacter, OldCharacter);
}

void ARPGPlayerController::OnUnPossess()
{
	Super::OnUnPossess();
}

void ARPGPlayerController::SetControlMode(ERPGControlMode NewControlMode)
{
	UE_LOG(LogPC, Log, TEXT("Control mode changed to %s"), *UEnum::GetValueAsString(NewControlMode));
	
	CurrentControlMode = NewControlMode;

	FlushPressedKeys();

	OnControlModeChanged.Broadcast(static_cast<int32>(NewControlMode));
}

// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Controllers/BaseControlComponent.h"
#include "CharacterControlComponent.generated.h"

class ABaseCharacter;
class URPGCharacterMovementComponent;

class UInputMappingContext;
class UInputAction;

/**
 * The component that handles character control input and movement
 */
UCLASS( meta=(BlueprintSpawnableComponent) )
class RPGCORE_API UCharacterControlComponent : public UBaseControlComponent
{
	GENERATED_BODY()

public:	
	UCharacterControlComponent(const FObjectInitializer& ObjectInitializer);

	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void UpdateControlledCharacter();
		
public:
	UPROPERTY(BlueprintReadOnly, Category = "Character")
	ABaseCharacter* ControlledCharacter = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Character")
	URPGCharacterMovementComponent* CharacterMovement = nullptr;

public:
	virtual void EnableAllInputs() override;
	virtual void DisableAllInputs() override;

protected:
	// Look input handler
	UFUNCTION(BlueprintCallable, Category = "Character Control")
	virtual void Look(const FInputActionValue& Value);

	// Zoom input handler
	UFUNCTION(BlueprintCallable, Category = "Character Control")
	virtual void Zoom(const FInputActionValue& Value);

	// Move input handler
	UFUNCTION(BlueprintCallable, Category = "Character Control")
	virtual void Move(const FInputActionValue& Value);
	
	// EndMove input handler
	UFUNCTION(BlueprintCallable, Category = "Character Control")
	virtual void EndMove(const FInputActionValue& Value);

	// Jump input handler
	UFUNCTION(BlueprintCallable, Category = "Character Control")
	virtual void Jump();

	// Sprint input handler
	UFUNCTION(BlueprintCallable, Category = "Character Control")
	virtual void Sprint(const FInputActionValue& Value);

protected: // Ehanced Input
	// Input mapping context
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* MoveContext = nullptr;
	DEFINE_INPUTMAPPING_FUNCTIONS(InputSubsystem, MoveContext, ControlPriority)

	// Action to look around
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* LookAction = nullptr;

	// Action to zoom in/out
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* ZoomAction = nullptr;

	// Action to move the character
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* MoveAction = nullptr;

	// Action to make character jump
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* JumpAction = nullptr;

	// Action to let character sprint
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* SprintAction = nullptr;

protected:
	// Called when the controlled character changes
	UFUNCTION()
	void OnControlledCharacterChanged();

};

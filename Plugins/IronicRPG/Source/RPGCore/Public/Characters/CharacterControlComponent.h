// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Controllers/BaseControlComponent.h"
#include "CharacterControlComponent.generated.h"

class APlayableCharacter;
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
		
protected:
	UPROPERTY(BlueprintReadOnly, Category = "Character")
	APlayableCharacter* ControlledCharacter = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Character")
	URPGCharacterMovementComponent* CharacterMovement = nullptr;

public:
	virtual void EnableAllInputs() override;
	virtual void DisableAllInputs() override;

protected:
	// Look input handler
	UFUNCTION(BlueprintNativeEvent, Category = "Character Control")
	void Look(const FInputActionValue& Value);
	virtual void Look_Implementation(const FInputActionValue& Value);

	// Zoom input handler
	UFUNCTION(BlueprintNativeEvent, Category = "Character Control")
	void Zoom(const FInputActionValue& Value);
	virtual void Zoom_Implementation(const FInputActionValue& Value);

	// Move input handler
	UFUNCTION(BlueprintNativeEvent, Category = "Character Control")
	void Move(const FInputActionValue& Value);
	virtual void Move_Implementation(const FInputActionValue& Value);
	
	// EndMove input handler
	UFUNCTION(BlueprintNativeEvent, Category = "Character Control")
	void EndMove(const FInputActionValue& Value);
	virtual void EndMove_Implementation(const FInputActionValue& Value);

	// Jump input handler
	UFUNCTION(BlueprintNativeEvent, Category = "Character Control")
	void Jump();
	virtual void Jump_Implementation();

	// Sprint input handler
	UFUNCTION(BlueprintNativeEvent, Category = "Character Control")
	void Sprint(const FInputActionValue& Value);
	virtual void Sprint_Implementation(const FInputActionValue& Value);

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

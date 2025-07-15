// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterControlComponent.generated.h"

class ABaseCharacter;
class URPGCharacterMovementComponent;

class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;
class UInputAction;

UCLASS( meta=(BlueprintSpawnableComponent) )
class RPGCORE_API UCharacterControlComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	virtual void InitializeComponent() override;

	virtual void BeginPlay() override;
		
public:
	UPROPERTY(BlueprintReadOnly, Category = "Character")
	APlayerController* Controller = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Character")
	ABaseCharacter* ControlledCharacter = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Character")
	URPGCharacterMovementComponent* CharacterMovement = nullptr;

protected:
	// Look input handler
	UFUNCTION(BlueprintCallable, Category = "Character Control")
	void Look(const FInputActionValue& Value);

	// Move input handler
	UFUNCTION(BlueprintCallable, Category = "Character Control")
	void Move(const FInputActionValue& Value);
	
	// EndMove input handler
	UFUNCTION(BlueprintCallable, Category = "Character Control")
	void EndMove(const FInputActionValue& Value);

	// Jump input handler
	UFUNCTION(BlueprintCallable, Category = "Character Control")
	void Jump();

	// Sprint input handler
	UFUNCTION(BlueprintCallable, Category = "Character Control")
	void Sprint(const FInputActionValue& Value);

protected: // Ehanced Input
	// Input subsystem
	UPROPERTY()
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = nullptr;

	// Input mapping context
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* InputMapping = nullptr;

	// Input actions
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* LookAction = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* MoveAction = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* JumpAction = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* SprintAction = nullptr;
};

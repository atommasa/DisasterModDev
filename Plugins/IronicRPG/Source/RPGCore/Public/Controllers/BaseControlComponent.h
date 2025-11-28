// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Controllers/ControllerHelperMacros.h"
#include "Controllers/RPGPlayerController.h"
#include "BaseControlComponent.generated.h"

class UEnhancedInputLocalPlayerSubsystem;

/**
 * The base class for all control conponents, such as character control and UI control.
 * Those components should be added to the ARPGPlayerController.
 */
UCLASS(Abstract, ClassGroup=(ControlComponents), meta=(BlueprintSpawnableComponent) )
class RPGCORE_API UBaseControlComponent : public UActorComponent
{
	GENERATED_BODY()
		
protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable, Category = "Control")
	virtual void EnableAllInputs() {}

	UFUNCTION(BlueprintCallable, Category = "Control")
	virtual void DisableAllInputs() {}

public:
	// Get the priority of this control component
	UFUNCTION(BlueprintCallable, Category = "Control")
	int32 GetControlPriority() const { return ControlPriority; }

protected:
	// The priority of this control component, higher priority components get input first
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Control")
	int32 ControlPriority = 0;

	// The controller that owns this component
	UPROPERTY(BlueprintReadOnly, Category = "Character")
	ARPGPlayerController* Controller = nullptr;

	// Input subsystem
	UPROPERTY()
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = nullptr;

protected:
	UFUNCTION()
	virtual void OnControlModeChanged(ERPGControlMode NewControlMode);

	UPROPERTY(EditAnywhere, Category = "Control")
	ERPGControlMode AllowedControlMode = ERPGControlMode::None;

};

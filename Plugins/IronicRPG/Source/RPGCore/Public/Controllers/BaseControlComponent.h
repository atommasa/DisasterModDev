// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Controllers/RPGPlayerController.h"
#include "BaseControlComponent.generated.h"

class UEnhancedInputLocalPlayerSubsystem;

USTRUCT(BlueprintType)
struct FAdditionalInputData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	UInputAction* InputAction = nullptr;

	UPROPERTY(EditAnywhere)
	ETriggerEvent TriggerEvent = ETriggerEvent::Triggered;

	UPROPERTY(EditAnywhere, meta=(FunctionReference, PrototypeFunction = "/Script/RPGCore.BaseControlComponent.Prototype_OnAdditionalInputTriggered", DefaultBindingName = "InputTriggered"))
	FMemberReference OnInputTriggered;
};

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
	virtual void EnableAllInputs();

	UFUNCTION(BlueprintCallable, Category = "Control")
	virtual void DisableAllInputs();

public:
	// Get the priority of this control component
	UFUNCTION(BlueprintCallable, Category = "Control")
	int32 GetControlPriority() const { return ControlPriority; }

protected:
	// Input mapping context
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* InputMappingContext = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TArray<FAdditionalInputData> AdditionalInputActions;

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
	virtual void OnControlModeChanged(const int32& NewControlMode);

	UPROPERTY(EditAnywhere, Category = "Control")
	ERPGControlMode AllowedControlMode = ERPGControlMode::None;

#if WITH_EDITOR
	// This Prototype function defines the signature of the function for the editor
	UFUNCTION(BlueprintInternalUseOnly)
	void Prototype_OnAdditionalInputTriggered(const FInputActionValue& InputActionValue) {}
#endif

};

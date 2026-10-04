// Copyright Ironic Studio. All Rights Reserved.


#include "Controllers/BaseControlComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"

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

	if (UEnhancedInputComponent* InputComponent = CastChecked<UEnhancedInputComponent>(Controller->InputComponent))
	{
		for (const FAdditionalInputData& Data : AdditionalInputActions)
		{
			InputComponent->BindActionValueLambda(Data.InputAction, Data.TriggerEvent, [this, Data](const FInputActionValue& InputActionValue)
				{
					UObject* Owner = GetOwner();
					check(Owner);

					if (UFunction* Func = Data.OnInputTriggered.ResolveMember<UFunction>(Owner->GetClass()))
					{
						struct FInputActionEventParams
						{
							FInputActionValue Value;
						};

						FInputActionEventParams Params;
						Params.Value = InputActionValue;

						Owner->ProcessEvent(Func, &Params);
					}
				});
		}
	}
}

void UBaseControlComponent::EnableAllInputs()
{
	if (InputSubsystem)
	{
		InputSubsystem->AddMappingContext(InputMappingContext, ControlPriority);
	}
}

void UBaseControlComponent::DisableAllInputs()
{
	if (InputSubsystem)
	{
		InputSubsystem->RemoveMappingContext(InputMappingContext);
	}
}

void UBaseControlComponent::OnControlModeChanged(const int32& NewControlMode)
{
	(NewControlMode & static_cast<int32>(AllowedControlMode)) != 0 ? EnableAllInputs() : DisableAllInputs();
}

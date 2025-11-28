// Copyright Ironic Studio. All Rights Reserved.


#include "Components/UIControlComponent.h"
#include "UISubsystem.h"
#include "Widgets/WidgetBase.h"
#include "Widgets/MenuBase.h"

#include "EnhancedInputSubsystems.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"

UUIControlComponent::UUIControlComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;

	AllowedControlMode = ERPGControlMode::UI;
}

void UUIControlComponent::InitializeComponent()
{
	Super::InitializeComponent();


}

// Called when the game starts
void UUIControlComponent::BeginPlay()
{
	Super::BeginPlay();

	// Find the UISubsystem
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		UISubsystem = GI->GetSubsystem<UUISubsystem>();
	}

	check(UISubsystem);

	EnableAllInputs();

	// Bind input actions
	if (UEnhancedInputComponent* InputComponent = CastChecked<UEnhancedInputComponent>(Controller->InputComponent))
	{
		if (NavigateAction)
		{
			InputComponent->BindAction(NavigateAction, ETriggerEvent::Triggered, this, &UUIControlComponent::Navigate);
			InputComponent->BindAction(NavigateAction, ETriggerEvent::Completed, this, &UUIControlComponent::StopNavigate);
		}
		if (SwitchUIPageAction)
		{
			InputComponent->BindAction(SwitchUIPageAction, ETriggerEvent::Triggered, this, &UUIControlComponent::SwitchUIPage);
		}
		if (ConfirmAction)
		{
			InputComponent->BindAction(ConfirmAction, ETriggerEvent::Started, this, &UUIControlComponent::Confirm);
		}
		if (CancelAction)
		{
			InputComponent->BindAction(CancelAction, ETriggerEvent::Started, this, &UUIControlComponent::Cancel);
		}
	}
}

// Called every frame
void UUIControlComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bCanNavigation)
	{
		if (CurrentNavigationTime < NavigationCooldown)
		{
			CurrentNavigationTime += DeltaTime;
		}
		else
		{
			bCanNavigation = true;
		}
	}
}

void UUIControlComponent::EnableAllInputs()
{
	EnableUIContext();
}

void UUIControlComponent::DisableAllInputs()
{
	DisableUIContext();
}

void UUIControlComponent::Navigate(const FInputActionValue& Value)
{
	if (!bCanNavigation)
	{
		return;
	}

	FVector2D Dir = Value.Get<FVector2D>();

	// Get the top UI
	bool bSuccess = false;
	if (UWidgetBase * TopUI = GetTopUI())
	{
		if (TopUI->Implements<UNavigationInterface>())
		{
			if (INavigationInterface* NaviUI = Cast<INavigationInterface>(TopUI))
			{
				// Call the Navigate function on the top UI
				if (Dir.X > 0.5f)
					bSuccess = NaviUI->Navigate(EUINavigation::Right);
				else if (Dir.X < -0.5f)
					bSuccess = NaviUI->Navigate(EUINavigation::Left);
				else if (Dir.Y > 0.5f)
					bSuccess = NaviUI->Navigate(EUINavigation::Up);
				else if (Dir.Y < -0.5f)
					bSuccess = NaviUI->Navigate(EUINavigation::Down);
			}
		}
	}

	bCanNavigation = !bSuccess;
	bIsNavigating = bSuccess;

	CurrentNavigationTime = 0.0f;
}

void UUIControlComponent::StopNavigate(const FInputActionValue& Value)
{
	bCanNavigation = true;
	bIsNavigating = false;
}

void UUIControlComponent::SwitchUIPage(const FInputActionValue& Value)
{
	float Dir = Value.Get<float>();

	// Get the top UI
	UWidgetBase* TopUI = GetTopUI();
	if (TopUI)
	{
		// Call the SwitchUIPage function on the top UI
		if (UMenuBase* MenuUI = Cast<UMenuBase>(TopUI))
		{
			MenuUI->SwitchWidgetByDirection(Dir > 0 ? EUINavigation::Right : EUINavigation::Left);
		}
	}
}

void UUIControlComponent::Confirm()
{
	// Get the top UI
	UWidgetBase* TopUI = GetTopUI();
	if (TopUI)
	{
		// Call the Confirm function on the top UI
		TopUI->Confirm();
	}
}

void UUIControlComponent::Cancel()
{
	// Get the top UI
	UWidgetBase* TopUI = GetTopUI();
	if (TopUI)
	{
		// Call the Cancel function on the top UI
		TopUI->Cancel();
	}
}

UWidgetBase* UUIControlComponent::GetTopUI() const
{
	return UISubsystem ? UISubsystem->GetTopUI() : nullptr;
}

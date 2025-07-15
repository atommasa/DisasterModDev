// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/UIControlComponent.h"
#include "UISubsystem.h"
#include "Widgets/WidgetBase.h"
#include "Widgets/MenuBase.h"

#include "EnhancedInputSubsystems.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"

// Sets default values for this component's properties
UUIControlComponent::UUIControlComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
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

	// Find the input subsystem, and add the input mapping context
	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
		if (InputSubsystem)
		{
			if (InputMapping)
			{
				InputSubsystem->AddMappingContext(InputMapping, 0);
			}
		}

		// Bind input actions
		if (UEnhancedInputComponent* InputComponent = CastChecked<UEnhancedInputComponent>(PC->InputComponent))
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

void UUIControlComponent::Navigate(const FInputActionValue& Value)
{
	if (!bCanNavigation)
	{
		return;
	}

	FVector2D Dir = Value.Get<FVector2D>();

	// Get the top UI
	bool bSuccess = false;
	UWidgetBase* TopUI = GetTopUI();
	if (TopUI && TopUI->Implements<UNavigationInterface>())
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

	if (bSuccess)
	{
		bCanNavigation = false;
		bIsNavigating = true;
	}
	else
	{
		bCanNavigation = true;
		bIsNavigating = false;
	}

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

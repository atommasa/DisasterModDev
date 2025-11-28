// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Controllers/BaseControlComponent.h"
#include "UIControlComponent.generated.h"

class UWidgetBase;
class UUISubsystem;

class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;
class UInputAction;

UCLASS( meta=(BlueprintSpawnableComponent) )
class UIFRAMEWORK_API UUIControlComponent : public UBaseControlComponent
{
	GENERATED_BODY()

protected:
	UUIControlComponent(const FObjectInitializer& ObjectInitializer);

	virtual void InitializeComponent() override;

	// Called when the game starts
	virtual void BeginPlay() override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	virtual void EnableAllInputs() override;
	virtual void DisableAllInputs() override;

public: // UActorComponent
	// Navigate in the specified direction
	UFUNCTION(BlueprintCallable, Category = "UIControl")
	void Navigate(const FInputActionValue& Value);

	// Call when navigation stops
	UFUNCTION(BlueprintCallable, Category = "UIControl")
	void StopNavigate(const FInputActionValue& Value);

	// Switch UI page
	UFUNCTION(BlueprintCallable, Category = "UIControl")
	void SwitchUIPage(const FInputActionValue& Value);

	// Confirm current UI selection
	UFUNCTION(BlueprintCallable, Category = "UIControl")
	void Confirm();

	// Cancel current UI
	UFUNCTION(BlueprintCallable, Category = "UIControl")
	void Cancel();

	// Navigation cooldown time
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UIControl")
	float NavigationCooldown = 0.15f;

	// Navigation time
	float CurrentNavigationTime = 0.0f;

	// Whether the UI can navigate
	bool bCanNavigation = true;

	// Whether the UI is currently navigating
	bool bIsNavigating = false;

public:
	// Cached reference to UI subsystem
	UPROPERTY()
	UUISubsystem* UISubsystem = nullptr;

	// Get the top UI
	UWidgetBase* GetTopUI() const;

protected: // Ehanced Input
	// Input mapping context
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputMappingContext* UIContext = nullptr;
	DEFINE_INPUTMAPPING_FUNCTIONS(InputSubsystem, UIContext, ControlPriority)

	// Input action for navigating
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* NavigateAction = nullptr;

	// Input action for switching UI page
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* SwitchUIPageAction = nullptr;

	// Input action for confirming
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* ConfirmAction = nullptr;

	// Input action for canceling
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	UInputAction* CancelAction = nullptr;

};

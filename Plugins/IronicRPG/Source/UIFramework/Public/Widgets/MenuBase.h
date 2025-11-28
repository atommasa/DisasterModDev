// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/WidgetBase.h"
#include "NavigationInterface.h"
#include "MenuBase.generated.h"

/**
 * This class is used to create a menu with multiple widgets that can be switched between.
 * Usually used for menus with multiple pages, like settings or inventory.
 */
UCLASS()
class UIFRAMEWORK_API UMenuBase : public UWidgetBase, public INavigationInterface
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;

public: // Widget Actions
	// Switch widget
	UFUNCTION(BlueprintCallable)
	bool SwitchWidget(UWidgetBase* Widget);

	// Switch widget by index
	UFUNCTION(BlueprintCallable)
	bool SwitchWidgetByIndex(int32 Index);

	// Switch widget by direction
	UFUNCTION(BlueprintCallable)
	bool SwitchWidgetByDirection(EUINavigation Direction);

	UFUNCTION(BlueprintImplementableEvent)
	void OnSwitchWidget(int32 CurrentIndex, int32 LastIndex);

public: // UDisasterWidgetBase
	// Confirm current selection button
	virtual bool Confirm_Implementation() override;

	// Navigate to the next button in the specified direction
	virtual bool Navigate(EUINavigation Direction) override;

public:
	UFUNCTION(BlueprintCallable)
	UWidgetBase* GetCurrentWidget() const { return CurrentWidget; }

protected:
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	class UWidgetSwitcher* WidgetSwitcher;

	UPROPERTY()
	UWidgetBase* CurrentWidget = nullptr;

	// If true, allows switching input
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu")
	bool bAllowSwitchingInput = true;

	// If true, switching pages will wrap around when reaching the ends
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu")
	bool bWrapSwitching = true;

protected: // Sound Effect
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	USoundBase* SwitchWidgetSound;

};

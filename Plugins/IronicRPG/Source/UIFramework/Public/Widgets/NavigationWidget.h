// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/WidgetBase.h"
#include "NavigationInterface.h"
#include "NavigationWidget.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class UIFRAMEWORK_API UNavigationWidget : public UWidgetBase, public INavigationInterface
{
	GENERATED_BODY()

protected: // UWidgetBase
	virtual void NativeConstruct() override;

protected:
	virtual void RebuildNavigation();

	// Get all buttons and their coordinate in the navigation grid.
	// Called by RebuildNavigation() internally.
	virtual void CollectNavigatableButtons(TMap<FIntPoint, UInteractiveWidget*>& OutButtons) PURE_VIRTUAL(UNavigationWidget::CollectNavigatableButtons, );

protected:
	// Grid buttons
	UPROPERTY(BlueprintReadWrite, Category = "Buttons")
	TMap<FIntPoint, class UInteractiveWidget*> GridButtonMap;

	// Current focus button
	UPROPERTY(BlueprintReadWrite, Category = "Buttons")
	FIntPoint CurrentFocusCoord;

	// Default focus button
	UPROPERTY(EditDefaultsOnly, Category = "Buttons")
	FIntPoint DefaultFocusCoord;

	UPROPERTY(BlueprintReadWrite, Category = "Container")
	int32 MaxRow;

	UPROPERTY(BlueprintReadWrite, Category = "Container")
	int32 MaxCol;

public:
	// Add a button to the container at the specified coordinate
	UFUNCTION(BlueprintCallable)
	virtual void AddButton(FIntPoint Coord, class UInteractiveWidget* Button) PURE_VIRTUAL(UNavigationWidget::AddButton, );

	// Remove a button from the container at the specified coordinate
	UFUNCTION(BlueprintCallable)
	virtual void RemoveButton(FIntPoint Coord) PURE_VIRTUAL(UNavigationWidget::RemoveButton, );

	// Set the focus button to the specified coordinate
	UFUNCTION(BlueprintCallable)
	virtual void SetNavigationFocus(FIntPoint Coord) override;

	// Set the focus button to the specified content
	UFUNCTION(BlueprintCallable)
	virtual void SetNavigationFocusByWidget(class UInteractiveWidget* Widget) override;

	// Set the focus button to the default coordinate
	UFUNCTION(BlueprintCallable)
	virtual void ResetNavigationFocus() override;

	// Navigate to the next button in the specified direction
	UFUNCTION(BlueprintCallable)
	virtual bool Navigate(EUINavigation Direction) override;

	// Confirm current button
	virtual bool Confirm() override;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void OnNavigate(FIntPoint CurrentCoord, FIntPoint LastCoord);
	void OnNavigate_Implementation(FIntPoint CurrentCoord, FIntPoint LastCoord) {}

protected: // Warp
	UPROPERTY(EditDefaultsOnly, Category = "Navigation")
	bool bAllowWrap = true;

	UPROPERTY(EditDefaultsOnly, Category = "Navigation")
	bool bWarpProtection = true;

	int32 Warp(int32 Value, int32 Min, int32 Max);

protected: // Sound Effect
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	USoundBase* NavigationSound;

#if WITH_EDITORONLY_DATA
private: // Debug
	UPROPERTY(EditDefaultsOnly, Category = "Debugging")
	bool bHighlightFocusing = false;
#endif

};

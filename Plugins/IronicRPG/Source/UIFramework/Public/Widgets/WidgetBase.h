// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WidgetBase.generated.h"

/**
 * This class is used to create a widget with multiple buttons that can be navigated with the keyboard.
 */
UCLASS(Abstract)
class UIFRAMEWORK_API UWidgetBase : public UUserWidget
{
	GENERATED_BODY()
	
protected: // UUserWidget
	virtual void NativeConstruct() override;

public:
	// Confirm current UI
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool Confirm();
	virtual bool Confirm_Implementation();

	// Cancel current UI
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Cancel();
	virtual void Cancel_Implementation();

	// Check if this UI is the topmost UI
	UFUNCTION(BlueprintCallable)
	bool IsTop() const;

protected:
	// Referencr to the UIControlComponent that owns this widget
	UPROPERTY(BlueprintReadOnly, Category = "UIComponent")
	class UUIControlComponent* UIControlComponent = nullptr;

	// Whether this UI can be canceled (closed) by the cancel action
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	bool bCanCancel = true;

protected: // Sound Effect
	UFUNCTION(BlueprintCallable)
	void PlayUISoundEffect(USoundBase* Sound);

};

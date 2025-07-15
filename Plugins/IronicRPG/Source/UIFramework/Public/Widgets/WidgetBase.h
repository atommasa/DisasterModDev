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
	UFUNCTION(BlueprintCallable)
	virtual bool Confirm();

	// Cancel current UI
	UFUNCTION(BlueprintCallable)
	virtual void Cancel();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "UIComponent")
	class UUIControlComponent* UIControlComponent = nullptr;

	UFUNCTION(BlueprintCallable)
	bool IsTop() const;

protected: // Sound Effect
	UFUNCTION(BlueprintCallable)
	void PlayUISoundEffect(USoundBase* Sound);

};

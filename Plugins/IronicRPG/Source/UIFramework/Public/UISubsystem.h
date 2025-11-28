// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UISubsystem.generated.h"

class UWidgetBase;

/**
 * This class manages the UI stack and handles the opening and closing of UIs.
 */
UCLASS(Blueprintable)
class UIFRAMEWORK_API UUISubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public: // Subsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public: // UI Management
	// Push a UI to the stack
	UFUNCTION(BlueprintCallable, Category = "UIManager")
	UWidgetBase* OpenUI(const FString UIName, const bool bHideLastUI = true);

	UFUNCTION(BlueprintCallable, Category = "UIManager")
	UWidgetBase* OpenUIByClass(TSubclassOf<UWidgetBase> UIClass, const bool bHideLastUI = true);

	// Pop the top UI from the stack
	UFUNCTION(BlueprintCallable, Category = "UIManager")
	void CloseUI();

	// Close all UI and clear the stack
	UFUNCTION(BlueprintCallable, Category = "UIManager")
	void CloseAllUI();

	// Is the UI is the top of the stack? (i.e. current focusing UI)
	UFUNCTION(BlueprintCallable, Category = "UIManager")
	bool IsTopUI(UWidgetBase* UI) const { return !UIStack.IsEmpty() && UIStack.Top() == UI; }

	// Is the UI stack empty?
	UFUNCTION(BlueprintCallable, Category = "UIManager")
	bool IsUIStackEmpty() const { return UIStack.IsEmpty(); }

	// Returns the top UI in the stack
	UFUNCTION(BlueprintCallable, Category = "UIManager")
	UWidgetBase* GetTopUI() const { return UIStack.IsEmpty() ? nullptr : UIStack.Top(); }

protected:
	// The UI stack
	TArray<UWidgetBase*> UIStack;

	// Decides the UI to be used for each UIName
	UPROPERTY(EditAnywhere, Category = "UIManager")
	TMap<FString, TSubclassOf<UWidgetBase>> UIInfos;

private:
	void SetInputModeForUI(UWidgetBase* ActiveUI);

	void RessetInputMode();

};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NavigationInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UNavigationInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class UIFRAMEWORK_API INavigationInterface
{
	GENERATED_BODY()

public:
	virtual bool Navigate(EUINavigation Direction) = 0;

	virtual void SetNavigationFocus(FIntPoint Coord) {}
	virtual void SetNavigationFocusByWidget(class UInteractiveWidget* Widget) {}
	virtual void ResetNavigationFocus() {}
	virtual FIntPoint GetCurrentFocusCoord() const { return FIntPoint(0, 0); }
};

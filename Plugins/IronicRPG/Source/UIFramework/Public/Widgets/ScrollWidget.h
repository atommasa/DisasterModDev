// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/NavigationWidget.h"
#include "ScrollWidget.generated.h"

/**
 * The scrollable widget that supports navigation.
 */
UCLASS()
class UIFRAMEWORK_API UScrollWidget : public UNavigationWidget
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UScrollBox* ScrollBox;

private:
	virtual void CollectNavigatableButtons(TMap<FIntPoint, UInteractiveWidget*>& OutButtons) override;
	
public:
	// Add a button to the grid at the specified coordinate
	virtual void AddButton(FIntPoint Coord, class UInteractiveWidget* Button) override;

	// Remove a button from the grid at the specified coordinate
	virtual void RemoveButton(FIntPoint Coord) override;

public:
	virtual bool Navigate(EUINavigation Direction) override;

};

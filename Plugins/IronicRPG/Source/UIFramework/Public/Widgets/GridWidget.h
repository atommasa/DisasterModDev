// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/NavigationWidget.h"
#include "GridWidget.generated.h"

/**
 * 
 */
UCLASS()
class UIFRAMEWORK_API UGridWidget : public UNavigationWidget
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UUniformGridPanel* GridPanel;

private:
	virtual void CollectNavigatableButtons(TMap<FIntPoint, UInteractiveWidget*>& OutButtons) override;
	
public:
	// Add a button to the grid at the specified coordinate
	virtual void AddButton(FIntPoint Coord, class UInteractiveWidget* Button) override;

	// Remove a button from the grid at the specified coordinate
	virtual void RemoveButton(FIntPoint Coord) override;

};

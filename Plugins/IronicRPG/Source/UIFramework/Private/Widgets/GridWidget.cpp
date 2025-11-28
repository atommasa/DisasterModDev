// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/GridWidget.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Widgets/Contents/InteractiveWidget.h"

void UGridWidget::CollectNavigatableButtons(TMap<FIntPoint, UInteractiveWidget*>& OutButtons)
{
    if (!GridPanel)
    {
        return;
    }

    for (UWidget* Child : GridPanel->GetAllChildren())
    {
        if (UInteractiveWidget* Button = Cast<UInteractiveWidget>(Child))
        {
            if (UUniformGridSlot* GridSlot = Cast<UUniformGridSlot>(Button->Slot))
            {
                int32 Row = GridSlot->GetRow();
                int32 Col = GridSlot->GetColumn();
                OutButtons.Add(FIntPoint(Col, Row), Button);
            }
        }
    }
}

void UGridWidget::AddButton(FIntPoint Coord, UInteractiveWidget* Button)
{
    if (!GridPanel || !Button)
    {
        return;
    }

    if (ButtonCoordMap.Contains(Coord))
    {
        // Button already exists at this coordinate
        ButtonCoordMap[Coord]->RemoveFromParent();
        ButtonCoordMap[Coord] = Button;
    }
    else
    {
        ButtonCoordMap.Add(Coord, Button);
    }

    GridPanel->AddChildToUniformGrid(Button, Coord.Y, Coord.X);

    RebuildNavigation();
}

void UGridWidget::RemoveButton(FIntPoint Coord)
{
    if (!GridPanel)
    {
        return;
    }

    if (ButtonCoordMap.Contains(Coord))
    {
        UInteractiveWidget* Button = ButtonCoordMap[Coord];
        ButtonCoordMap.Remove(Coord);

        if (Button)
        {
            Button->RemoveFromParent();
        }
    }

    // If the removed button was the current focus button, reset the focus to the default button
    if (CurrentFocusCoord == Coord)
    {
        CurrentFocusCoord = DefaultFocusCoord;
        SetNavigationFocus(CurrentFocusCoord);
    }

    RebuildNavigation();
}

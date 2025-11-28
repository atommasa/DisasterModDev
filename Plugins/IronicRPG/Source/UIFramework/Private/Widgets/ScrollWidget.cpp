// Copyright Ironic Studio. All Rights Reserved.


#include "Widgets/ScrollWidget.h"
#include "Components/ScrollBox.h"
#include "Widgets/Contents/InteractiveWidget.h"

void UScrollWidget::CollectNavigatableButtons(TMap<FIntPoint, UInteractiveWidget*>& OutButtons)
{
    TArray<UWidget*> Children = ScrollBox->GetAllChildren();
    for (int32 i = 0; i < Children.Num(); i++)
    {
        if (UInteractiveWidget* Button = Cast<UInteractiveWidget>(Children[i]))
        {
			OutButtons.Add(FIntPoint(0, i), Button); // For scroll widget, we only use Y coord for ordering
		}
	}
}

void UScrollWidget::AddButton(FIntPoint Coord, UInteractiveWidget* Button)
{
    if (!Button)
    {
        return;
    }

	// For ScrollWidget, we only use Y coordinate for ordering
    Coord.X = 0;

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

    if (UWidget* Child = ScrollBox->GetChildAt(Coord.Y))
    {
		ScrollBox->RemoveChildAt(Coord.Y);
        ScrollBox->InsertChildAt(Coord.Y, Button);
    }
    else
    {
		ScrollBox->AddChild(Button);
    }

    RebuildNavigation();
}

void UScrollWidget::RemoveButton(FIntPoint Coord)
{
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

bool UScrollWidget::Navigate(EUINavigation Direction)
{
	const bool bResult = Super::Navigate(Direction);

	// Scroll to the focused button
    ScrollBox->ScrollWidgetIntoView(ButtonCoordMap.FindRef(CurrentFocusCoord), true, EDescendantScrollDestination::IntoView, 10.0f);

    return bResult;
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/NavigationWidget.h"
#include "Widgets/Contents/InteractiveWidget.h"
#include "Components/UIControlComponent.h"
#include "UISubsystem.h"
#include "Kismet/GameplayStatics.h"

void UNavigationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Rebuild the navigation
	RebuildNavigation();

	// Set the default focus
	SetNavigationFocus(DefaultFocusCoord);
}

void UNavigationWidget::RebuildNavigation()
{
    ButtonCoordMap.Empty();
    MaxRow = 0;
    MaxCol = 0;

    TMap<FIntPoint, UInteractiveWidget*> NewButtons;
    CollectNavigatableButtons(NewButtons);

    for (const auto& Entry : NewButtons)
    {
        ButtonCoordMap.Add(Entry.Key, Entry.Value);
        MaxRow = FMath::Max(MaxRow, Entry.Key.Y + 1);
        MaxCol = FMath::Max(MaxCol, Entry.Key.X + 1);

        Entry.Value->OnWidgetHovered.AddUniqueDynamic(this, &UNavigationWidget::SetNavigationFocusByWidget);
    }
}

void UNavigationWidget::SetNavigationFocus(FIntPoint Coord)
{
    if (ButtonCoordMap.Contains(Coord) && ButtonCoordMap.Contains(CurrentFocusCoord))
    {
#if WITH_EDITORONLY_DATA
        if (bHighlightFocusing)
        {
            ButtonCoordMap[CurrentFocusCoord]->DebugHighlightHoveredWidget(false);
            ButtonCoordMap[Coord]->DebugHighlightHoveredWidget(true);
        }
#endif

        ButtonCoordMap[CurrentFocusCoord]->PlayUnhover();
        ButtonCoordMap[Coord]->PlayHover();

        // Call OnNavigate
        if (IsTop())
        {
            OnNavigate(Coord, CurrentFocusCoord);
        }

        // Set the focus to the button
        CurrentFocusCoord = Coord;
    }
}

void UNavigationWidget::SetNavigationFocusByWidget(UInteractiveWidget* Widget)
{
    if (!Widget)
    {
        return;
    }

    if (const FIntPoint* Coord = ButtonCoordMap.FindKey(Widget))
    {
#if WITH_EDITORONLY_DATA
        if (ButtonCoordMap.Contains(CurrentFocusCoord) && ButtonCoordMap.Contains(*Coord))
        {
            ButtonCoordMap[CurrentFocusCoord]->DebugHighlightHoveredWidget(false);
            ButtonCoordMap[*Coord]->DebugHighlightHoveredWidget(true);
        }
#endif
        
        CurrentFocusCoord = *Coord;
    }
}

void UNavigationWidget::ResetNavigationFocus()
{
    SetNavigationFocus(DefaultFocusCoord);
}

bool UNavigationWidget::Navigate(EUINavigation Direction)
{
    FIntPoint Offset(0, 0);

    switch (Direction)
    {
    case EUINavigation::Up:    Offset.Y = -1; break;
    case EUINavigation::Down:  Offset.Y = +1; break;
    case EUINavigation::Left:  Offset.X = -1; break;
    case EUINavigation::Right: Offset.X = +1; break;
    default: return false;
    }

    FIntPoint Next = CurrentFocusCoord;

    for (int32 i = 0; i < FMath::Max(MaxRow, MaxCol); ++i)
    {
        Next += Offset;

        if (UIControlComponent && (Next.X < 0 || Next.Y < 0 || Next.X >= MaxCol || Next.Y >= MaxRow))
        {
            if (!bAllowWrap)
            {
                return false; // Stop beyond the edge
            }
            else if (UIControlComponent->bIsNavigating && bWarpProtection)
            {
                return true; // Protecting continuous navigation from exceeding warp
            }
        }

        // Wrapping
        if (bAllowWrap)
        {
            Next.X = Warp(Next.X, 0, MaxCol - 1);
            Next.Y = Warp(Next.Y, 0, MaxRow - 1);
        }
        
        if (ButtonCoordMap.Contains(Next) && Next != CurrentFocusCoord)
        {
            SetNavigationFocus(Next);
            PlayUISoundEffect(NavigationSound);
            return true;
        }
    }
    
    return false;
}

bool UNavigationWidget::Confirm_Implementation()
{
    if (ButtonCoordMap.Contains(CurrentFocusCoord))
    {
        ButtonCoordMap[CurrentFocusCoord]->PlayClick();

#if WITH_EDITORONLY_DATA
        if (bHighlightFocusing)
        {
            ButtonCoordMap[CurrentFocusCoord]->DebugHighlightClickedWidget(true);
        }
#endif

        return true;
    }

    return false;
}

int32 UNavigationWidget::Warp(int32 Value, int32 Min, int32 Max)
{
    if (Value < Min)
    {
        Value = bAllowWrap ? Max : Min;
    }
    else if (Value > Max)
    {
        Value = bAllowWrap ? Min : Max;
    }

    return Value;
}

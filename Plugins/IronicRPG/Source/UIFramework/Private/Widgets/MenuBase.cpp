// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/MenuBase.h"
#include "Components/WidgetSwitcher.h"

bool UMenuBase::SwitchWidget(UWidgetBase* Widget)
{
	if (!Widget || !WidgetSwitcher)
	{
		return false;
	}
	
	if (WidgetSwitcher->GetNumWidgets() > 0)
	{
		int32 LastIndex = WidgetSwitcher->GetActiveWidgetIndex();

		WidgetSwitcher->SetActiveWidget(Widget);
		CurrentWidget = Widget;
		if (CurrentWidget && CurrentWidget->Implements<UNavigationInterface>())
		{
			Cast<INavigationInterface>(CurrentWidget)->ResetNavigationFocus();
		}

		OnSwitchWidget(WidgetSwitcher->GetActiveWidgetIndex(), LastIndex);
		return true;
	}

	return false;
}

bool UMenuBase::SwitchWidgetByIndex(int32 Index)
{
	if (WidgetSwitcher && WidgetSwitcher->GetNumWidgets() > Index)
	{
		int32 LastIndex = WidgetSwitcher->GetActiveWidgetIndex();

		WidgetSwitcher->SetActiveWidgetIndex(Index);
		CurrentWidget = Cast<UWidgetBase>(WidgetSwitcher->GetActiveWidget());
		if (CurrentWidget && CurrentWidget->Implements<UNavigationInterface>())
		{
			Cast<INavigationInterface>(CurrentWidget)->ResetNavigationFocus();
		}
		
		OnSwitchWidget(Index, LastIndex);
		return true;
	}

	return false;
}

bool UMenuBase::SwitchWidgetByDirection(EUINavigation Direction)
{
	if (WidgetSwitcher && WidgetSwitcher->GetNumWidgets() > 0)
	{
		const int32 NumWidgets = WidgetSwitcher->GetNumWidgets();
		const int32 CurrentIndex = WidgetSwitcher->GetActiveWidgetIndex();
		int32 NewIndex = CurrentIndex;

		switch (Direction)
		{
		case EUINavigation::Left:
			NewIndex = CurrentIndex - 1;
			if (NewIndex < 0)
			{
				NewIndex = bWrapSwitching ? NumWidgets - 1 : 0;
			}
			break;

		case EUINavigation::Right:
			NewIndex = CurrentIndex + 1;
			if (NewIndex >= NumWidgets)
			{
				NewIndex = bWrapSwitching ? 0 : NumWidgets - 1;
			}
			break;

		default:
			return false;
		}

		return SwitchWidgetByIndex(NewIndex);
	}

	return false;
}

bool UMenuBase::Confirm()
{
	return CurrentWidget ? CurrentWidget->Confirm() : false;
}

bool UMenuBase::Navigate(EUINavigation Direction)
{
	if (CurrentWidget && CurrentWidget->Implements<UNavigationInterface>())
	{
		return Cast<INavigationInterface>(CurrentWidget)->Navigate(Direction);
	}
	
	return false;
}

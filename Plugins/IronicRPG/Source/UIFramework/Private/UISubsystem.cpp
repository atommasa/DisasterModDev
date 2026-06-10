// Copyright Ironic Studio. All Rights Reserved.


#include "UISubsystem.h"
#include "Widgets/WidgetBase.h"

void UUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

}

void UUISubsystem::Deinitialize()
{
	// Clean up the UIStack
	CloseAllUI();
}

UWidgetBase* UUISubsystem::OpenUI(const FString UIName, const bool bHideLastUI)
{
	if (UIInfos.Contains(UIName))
	{
		TSubclassOf<UWidgetBase> WidgetClass = UIInfos[UIName];
		if (WidgetClass)
		{
			if (UWidgetBase* NewUI = CreateWidget<UWidgetBase>(GetWorld(), WidgetClass))
			{
				// If we are hiding the last UI, set its visibility to hidden
				if (bHideLastUI && !IsUIStackEmpty())
				{
					UWidgetBase* LastUI = UIStack.Top();
					if (LastUI)
					{
						LastUI->SetVisibility(ESlateVisibility::Hidden);
					}
				}

				NewUI->AddToViewport();
				UIStack.Push(NewUI);
				// SetInputModeForUI(NewUI);

				return NewUI;
			}
		}
	}

	return nullptr;
}

UWidgetBase* UUISubsystem::OpenUIByClass(TSubclassOf<UWidgetBase> UIClass, const bool bHideLastUI)
{
	if (UIClass)
	{
		if (UWidgetBase* NewUI = CreateWidget<UWidgetBase>(GetWorld(), UIClass))
		{
			// If we are hiding the last UI, set its visibility to hidden
			if (bHideLastUI && !IsUIStackEmpty())
			{
				UWidgetBase* LastUI = UIStack.Top();
				if (LastUI)
				{
					LastUI->SetVisibility(ESlateVisibility::Hidden);
				}
			}

			NewUI->AddToViewport();
			UIStack.Push(NewUI);
			// SetInputModeForUI(NewUI);

			return NewUI;
		}
	}

	return nullptr;
}

void UUISubsystem::CloseUI()
{
	if (!IsUIStackEmpty())
	{
		UWidgetBase* TopUI = UIStack.Pop();
		if (TopUI)
		{
			TopUI->RemoveFromParent();
		}
	}

	// If there are still UIs in the stack, show the new top UI
	if (!IsUIStackEmpty())
	{
		UWidgetBase* NewTopUI = UIStack.Top();
		if (NewTopUI)
		{
			NewTopUI->SetVisibility(ESlateVisibility::Visible);
			// SetInputModeForUI(NewTopUI);
		}
	}
	else
	{
		// RessetInputMode();
	}
}

void UUISubsystem::CloseAllUI()
{
	for (UWidgetBase* UI : UIStack)
	{
		if (UI)
		{
			UI->RemoveFromParent();
		}
	}

	UIStack.Empty();
}

void UUISubsystem::SetInputModeForUI(UWidgetBase* ActiveUI)
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(ActiveUI->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;

		PC->InputComponent->bBlockInput = true;
	}
}

void UUISubsystem::RessetInputMode()
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = false;

		PC->InputComponent->bBlockInput = false;
	}
}

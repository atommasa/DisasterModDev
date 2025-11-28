// Copyright Ironic Studio. All Rights Reserved.


#include "LoadingScreenSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"

ULoadingScreenSubsystem* ULoadingScreenSubsystem::Get(const UObject* WorldContextObject)
{
	if (UGameInstance* GameInstance = WorldContextObject->GetWorld()->GetGameInstance())
	{
		return GameInstance->GetSubsystem<ULoadingScreenSubsystem>();
	}

	return nullptr;
}

void ULoadingScreenSubsystem::StartLoadingScreen(bool bManualStop)
{
	if (bIsLoading)
	{
		return;
	}

	if (LoadingScreenWidgetClass)
	{
		LoadingScreenWidget = CreateWidget<UUserWidget>(GetWorld(), LoadingScreenWidgetClass);
		if (LoadingScreenWidget.IsValid())
		{
			LoadingScreenWidget->AddToViewport();
			bIsLoading = true;

			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(
				TimerHandle,
				[this]()
				{
					bCanStopLoading = true;
					if (bRequestedStop)
					{
						EndLoadingScreen();
					}
				},
				MinimumLoadingScreenDisplayTime,
				false
			);
		}
	}
}

void ULoadingScreenSubsystem::StopLoadingScreen()
{
	if (!bIsLoading)
	{
		return;
	}

	if (!bCanStopLoading)
	{
		bRequestedStop = true;
		return;
	}

	EndLoadingScreen();
}

void ULoadingScreenSubsystem::EndLoadingScreen()
{
	if (LoadingScreenWidget.IsValid())
	{
		LoadingScreenWidget->RemoveFromParent();
		LoadingScreenWidget.Reset();

		bIsLoading = false;
		bCanStopLoading = false;
		bRequestedStop = false;

		OnLoadingScreenStop.Broadcast();
	}
}

// Copyright Ironic Studio. All Rights Reserved.


#include "LoadingScreenSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

#include "Widgets/WidgetBase.h"

void ULoadingScreenSubsystem::StartLoadingScreen(bool bManualStop)
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[LoadingScreen] Start requested. AlreadyLoading=%d"),
		bIsLoading);

	bCanStopLoading = false;
	bRequestedStop = false;
	LoadingScreenStartTime = FPlatformTime::Seconds();

	ClearMinimumDisplayTicker();

	if (bIsLoading)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[LoadingScreen] Already visible; reset loading state."));

		return;
	}

	if (!LoadingScreenWidgetClass)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[LoadingScreen] Cannot start: LoadingScreenWidgetClass is null."));

		return;
	}

	UGameViewportClient* ViewportClient = GetGameInstance()->GetGameViewportClient();

	if (!ViewportClient)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[LoadingScreen] GameViewportClient is invalid.")
		);
		return;
	}

	LoadingScreenWidget = CreateWidget<UWidgetBase>(GetGameInstance(), LoadingScreenWidgetClass);

	if (!LoadingScreenWidget.IsValid())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[LoadingScreen] Failed to create widget.")
		);
		return;
	}

	LoadingScreenViewportContent = LoadingScreenWidget->TakeWidget();

	if (!LoadingScreenViewportContent.IsValid())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[LoadingScreen] Failed to create Slate widget.")
		);

		LoadingScreenWidget = nullptr;
		return;
	}

	ViewportClient->AddViewportWidgetContent(
		LoadingScreenViewportContent.ToSharedRef(),
		10000
	);

	bIsLoading = true;

	UGameplayStatics::SetGamePaused(this, true);

	if (ARPGPlayerController* PC = Cast<ARPGPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		SavedControlMode = PC->GetControlMode();
		PC->SetControlMode(ERPGControlMode::None);
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[LoadingScreen] Started. Minimum display time=%.2f"),
		MinimumLoadingScreenDisplayTime);
}

void ULoadingScreenSubsystem::StopLoadingScreen()
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[LoadingScreen] Stop requested. IsLoading=%d CanStop=%d"),
		bIsLoading,
		bCanStopLoading);

	if (!bIsLoading)
	{
		return;
	}

	bRequestedStop = true;

	TryEndLoadingScreen();
}

void ULoadingScreenSubsystem::EndLoadingScreen()
{
	if (!bIsLoading)
	{
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("[LoadingScreen] Ending loading screen."));

	ClearMinimumDisplayTicker();

	if (UGameViewportClient* ViewportClient = GetGameInstance()->GetGameViewportClient())
	{
		if (LoadingScreenViewportContent.IsValid())
		{
			ViewportClient->RemoveViewportWidgetContent(LoadingScreenViewportContent.ToSharedRef());
		}
	}

	LoadingScreenViewportContent.Reset();
	LoadingScreenWidget.Reset();

	bIsLoading = false;
	bCanStopLoading = true;
	bRequestedStop = false;
	LoadingScreenStartTime = 0.0;

	UGameplayStatics::SetGamePaused(this, false);

	if (ARPGPlayerController* PC = Cast<ARPGPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		PC->SetControlMode(SavedControlMode);
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[LoadingScreen] Broadcasting OnLoadingScreenStop."));

	OnLoadingScreenStop.Broadcast();
}

void ULoadingScreenSubsystem::TryEndLoadingScreen()
{
	if (!bIsLoading || !bRequestedStop)
	{
		return;
	}

	const double CurrentTime = FPlatformTime::Seconds();
	const double ElapsedTime = CurrentTime - LoadingScreenStartTime;

	const double RemainingTime = FMath::Max(0.0, static_cast<double>(MinimumLoadingScreenDisplayTime) - ElapsedTime);

	if (RemainingTime <= 0.0)
	{
		bCanStopLoading = true;

		UE_LOG(
			LogTemp,
			Display,
			TEXT("[LoadingScreen] Minimum display time satisfied. Ending now."));

		EndLoadingScreen();
		return;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[LoadingScreen] Waiting %.2f more seconds before ending."),
		RemainingTime);

	ClearMinimumDisplayTicker();

	const double EndTime = CurrentTime + RemainingTime;

	MinimumDisplayTickerHandle =
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(
				this,
				[this, EndTime](float DeltaTime)
				{
					if (!bIsLoading)
					{
						return false;
					}

					if (FPlatformTime::Seconds() < EndTime)
					{
						return true;
					}

					bCanStopLoading = true;

					UE_LOG(
						LogTemp,
						Display,
						TEXT("[LoadingScreen] Stop gate opened. RequestedStop=%d"),
						bRequestedStop);

					if (bRequestedStop)
					{
						EndLoadingScreen();
					}

					return false;
				}),
			0.0f);
}

void ULoadingScreenSubsystem::ClearMinimumDisplayTicker()
{
	if (MinimumDisplayTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(MinimumDisplayTickerHandle);

		MinimumDisplayTickerHandle.Reset();
	}
}
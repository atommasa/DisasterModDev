// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/RPGGameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Controllers/RPGPlayerController.h"
#include "LoadingScreenSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadingScreenStop);

/**
 * A subsystem to manage loading screens within the game instance.
 */
UCLASS(Blueprintable)
class LOADINGSCREEN_API ULoadingScreenSubsystem : public URPGGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// The widget class to use for the loading screen.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Loading Screen")
    TSubclassOf<class UWidgetBase> LoadingScreenWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Loading Screen")
	TWeakObjectPtr<class UWidgetBase> LoadingScreenWidget = nullptr;

	// The minimum time the loading screen should be displayed.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Loading Screen")
	float MinimumLoadingScreenDisplayTime = 2.0f;

public:
	// The instance of the loading screen widget.
    UFUNCTION(BlueprintCallable, Category = "Loading Screen")
    void StartLoadingScreen(bool bManualStop = true);

	// Stops the loading screen if it was started manually.
    UFUNCTION(BlueprintCallable, Category = "Loading Screen")
    void StopLoadingScreen();

	// Is loading?
	UFUNCTION(BlueprintCallable, Category = "Loading Screen")
	bool IsLoading() const { return bIsLoading; }

	FOnLoadingScreenStop OnLoadingScreenStop;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Loading Screen")
	bool bIsLoading = false;

	UPROPERTY(BlueprintReadOnly, Category = "Loading Screen")
	bool bCanStopLoading = false;

	UPROPERTY(BlueprintReadOnly, Category = "Loading Screen")
	bool bRequestedStop = false;

protected:
	double LoadingScreenStartTime = 0.0;

	FTSTicker::FDelegateHandle MinimumDisplayTickerHandle;

	void EndLoadingScreen();
	void TryEndLoadingScreen();
	void ClearMinimumDisplayTicker();

private:
	TSharedPtr<SWidget> LoadingScreenViewportContent;

	ERPGControlMode SavedControlMode;

};

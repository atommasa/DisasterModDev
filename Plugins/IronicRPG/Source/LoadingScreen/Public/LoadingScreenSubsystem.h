// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LoadingScreenSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadingScreenStop);

/**
 * A subsystem to manage loading screens within the game instance.
 */
UCLASS(Abstract, Blueprintable)
class LOADINGSCREEN_API ULoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static ULoadingScreenSubsystem* Get(const UObject* WorldContextObject);

public:
	// The widget class to use for the loading screen.
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Loading Screen")
    TSubclassOf<UUserWidget> LoadingScreenWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "Loading Screen")
	TWeakObjectPtr<UUserWidget> LoadingScreenWidget = nullptr;

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

	FOnLoadingScreenStop OnLoadingScreenStop;

protected:
	void EndLoadingScreen();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Loading Screen")
	bool bIsLoading = false;

	UPROPERTY(BlueprintReadOnly, Category = "Loading Screen")
	bool bCanStopLoading = false;

	UPROPERTY(BlueprintReadOnly, Category = "Loading Screen")
	bool bRequestedStop = false;

};

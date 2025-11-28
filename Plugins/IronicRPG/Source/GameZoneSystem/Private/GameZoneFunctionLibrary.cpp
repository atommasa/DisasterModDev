// Copyright Ironic Studio. All Rights Reserved.


#include "GameZoneFunctionLibrary.h"
#include "LoadingScreenSubsystem.h"
#include "Kismet/GameplayStatics.h"

void UGameZoneFunctionLibrary::TransitionToLevel(const UObject* WorldContextObject, FName LevelName, bool bAbsolute, FString Options)
{
	ULoadingScreenSubsystem* LoadingScreenSubsystem = ULoadingScreenSubsystem::Get(WorldContextObject);
	if (LoadingScreenSubsystem)
	{
		LoadingScreenSubsystem->StartLoadingScreen();
	}

	UGameplayStatics::OpenLevel(WorldContextObject, LevelName, bAbsolute, Options);

	if (LoadingScreenSubsystem)
	{
		LoadingScreenSubsystem->StopLoadingScreen();
	}
}

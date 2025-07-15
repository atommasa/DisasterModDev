// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DataTypes/RPGId.h"
#include "RPGGameMode.generated.h"

/**
 * 
 */
UCLASS()
class RPGGAMEPLAY_API ARPGGameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

protected:
	APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* SpawnTransform);

};

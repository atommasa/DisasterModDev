// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "InstancedStruct.h"
#include "Characters/CharacterDataTypes.h"
#include "RPGSaveGame.generated.h"

UCLASS()
class SAVESYSTEM_API URPGSaveGame : public USaveGame
{
	GENERATED_BODY()
	
public:
	UPROPERTY(SaveGame)
	TMap<FName, FInstancedStruct> SaveModules;

};

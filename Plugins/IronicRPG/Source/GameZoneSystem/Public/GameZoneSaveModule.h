// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RPGId.h"
#include "Levels/GameZoneContext.h"
#include "GameZoneSaveModule.generated.h"

USTRUCT(BlueprintType)
struct GAMEZONESYSTEM_API FGameZoneSaveModule
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    FGameZoneContext CurrentContext;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DataTypes/RPGId.h"
#include "AbilityDataTypes.generated.h"

USTRUCT(BlueprintType)
struct FAbilityInstanceData
{
    GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText Description;

	UPROPERTY(SaveGame)
	int32 CurrentLevel = 1;

};
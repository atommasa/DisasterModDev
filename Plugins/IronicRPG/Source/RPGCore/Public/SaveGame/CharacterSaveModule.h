// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseSaveModule.h"
#include "DataTypes/RPGId.h"
#include "Characters/CharacterDataTypes.h"
#include "CharacterSaveModule.generated.h"

USTRUCT(BlueprintType)
struct RPGCORE_API FCharacterSaveModule : public FBaseSaveModule
{
    GENERATED_BODY()

public:
    FCharacterSaveModule()
    {
        ModuleType = ESaveModuleType::Character;
    }

    UPROPERTY(SaveGame)
    TMap<FRPGId, FCharacterInstanceData> CharacterData;

    UPROPERTY(SaveGame)
	FRPGId CurrentCharacterId;

    UPROPERTY(SaveGame)
    TArray<FRPGId> CurrentParty;

};

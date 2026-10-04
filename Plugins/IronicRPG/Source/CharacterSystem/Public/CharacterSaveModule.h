// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RPGId.h"
#include "Characters/CharacterDataTypes.h"
#include "CharacterSaveModule.generated.h"

USTRUCT(BlueprintType)
struct CHARACTERSYSTEM_API FCharacterSaveModule
{
	GENERATED_BODY()

    UPROPERTY(SaveGame)
    TMap<FRPGId, FCharacterSaveData> CharacterData;

    UPROPERTY(SaveGame)
    int32 PlayerPartyIndex = 0;

    UPROPERTY(SaveGame)
    TArray<FRPGId> PartyMembers;
};

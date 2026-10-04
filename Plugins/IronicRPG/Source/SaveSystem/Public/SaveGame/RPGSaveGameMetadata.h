// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SaveGame/RPGSaveGameVersion.h"
#include "RPGSaveGameMetadata.generated.h"

/**
 * The matedata for identifying save game information.
 * This struct is for default save game metadata, if you want to customize it,
 * please create a new USTRUCT() (DO NOT INHERIT FROM THIS STRUCT) and add your own properties.
 */
USTRUCT(BlueprintType)
struct SAVESYSTEM_API FRPGSaveGameMetadata
{
    GENERATED_BODY()

    UPROPERTY()
    uint16 SaveDataVersion = ERPGSaveGameVersion::InitialRelease;

    UPROPERTY(BlueprintReadOnly)
	FString SaveSlotName;

    UPROPERTY(BlueprintReadOnly)
    FDateTime SaveTime = FDateTime::Now();

    UPROPERTY(BlueprintReadOnly)
	FTimespan PlayTime = FTimespan::Zero();

    UPROPERTY(BlueprintReadOnly)
    bool bIsClear = false;

	FRPGSaveGameMetadata() = default;

};

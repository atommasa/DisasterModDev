// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RPGId.h"
#include "Levels/GameZoneContext.h"
#include "Levels/GameZonePointData.h"
#include "GameZoneSaveModule.generated.h"

USTRUCT(BlueprintType)
struct GAMEZONESYSTEM_API FGameZoneSaveModule
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    FGameZoneContext CurrentContext;

    UPROPERTY(SaveGame)
    TMap<FGuid, FGameZonePointData> PointOverrides;
};

// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RPGId.h"
#include "Levels/GameZoneMapData.h"
#include "GameZoneMapTypes.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct GAMEZONESYSTEM_API FGameZoneMapWorldBounds
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly)
    FRPGId ZoneId;

    UPROPERTY(BlueprintReadOnly)
    FVector2D MinimumWorldXY = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FVector2D MaximumWorldXY = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FGuid MapBakeRevision;

    UPROPERTY(BlueprintReadOnly)
    bool bIsValid = false;
};

USTRUCT(BlueprintType)
struct GAMEZONESYSTEM_API FGameZoneMapLayerCatalogEntry
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly)
    FRPGId ZoneId;

    UPROPERTY(BlueprintReadOnly)
    FGameZoneMapLayerId LayerId;

    UPROPERTY(BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly)
    int32 SortOrder = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 ElevationOrder = 0;
};

USTRUCT(BlueprintType)
struct GAMEZONESYSTEM_API FGameZoneMapSheetCatalogEntry
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly)
    FRPGId ZoneId;

    UPROPERTY(BlueprintReadOnly)
    FGameZoneMapLayerId LayerId;

    UPROPERTY(BlueprintReadOnly)
    FGameZoneMapSheetId SheetId;

    UPROPERTY(BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly)
    int32 SortOrder = 0;

    UPROPERTY(BlueprintReadOnly)
    FGuid MapBakeRevision;
};

USTRUCT(BlueprintType)
struct GAMEZONESYSTEM_API FResolvedGameZoneMapSheet
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly)
    FRPGId ZoneId;

    UPROPERTY(BlueprintReadOnly)
    FGameZoneMapLayerId LayerId;

    UPROPERTY(BlueprintReadOnly)
    FGameZoneMapSheetId SheetId;

    UPROPERTY(BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly)
    int32 SortOrder = 0;

    UPROPERTY(BlueprintReadOnly)
    FGuid MapBakeRevision;

    UPROPERTY(BlueprintReadOnly)
    FVector WorldOrigin = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FVector2D WorldSize = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    float WorldYaw = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    FGameZoneMapProjection QueryProjection;
};

USTRUCT(BlueprintType)
struct GAMEZONESYSTEM_API FGameZoneMapTextureResult
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly)
    FGuid RequestId;

    UPROPERTY(BlueprintReadOnly)
    FRPGId ZoneId;

    UPROPERTY(BlueprintReadOnly)
    FGameZoneMapSheetId SheetId;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<UTexture2D> Texture = nullptr;

    UPROPERTY(BlueprintReadOnly)
    bool bSucceeded = false;

    UPROPERTY(BlueprintReadOnly)
    bool bUsedDefaultTexture = false;
};

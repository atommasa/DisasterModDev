// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "GameplayTagContainer.h"
#include "Assets/RPGPrimaryAsset.h"
#include "GameZonePointData.generated.h"

UENUM(BlueprintType)
enum class EGameZonePointState : uint8
{
    Activated,
    Deactivated,
    Hide,
};

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EMapMarkerDisplayMode : uint8
{
    None        = 0x00,
    WorldMap    = 0x01,
    MiniMap     = 0x02,
};

ENUM_CLASS_FLAGS(EMapMarkerDisplayMode);

UENUM(BlueprintType)
enum class EGameZonePointUpdateMode : uint8
{
    EventDriven,
    Tracked,
};

UENUM(BlueprintType)
enum class EGameZonePointSavePolicy : uint8
{
    NotSaveable,
    Saveable,
    Custom
};

/**
 * This struct is used to store data about map markers, which can be used to display markers on the map or minimap.
 * It contains information about the marker type, its world location, and a reference to an asset that can be used to display the marker.
 */
USTRUCT(BlueprintType)
struct RPGCORE_API FGameZonePointData
{
	GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, SaveGame)
    FGuid PointId;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    FRPGId ZoneId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(IdType = "MapMarker"))
    FRPGId MarkerTypeId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FGameplayTagContainer MarkerTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    EGameZonePointState MarkerState = EGameZonePointState::Activated;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(Bitmask, BitmaskEnum = "/Script/RPGCore.EMapMarkerDisplayMode"))
    int32 DisplayMode = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
    EGameZonePointSavePolicy SavePolicy = EGameZonePointSavePolicy::NotSaveable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
    FRPGId ReferenceAssetId;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, SaveGame)
	FTransform WorldTransform;

};


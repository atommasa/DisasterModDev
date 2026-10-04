// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RPGId.h"
#include "NativeGameplayTags.h"
#include "GameZoneContext.generated.h"

/**
 * The helper struct to identify the entry in game zones.
 */
USTRUCT(BlueprintType)
struct RPGCORE_API FGameZoneEntryId
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FGuid EntryGuid;

	FGameZoneEntryId& operator=(FGuid& NewGuid) noexcept
	{
		EntryGuid = NewGuid;
		return *this;
	}

	FGameZoneEntryId& operator=(const FGuid& NewGuid) noexcept
	{
		EntryGuid = NewGuid;
		return *this;
	}

	bool IsValid() const { return EntryGuid.IsValid(); }

};

/**
 * This structure defines the context of a game zone, including the zone ID, sub-zone ID, and the player's location and rotation within that zone.
 * It is used to set warping points to specific locations in the game world.
 * Also used to save the player's current location and rotation when saving the game state.
 */
USTRUCT(BlueprintType)
struct RPGCORE_API FGameZoneContext
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(IdType = "Zone", DisplayName = "ZoneId"))
	FRPGId ZoneId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bUseSavedTransform = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(SourceWorld = "ZoneId", EditCondition = "!bUseSavedTransform"))
	FGameZoneEntryId EntryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(EditCondition = "bUseSavedTransform"))
	FTransform SavedTransform;

	FGameZoneContext() = default;

	FGameZoneContext(
		const FRPGId& InZoneId,
		const FGuid& InEntryId
	)
		: ZoneId(InZoneId)
		, bUseSavedTransform(false)
		, EntryId(InEntryId)
	{
	}

	FGameZoneContext(
		const FRPGId& InZoneId,
		const FTransform& InSavedTransform
	)
		: ZoneId(InZoneId)
		, bUseSavedTransform(true)
		, SavedTransform(InSavedTransform)
	{
	}
};

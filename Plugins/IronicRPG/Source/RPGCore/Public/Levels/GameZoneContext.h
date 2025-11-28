// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RPGId.h"
#include "GameZoneContext.generated.h"

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(IdType = "SubZone", DisplayName = "SubZoneId"))
	TArray<FRPGId> SubZoneIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bUseSavedTransform = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(EditCondition = "!bUseSavedTransform"))
	FName EntryPointTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta = (EditCondition = "bUseSavedTransform"))
	FTransform SavedTransform;

	FGameZoneContext() = default;

	FGameZoneContext(const FRPGId& InZoneId, const TArray<FRPGId>& InSubZoneId, const FName& InEntryPointTag)
		: ZoneId(InZoneId)
		, SubZoneIds(InSubZoneId)
		, EntryPointTag(InEntryPointTag)
	{
	}

	FGameZoneContext(const FRPGId& InZoneId, const TArray<FRPGId>& InSubZoneId, const FTransform& InSavedTransform)
		: ZoneId(InZoneId)
		, SubZoneIds(InSubZoneId)
		, SavedTransform(InSavedTransform)
	{
	}
};

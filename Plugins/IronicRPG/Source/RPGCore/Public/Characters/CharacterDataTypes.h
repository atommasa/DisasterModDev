// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DataTypes/RPGId.h"
#include "CharacterDataTypes.generated.h"

class USkeletalMesh;

/**
 * This enum defines the availability status of a character in the game.
 */
UENUM(BlueprintType)
enum class ECharacterAvailabilityStatus : uint8
{
	// Character is not available for selection or use (e.g., not yet unlocked)
	Unavailable = 0x00 UMETA(DisplayName = "Unavailable"),

	// Character is temporarily unavailable (e.g., on a mission, captured, etc.)
	TemporaryLeave = 0x01 UMETA(DisplayName = "TemporaryLeave"),

	// Character is fully available for selection and use
	Available = 0x02 UMETA(DisplayName = "Available"),

	// Character is locked in the party, cannot be removed
	LockedInParty = 0x04 UMETA(DisplayName = "LockedInParty"),

};

/**
 * This struct holds the save data for a character, including availability status, mesh, skills, and attributes.
 */
USTRUCT(BlueprintType)
struct RPGCORE_API FCharacterSaveData
{
    GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, SaveGame)
	ECharacterAvailabilityStatus AvailabilityStatus = ECharacterAvailabilityStatus::Unavailable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, SaveGame, meta=(AssetBundles = "Character"))
	TObjectPtr<USkeletalMesh> CharacterMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, SaveGame)
	TArray<FRPGId> Skills;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, SaveGame)
    TMap<FGameplayAttribute, float> Attributes;

	// Indicates whether this data is to save to or load from a save file
	UPROPERTY(BlueprintReadOnly, SaveGame)
	bool bIsSaveData = false;

};
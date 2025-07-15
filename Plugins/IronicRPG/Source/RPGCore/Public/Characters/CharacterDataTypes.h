// Fill out your copyright notice in the Description page of Project Settings.

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
	Unavailable UMETA(DisplayName = "Unavailable"),

	// Character is temporarily unavailable (e.g., on a mission, captured, etc.)
	TemporaryLeave UMETA(DisplayName = "TemporaryLeave"),

	// Character is available for selection and use, but may have limitations (e.g., HP == 0)
	Exhausted UMETA(DisplayName = "Exhausted"),

	// Character is fully available for selection and use
	Available UMETA(DisplayName = "Available"),

	// Character is locked in the party, cannot be removed
	LockedInParty UMETA(DisplayName = "LockedInParty"),

	// Emeny character or NPC, not available for player control
	EnemyOrNPC UMETA(DisplayName = "EnemyOrNPC")

};

USTRUCT(BlueprintType)
struct FCharacterInstanceData
{
    GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, SaveGame)
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, SaveGame)
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, SaveGame)
	ECharacterAvailabilityStatus AvailabilityStatus = ECharacterAvailabilityStatus::Unavailable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, SaveGame)
	TSoftObjectPtr<USkeletalMesh> CharacterMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, SaveGame)
	TArray<FRPGId> Skills;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, SaveGame)
    TMap<FString, float> Attributes;

};
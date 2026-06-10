// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DataTypes/RPGId.h"
#include "Abilities/AbilityDataTypes.h"
#include "CharacterDataTypes.generated.h"

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

	// The skeletal mesh representing the character's appearance
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, SaveGame, meta=(AssetBundles = "Character"))
	TObjectPtr<class USkeletalMesh> CharacterMesh = nullptr;

	// Array of abilities the character has learned
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, SaveGame)
	TArray<FAbilityData> LearnedAbilities;

	// Map of input IDs to equipped abilities
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, SaveGame, meta=(IdType = "Ability"))
	TMap<int32, FRPGId> EquippedAbilities;

	// Map of gameplay attributes and their corresponding values
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, SaveGame)
    TMap<FGameplayAttribute, float> Attributes;

	// Indicates whether this data is to save to or load from a save file
	UPROPERTY(BlueprintReadOnly, SaveGame)
	bool bIsSaveData = false;

};

/*
 * Struct  
 */
USTRUCT(BlueprintType)
struct RPGCORE_API FCharacterAnimInput
{
	GENERATED_BODY()

	FCharacterAnimInput() = default;

	// The gameplay tag representing the animation type
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer AnimTagContainer = FGameplayTagContainer::EmptyContainer;

	// The direction value for the animation
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float Direction = 0.f;

	// Is the character in air
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bIsInAir = false;
};

/*
 * Struct to define an animation montage entry for a character.
 */
USTRUCT(BlueprintType)
struct FCharacterAnimEntry
{
	GENERATED_BODY()

	FCharacterAnimEntry() = default;

	// The animation montage to play.
	UPROPERTY(BlueprintReadOnly)
	TSoftObjectPtr<UAnimMontage> Montage = nullptr;

	// The play rate of the montage.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float PlayRate = 1.0f;

	// The section of the montage to start from.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName StartSection = NAME_None;

	// Whether to stop the montage when the ability ends.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bStopWhenAbilityEnds = true;

	// The scale applied to root motion translation from the montage.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float AnimRootMotionTranslationScale = 1.f;

	// The time in seconds to start the montage from.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float StartTimeSeconds = 0.f;

	// Whether to allow interruption after blend out.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bAllowInterruptAfterBlendOut = false;

	// Check if the montage is valid
	bool IsValid() const { return Montage != nullptr; }

};
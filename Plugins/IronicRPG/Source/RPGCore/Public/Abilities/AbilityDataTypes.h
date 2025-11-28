// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DataTypes/RPGId.h"
#include "AbilityDataTypes.generated.h"

/**
 * The type of magnitude calculation.
 */
UENUM(BlueprintType)
enum class EMagnitudeCalcType : uint8
{
	Flat,               // Use value directly as is (result = value)
	PercentOfMax,       // A percentage of the maximum value (result = max * value)
	PercentOfCurrent,   // A percentage of the current value (result = current * value)
};

/**
 * The timing policy for when the cost is applied.
 */
UENUM(BlueprintType)
enum class ECostPolicy : uint8
{
	OnActivation,		// When the ability is activated
	OnHit,				// When the ability hits a target
	Custom,				// Custom timing defined by the ability
};

/**
 * Struct to define an ability cost entry.
 */
USTRUCT(BlueprintType)
struct FAbilityCostEntry
{
    GENERATED_BODY()

	// The attribute used for the cost.
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayAttribute Attribute;

	// The magnitude of the cost.
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FScalableFloat Magnitude;

	// The calculation type for the magnitude.
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EMagnitudeCalcType CalcType = EMagnitudeCalcType::Flat;

	// If the attribute goes to zero after cost deduction, set it to 1 instead.
	// Such as for HP cost, to prevent character death.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bCanEndure = false;

};

/**
 * Struct to define the overall ability cost, including multiple cost entries.
 * And also includes cooldown information.
 */
USTRUCT(BlueprintType)
struct FAbilityCostSpec
{
    GENERATED_BODY()

	// Array of cost attributes for the ability.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Cost")
	TArray<FAbilityCostEntry> CostAttributes;

	// The policy for when the cost is applied.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Cost")
	ECostPolicy CostPolicy = ECostPolicy::OnActivation;

	// The cooldown time associated with this cost.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Cooldown")
	FScalableFloat CooldownDuration;

	// The gameplay tags representing the cooldown effect.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Cooldown")
	FGameplayTagContainer CooldownTags;

};

/**
 * Struct to define animation montage data for abilities.
 */
USTRUCT(BlueprintType)
struct FAnimMontageData
{
	GENERATED_BODY()

	FAnimMontageData() = default;

	// The name of this montage data entry.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Name = NAME_None;

	// The animation montage to play.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UAnimMontage> Montage = nullptr;

	// The section of the montage to start from.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName StartSection = NAME_None;

	// The play rate of the montage.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float PlayRate = 1.0f;

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
};
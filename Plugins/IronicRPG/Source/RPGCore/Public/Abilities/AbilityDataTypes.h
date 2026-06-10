// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DataTypes/RPGId.h"
#include "Abilities/AbilityTargetResolver.h"
#include "AbilityDataTypes.generated.h"

/**
 * The type of magnitude calculation.
 */
UENUM(BlueprintType)
enum class EMagnitudeCalcType : uint8
{
	// Use value directly as is (result = value)
	Flat,

	// A percentage of the maximum value (result = max * value)
	PercentOfMax,

	// A percentage of the current value (result = current * value)
	PercentOfCurrent,
};

/**
 * The timing policy for when the cost is applied.
 */
UENUM(BlueprintType)
enum class ECostPolicy : uint8
{
	// When the ability is activated
	OnActivation,

	// When the ability hits a target
	OnHit,

	// Custom timing defined by the ability
	Custom,
};

/**
 * Who provides the attribute used to calculate final damage (or other results) of the gameplay effect.
 */
UENUM(BlueprintType)
enum class EAttributeProvider : uint8
{
	Source,
	Target,
};

/**
 * The way an ability was granted to a character.
 */
UENUM(BlueprintType)
enum class EAbilitySavePolicy : uint8
{
	NonSavable		UMETA(Hidden),
	Default,
	Equippable,
};

/**
 * The policy for when to apply an ability effect.
 */
UENUM(BlueprintType)
enum class EEffectApplyPolicy : uint8
{
	// Apply the effect when receiving a gameplay event notification with matching tags.
	ByNotification,			

	// Apply the effect immediately when the ability is activated.
	ByActivation,			

	// Apply the effect right before the ability ends.
	BeforeEndAbility,		
};

/**
 * The policy for rounding the final magnitude of an ability effect.
 */
UENUM(BlueprintType)
enum class EMagnitudeRoundingMode : uint8
{
	// No rounding, use the raw float value.
	None, 		

	// Always round down (floor)
	RoundDown,		

	// Always round up (ceil)
	RoundUp,			

	// Round to the nearest integer
	RoundNearest,		

	// Round to nearest integer, but .5 rounds to the nearest even number (e.g. 1.5 -> 2, 2.5 -> 2)
	RoundHalfToEven,	
};

/**
* The relation between the source and target of an ability effect, used to determine which rounding policy to use.
 */
UENUM()
enum class EEffectRelationType : uint8
{
	// Unknown target type, should be treated as an error case.
	Unknown UMETA(Hidden),		

	// The source is an enemy to the player character, such as a damage effect from an enemy to an ally.
	EnemyToAlly,			

	// The source is an ally to the player character, such as a damage effect from an ally to an enemy.
	AllyToEnemy,			

	// Any source type to a neutral target.
	AnyToNeutral,			

	// Neutral source to any target type.
	NeutralToAny,

	// The source and target are the same type, such as a self-buff or heal.
	ToSelf,

	Num UMETA(Hidden)
};

/**
 * Struct to define an ability by its Id and level.
 */
USTRUCT(BlueprintType)
struct FAbilityData
{
	GENERATED_BODY()

	// The Id of the ability.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(IdType = "Ability"))
	FRPGId AbilityId;

	// The level of the ability.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level = 1;

	// The way this ability was granted.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(InvalidEnumValues = "NonSavable", EditCondition = "bSavePolicyLock == false", HideEditConditionToggle))
	EAbilitySavePolicy AbilityGrantType = EAbilitySavePolicy::Default;

	// The ability asset reference.
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<const class UAbilityAsset> AbilityAsset = nullptr;

	// The handle of the granted ability spec
	UPROPERTY(BlueprintReadOnly)
	FGameplayAbilitySpecHandle AbilitySpecHandle;

	FAbilityData() = default;

	FAbilityData(FRPGId InAbilityId, int32 InLevel, EAbilitySavePolicy InGrantType)
		: AbilityId(InAbilityId), Level(InLevel), AbilityGrantType(InGrantType)
	{
	}

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	bool bSavePolicyLock = false;
#endif // WITH_EDITORONLY_DATA

};

/**
 * A term of the ability formula.
 * Result = Attribute.Value * Coefficient + FlatBonus
 */
USTRUCT(BlueprintType)
struct FAbilityFormulaTerm
{
	GENERATED_BODY()

	// Whose attribute? Usually the Source, but can use Target.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EAttributeProvider Provider = EAttributeProvider::Source;

	// The attribute used in this term.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayAttribute Attribute;

	// The coefficient multiplying the attribute.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FScalableFloat Coefficient = 1.f;

	// Optional flat bonus added to this term.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float FlatBonus = 0.f;

};

/**
 * An ability formula consisting of multiple terms.
 * Result = SUM(FAbilityFormulaTerm) * BaseCoefficient + BaseBonus
 */
USTRUCT(BlueprintType)
struct FAbilityFormula
{
	GENERATED_BODY()

	// The terms that will sum in the formula.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FAbilityFormulaTerm> Terms;

	// The coefficient multiplying the sum of the terms.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FScalableFloat BaseCoefficient = 1.f;

	// Optional flat bonus added to sum result.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BaseBonus = 0.f;
};

USTRUCT(BlueprintType)
struct FAbilityModifierFormula
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayAttribute ModifiedAttribute;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<EGameplayModOp::Type> ModifierOp = EGameplayModOp::Additive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FAbilityFormula Formula;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag EffectTypeTag;
};

/**
 * An ability effect specification formula.
 */
USTRUCT(BlueprintType)
struct FAbilityEffectSpec
{
	GENERATED_BODY()

	// The policy for when to apply the effect.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EEffectApplyPolicy ApplyPolicy = EEffectApplyPolicy::ByNotification;

	// Tag identifying the effect.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditCondition = "ApplyPolicy == EEffectApplyPolicy::ByNotification"))
	FGameplayTag EffectTag;

	// The gameplay effect class to apply.
	UPROPERTY(EditAnywhere)
	TSubclassOf<UGameplayEffect> EffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FAbilityModifierFormula> ModifierFormulas;

	// The resolver used to find targets for this effect.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced)
	TObjectPtr<UAbilityTargetResolver> TargetResolver;

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

	// Array of cost entries for the ability.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Cost")
	TArray<FAbilityCostEntry> CostEntries;

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

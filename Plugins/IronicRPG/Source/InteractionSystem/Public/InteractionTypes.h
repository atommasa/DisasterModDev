// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "InteractionTypes.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EInteractionActivationMode : uint8
{
	Press,
	Hold,
};

UENUM(BlueprintType)
enum class EInteractionActivationState : uint8
{
	Idle,
	Charging,
	Pending,
};

UENUM(BlueprintType)
enum class EInteractionUnavailablePresentation : uint8
{
	Hidden,
	Disabled,
};

UENUM(BlueprintType)
enum class EInteractionResultCode : uint8
{
	Succeeded,
	Failed,
	Canceled,
	TimedOut,
	TargetInvalidated,
};

USTRUCT(BlueprintType)
struct INTERACTIONSYSTEM_API FInteractionActivationPolicy
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	EInteractionActivationMode Mode = EInteractionActivationMode::Press;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction",
		meta=(EditCondition="Mode == EInteractionActivationMode::Hold", ClampMin="0.05", Units="s"))
	float HoldDuration = 0.5f;
};

USTRUCT(BlueprintType)
struct INTERACTIONSYSTEM_API FInteractionDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	FGameplayTag ActionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	bool bInitiallyEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	EInteractionUnavailablePresentation UnavailablePresentation = EInteractionUnavailablePresentation::Hidden;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	FGameplayTag InitialUnavailableReasonTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	FText InitialUnavailableReasonText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Detection",
		meta=(UseComponentPicker, AllowedClasses="/Script/Engine.PrimitiveComponent"))
	TArray<FComponentReference> DetectionComponents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Detection",
		meta=(UseComponentPicker, AllowedClasses="/Script/Engine.SceneComponent"))
	FComponentReference InteractionAnchor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Detection")
	bool bRequireLineOfSight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Activation")
	FInteractionActivationPolicy ActivationPolicy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Execution")
	bool bPlayerCancelable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Execution")
	bool bCancelWhenEligibilityLost = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Execution")
	bool bCancelOnGameplayControlLost = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Execution", meta=(ClampMin="0.0", Units="s"))
	float ExecutionTimeoutSeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct INTERACTIONSYSTEM_API FInteractionFocusView
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	FGuid TargetId;

	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	FGameplayTag ActionTag;

	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	FGameplayTag UnavailableReasonTag;

	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	FText UnavailableReasonText;

	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	FInteractionActivationPolicy ActivationPolicy;

	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	EInteractionActivationState ActivationState = EInteractionActivationState::Idle;

	UPROPERTY(BlueprintReadOnly, Category="Interaction", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ActivationProgress = 0.0f;
};

USTRUCT(BlueprintType)
struct INTERACTIONSYSTEM_API FInteractionViewSnapshot
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	int32 Revision = 0;

	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	bool bHasFocusedInteraction = false;

	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	FInteractionFocusView FocusedInteraction;

	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	FGuid ActiveRequestId;
};

USTRUCT(BlueprintType)
struct INTERACTIONSYSTEM_API FInteractionResult
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	EInteractionResultCode Code = EInteractionResultCode::Succeeded;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	FGameplayTag ReasonTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	FText ReasonText;
};

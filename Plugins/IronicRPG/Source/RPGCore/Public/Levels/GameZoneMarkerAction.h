// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Levels/GameZonePointData.h"
#include "GameZoneMarkerAction.generated.h"

class APlayerController;
class UGameZoneMarkerAction;
class UTexture2D;

UENUM(BlueprintType)
enum class EGameZoneMarkerActionAvailability : uint8
{
    Available,
    Unavailable,
    Hidden,
};

UENUM(BlueprintType)
enum class EGameZoneMarkerActionConfirmation : uint8
{
    None,
    Required,
};

UENUM(BlueprintType)
enum class EGameZoneMarkerActionCompletion : uint8
{
    Succeeded,
    Failed,
    Cancelled,
};

USTRUCT(BlueprintType)
struct RPGCORE_API FGameZoneMarkerActionDefinition
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag ActionTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UTexture2D> Icon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 SortOrder = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EGameZoneMarkerActionConfirmation Confirmation = EGameZoneMarkerActionConfirmation::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditCondition="Confirmation == EGameZoneMarkerActionConfirmation::Required"))
    FText ConfirmationText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<UGameZoneMarkerAction> ActionClass;
};

USTRUCT(BlueprintType)
struct RPGCORE_API FGameZoneMarkerActionContext
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly)
    FGameZonePointData Point;

    UPROPERTY(BlueprintReadOnly)
    EMapMarkerDisplayMode PresentationMode = EMapMarkerDisplayMode::None;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<APlayerController> PlayerController = nullptr;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<UObject> WorldContextObject = nullptr;
};

USTRUCT(BlueprintType)
struct RPGCORE_API FGameZoneMarkerActionAvailabilityResult
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite)
    EGameZoneMarkerActionAvailability Availability = EGameZoneMarkerActionAvailability::Available;

    UPROPERTY(BlueprintReadWrite)
    FText Reason;
};

USTRUCT(BlueprintType)
struct RPGCORE_API FGameZoneMarkerActionResult
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite)
    EGameZoneMarkerActionCompletion Completion = EGameZoneMarkerActionCompletion::Failed;

    UPROPERTY(BlueprintReadWrite)
    FText Message;
};

USTRUCT(BlueprintType)
struct RPGCORE_API FGameZoneMarkerActionOption
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly)
    FGameZoneMarkerActionDefinition Definition;

    UPROPERTY(BlueprintReadOnly)
    FGameZoneMarkerActionAvailabilityResult Availability;
};

/**
 * One runtime execution of a data-authored marker action.
 *
 * The definition lives on UMapMarkerTypeAsset. The subsystem creates a fresh
 * instance for every execution so asynchronous state is never shared.
 */
UCLASS(Abstract, Blueprintable)
class RPGCORE_API UGameZoneMarkerAction : public UObject
{
    GENERATED_BODY()

public:
    /** Read-only query on the class default object. Return empty text to use the authored name. */
    UFUNCTION(BlueprintNativeEvent, Category="RPG|GameZone|Marker Action")
    FText GetDisplayName(const FGameZoneMarkerActionContext& Context, const FText& DefaultDisplayName) const;
    virtual FText GetDisplayName_Implementation(const FGameZoneMarkerActionContext& Context, const FText& DefaultDisplayName) const;

    UFUNCTION(BlueprintNativeEvent, Category="RPG|GameZone|Marker Action")
    FGameZoneMarkerActionAvailabilityResult GetAvailability(const FGameZoneMarkerActionContext& Context) const;
    virtual FGameZoneMarkerActionAvailabilityResult GetAvailability_Implementation(const FGameZoneMarkerActionContext& Context) const;

    void StartAction(const FGameZoneMarkerActionContext& Context, TFunction<void(const FGameZoneMarkerActionResult&)> Completion);
    void CancelAction();

protected:
    UFUNCTION(BlueprintNativeEvent, Category="RPG|GameZone|Marker Action")
    void ExecuteAction(const FGameZoneMarkerActionContext& Context);
    virtual void ExecuteAction_Implementation(const FGameZoneMarkerActionContext& Context);

    UFUNCTION(BlueprintNativeEvent, Category="RPG|GameZone|Marker Action")
    void CancelActionExecution();
    virtual void CancelActionExecution_Implementation();

    UFUNCTION(BlueprintCallable, Category="RPG|GameZone|Marker Action")
    void CompleteAction(const FGameZoneMarkerActionResult& Result);

private:
    TFunction<void(const FGameZoneMarkerActionResult&)> CompletionCallback;
    bool bIsRunning = false;
};

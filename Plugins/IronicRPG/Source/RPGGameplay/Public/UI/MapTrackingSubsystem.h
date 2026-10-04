// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "MapTrackingSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRPGMapTrackingChanged, FGuid, PreviousPointId, FGuid, CurrentPointId);

/** Player intent shared by all map presentations. Independent of marker state and Live update mode. */
UCLASS()
class RPGGAMEPLAY_API UMapTrackingSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    /** Replaces the single tracked target. Valid IDs may refer to currently unloaded markers. */
    UFUNCTION(BlueprintCallable, Category="RPG|Map|Tracking")
    bool TrackMarker(const FGuid& PointId);

    UFUNCTION(BlueprintCallable, Category="RPG|Map|Tracking")
    void ClearTrackedMarker();

    UFUNCTION(BlueprintPure, Category="RPG|Map|Tracking")
    bool IsMarkerTracked(const FGuid& PointId) const;

    UFUNCTION(BlueprintPure, Category="RPG|Map|Tracking")
    FGuid GetTrackedMarkerId() const { return TrackedMarkerId; }

public:
    UPROPERTY(BlueprintAssignable, Category="RPG|Map|Tracking")
    FRPGMapTrackingChanged OnTrackingChanged;

private:
    /** Session-only intent; closing a map or unloading a marker does not clear it. */
    UPROPERTY(Transient)
    FGuid TrackedMarkerId;
};

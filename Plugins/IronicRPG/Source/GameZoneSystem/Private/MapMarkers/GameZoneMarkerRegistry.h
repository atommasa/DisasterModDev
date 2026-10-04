// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MapMarkers/GameZoneMarkerTypes.h"

class UGameZonePointComponent;

enum class EGameZoneMarkerMutationError : uint8
{
    None,
    InvalidPointId,
    DuplicateBakedSource,
    DuplicateLiveSource,
    LiveSourceMismatch,
    InvalidSaveOverride,
};

struct FGameZoneMarkerMutationResult
{
    EGameZoneMarkerMutationError Error = EGameZoneMarkerMutationError::None;
    TArray<FMapMarkerChange> Changes;
    int64 Revision = 0;

    bool WasApplied() const
    {
        return Error == EGameZoneMarkerMutationError::None && Changes.Num() > 0;
    }
};

class FGameZoneMarkerRegistry
{
public:
    FGameZoneMarkerMutationResult CommitBakedZone(const FRPGId& ZoneId, const TMap<FGuid, FGameZonePointData>& Points);
    FGameZoneMarkerMutationResult RemoveBakedZone(const FRPGId& ZoneId);
    FGameZoneMarkerMutationResult RegisterLive(UGameZonePointComponent& Component, const FRPGId& RuntimeZoneId);
    FGameZoneMarkerMutationResult UnregisterLive(UGameZonePointComponent& Component);
    FGameZoneMarkerMutationResult ReplaceSaveOverrides(const TMap<FGuid, FGameZonePointData>& Overrides);
    FGameZoneMarkerMutationResult SetSaveOverride(const FGameZonePointData& Override);
    FGameZoneMarkerMutationResult ClearSaveOverride(const FGuid& PointId);

    // Updates automatic Saveable entries from live snapshots, preserves
    // explicit Custom entries, and omits NotSaveable entries.
    TMap<FGuid, FGameZonePointData> CaptureSaveOverrides();

    FGameZoneMarkerMutationResult NotifyLiveChanged(
        UGameZonePointComponent& Component);

    // Read-only: an invalid live source may resolve through baked data, but this
    // does not mutate the record or advance the revision.
    bool TryResolve(const FGuid& PointId, FResolvedGameZonePoint& OutPoint) const;

    TArray<FResolvedGameZonePoint> BuildSnapshot(
        const TSet<FRPGId>& ZoneIds,
        EMapMarkerDisplayMode Mode) const;

    // Converts stale weak live sources into explicit fallback/removal changes.
    // The caller owns when to run this safe-point mutation and publish Changes.
    FGameZoneMarkerMutationResult SanitizeInvalidLive();

    int64 GetRevision() const { return Revision; }

private:
    struct FRecord
    {
        TOptional<FGameZonePointData> BakedData;
        FRPGId BakedSourceZoneId;
        TOptional<FGameZonePointData> SavedData;
        TWeakObjectPtr<UGameZonePointComponent> LiveComponent;
        TOptional<FGameZonePointData> LiveInitialData;
        FRPGId LiveZoneId;
        int64 LastRevision = 0;
    };

    TMap<FGuid, FRecord> Records;
    int64 Revision = 0;
};

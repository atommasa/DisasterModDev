// Copyright Ironic Studio. All Rights Reserved.

#include "UI/MapTrackingSubsystem.h"

bool UMapTrackingSubsystem::TrackMarker(const FGuid& PointId)
{
    if (!PointId.IsValid())
    {
        return false;
    }
    if (TrackedMarkerId != PointId)
    {
        const FGuid PreviousPointId = TrackedMarkerId;
        TrackedMarkerId = PointId;
        OnTrackingChanged.Broadcast(PreviousPointId, TrackedMarkerId);
    }
    return true;
}

void UMapTrackingSubsystem::ClearTrackedMarker()
{
    if (TrackedMarkerId.IsValid())
    {
        const FGuid PreviousPointId = TrackedMarkerId;
        TrackedMarkerId.Invalidate();
        OnTrackingChanged.Broadcast(PreviousPointId, TrackedMarkerId);
    }
}

bool UMapTrackingSubsystem::IsMarkerTracked(const FGuid& PointId) const
{
    return PointId.IsValid() && TrackedMarkerId == PointId;
}

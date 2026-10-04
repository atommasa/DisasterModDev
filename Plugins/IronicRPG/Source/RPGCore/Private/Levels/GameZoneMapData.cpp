// Copyright Ironic Studio. All Rights Reserved.

#include "Levels/GameZoneMapData.h"

bool FGameZoneMapSheetMapping::ProjectWorldLocation(
    const FVector& WorldLocation,
    FGameZoneMapProjection& OutProjection) const
{
    OutProjection = {};

    if (WorldSize.X <= UE_SMALL_NUMBER || WorldSize.Y <= UE_SMALL_NUMBER)
    {
        return false;
    }

    const FVector Delta = WorldLocation - WorldOrigin;
    const double YawRadians = FMath::DegreesToRadians(static_cast<double>(WorldYaw));
    const double CosYaw = FMath::Cos(YawRadians);
    const double SinYaw = FMath::Sin(YawRadians);

    const double LocalX = Delta.X * CosYaw + Delta.Y * SinYaw;
    const double LocalY = -Delta.X * SinYaw + Delta.Y * CosYaw;

    OutProjection.UV.X = LocalX / WorldSize.X + 0.5;
    OutProjection.UV.Y = 0.5 - LocalY / WorldSize.Y;
    OutProjection.bIsInsideSheet =
        OutProjection.UV.X >= -KINDA_SMALL_NUMBER
        && OutProjection.UV.X <= 1.0 + KINDA_SMALL_NUMBER
        && OutProjection.UV.Y >= -KINDA_SMALL_NUMBER
        && OutProjection.UV.Y <= 1.0 + KINDA_SMALL_NUMBER;
    return true;
}

bool FGameZoneMapRegion::Contains(
    const FVector& WorldLocation,
    double BoundaryTolerance) const
{
    if (!Bounds.IsValid || !Bounds.ExpandBy(BoundaryTolerance).IsInsideOrOn(WorldLocation))
    {
        return false;
    }

    for (const FPlane& Plane : Planes)
    {
        if (Plane.PlaneDot(WorldLocation) > BoundaryTolerance)
        {
            return false;
        }
    }

    return true;
}

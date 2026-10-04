// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Maps/GameZoneMapTypes.h"

struct FRPGMiniMapUVBasis
{
    FVector2D Center = FVector2D::ZeroVector;
    FVector2D AxisX = FVector2D::ZeroVector;
    FVector2D AxisY = FVector2D::ZeroVector;
    bool bIsValid = false;
};

/** Bridges the shared North-up viewport scale to the existing Sheet projection, without changing raw Sheet UVs. */
class FRPGMiniMapProjection
{
public:
    static FRPGMiniMapUVBasis BuildUVBasis(
        const FResolvedGameZoneMapSheet& Sheet, const FVector& Center, const FVector2D& ViewportSize, float LocalUnitsPerWorldMeter);
    static bool TryGetNorthUpAngle(float WorldYaw, float& OutAngle);
};

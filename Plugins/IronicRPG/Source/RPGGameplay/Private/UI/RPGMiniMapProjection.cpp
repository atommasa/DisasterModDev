// Copyright Ironic Studio. All Rights Reserved.

#include "UI/RPGMiniMapProjection.h"

FRPGMiniMapUVBasis FRPGMiniMapProjection::BuildUVBasis(
    const FResolvedGameZoneMapSheet& Sheet, const FVector& Center, const FVector2D& ViewportSize, float LocalUnitsPerWorldMeter)
{
    FRPGMiniMapUVBasis Result;
    if (Center.ContainsNaN() || Sheet.WorldOrigin.ContainsNaN() || !FMath::IsFinite(Sheet.WorldYaw)
        || !FMath::IsFinite(Sheet.WorldSize.X) || !FMath::IsFinite(Sheet.WorldSize.Y)
        || Sheet.WorldSize.X <= UE_SMALL_NUMBER || Sheet.WorldSize.Y <= UE_SMALL_NUMBER
        || !FMath::IsFinite(ViewportSize.X) || !FMath::IsFinite(ViewportSize.Y) || ViewportSize.X <= 0.0 || ViewportSize.Y <= 0.0
        || !FMath::IsFinite(LocalUnitsPerWorldMeter) || LocalUnitsPerWorldMeter <= 0.0f)
    {
        return Result;
    }

    FGameZoneMapSheetMapping Mapping;
    Mapping.WorldOrigin = Sheet.WorldOrigin;
    Mapping.WorldSize = Sheet.WorldSize;
    Mapping.WorldYaw = Sheet.WorldYaw;
    const double WorldUnitsPerLocalUnit = 100.0 / LocalUnitsPerWorldMeter;
    // Viewport right is world -X, and viewport down is world -Y.
    const FVector Right = FVector(-ViewportSize.X * WorldUnitsPerLocalUnit, 0.0, 0.0);
    const FVector Down = FVector(0.0, -ViewportSize.Y * WorldUnitsPerLocalUnit, 0.0);
    FGameZoneMapProjection CenterProjection;
    FGameZoneMapProjection RightProjection;
    FGameZoneMapProjection DownProjection;
    if (!Mapping.ProjectWorldLocation(Center, CenterProjection)
        || !Mapping.ProjectWorldLocation(Center + Right, RightProjection)
        || !Mapping.ProjectWorldLocation(Center + Down, DownProjection))
    {
        return Result;
    }

    Result.Center = CenterProjection.UV;
    Result.AxisX = RightProjection.UV - Result.Center;
    Result.AxisY = DownProjection.UV - Result.Center;
    const auto FitsMaterialVector = [](const FVector2D& Value)
    {
        return FMath::IsFinite(static_cast<float>(Value.X)) && FMath::IsFinite(static_cast<float>(Value.Y));
    };
    Result.bIsValid = FitsMaterialVector(Result.Center) && FitsMaterialVector(Result.AxisX) && FitsMaterialVector(Result.AxisY);
    return Result.bIsValid ? Result : FRPGMiniMapUVBasis();
}

bool FRPGMiniMapProjection::TryGetNorthUpAngle(float WorldYaw, float& OutAngle)
{
    OutAngle = 0.0f;
    if (!FMath::IsFinite(WorldYaw))
    {
        return false;
    }
    // A source icon pointing up has zero rotation when facing world +Y; world +X faces left.
    OutAngle = FRotator::NormalizeAxis(WorldYaw - 90.0f);
    return true;
}

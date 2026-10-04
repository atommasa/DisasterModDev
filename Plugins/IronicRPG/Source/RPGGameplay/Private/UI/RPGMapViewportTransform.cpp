// Copyright Ironic Studio. All Rights Reserved.

#include "UI/RPGMapViewportTransform.h"

namespace
{
constexpr double WorldUnitsPerMeter = 100.0;

bool IsFiniteVector(const FVector2D& Value)
{
    return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y);
}
}

FRPGMapViewportTransform::FRPGMapViewportTransform(const FRPGMapViewportParameters& InParameters)
    : Parameters(InParameters)
    , ViewportCenter(InParameters.ViewportSize * 0.5)
{
    Parameters.EdgePadding = FMath::Max(0.0f, Parameters.EdgePadding);
    bIsValid = IsFiniteVector(Parameters.ViewportSize)
        && Parameters.ViewportSize.X > UE_SMALL_NUMBER
        && Parameters.ViewportSize.Y > UE_SMALL_NUMBER
        && !Parameters.ViewCenterWorldLocation.ContainsNaN()
        && FMath::IsFinite(Parameters.LocalUnitsPerWorldMeter)
        && Parameters.LocalUnitsPerWorldMeter > UE_SMALL_NUMBER
        && FMath::IsFinite(Parameters.EdgePadding);
}

bool FRPGMapViewportTransform::IsValid() const
{
    return bIsValid;
}

FVector2D FRPGMapViewportTransform::ProjectNorthUpWorldOffset(const FVector& WorldOffset, float LocalUnitsPerWorldMeter)
{
    if (WorldOffset.ContainsNaN() || !FMath::IsFinite(LocalUnitsPerWorldMeter) || LocalUnitsPerWorldMeter <= UE_SMALL_NUMBER)
    {
        return FVector2D::ZeroVector;
    }

    const double LocalUnitsPerWorldUnit = LocalUnitsPerWorldMeter / WorldUnitsPerMeter;
    // North-up uses world +Y as north, so world +X points west in viewport space.
    return FVector2D(-WorldOffset.X, -WorldOffset.Y) * LocalUnitsPerWorldUnit;
}

FRPGMapMarkerPlacement FRPGMapViewportTransform::PlaceMarker(
    const FVector& MarkerWorldLocation,
    const FVector2D& MarkerHalfExtent,
    bool bPlayerTracked) const
{
    FRPGMapMarkerPlacement Result;
    if (!bIsValid || MarkerWorldLocation.ContainsNaN() || !IsFiniteVector(MarkerHalfExtent))
    {
        return Result;
    }

    const FVector2D SafeMarkerHalfExtent(
        FMath::Max(0.0, MarkerHalfExtent.X),
        FMath::Max(0.0, MarkerHalfExtent.Y));
    const FVector2D LocalOffset = ProjectWorldLocation(MarkerWorldLocation);
    if (!IsFiniteVector(LocalOffset))
    {
        return Result;
    }

    Result.UnclampedLocalPosition = ViewportCenter + LocalOffset;
    Result.LocalPosition = Result.UnclampedLocalPosition;
    Result.DirectionFromCenter = LocalOffset.GetSafeNormal();

    FVector2D ClampedOffset = LocalOffset;
    switch (Parameters.Shape)
    {
    case ERPGMapViewportShape::Rectangle:
        {
            const FVector2D EffectiveHalfExtent = ViewportCenter
                - SafeMarkerHalfExtent
                - FVector2D(Parameters.EdgePadding, Parameters.EdgePadding);
            if (EffectiveHalfExtent.X <= UE_SMALL_NUMBER || EffectiveHalfExtent.Y <= UE_SMALL_NUMBER)
            {
                return Result;
            }

            Result.bIsInsideViewport = FMath::Abs(LocalOffset.X) <= EffectiveHalfExtent.X
                && FMath::Abs(LocalOffset.Y) <= EffectiveHalfExtent.Y;
            if (!Result.bIsInsideViewport && bPlayerTracked)
            {
                const double AbsoluteX = FMath::Abs(LocalOffset.X);
                const double AbsoluteY = FMath::Abs(LocalOffset.Y);
                const double XScale = AbsoluteX > UE_SMALL_NUMBER
                    ? EffectiveHalfExtent.X / AbsoluteX
                    : TNumericLimits<double>::Max();
                const double YScale = AbsoluteY > UE_SMALL_NUMBER
                    ? EffectiveHalfExtent.Y / AbsoluteY
                    : TNumericLimits<double>::Max();
                ClampedOffset *= FMath::Min(XScale, YScale);
            }
            break;
        }

    case ERPGMapViewportShape::Circle:
        {
            const double MarkerRadius = SafeMarkerHalfExtent.Size();
            const double EffectiveRadius = FMath::Min(ViewportCenter.X, ViewportCenter.Y)
                - MarkerRadius
                - Parameters.EdgePadding;
            if (EffectiveRadius <= UE_SMALL_NUMBER)
            {
                return Result;
            }

            Result.bIsInsideViewport = LocalOffset.SizeSquared() <= FMath::Square(EffectiveRadius);
            if (!Result.bIsInsideViewport && bPlayerTracked)
            {
                ClampedOffset = Result.DirectionFromCenter * EffectiveRadius;
            }
            break;
        }

    default:
        return Result;
    }

    Result.bHasValidGeometry = true;
    Result.bIsVisible = Result.bIsInsideViewport || bPlayerTracked;
    Result.bIsClampedToEdge = !Result.bIsInsideViewport && Result.bIsVisible;
    if (Result.bIsClampedToEdge)
    {
        Result.LocalPosition = ViewportCenter + ClampedOffset;
    }
    return Result;
}

FVector2D FRPGMapViewportTransform::ProjectWorldLocation(const FVector& MarkerWorldLocation) const
{
    return ProjectNorthUpWorldOffset(MarkerWorldLocation - Parameters.ViewCenterWorldLocation, Parameters.LocalUnitsPerWorldMeter);
}

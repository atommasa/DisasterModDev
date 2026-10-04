// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class ERPGMapViewportShape : uint8
{
    Rectangle,
    Circle,
};

struct FRPGMapViewportParameters
{
    FVector2D ViewportSize = FVector2D::ZeroVector;
    FVector ViewCenterWorldLocation = FVector::ZeroVector;
    float LocalUnitsPerWorldMeter = 1.0f;
    float EdgePadding = 0.0f;
    ERPGMapViewportShape Shape = ERPGMapViewportShape::Rectangle;
};

struct FRPGMapMarkerPlacement
{
    FVector2D UnclampedLocalPosition = FVector2D::ZeroVector;
    FVector2D LocalPosition = FVector2D::ZeroVector;
    FVector2D DirectionFromCenter = FVector2D::ZeroVector;
    bool bHasValidGeometry = false;
    bool bIsInsideViewport = false;
    bool bIsVisible = false;
    bool bIsClampedToEdge = false;
};

/** Pure North-up world-to-viewport placement shared by map presentation adapters. */
class FRPGMapViewportTransform
{
public:
    explicit FRPGMapViewportTransform(const FRPGMapViewportParameters& InParameters);

public:
    bool IsValid() const;

public:
    static FVector2D ProjectNorthUpWorldOffset(const FVector& WorldOffset, float LocalUnitsPerWorldMeter);

public:
    FRPGMapMarkerPlacement PlaceMarker(
        const FVector& MarkerWorldLocation,
        const FVector2D& MarkerHalfExtent,
        bool bPlayerTracked) const;

private:
    FVector2D ProjectWorldLocation(const FVector& MarkerWorldLocation) const;

private:
    FRPGMapViewportParameters Parameters;
    FVector2D ViewportCenter = FVector2D::ZeroVector;
    bool bIsValid = false;
};

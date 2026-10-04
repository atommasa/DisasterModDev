// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/RPGMapMarkerWidget.h"

struct FMapMarkerSelectionTarget
{
    FWorldMapMarkerView View;
    FVector2D BoundsMinimum = FVector2D::ZeroVector;
    FVector2D BoundsMaximum = FVector2D::ZeroVector;
    int32 ZOrder = 0;
};

/** Resolves a pointer position to a deterministic set of selectable World Map markers. */
class FMapMarkerSelectionResolver
{
public:
    static TArray<FWorldMapMarkerView> Resolve(
        const FVector2D& PointerPosition,
        float SelectionPadding,
        TConstArrayView<FMapMarkerSelectionTarget> Targets);
};

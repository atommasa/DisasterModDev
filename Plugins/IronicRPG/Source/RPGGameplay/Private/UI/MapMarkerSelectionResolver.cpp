// Copyright Ironic Studio. All Rights Reserved.

#include "UI/MapMarkerSelectionResolver.h"

namespace
{
struct FResolvedSelectionCandidate
{
    FWorldMapMarkerView View;
    double DistanceSquared = 0.0;
    int32 ZOrder = 0;
};

bool IsGuidLess(const FGuid& Left, const FGuid& Right)
{
    if (Left.A != Right.A)
    {
        return Left.A < Right.A;
    }
    if (Left.B != Right.B)
    {
        return Left.B < Right.B;
    }
    if (Left.C != Right.C)
    {
        return Left.C < Right.C;
    }
    return Left.D < Right.D;
}
}

TArray<FWorldMapMarkerView> FMapMarkerSelectionResolver::Resolve(
    const FVector2D& PointerPosition,
    float SelectionPadding,
    TConstArrayView<FMapMarkerSelectionTarget> Targets)
{
    TArray<FResolvedSelectionCandidate> Candidates;
    Candidates.Reserve(Targets.Num());

    const double Padding = FMath::Max(0.0f, SelectionPadding);
    for (const FMapMarkerSelectionTarget& Target : Targets)
    {
        const FGuid& PointId = Target.View.Point.Data.PointId;
        if (!PointId.IsValid() || Target.View.Point.Data.MarkerState == EGameZonePointState::Hide)
        {
            continue;
        }

        const FVector2D BoundsMinimum(
            FMath::Min(Target.BoundsMinimum.X, Target.BoundsMaximum.X),
            FMath::Min(Target.BoundsMinimum.Y, Target.BoundsMaximum.Y));
        const FVector2D BoundsMaximum(
            FMath::Max(Target.BoundsMinimum.X, Target.BoundsMaximum.X),
            FMath::Max(Target.BoundsMinimum.Y, Target.BoundsMaximum.Y));
        const FVector2D ExpandedMinimum = BoundsMinimum - FVector2D(Padding, Padding);
        const FVector2D ExpandedMaximum = BoundsMaximum + FVector2D(Padding, Padding);
        if (PointerPosition.X < ExpandedMinimum.X || PointerPosition.X > ExpandedMaximum.X
            || PointerPosition.Y < ExpandedMinimum.Y || PointerPosition.Y > ExpandedMaximum.Y)
        {
            continue;
        }

        const FVector2D ClosestPoint(
            FMath::Clamp(PointerPosition.X, BoundsMinimum.X, BoundsMaximum.X),
            FMath::Clamp(PointerPosition.Y, BoundsMinimum.Y, BoundsMaximum.Y));
        Candidates.Add({Target.View, FVector2D::DistSquared(PointerPosition, ClosestPoint), Target.ZOrder});
    }

    Candidates.Sort([](const FResolvedSelectionCandidate& Left, const FResolvedSelectionCandidate& Right)
        {
            if (!FMath::IsNearlyEqual(Left.DistanceSquared, Right.DistanceSquared))
            {
                return Left.DistanceSquared < Right.DistanceSquared;
            }
            if (Left.ZOrder != Right.ZOrder)
            {
                return Left.ZOrder > Right.ZOrder;
            }
            return IsGuidLess(Left.View.Point.Data.PointId, Right.View.Point.Data.PointId);
        });

    TArray<FWorldMapMarkerView> Result;
    Result.Reserve(Candidates.Num());
    for (FResolvedSelectionCandidate& Candidate : Candidates)
    {
        Result.Add(MoveTemp(Candidate.View));
    }
    return Result;
}

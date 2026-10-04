// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Rebuildable XY broad phase; never owns marker state or gameplay data. Coordinates are centimeters. */
class FRPGMiniMapMarkerIndex
{
public:
    void Reset();
    void Upsert(const FGuid& Id, const FVector& Location);
    void Remove(const FGuid& Id);
    void Query(const FVector2D& Center, double HalfExtent, TArray<FGuid>& OutIds) const;
    int32 Num() const { return Entries.Num(); }

private:
    struct FEntry { FVector2D Position; FInt64Point Cell; };
    static constexpr double CellSize = 10000.0;
    static bool GetCell(const FVector2D& Position, FInt64Point& OutCell);
    TMap<FGuid, FEntry> Entries;
    TMap<FInt64Point, TSet<FGuid>> Cells;
};

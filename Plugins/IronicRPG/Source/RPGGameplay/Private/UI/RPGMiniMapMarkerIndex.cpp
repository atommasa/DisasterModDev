// Copyright Ironic Studio. All Rights Reserved.

#include "UI/RPGMiniMapMarkerIndex.h"

bool FRPGMiniMapMarkerIndex::GetCell(const FVector2D& Position, FInt64Point& OutCell)
{
    // Stay well inside int64 conversion/iteration limits, including for malformed data.
    if (Position.ContainsNaN() || FMath::Abs(Position.X / CellSize) > 1.e15 || FMath::Abs(Position.Y / CellSize) > 1.e15)
    {
        return false;
    }
    OutCell = FInt64Point(FMath::FloorToInt64(Position.X / CellSize), FMath::FloorToInt64(Position.Y / CellSize));
    return true;
}

void FRPGMiniMapMarkerIndex::Reset()
{
    Entries.Reset();
    Cells.Reset();
}

void FRPGMiniMapMarkerIndex::Upsert(const FGuid& Id, const FVector& Location)
{
    FInt64Point Cell;
    const FVector2D Position(Location.X, Location.Y);
    if (!Id.IsValid() || Location.ContainsNaN() || !GetCell(Position, Cell))
    {
        Remove(Id);
        return;
    }
    if (FEntry* Existing = Entries.Find(Id); Existing && Existing->Cell == Cell)
    {
        Existing->Position = Position;
        return;
    }
    Remove(Id);
    Entries.Add(Id, {Position, Cell});
    Cells.FindOrAdd(Cell).Add(Id);
}

void FRPGMiniMapMarkerIndex::Remove(const FGuid& Id)
{
    FEntry Entry;
    if (Entries.RemoveAndCopyValue(Id, Entry))
    {
        TSet<FGuid>& Bucket = Cells.FindChecked(Entry.Cell);
        Bucket.Remove(Id);
        if (Bucket.IsEmpty()) { Cells.Remove(Entry.Cell); }
    }
}

void FRPGMiniMapMarkerIndex::Query(const FVector2D& Center, double HalfExtent, TArray<FGuid>& OutIds) const
{
    OutIds.Reset();
    FInt64Point MinCell, MaxCell;
    if (!FMath::IsFinite(HalfExtent) || HalfExtent < 0.0
        || !GetCell(Center - FVector2D(HalfExtent), MinCell) || !GetCell(Center + FVector2D(HalfExtent), MaxCell))
    {
        return;
    }
    const auto Visit = [&](const TSet<FGuid>& Bucket)
    {
        for (const FGuid& Id : Bucket)
        {
            const FVector2D Delta = Entries.FindChecked(Id).Position - Center;
            if (FMath::Abs(Delta.X) <= HalfExtent && FMath::Abs(Delta.Y) <= HalfExtent) { OutIds.Add(Id); }
        }
    };
    const double CellCount = double(MaxCell.X - MinCell.X + 1) * double(MaxCell.Y - MinCell.Y + 1);
    // Huge zoom-out queries visit occupied cells, not billions of empty cells. No candidate truncation.
    if (CellCount > FMath::Max(64, Cells.Num()))
    {
        for (const auto& Pair : Cells)
        {
            if (Pair.Key.X >= MinCell.X && Pair.Key.X <= MaxCell.X && Pair.Key.Y >= MinCell.Y && Pair.Key.Y <= MaxCell.Y)
            {
                Visit(Pair.Value);
            }
        }
    }
    else
    {
        for (int64 X = MinCell.X; X <= MaxCell.X; ++X)
        {
            for (int64 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
            {
                if (const TSet<FGuid>* Bucket = Cells.Find(FInt64Point(X, Y))) { Visit(*Bucket); }
            }
        }
    }
}

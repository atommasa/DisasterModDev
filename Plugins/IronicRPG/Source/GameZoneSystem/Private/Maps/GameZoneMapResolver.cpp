// Copyright Ironic Studio. All Rights Reserved.

#include "Maps/GameZoneMapResolver.h"

FGameZoneMapResolver::FGameZoneMapResolver(
    const FRPGId& InZoneId,
    TConstArrayView<FGameZoneMapLayer> InLayers,
    TConstArrayView<FGameZoneMapSheet> InSheets,
    TConstArrayView<FGameZoneMapSheetMapping> InMappings,
    TConstArrayView<FGameZoneMapRegion> InRegions,
    const FGameZoneMapSheetId& InDefaultSheetId,
    const FGuid& InMapBakeRevision)
    : ZoneId(InZoneId)
    , Layers(InLayers)
    , Sheets(InSheets)
    , Mappings(InMappings)
    , Regions(InRegions)
    , DefaultSheetId(InDefaultSheetId)
    , MapBakeRevision(InMapBakeRevision)
    , WorldBounds(BuildWorldBounds())
    , bIsValid(BuildIndices())
{
}

bool FGameZoneMapResolver::TryResolveAtLocation(
    const FVector& WorldLocation,
    TConstArrayView<FGameZoneMapSheetOverride> Overrides,
    FResolvedGameZoneMapSheet& OutSheet) const
{
    if (!bIsValid)
    {
        return false;
    }

    if (const FGameZoneMapSheetOverride* Override = SelectOverride(Overrides))
    {
        return TryResolveById(Override->SheetId, WorldLocation, OutSheet);
    }

    const FGameZoneMapRegion* Region = nullptr;
    const EGameZoneMapRegionSelection RegionSelection = SelectRegion(
        WorldLocation,
        Region);
    if (RegionSelection == EGameZoneMapRegionSelection::Ambiguous)
    {
        return false;
    }

    if (RegionSelection == EGameZoneMapRegionSelection::Selected)
    {
        return TryResolveById(Region->SheetId, WorldLocation, OutSheet);
    }

    return TryResolveById(DefaultSheetId, WorldLocation, OutSheet);
}

bool FGameZoneMapResolver::TryResolveInLayerAtLocation(
    const FGameZoneMapLayerId& LayerId,
    const FVector& WorldLocation,
    FResolvedGameZoneMapSheet& OutSheet) const
{
    OutSheet = {};
    if (!bIsValid || !LayerIndices.Contains(LayerId))
    {
        return false;
    }

    const FGameZoneMapSheet* StableFallback = nullptr;
    const FGameZoneMapSheet* ContainingSheet = nullptr;

    for (const FGameZoneMapSheet& Sheet : Sheets)
    {
        if (Sheet.LayerId != LayerId)
        {
            continue;
        }

        if (!StableFallback
            || Sheet.SortOrder < StableFallback->SortOrder
            || (Sheet.SortOrder == StableFallback->SortOrder
                && Sheet.SheetId.ToString() < StableFallback->SheetId.ToString()))
        {
            StableFallback = &Sheet;
        }

        const int32* MappingIndex = MappingIndices.Find(Sheet.SheetId);
        FGameZoneMapProjection Projection;
        if (!MappingIndex
            || !Mappings[*MappingIndex].ProjectWorldLocation(WorldLocation, Projection)
            || !Projection.bIsInsideSheet)
        {
            continue;
        }

        if (ContainingSheet)
        {
            return false;
        }

        ContainingSheet = &Sheet;
    }

    const FGameZoneMapSheet* SelectedSheet = ContainingSheet
        ? ContainingSheet
        : StableFallback;
    return SelectedSheet
        && TryResolveById(SelectedSheet->SheetId, WorldLocation, OutSheet);
}

bool FGameZoneMapResolver::TryResolveById(
    const FGameZoneMapSheetId& SheetId,
    const FVector& QueryLocation,
    FResolvedGameZoneMapSheet& OutSheet) const
{
    if (!TryResolveById(SheetId, OutSheet))
    {
        return false;
    }

    const int32* MappingIndex = MappingIndices.Find(SheetId);
    FGameZoneMapProjection Projection;
    if (!MappingIndex
        || !Mappings[*MappingIndex].ProjectWorldLocation(QueryLocation, Projection))
    {
        OutSheet = {};
        return false;
    }

    OutSheet.QueryProjection = Projection;
    return true;
}

bool FGameZoneMapResolver::TryResolveById(
    const FGameZoneMapSheetId& SheetId,
    FResolvedGameZoneMapSheet& OutSheet) const
{
    OutSheet = {};
    if (!bIsValid)
    {
        return false;
    }

    const int32* SheetIndex = SheetIndices.Find(SheetId);
    const int32* MappingIndex = MappingIndices.Find(SheetId);
    if (!SheetIndex || !MappingIndex)
    {
        return false;
    }

    const FGameZoneMapSheet& Sheet = Sheets[*SheetIndex];
    const FGameZoneMapSheetMapping& Mapping = Mappings[*MappingIndex];
    if (!LayerIndices.Contains(Sheet.LayerId))
    {
        return false;
    }

    OutSheet = {};
    OutSheet.ZoneId = ZoneId;
    OutSheet.LayerId = Sheet.LayerId;
    OutSheet.SheetId = Sheet.SheetId;
    OutSheet.DisplayName = Sheet.DisplayName;
    OutSheet.SortOrder = Sheet.SortOrder;
    OutSheet.MapBakeRevision = MapBakeRevision;
    OutSheet.WorldOrigin = Mapping.WorldOrigin;
    OutSheet.WorldSize = Mapping.WorldSize;
    OutSheet.WorldYaw = Mapping.WorldYaw;
    return true;
}

bool FGameZoneMapResolver::TryProjectWorldLocation(
    const FGameZoneMapSheetId& SheetId,
    const FVector& WorldLocation,
    FGameZoneMapProjection& OutProjection) const
{
    if (!bIsValid)
    {
        return false;
    }

    const int32* MappingIndex = MappingIndices.Find(SheetId);
    return MappingIndex
        && Mappings[*MappingIndex].ProjectWorldLocation(WorldLocation, OutProjection);
}

bool FGameZoneMapResolver::BuildIndices()
{
    if (!ZoneId.IsValid())
    {
        return false;
    }

    for (int32 Index = 0; Index < Layers.Num(); ++Index)
    {
        const FGameZoneMapLayerId& LayerId = Layers[Index].LayerId;
        if (!LayerId.IsValid() || LayerIndices.Contains(LayerId))
        {
            return false;
        }

        LayerIndices.Add(LayerId, Index);
    }

    for (int32 Index = 0; Index < Sheets.Num(); ++Index)
    {
        const FGameZoneMapSheet& Sheet = Sheets[Index];
        if (!Sheet.SheetId.IsValid()
            || SheetIndices.Contains(Sheet.SheetId)
            || !LayerIndices.Contains(Sheet.LayerId))
        {
            return false;
        }

        SheetIndices.Add(Sheet.SheetId, Index);
    }

    for (int32 Index = 0; Index < Mappings.Num(); ++Index)
    {
        const FGameZoneMapSheetMapping& Mapping = Mappings[Index];
        if (!Mapping.SheetId.IsValid()
            || MappingIndices.Contains(Mapping.SheetId)
            || !SheetIndices.Contains(Mapping.SheetId))
        {
            return false;
        }

        FGameZoneMapProjection IgnoredProjection;
        if (!Mapping.ProjectWorldLocation(Mapping.WorldOrigin, IgnoredProjection))
        {
            return false;
        }

        MappingIndices.Add(Mapping.SheetId, Index);
    }

    if (MappingIndices.Num() != SheetIndices.Num())
    {
        return false;
    }

    TSet<FGameZoneMapRegionId> RegionIds;
    for (const FGameZoneMapRegion& Region : Regions)
    {
        if (!Region.RegionId.IsValid()
            || RegionIds.Contains(Region.RegionId)
            || !SheetIndices.Contains(Region.SheetId))
        {
            return false;
        }

        RegionIds.Add(Region.RegionId);
    }

    return DefaultSheetId.IsValid() && SheetIndices.Contains(DefaultSheetId);
}

FGameZoneMapWorldBounds FGameZoneMapResolver::BuildWorldBounds() const
{
    FGameZoneMapWorldBounds Result;
    Result.ZoneId = ZoneId;
    Result.MapBakeRevision = MapBakeRevision;

    TSet<FGameZoneMapSheetId> DefinedSheetIds;
    for (const FGameZoneMapSheet& Sheet : Sheets)
    {
        if (Sheet.SheetId.IsValid())
        {
            DefinedSheetIds.Add(Sheet.SheetId);
        }
    }

    for (const FGameZoneMapSheetMapping& Mapping : Mappings)
    {
        const bool bHasFiniteGeometry =
            FMath::IsFinite(Mapping.WorldOrigin.X)
            && FMath::IsFinite(Mapping.WorldOrigin.Y)
            && FMath::IsFinite(Mapping.WorldSize.X)
            && FMath::IsFinite(Mapping.WorldSize.Y)
            && FMath::IsFinite(Mapping.WorldYaw);
        if (!DefinedSheetIds.Contains(Mapping.SheetId)
            || !bHasFiniteGeometry
            || Mapping.WorldSize.X <= UE_SMALL_NUMBER
            || Mapping.WorldSize.Y <= UE_SMALL_NUMBER)
        {
#if !UE_BUILD_SHIPPING
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("[GameZone] Ignoring invalid map bounds mapping %s in Zone %s."),
                *Mapping.SheetId.ToString(),
                *ZoneId.ToString());
#endif
            continue;
        }

        const FVector2D HalfSize = Mapping.WorldSize * 0.5;
        const double YawRadians = FMath::DegreesToRadians(static_cast<double>(Mapping.WorldYaw));
        const double CosYaw = FMath::Cos(YawRadians);
        const double SinYaw = FMath::Sin(YawRadians);
        const FVector2D LocalCorners[] = {
            FVector2D(-HalfSize.X, -HalfSize.Y),
            FVector2D(HalfSize.X, -HalfSize.Y),
            FVector2D(-HalfSize.X, HalfSize.Y),
            FVector2D(HalfSize.X, HalfSize.Y),
        };

        for (const FVector2D& LocalCorner : LocalCorners)
        {
            const FVector2D WorldCorner(
                Mapping.WorldOrigin.X + LocalCorner.X * CosYaw - LocalCorner.Y * SinYaw,
                Mapping.WorldOrigin.Y + LocalCorner.X * SinYaw + LocalCorner.Y * CosYaw);
            if (!Result.bIsValid)
            {
                Result.MinimumWorldXY = WorldCorner;
                Result.MaximumWorldXY = WorldCorner;
                Result.bIsValid = true;
                continue;
            }

            Result.MinimumWorldXY.X = FMath::Min(Result.MinimumWorldXY.X, WorldCorner.X);
            Result.MinimumWorldXY.Y = FMath::Min(Result.MinimumWorldXY.Y, WorldCorner.Y);
            Result.MaximumWorldXY.X = FMath::Max(Result.MaximumWorldXY.X, WorldCorner.X);
            Result.MaximumWorldXY.Y = FMath::Max(Result.MaximumWorldXY.Y, WorldCorner.Y);
        }
    }

    return Result;
}

const FGameZoneMapSheetOverride* FGameZoneMapResolver::SelectOverride(
    TConstArrayView<FGameZoneMapSheetOverride> Overrides) const
{
    const FGameZoneMapSheetOverride* Best = nullptr;

    for (const FGameZoneMapSheetOverride& Override : Overrides)
    {
        if (!SheetIndices.Contains(Override.SheetId))
        {
            continue;
        }

        if (!Best
            || Override.Priority > Best->Priority
            || (Override.Priority == Best->Priority && Override.Sequence > Best->Sequence))
        {
            Best = &Override;
        }
    }

    return Best;
}

EGameZoneMapRegionSelection FGameZoneMapResolver::SelectRegion(
    const FVector& WorldLocation,
    const FGameZoneMapRegion*& OutRegion) const
{
    OutRegion = nullptr;
    const FGameZoneMapRegion* Best = nullptr;
    double BestBoundsVolume = 0.0;
    bool bAmbiguous = false;

    for (const FGameZoneMapRegion& Region : Regions)
    {
        if (!Region.Contains(WorldLocation))
        {
            continue;
        }

        const double BoundsVolume = Region.Bounds.GetVolume();
        if (!Best
            || Region.Priority > Best->Priority
            || (Region.Priority == Best->Priority && BoundsVolume < BestBoundsVolume))
        {
            Best = &Region;
            BestBoundsVolume = BoundsVolume;
            bAmbiguous = false;
            continue;
        }

        if (Region.Priority == Best->Priority
            && FMath::IsNearlyEqual(BoundsVolume, BestBoundsVolume))
        {
            bAmbiguous = true;
        }
    }

    if (bAmbiguous)
    {
        return EGameZoneMapRegionSelection::Ambiguous;
    }

    OutRegion = Best;
    return Best
        ? EGameZoneMapRegionSelection::Selected
        : EGameZoneMapRegionSelection::None;
}

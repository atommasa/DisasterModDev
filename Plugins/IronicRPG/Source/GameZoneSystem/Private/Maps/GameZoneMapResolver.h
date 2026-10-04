// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Maps/GameZoneMapTypes.h"

struct FGameZoneMapSheetOverride
{
    FGameZoneMapSheetId SheetId;
    int32 Priority = 0;
    uint64 Sequence = 0;
};

enum class EGameZoneMapRegionSelection : uint8
{
    None,
    Selected,
    Ambiguous,
};

class FGameZoneMapResolver
{
public:
    FGameZoneMapResolver(
        const FRPGId& InZoneId,
        TConstArrayView<FGameZoneMapLayer> InLayers,
        TConstArrayView<FGameZoneMapSheet> InSheets,
        TConstArrayView<FGameZoneMapSheetMapping> InMappings,
        TConstArrayView<FGameZoneMapRegion> InRegions,
        const FGameZoneMapSheetId& InDefaultSheetId,
        const FGuid& InMapBakeRevision);

    bool IsValid() const { return bIsValid; }
    const FGameZoneMapWorldBounds& GetWorldBounds() const { return WorldBounds; }

    bool TryResolveAtLocation(
        const FVector& WorldLocation,
        TConstArrayView<FGameZoneMapSheetOverride> Overrides,
        FResolvedGameZoneMapSheet& OutSheet) const;

    bool TryResolveInLayerAtLocation(
        const FGameZoneMapLayerId& LayerId,
        const FVector& WorldLocation,
        FResolvedGameZoneMapSheet& OutSheet) const;

    bool TryResolveById(
        const FGameZoneMapSheetId& SheetId,
        const FVector& QueryLocation,
        FResolvedGameZoneMapSheet& OutSheet) const;

    bool TryResolveById(
        const FGameZoneMapSheetId& SheetId,
        FResolvedGameZoneMapSheet& OutSheet) const;

    bool TryProjectWorldLocation(
        const FGameZoneMapSheetId& SheetId,
        const FVector& WorldLocation,
        FGameZoneMapProjection& OutProjection) const;

private:
    bool BuildIndices();
    FGameZoneMapWorldBounds BuildWorldBounds() const;

    const FGameZoneMapSheetOverride* SelectOverride(
        TConstArrayView<FGameZoneMapSheetOverride> Overrides) const;

    EGameZoneMapRegionSelection SelectRegion(
        const FVector& WorldLocation,
        const FGameZoneMapRegion*& OutRegion) const;

private:
    FRPGId ZoneId;
    TArray<FGameZoneMapLayer> Layers;
    TArray<FGameZoneMapSheet> Sheets;
    TArray<FGameZoneMapSheetMapping> Mappings;
    TArray<FGameZoneMapRegion> Regions;
    FGameZoneMapSheetId DefaultSheetId;
    FGuid MapBakeRevision;
    FGameZoneMapWorldBounds WorldBounds;

    TMap<FGameZoneMapLayerId, int32> LayerIndices;
    TMap<FGameZoneMapSheetId, int32> SheetIndices;
    TMap<FGameZoneMapSheetId, int32> MappingIndices;
    bool bIsValid = false;
};

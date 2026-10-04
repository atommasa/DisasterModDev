// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "Levels/GameZoneMapData.h"
#include "Misc/DataValidation.h"

EDataValidationResult ValidateGameZoneMapAssetData(
    TConstArrayView<FGameZoneMapLayer> Layers,
    TConstArrayView<FGameZoneMapSheet> Sheets,
    const FGameZoneMapSheetId& DefaultSheetId,
    TConstArrayView<FGameZoneMapSheetMapping> Mappings,
    TConstArrayView<FGameZoneMapRegion> Regions,
    FDataValidationContext& Context);

EDataValidationResult ValidateGameZoneMapBakeData(
    TConstArrayView<FGameZoneMapLayer> Layers,
    TConstArrayView<FGameZoneMapSheet> Sheets,
    const FGameZoneMapSheetId& DefaultSheetId,
    TConstArrayView<FGameZoneMapSheetMapping> Mappings,
    TConstArrayView<FGameZoneMapRegion> Regions,
    FDataValidationContext& Context);

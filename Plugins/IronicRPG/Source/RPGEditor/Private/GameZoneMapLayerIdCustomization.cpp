// Copyright Ironic Studio. All Rights Reserved.

#include "GameZoneMapLayerIdCustomization.h"

#include "Levels/GameZoneAsset.h"

FGameZoneMapLayerIdCustomization::FGameZoneMapLayerIdCustomization()
    : FGameZoneMapIdCustomization(
        TEXT("ZoneMapLayerReference"),
        [](const UGameZoneAsset& ZoneAsset, TArray<FGameZoneMapIdOption>& OutOptions)
        {
            for (const FGameZoneMapLayer& Layer : ZoneAsset.GetMapLayers())
            {
                OutOptions.Add({Layer.LayerId.Value, Layer.DisplayName});
            }
        })
{
}

TSharedRef<IPropertyTypeCustomization> FGameZoneMapLayerIdCustomization::MakeInstance()
{
    return MakeShared<FGameZoneMapLayerIdCustomization>();
}

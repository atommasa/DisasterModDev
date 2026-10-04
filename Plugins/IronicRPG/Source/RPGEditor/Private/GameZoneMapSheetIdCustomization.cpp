// Copyright Ironic Studio. All Rights Reserved.

#include "GameZoneMapSheetIdCustomization.h"

#include "Levels/GameZoneAsset.h"

FGameZoneMapSheetIdCustomization::FGameZoneMapSheetIdCustomization()
    : FGameZoneMapIdCustomization(
        TEXT("ZoneMapSheetReference"),
        [](const UGameZoneAsset& ZoneAsset, TArray<FGameZoneMapIdOption>& OutOptions)
        {
            for (const FGameZoneMapSheet& Sheet : ZoneAsset.GetMapSheets())
            {
                OutOptions.Add({Sheet.SheetId.Value, Sheet.DisplayName});
            }
        })
{
}

TSharedRef<IPropertyTypeCustomization> FGameZoneMapSheetIdCustomization::MakeInstance()
{
    return MakeShared<FGameZoneMapSheetIdCustomization>();
}

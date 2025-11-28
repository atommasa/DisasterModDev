// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

struct FRPGAssetItem
{
    FAssetData AssetData;

    FRPGAssetItem(const FAssetData& InAssetData)
        : AssetData(InAssetData) {
    }

    FName GetName() const { return AssetData.AssetName; }
    FName GetId()   const { return AssetData.GetPrimaryAssetId().PrimaryAssetName; }
    FName GetType() const { return AssetData.GetPrimaryAssetId().PrimaryAssetType; }

    FString GetIconStyleName() const 
    { 
        return FString::Printf(TEXT("AssetType.%s"), *GetType().ToString());
    }
};

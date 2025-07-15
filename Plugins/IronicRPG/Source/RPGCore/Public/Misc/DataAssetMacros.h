// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Engine/AssetManager.h"
#include "Engine/DataAsset.h"

/*----------------------------------------------------------------------------
    Common Macros
----------------------------------------------------------------------------*/

/**
 * @brief Get primary data asset by FRPGId.
 * 
 * @param AssetType The asset type of this primary data asset
 * @param AssetIdStruct The FRPGId you want to use to retrieve the asset
*/
#define GET_PRIMARY_ASSET_BY_RPGID(AssetType, AssetIdStruct, OutDataClass)                  \
[&]() -> OutDataClass* {                                                                     \
    FPrimaryAssetId AssetId(FPrimaryAssetType(AssetType), AssetIdStruct.Id);                 \
    UObject* LoadedObject = UAssetManager::Get().GetPrimaryAssetObject(AssetId);             \
    return Cast<OutDataClass>(LoadedObject);                                                 \
}()

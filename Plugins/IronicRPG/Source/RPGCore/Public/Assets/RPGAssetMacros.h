// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Engine/AssetManager.h"
#include "Engine/DataAsset.h"

/*----------------------------------------------------------------------------
    Common Macros
----------------------------------------------------------------------------*/

/**
 * @brief Define getter functions for data assets.
 *
 * @param PropType      The data type of this property
 * @param PropName      The name of this property
 */
#define ASSET_PROP_GETTER(PropType, PropName) \
public: \
    PropType Get##PropName() const { return PropName; } \
protected:

/**
 * @brief Define the name of this asset type.
 *
 * This macro should be used in the class declaration of a UPrimaryDataAsset derived class.
 * 
 * @param TypeName      The name of this asset type
 */
#define DEFINE_ASSET_TYPE(TypeName, IdPrefix)                                               \
public:																					     \
	static FName GetAssetTypeStatic() { return TEXT(#TypeName); }                            \
	virtual FName GetAssetType() const override final { return GetAssetTypeStatic(); }       \
	static FName GetAssetIdPrefixStatic() { return TEXT(#IdPrefix); }                        \
	virtual FName GetAssetIdPrefix() const override final { return GetAssetIdPrefixStatic(); }

 /**
 * @brief Define the asset bundles for this asset type.
 * This macro should be used in the class declaration of a UPrimaryDataAsset derived class.
 * @param BundleName    The name of the asset bundle
 * @param ...           The names of the asset bundles to include
 */
#define DEFINE_ASSET_BUNDLES(...)                                               \
public:                                                                                      \
	static TArray<FName> GetAssetBundles() { return { __VA_ARGS__ }; }                                                                             \

/**
 * @brief Get primary data asset by FRPGId.
 * 
 * @param OutDataClass      The class type you want to cast the asset to
 * @param RPGId             The FRPGId you want to use to retrieve the asset
 */
#define GET_ASSET_BY_RPGID(RPGId, ...)                                                      \
	[RPGId]() {                                                                              \
		FPrimaryAssetId PrimaryAssetId(FName(RPGId.GetIdTypeString()), RPGId.Id);            \
		UObject* LoadedObject = UAssetManager::Get().GetPrimaryAssetObject(PrimaryAssetId);  \
		return __VA_ARGS__(LoadedObject);                                                    \
	}()

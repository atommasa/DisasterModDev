// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Assets/RPGPrimaryAsset.h"
#include "RPGAssetLibrary.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRPGAssetLibrary, Warning, All);

/**
 * Library for managing RPG assets in Unreal Engine.
 */
UCLASS()
class RPGCORE_API URPGAssetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public: // C++ Functions
	/*
	 * @brief Retrieves an asset by its ID asynchronously.
	 *
	 * @param Id			The unique identifier for the asset.
	 * @param OnResult		Callback function to handle the result when the asset is loaded.
	 *
	 * @return A handle to the streamable asset.
	 */
	static TSharedPtr<FStreamableHandle> GetAssetByRPGIdAsync(const FRPGId& Id, const TArray<FName>& Bundles, TFunction<void(URPGPrimaryAsset*)> OnResult);

	/*
	 * @brief Retrieves an array of assets by their IDs asynchronously.
	 *
	 * @param Ids			Array of unique identifiers for the assets.
	 * @param OnResult		Callback function to handle the result when the assets are loaded.
	 *
	 * @return A handle to the streamable asset array.
	 */
	static TSharedPtr<FStreamableHandle> GetAssetArrayByRPGIdsAsync(const TArray<FRPGId>& Ids, const TArray<FName>& Bundles, TFunction<void(TArray<URPGPrimaryAsset*>)> OnResult);

public: // BlueprintCallable Functions
	/*
	 * @brief Retrieves an asset by its ID.
	 * 
	 * @param AssetType		The type of the asset (e.g., "Character", "Item").
	 * @param Id			The unique identifier for the asset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Asset")
	static URPGPrimaryAsset* GetAssetByRPGId(const FRPGId& Id);

	/*
	 * @brief Retrieves an array of assets by their IDs.
	 *
	 * @param AssetType		The type of the asset (e.g., "Character", "Item").
	 * @param Ids			Array of unique identifiers for the assets.
	 * @param OutAssets		Output array containing the found assets.
	 */
	UFUNCTION(BlueprintCallable, Category = "Asset")
	static void GetAssetArrayByRPGIds(const TArray<FRPGId>& Ids, TArray<URPGPrimaryAsset*>& OutAssets);

	/*
	 * @brief Retrieves all assets of a specific type.
	 *
	 * @param AssetType		The type of the asset (e.g., "Character", "Item").
	 * @param OutAssets		Output array containing all found assets of the specified type.
	 */
	UFUNCTION(BlueprintCallable, Category = "Asset")
	static void GetAllAssetsOfType(FName AssetType, TArray<URPGPrimaryAsset*>& OutAssets);

};

// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Assets/RPGPrimaryAsset.h"
#include "RPGFlow.h"
#include "RPGAssetLibrary.generated.h"

RPGCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogRPGAssetLibrary, Warning, All);

/**
 * Library for managing RPG assets in Unreal Engine.
 */
UCLASS()
class RPGCORE_API URPGAssetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public: // C++ Functions
	template<typename TAsset = URPGPrimaryAsset>
	static TAsset* GetRPGAsset(const FRPGId& Id)
	{
		static_assert(TIsDerivedFrom<TAsset, URPGPrimaryAsset>::IsDerived,
			"GetRPGAsset can only be used to get URPGPrimaryAsset or its subclasses.");

		UAssetManager& AssetManager = UAssetManager::Get();

		return AssetManager.GetPrimaryAssetObject<TAsset>(Id.ToPrimaryAssetId());
	}

	/*
	 * @brief Retrieves an asset by its ID asynchronously.
	 *
	 * @param Id			The unique identifier for the asset.
	 * @param OnResult		Callback function to handle the result when the asset is loaded.
	 *
	 * @return A handle to the streamable asset.
	 */
	template<typename TAsset = URPGPrimaryAsset>
	static TRPGCoroutine<TRPGAsyncResult<TAsset*>> LoadAssetByRPGIdAsync(FRPGId Id, const TArray<FName>& Bundles)
	{
		static_assert(TIsDerivedFrom<TAsset, URPGPrimaryAsset>::IsDerived,
			"LoadAssetByRPGIdAsync can only be used to get URPGPrimaryAsset or its subclasses.");
		
		if (!Id.IsValid())
		{
			co_return TRPGAsyncResult<TAsset*>::Failure(
				TEXT("Asset.InvalidId"),
				TEXT("The RPG asset id is invalid.")
			);
		}

		const FPrimaryAssetId& PrimaryAssetId = Id.ToPrimaryAssetId();
		UAssetManager& AssetManager = UAssetManager::Get();

		TAsset* ExistingObject = AssetManager.GetPrimaryAssetObject<TAsset>(PrimaryAssetId);
		if (ExistingObject && Bundles.IsEmpty())
		{
			co_return TRPGAsyncResult<TAsset*>::Success(ExistingObject);
		}

		// Asset is not loaded, proceed with async loading
		TSharedPtr<FStreamableHandle> Handle = AssetManager.LoadPrimaryAsset(PrimaryAssetId, Bundles);

		co_await RPGFlow::Streamable(Handle);

		TAsset* LoadedAsset = AssetManager.GetPrimaryAssetObject<TAsset>(PrimaryAssetId);
		if (!LoadedAsset)
		{
			co_return TRPGAsyncResult<TAsset*>::Failure(
				TEXT("Asset.TypeMismatch"),
				FString::Printf(
					TEXT("Loaded asset %s is not of type %s."),
					*PrimaryAssetId.ToString(),
					*TAsset::StaticClass()->GetName()));
		}

		co_return TRPGAsyncResult<TAsset*>::Success(LoadedAsset);
	}
	/**
	 * Asynchronous blueprint loading method for assets.
	 */
	UFUNCTION(BlueprintCallable ,meta=(WorldContext = "WorldContextObject", Latent, LatentInfo = "LatentInfo", InternalUseParam = "ReturnValue",
		DeterminesOutputType = "AssetClass", DynamicOutputParam = "LoadedAssets", AutoCreateRefTerm = "Bundles"))
	static FRPGVoidCoroutine LoadAssetsByRPGIdAsync(
		UObject* WorldContextObject,
		TArray<FRPGId> Id,
		TArray<FName> Bundles,
		TSubclassOf<URPGPrimaryAsset> AssetClass,
		TArray<URPGPrimaryAsset*>& LoadedAssets,
		FLatentActionInfo LatentInfo
	);

	static TSharedPtr<FStreamableHandle> LoadAssetByRPGIdAsync(const FRPGId& Id, const TArray<FName>& Bundles, TFunction<void(URPGPrimaryAsset*)> OnResult);

	/*
	 * @brief Retrieves an array of assets by their IDs asynchronously.
	 *
	 * @param Ids			Array of unique identifiers for the assets.
	 * @param OnResult		Callback function to handle the result when the assets are loaded.
	 *
	 * @return A handle to the streamable asset array.
	 */
	static TSharedPtr<FStreamableHandle> LoadAssetArrayByRPGIdsAsync(const TArray<FRPGId>& Ids, const TArray<FName>& Bundles, TFunction<void(TArray<URPGPrimaryAsset*>)> OnResult);

	template<typename TAsset = URPGPrimaryAsset>
	static TRPGCoroutine<TRPGAsyncResult<TArray<TAsset*>>> LoadAssetArrayByRPGIdsAsync(const TArray<FRPGId>& Ids, const TArray<FName>& Bundles)
	{
		static_assert(TIsDerivedFrom<TAsset, URPGPrimaryAsset>::IsDerived,
			"LoadAssetArrayByRPGIdsAsync can only be used to get URPGPrimaryAsset or its subclasses.");

		if (Ids.IsEmpty())
		{
			co_return TRPGAsyncResult<TArray<TAsset*>>::Failure(
				TEXT("Asset.EmptyIdArray"),
				TEXT("The RPG ids array is empty.")
			);
		}

		UAssetManager& AssetManager = UAssetManager::Get();

		TArray<TAsset*> LoadedAssets;
		TArray<FPrimaryAssetId> PrimaryAssetIds;
		for (const FRPGId& Id : Ids)
		{
			const FPrimaryAssetId& PrimaryAssetId = Id.ToPrimaryAssetId();

			// Check if the asset is already loaded
			UObject* ExistingObject = AssetManager.GetPrimaryAssetObject<TAsset>(PrimaryAssetId);
			if (ExistingObject && Bundles.IsEmpty())
			{
				LoadedAssets.Add(Cast<TAsset>(ExistingObject));
				continue;
			}

			PrimaryAssetIds.Add(PrimaryAssetId);
		}

		if (LoadedAssets.Num() == Ids.Num())
		{
			// All assets are already loaded, return them
			co_return TRPGAsyncResult<TArray<TAsset*>>::Success(LoadedAssets);
		}

		// Asset is not loaded, proceed with async loading
		TSharedPtr<FStreamableHandle> Handle = AssetManager.LoadPrimaryAssets(PrimaryAssetIds, Bundles);

		co_await RPGFlow::Streamable(Handle);

		for (const FPrimaryAssetId& Pid : PrimaryAssetIds)
		{
			UObject* LoadedObject = UAssetManager::Get().GetPrimaryAssetObject(Pid);
			TAsset* Asset = Cast<TAsset>(LoadedObject);
			if (Asset)
			{
				LoadedAssets.Add(Asset);
			}
			else
			{
				UE_LOG(LogRPGAssetLibrary, Warning, TEXT("Failed to load asset with ID: %s"), *Pid.ToString());
			}
		}

		co_return TRPGAsyncResult<TArray<TAsset*>>::Success(LoadedAssets);
	}

public: // BlueprintCallable Functions
	/*
	 * @brief Retrieves an asset by its ID.
	 * 
	 * @param AssetType		The type of the asset (e.g., "Character", "Item").
	 * @param Id			The unique identifier for the asset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Asset")
	static URPGPrimaryAsset* LoadAssetByRPGId(const FRPGId& Id);

	/*
	 * @brief Retrieves an array of assets by their IDs.
	 *
	 * @param AssetType		The type of the asset (e.g., "Character", "Item").
	 * @param Ids			Array of unique identifiers for the assets.
	 * @param OutAssets		Output array containing the found assets.
	 */
	UFUNCTION(BlueprintCallable, Category = "Asset")
	static void LoadAssetArrayByRPGIds(const TArray<FRPGId>& Ids, TArray<URPGPrimaryAsset*>& OutAssets);

	/*
	 * @brief Retrieves all assets of a specific type.
	 *
	 * @param AssetType		The type of the asset (e.g., "Character", "Item").
	 * @param OutAssets		Output array containing all found assets of the specified type.
	 */
	UFUNCTION(BlueprintCallable, Category = "Asset")
	static void GetAllAssetsOfType(FName AssetType, TArray<URPGPrimaryAsset*>& OutAssets);

};

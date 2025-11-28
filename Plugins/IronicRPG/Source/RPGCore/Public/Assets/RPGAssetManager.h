// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "RPGAssetManager.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRPGAssetManager, Warning, All);

DECLARE_MULTICAST_DELEGATE(FOnAssetsLoadingComplete);

/**
 * 
 */
UCLASS(config = IronicRPG, defaultconfig)
class RPGCORE_API URPGAssetManager : public UAssetManager
{
	GENERATED_BODY()
	
protected:
	virtual void StartInitialLoading() override;

	// Scans for RPG asset types and registers them with the asset manager.
	virtual void ScanRPGAssetTypes();

public:
	virtual TSharedPtr<FStreamableHandle> LoadPrimaryAssets(
		const TArray<FPrimaryAssetId>& AssetsToLoad,
		const TArray<FName>& LoadBundles,
		FAssetManagerLoadParams&& LoadParams,
		UE::FSourceLocation Location = UE::FSourceLocation::Current()) override;

	static URPGAssetManager& Get() { return *CastChecked<URPGAssetManager>(GEngine->AssetManager); }

protected:
	// Starts tracking the loading progress of assets. This should be called before any assets are loaded.
	void StartTracking();

	// Adds a handle to the list of active handles for tracking loading progress.
	void AddHandle(TSharedPtr<FStreamableHandle> Handle);

	// Checks if all active handles have completed loading.
	bool IsLoadingComplete() const;

	// Returns the average loading progress of all active handles. Between 0.0 (not started) and 1.0 (complete).
	float GetLoadingProgress() const;

	FOnAssetsLoadingComplete OnAssetsLoadingComplete;

public:
	void LoadLevelAssets(const FPrimaryAssetId& PersistentLevelId, const TArray<FPrimaryAssetId>& SubLevelIds);
	void OnLevelAssetsLoaded(FPrimaryAssetId PersistentLevelId, TArray<FPrimaryAssetId> SubLevelIds);

protected:
	bool bIsTrackingActive = false;

	TArray<TSharedPtr<FStreamableHandle>> ActiveHandles;

public:
	// Returns the class associated with a given asset type name.
	UClass* GetAssetTypeClass(const FName& AssetTypeName) const { return AssetTypeMap.FindRef(AssetTypeName); }

	// Returns the prefix for a given Id type
	UFUNCTION(BlueprintCallable, Category = "RPGId")
	FName GetIdPrefix(const FName& IdType) const;

	// Returns the Id type for a given prefix
	UFUNCTION(BlueprintCallable, Category = "RPGId")
	FName GetIdTypeFromPrefix(const FName& Prefix) const;

	// Returns all registered Id types
	UFUNCTION(BlueprintCallable, Category = "RPGId")
	TArray<FName> GetAllAssetTypes() const;

protected:
	// A map that maps Id prefixes with Id types
	// The key represents the prefix and the value represents the type
	UPROPERTY(BlueprintReadOnly, config, Category = "RPGId")
	TMap<FName, FName> IdPrefixMap;

	// A map that maps asset type names to their corresponding UClass types
	UPROPERTY(BlueprintReadOnly, config, Category = "Asset")
	TMap<FName, UClass*> AssetTypeMap;
};

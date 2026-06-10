// Copyright Ironic Studio. All Rights Reserved.


#include "Assets/RPGAssetLibrary.h"
#include "Assets/RPGAssetMacros.h"

DEFINE_LOG_CATEGORY(LogRPGAssetLibrary);

TSharedPtr<FStreamableHandle> URPGAssetLibrary::GetAssetByRPGIdAsync(const FRPGId& Id, const TArray<FName>& Bundles, TFunction<void(URPGPrimaryAsset*)> OnResult)
{
	UObject* ExistingObject = GET_ASSET_BY_RPGID(Id);
	if (ExistingObject && Bundles.Num() == 0)
	{
		OnResult(Cast<URPGPrimaryAsset>(ExistingObject));
		return nullptr;
	}

	const FPrimaryAssetId PrimaryAssetId(FPrimaryAssetType(FName(Id.GetIdTypeString())), Id.Id);

	// Asset is not loaded, proceed with async loading
	TSharedPtr<FStreamableHandle> Handle = UAssetManager::Get().LoadPrimaryAsset(
		PrimaryAssetId,
		Bundles,
		FStreamableDelegate::CreateLambda([PrimaryAssetId, OnResult]()
			{
				UObject* LoadedObject = UAssetManager::Get().GetPrimaryAssetObject(PrimaryAssetId);
				URPGPrimaryAsset* Asset = Cast<URPGPrimaryAsset>(LoadedObject);
				OnResult(Asset);
			})
	);

	if (!Handle.IsValid())
	{
		UObject* Obj = GET_ASSET_BY_RPGID(Id);
		OnResult(Cast<URPGPrimaryAsset>(Obj));
		return nullptr;
	}

	return Handle;
}

TSharedPtr<FStreamableHandle> URPGAssetLibrary::GetAssetArrayByRPGIdsAsync(const TArray<FRPGId>& Ids, const TArray<FName>& Bundles, TFunction<void(TArray<URPGPrimaryAsset*>)> OnResult)
{
	TArray<URPGPrimaryAsset*> LoadedAssets;
	TArray<FPrimaryAssetId> PrimaryAssetIds;
	for (const FRPGId& Id : Ids)
	{
		// Check if the asset is already loaded
		UObject* ExistingObject = GET_ASSET_BY_RPGID(Id);
		if (ExistingObject && Bundles.Num() == 0)
		{
			LoadedAssets.Add(Cast<URPGPrimaryAsset>(ExistingObject));
			continue;
		}

		FPrimaryAssetId PrimaryAssetId(FPrimaryAssetType(FName(Id.GetIdTypeString())), Id.Id);
		PrimaryAssetIds.Add(PrimaryAssetId);
	}

	if (LoadedAssets.Num() == Ids.Num())
	{
		// All assets are already loaded, return them
		OnResult(LoadedAssets);
		return nullptr;
	}

	// Asset is not loaded, proceed with async loading
	TSharedPtr<FStreamableHandle> Handle = UAssetManager::Get().LoadPrimaryAssets(
		PrimaryAssetIds,
		Bundles,
		FStreamableDelegate::CreateLambda([LoadedAssets, PrimaryAssetIds, OnResult]()
			{
				TArray<URPGPrimaryAsset*> OutAssets = LoadedAssets;
				for (const FPrimaryAssetId& Pid : PrimaryAssetIds)
				{
					UObject* LoadedObject = UAssetManager::Get().GetPrimaryAssetObject(Pid);
					URPGPrimaryAsset* Asset = Cast<URPGPrimaryAsset>(LoadedObject);
					if (Asset)
					{
						OutAssets.Add(Asset);
					}
					else
					{
						UE_LOG(LogRPGAssetLibrary, Warning, TEXT("Failed to load asset with ID: %s"), *Pid.ToString());
					}
				}
				
				OnResult(OutAssets);
			})
	);

	if (!Handle.IsValid())
	{
		TArray<URPGPrimaryAsset*> Results;
		for (const FRPGId& Id : Ids)
		{
			if (UObject* Obj = GET_ASSET_BY_RPGID(Id))
			{
				if (URPGPrimaryAsset* Asset = Cast<URPGPrimaryAsset>(Obj))
				{
					Results.Add(Asset);
				}
			}
		}
		
		OnResult(Results);
		return nullptr;
	}

	return Handle;
}

URPGPrimaryAsset* URPGAssetLibrary::GetAssetByRPGId(const FRPGId& Id)
{
	// Check if the asset is already loaded
	if (URPGPrimaryAsset* ExistingAsset = Cast<URPGPrimaryAsset>(GET_ASSET_BY_RPGID(Id)))
	{
		return ExistingAsset;
	}

	const FPrimaryAssetId PrimaryAssetId(FPrimaryAssetType(FName(Id.GetIdTypeString())), Id.Id);

	// Synchronously load the asset
	if (TSharedPtr<FStreamableHandle> Handle = UAssetManager::Get().LoadPrimaryAsset(PrimaryAssetId))
	{
		Handle->WaitUntilComplete();
	}

	return Cast<URPGPrimaryAsset>(UAssetManager::Get().GetPrimaryAssetObject(PrimaryAssetId));
}

void URPGAssetLibrary::GetAssetArrayByRPGIds(const TArray<FRPGId>& Ids, OUT TArray<URPGPrimaryAsset*>& OutAssets)
{
	for (const FRPGId& Id : Ids)
	{
		auto* Asset = GetAssetByRPGId(Id);
		if (Asset)
		{
			OutAssets.Add(Asset);
		}
		else
		{
			UE_LOG(LogRPGAssetLibrary, Warning, TEXT("Failed to find asset with ID: %s"), *Id.ToString());
		}
	}
}

void URPGAssetLibrary::GetAllAssetsOfType(FName AssetType, OUT TArray<URPGPrimaryAsset*>& OutAssets)
{
	UAssetManager& Manager = UAssetManager::Get();
	TArray<FPrimaryAssetId> AssetIds;
	Manager.GetPrimaryAssetIdList(
		FPrimaryAssetType(FPrimaryAssetType(AssetType)),
		AssetIds
	);

	for (const FPrimaryAssetId& Id : AssetIds)
	{
		UObject* Obj = Manager.GetPrimaryAssetObject(Id);
		if (URPGPrimaryAsset* Casted = Cast<URPGPrimaryAsset>(Obj))
		{
			OutAssets.Add(Casted);
		}
	}
}

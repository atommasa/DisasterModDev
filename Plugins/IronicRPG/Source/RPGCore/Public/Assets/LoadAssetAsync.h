// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "DataTypes/RPGId.h"
#include "LoadAssetAsync.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAssetLoaded, URPGPrimaryAsset*, LoadedAsset);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAssetFailed, FName, AssetType);

/**
 * 
 */
UCLASS()
class RPGCORE_API ULoadAssetAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
	
public:
	/**
	 * Asynchronously loads an asset by its ID.
	 *
	 * @param AssetType The type of the asset (e.g., "Character", "Item").
	 * @param Id The unique identifier for the asset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Asset", meta = (BlueprintInternalUseOnly = "true"))
	static ULoadAssetAsync* LoadAssetById(FName AssetType, const FRPGId& Id);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FOnAssetLoaded OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FOnAssetFailed OnFailure;

};

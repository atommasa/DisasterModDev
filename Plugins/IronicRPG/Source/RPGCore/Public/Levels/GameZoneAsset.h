// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Assets/RPGPrimaryAsset.h"
#include "GameZoneAsset.generated.h"

UENUM(BlueprintType)
enum class EZoneType : uint8
{
	None,
	Menu,
	World,
};

UENUM(BlueprintType)
enum class ESubZoneType : uint8
{
	None,

	// A general outdoor area
	Wilderness,

	// A safe area such as a town or city
	Safeness,

	// A combat area where players can engage in battles
	Combat,

	// A specialized area for questing or story progression
	Special,
};

/**
 * This class represents a game zone asset, which includes information about the level to load and the type of zone.
 */
UCLASS()
class RPGCORE_API UGameZoneAsset : public URPGPrimaryAsset
{
	GENERATED_BODY()
	DEFINE_ASSET_TYPE(Zone, z)
	DEFINE_ASSET_BUNDLES("World", "Music", "Asset")
	
public:
	// The type of zone (e.g., Main Menu, World)
	UPROPERTY(EditDefaultsOnly, Category = "Content|Zone")
	EZoneType ZoneType = EZoneType::None;

	// The Level that will be loaded when entering this zone
	UPROPERTY(EditDefaultsOnly, Category = "Content|Zone", meta=(AssetBundles = "World"))
	TSoftObjectPtr<UWorld> LevelToLoad;

	// The background music to play when this zone is active
	UPROPERTY(EditDefaultsOnly, Category = "Content|Zone|Audio", meta=(AssetBundles = "Music"))
	TArray<TSoftObjectPtr<USoundBase>> BackgroundMusic;

	// A list of required assets that must be loaded before entering this zone
	UPROPERTY(EditDefaultsOnly, Category = "Content|Zone|Assets", meta=(AssetBundles = "Asset"))
	TArray<TSoftObjectPtr<UPrimaryDataAsset>> RequiredAssets;

};

/**
 * This class represents a sub-game zone asset, which is a specific type of game zone that can be used for more granular control over game zones.
 */
UCLASS()
class RPGCORE_API USubGameZoneAsset : public URPGPrimaryAsset
{
	GENERATED_BODY()
	DEFINE_ASSET_TYPE(SubZone, sz)
	DEFINE_ASSET_BUNDLES("World", "Music", "Asset")

public:
	// The type of sub-zone
	UPROPERTY(EditDefaultsOnly, Category = "Content|Zone")
	ESubZoneType ZoneType = ESubZoneType::None;

	// The Level that will be loaded when entering this sub-zone
	UPROPERTY(EditDefaultsOnly, Category = "Content|Zone", meta=(AssetBundles = "World"))
	TSoftObjectPtr<UWorld> LevelToLoad;

	// The background music to play when this sub-zone is active, and it can override the main zone's music
	UPROPERTY(EditDefaultsOnly, Category = "Content|Zone|Audio", meta=(AssetBundles = "Music"))
	TSoftObjectPtr<USoundBase> BackgroundMusic;

	// A list of required assets that must be loaded before entering this sub-zone
	UPROPERTY(EditDefaultsOnly, Category = "Content|Zone|Assets", meta=(AssetBundles = "Asset"))
	TArray<TSoftObjectPtr<UPrimaryDataAsset>> RequiredAssets;

};

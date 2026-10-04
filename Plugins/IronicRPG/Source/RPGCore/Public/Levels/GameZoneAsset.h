// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Assets/RPGPrimaryAsset.h"
#include "Levels/GameZoneBinding.h"
#include "Levels/GameZoneMapData.h"
#include "Levels/GameZonePointData.h"
#include "GameZoneAsset.generated.h"

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

#if WITH_EDITOR
enum class EGameZoneMapBakeResult : uint8
{
	Succeeded,
	NoChange,
	Failed,
};

struct RPGCORE_API FGameZoneMapBakeAuditResult
{
	bool IsVerifiedForRelease() const
	{
		return bCanEvaluate
			&& bSnapshotMatches
			&& !bHasUnsavedLevelChanges
			&& !bHasUnsavedAssetChanges;
	}

	bool IsVerifiedForPIE() const { return IsVerifiedForRelease(); }

	bool bCanEvaluate = false;
	bool bSnapshotMatches = false;
	bool bHasUnsavedLevelChanges = false;
	bool bHasUnsavedAssetChanges = false;
	FString IssueFingerprint;
	TArray<FText> Issues;
};
#endif

/** This class represents a game zone asset and the level content loaded for it. */
UCLASS()
class RPGCORE_API UGameZoneAsset : public URPGPrimaryAsset
{
	GENERATED_BODY()

#if WITH_EDITOR
    friend class FGameZoneBindingCoordinator;
#endif
	DEFINE_ASSET_TYPE(Zone, z)
	DEFINE_ASSET_BUNDLES("World", "Music", "Asset")

protected:
	// The Level that will be loaded when entering this zone
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Zone", meta=(AssetBundles = "World"))
	TSoftObjectPtr<UWorld> LevelToLoad;
	ASSET_PROP_GETTER(TSoftObjectPtr<UWorld>, LevelToLoad);

	UPROPERTY(VisibleAnywhere, Category = "Content|Zone|Binding")
	FGuid GameZoneBindingId;

public:
	const FGuid& GetGameZoneBindingId() const { return GameZoneBindingId; }

#if WITH_EDITORONLY_DATA
protected:
	UPROPERTY(VisibleAnywhere, Category = "Content|Zone|Binding")
	EGameZoneBindingVerificationStatus BindingVerificationStatus = EGameZoneBindingVerificationStatus::LegacyUnverified;

	UPROPERTY(VisibleAnywhere, Category = "Content|Zone|Map|Baked")
	FSoftObjectPath BakedSourceLevel;

	UPROPERTY(VisibleAnywhere, Category = "Content|Zone|Map|Baked")
	FString MapBakeInputFingerprint;

	UPROPERTY(VisibleAnywhere, Category = "Content|Zone|Map|Baked")
	FString MapBakeOutputFingerprint;

public:
	EGameZoneBindingVerificationStatus GetBindingVerificationStatus() const { return BindingVerificationStatus; }
	const FSoftObjectPath& GetBakedSourceLevel() const { return BakedSourceLevel; }
	const FString& GetMapBakeInputFingerprint() const { return MapBakeInputFingerprint; }
	const FString& GetMapBakeOutputFingerprint() const { return MapBakeOutputFingerprint; }
#endif

protected:
	// The background music to play when this zone is active
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Zone|Audio", meta=(AssetBundles = "Music"))
	TArray<TSoftObjectPtr<USoundBase>> BackgroundMusic;
	ASSET_PROP_GETTER(TArray<TSoftObjectPtr<USoundBase>>, BackgroundMusic);

	// A list of required assets that must be loaded before entering this zone
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Zone|Asset", meta=(AssetBundles = "Asset"))
	TArray<TSoftObjectPtr<UPrimaryDataAsset>> RequiredAssets;
	ASSET_PROP_GETTER(TArray<TSoftObjectPtr<UPrimaryDataAsset>>, RequiredAssets);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Content|Zone|Marker")
	TMap<FGuid, FGameZonePointData> BakedPoints;

public:
	const TMap<FGuid, FGameZonePointData>& GetBakedPoints() const { return BakedPoints; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Zone|Map")
	TArray<FGameZoneMapLayer> MapLayers;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Zone|Map")
	TArray<FGameZoneMapSheet> MapSheets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content|Zone|Map", meta=(ZoneMapSheetReference))
	FGameZoneMapSheetId DefaultSheetId;

	UPROPERTY(VisibleAnywhere, Category = "Content|Zone|Map|Baked")
	TArray<FGameZoneMapSheetMapping> BakedSheetMappings;

	UPROPERTY(VisibleAnywhere, Category = "Content|Zone|Map|Baked")
	TArray<FGameZoneMapRegion> BakedMapRegions;

	UPROPERTY(VisibleAnywhere, Category = "Content|Zone|Map|Baked")
	int32 MapDataSchemaVersion = 0;

	UPROPERTY(VisibleAnywhere, Category = "Content|Zone|Map|Baked")
	FGuid MapBakeRevision;

public:
	const TArray<FGameZoneMapLayer>& GetMapLayers() const { return MapLayers; }
	const TArray<FGameZoneMapSheet>& GetMapSheets() const { return MapSheets; }
	const FGameZoneMapSheetId& GetDefaultSheetId() const { return DefaultSheetId; }
	const TArray<FGameZoneMapSheetMapping>& GetBakedSheetMappings() const { return BakedSheetMappings; }
	const TArray<FGameZoneMapRegion>& GetBakedMapRegions() const { return BakedMapRegions; }
	int32 GetMapDataSchemaVersion() const { return MapDataSchemaVersion; }
	const FGuid& GetMapBakeRevision() const { return MapBakeRevision; }

#if WITH_EDITOR
public:
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

public:
	TSet<FRPGId> GetStaticPointMarkerTypes() const;

#if WITH_EDITOR
public:
	UFUNCTION(CallInEditor, Category = "Map Bake", meta=(DisplayName = "Validate & Bake Now"))
	virtual void BakeMapData();

	EGameZoneMapBakeResult BakeMapDataWithResult(bool bRequireSavedSource = false);
	FGameZoneMapBakeAuditResult AuditMapDataForRelease() const;
	FGameZoneMapBakeAuditResult AuditMapDataForPIE() const;

	UFUNCTION(CallInEditor, Category = "Zone Binding")
	void MigrateLegacyBinding();

private:
	virtual void PostLoad() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

private:
	FSoftObjectPath CachedLevelToLoad;
	FRPGId CachedZoneId;
	FGuid CachedGameZoneBindingId;
#if WITH_EDITORONLY_DATA
	EGameZoneBindingVerificationStatus CachedBindingVerificationStatus =
		EGameZoneBindingVerificationStatus::LegacyUnverified;
#endif

#endif

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
	UPROPERTY(EditDefaultsOnly, Category = "Content|Zone|Asset", meta=(AssetBundles = "Asset"))
	TArray<TSoftObjectPtr<UPrimaryDataAsset>> RequiredAssets;

};

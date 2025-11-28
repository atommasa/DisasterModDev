// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DataTypes/RPGId.h"
#include "DataTypes/Alias.h"
#include "Assets/RPGAssetMacros.h"
#include "RPGPrimaryAsset.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnRPGAssetModified, const FRPGId&);

/**
 * The base class of the primary asset classes in Ironic RPG Plugin
 */
UCLASS(Abstract)
class RPGCORE_API URPGPrimaryAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(IdClaim))
	FRPGId Id;
	ASSET_PROP_GETTER(FRPGId, Id)
		
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FAlias DisplayName;
	ASSET_PROP_GETTER(FAlias, DisplayName)

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta=(MultiLine = true))
	FAlias Description;
	ASSET_PROP_GETTER(FAlias, Description)

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override final
	{
		return FPrimaryAssetId(FPrimaryAssetType(GetAssetType()), Id.Id);
	}
	
	virtual void GetAssetRegistryTags(class FAssetRegistryTagsContext Context) const override;

	virtual FName GetAssetType() const { return NAME_None; }
	virtual FName GetAssetIdPrefix() const { return NAME_None; }

public:
	FOnRPGAssetModified OnRPGAssetModified;

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
public:
	virtual void UpdateAssetBundleData() override;

	virtual void AddStructSoftObjectToBundle(FName BundleName, const void* StructPtr, const UStruct* StructType);
#endif // WITH_EDITORONLY_DATA

};

// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DataTypes/RPGId.h"
#include "Assets/RPGAssetMacros.h"
#include "RPGPrimaryAsset.generated.h"

#if WITH_EDITOR
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRPGAssetModified, const FRPGId&);
#endif // WITH_EDITOR

/**
 * The base class of the primary asset classes in Ironic RPG Plugin
 */
UCLASS(Abstract, Const)
class RPGCORE_API URPGPrimaryAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

#if WITH_EDITOR
	friend class FGameZoneBindingCoordinator;
	friend class FRPGIdClaimEditor;
#endif

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, meta=(IdClaim))
	FRPGId Id;
	ASSET_PROP_GETTER(FRPGId, Id)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText DisplayName;
	ASSET_PROP_GETTER(FText, DisplayName)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(MultiLine = true))
	FText Description;
	ASSET_PROP_GETTER(FText, Description)

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override final
	{
		return FPrimaryAssetId(FPrimaryAssetType(GetAssetType()), Id.Id);
	}

	virtual void GetAssetRegistryTags(class FAssetRegistryTagsContext Context) const override;

	virtual FName GetAssetType() const { return NAME_None; }
	virtual FName GetAssetIdPrefix() const { return NAME_None; }

#if WITH_EDITOR
public:
	FOnRPGAssetModified OnRPGAssetModified;
#endif // WITH_EDITOR

protected:
#if WITH_EDITOR
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
public:
	virtual void UpdateAssetBundleData() override;

	virtual void AddStructSoftObjectToBundle(FName BundleName, const void* StructPtr, const UStruct* StructType);
#endif // WITH_EDITORONLY_DATA

};

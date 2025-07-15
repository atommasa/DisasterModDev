// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_Blueprint.h"
#include "AssetTypeActions_Base.h"

/**
 * 
 */
class NarrativeAssetAction : public FAssetTypeActions_Blueprint
{
public:
	NarrativeAssetAction(EAssetTypeCategories::Type Category);

	// FAssetTypeActions_Base interface
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DialogueAsset", "Dialogue Asset"); }
	virtual FColor GetTypeColor() const override { return FColor::Yellow; }
	virtual UClass* GetSupportedClass() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual uint32 GetCategories() override { return AssetCategory; }

private:
	EAssetTypeCategories::Type AssetCategory;
};

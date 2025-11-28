// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_FourParams(FOnRPGAssetCreated, const class URPGPrimaryAsset* /*NewAsset*/, FString /*Type*/, FString /*Id*/, FString /*Name*/);

/**
 * 
 */
class SRPGAssetWizard : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SRPGAssetWizard) {}
        SLATE_EVENT(FOnRPGAssetCreated, OnAssetCreated)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
	// The list of asset types to choose from
    TArray<TSharedPtr<FString>> AssetTypeOptions;

	// The currently selected asset type
    TSharedPtr<FString> SelectedType;
    FString EnteredId;
    FString EnteredName;

    TSharedPtr<SEditableTextBox> IdEditor;

	// Callback when the user clicks the "Create" button
    FOnRPGAssetCreated OnAssetCreated;

private:
    void UpdateIdPrefix();

    FReply OnCreateClicked();
};

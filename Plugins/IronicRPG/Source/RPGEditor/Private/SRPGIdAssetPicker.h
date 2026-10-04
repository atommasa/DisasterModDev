// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ContentBrowserDelegates.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/SCompoundWidget.h"

class SComboButton;
class SRPGIdCategoryPickerFilter;

/** RPG Id reference entry whose category control lives inside its asset picker popup. */
class SRPGIdAssetPicker : public SCompoundWidget
{
	friend class FCategoryAssetPickerTransientSelectionTest;

public:
	SLATE_BEGIN_ARGS(SRPGIdAssetPicker) {}
		SLATE_ATTRIBUTE(FString, ObjectPath)
		SLATE_ATTRIBUTE(bool, IsEnabled)
		SLATE_ARGUMENT(FName, LimitedType)
		SLATE_EVENT(FOnSetObject, OnObjectChanged)
		SLATE_EVENT(FOnShouldFilterAsset, OnShouldFilterAsset)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:
	void RefreshDisplayedAsset();

private:
	TSharedRef<SWidget> BuildMenu();
	void ExtendAssetPickerTopBar(TSharedRef<class SHorizontalBox> TopBar);
	void HandleCategoryChanged();
	EActiveTimerReturnType RefreshAssetViewAfterCategoryChanged(double, float);
	bool ShouldFilterAsset(const FAssetData& AssetData) const;

	FAssetData GetCurrentAssetData() const;
	bool TryGetClipboardAsset(FAssetData& OutAssetData) const;
	FText GetAssetName() const;
	FText GetAssetToolTip() const;
	bool CanEditOrCopy() const;
	bool CanPaste() const;
	bool CanBrowse() const;
	void UseSelected();
	void BrowseToCurrent();
	void EditCurrent();
	void CopyCurrent();
	void Paste();
	void Clear();
	void SelectAsset(const FAssetData& AssetData);
	void SelectAssetFromKeyboard(const TArray<FAssetData>& AssetData);
	void CloseMenu();

private:
	TAttribute<FString> ObjectPath;
	TAttribute<bool> EntryEnabled;
	FOnSetObject OnObjectChanged;
	FOnShouldFilterAsset BaseAssetFilter;
	FName LimitedType = NAME_None;

	TSharedPtr<SComboButton> MenuButton;
	TSharedPtr<SRPGIdCategoryPickerFilter> CategoryFilter;
	FRefreshAssetViewDelegate RefreshAssetView;
};

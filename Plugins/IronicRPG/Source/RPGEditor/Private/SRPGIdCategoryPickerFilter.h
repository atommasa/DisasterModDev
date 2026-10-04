// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RPGIdCategoryPickerFilter.h"
#include "Widgets/SCompoundWidget.h"

struct FAssetData;
class SComboButton;

/** Compact, transient category selector placed beside an RPG Id reference picker. */
class SRPGIdCategoryPickerFilter : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRPGIdCategoryPickerFilter) {}
		SLATE_ARGUMENT(FName, LimitedType)
		SLATE_EVENT(FSimpleDelegate, OnSelectionChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SRPGIdCategoryPickerFilter() override;

public:
	bool ShouldFilterAsset(const FAssetData& Asset) const;

private:
	TSharedRef<SWidget> BuildMenu();
	void BuildTypeMenu(class FMenuBuilder& Menu, FName AssetType);
	void Select(TOptional<FRPGIdCategoryPickerChoice> Choice);
	void HandleFilterChanged();
	FText GetButtonText() const;
	FText GetToolTipText() const;

private:
	TUniquePtr<FRPGIdCategoryPickerFilter> Filter;
	TSharedPtr<SComboButton> ComboButton;
	FSimpleDelegate OnSelectionChanged;
	FDelegateHandle FilterChangedHandle;
};

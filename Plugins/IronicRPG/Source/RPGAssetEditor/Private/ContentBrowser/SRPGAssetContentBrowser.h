// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

struct FRPGAssetItem;
struct FRPGCollectionItem;

/**
 * 
 */
class SRPGAssetContentBrowser : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRPGAssetContentBrowser)
	{}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

private:
    class FAssetRegistryModule* AssetRegistryModule;

	class UAssetEditorSubsystem* AssetEditorSubsystem;

	FName CurrentType = NAME_None;

private:
	FReply OnAddAssetButtonClicked();

private:
	// List of RPG Asset Items
    TMap<FName, TArray<TSharedPtr<FRPGAssetItem>>> AssetItems;
	
	TArray<TSharedPtr<FRPGAssetItem>> VisibleAssetItems;

	// ListView to display RPG Asset Items
    TSharedPtr<SListView<TSharedPtr<FRPGAssetItem>>> AssetListView;

	// Function to generate a row for the Asset List
    TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FRPGAssetItem> Item, const TSharedRef<STableViewBase>& OwnerTable);

	// Function to populate the Asset List with RPG Assets
    void LoadAssetItems();

	void AddAssetItem(const FRPGAssetItem& AssetDataItem);

	void SetVisibleAssetItemsByType(FName Type);

	void OnAssetAdded(const FAssetData& AssetData);

	void OnAssetRemoved(const FAssetData& AssetData);

	TSharedPtr<SWidget> OnContextMenuOpening();

	void OnOpenAssets();

	void OnDeleteAssets();

private:
	
	TArray<TSharedPtr<FRPGCollectionItem>> CollectionItems;

	TSharedPtr<STreeView<TSharedPtr<FRPGCollectionItem>>> CollectionTreeView;

	TSharedRef<ITableRow> OnGenerateTreeRow(TSharedPtr<FRPGCollectionItem> Collection, const TSharedRef<STableViewBase>& OwnerTable);

	void OnGetChildren(TSharedPtr<FRPGCollectionItem> InParent, TArray<TSharedPtr<FRPGCollectionItem>>& OutChildren);

	void OnSelectionChanged(TSharedPtr<FRPGCollectionItem> SelectedItem, ESelectInfo::Type SelectInfo);

	void LoadCollectionItems();

	void OnAreaExpansionChanged(bool bInIsExpanded);

	FReply OnAddSectionClicked();

	void OnAddToSection(const struct FRPGAssetSection& Section);
};

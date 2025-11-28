// Copyright Ironic Studio. All Rights Reserved.


#include "ContentBrowser/SRPGAssetContentBrowser.h"
#include "SlateOptMacros.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ObjectTools.h"
#include  "Assets/RPGAssetManager.h"
#include "RPGAssetEditorSave.h"

#include "Assets/RPGPrimaryAsset.h"

#include "ContentBrowser/RPGAssetItem.h"
#include "ContentBrowser/RPGCollectionItem.h"
#include "ContentBrowser/SRPGAssetRow.h"
#include "ContentBrowser/SRPGSourceTreeArea.h"

#include "SPositiveActionButton.h"
#include "ContentBrowser/RPGAssetWizard.h"

#include "RPGAssetEditor.h"
#include "EditorStyleSet.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Widgets/Text/SRichTextBlock.h"

void SRPGAssetContentBrowser::Construct(const FArguments& InArgs)
{
	AssetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule->Get().OnAssetAdded().AddSP(this, &SRPGAssetContentBrowser::OnAssetAdded);
	AssetRegistryModule->Get().OnAssetRemoved().AddSP(this, &SRPGAssetContentBrowser::OnAssetRemoved);
	check(AssetRegistryModule);

	AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	check(AssetEditorSubsystem);

	LoadCollectionItems();
	LoadAssetItems();

	SAssignNew(CollectionTreeView, STreeView<TSharedPtr<FRPGCollectionItem>>)
		.TreeItemsSource(&CollectionItems)
		.OnGenerateRow(this, &SRPGAssetContentBrowser::OnGenerateTreeRow)
		.OnGetChildren(this, &SRPGAssetContentBrowser::OnGetChildren)
		.OnSelectionChanged(this, &SRPGAssetContentBrowser::OnSelectionChanged)
		.SelectionMode(ESelectionMode::Single)
		.TreeViewStyle(FCoreStyle::Get(), "TreeView");

	ChildSlot
		[
			SNew(SBorder)
				.Padding(2)
				.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
				[
					SNew(SSplitter)
						+ SSplitter::Slot()
						.Value(0.15f)
						[
							SNew(SRPGSourceTreeArea, CollectionTreeView.ToSharedRef())
								.Label(FText::FromString("Asset Types"))
								.EmptyBodyLabel(FText::FromString("No Asset Types Found"))
								.OnExpansionChanged(this, &SRPGAssetContentBrowser::OnAreaExpansionChanged)
								.Visibility(EVisibility::Visible)
								.IsEmpty_Lambda([this]() {
								return CollectionItems.Num() == 0;
									})
								.HeaderContent()
								[
									SNew(SButton)
										.ButtonStyle(FAppStyle::Get(), "SimpleButton")
										.ToolTipText(NSLOCTEXT("RPGAssetContentBrowser", "AddSectionButtonTooltip", "Add a section."))
										.OnClicked_Raw(this, &SRPGAssetContentBrowser::OnAddSectionClicked)
										.ContentPadding(FMargin(1, 0))
										[
											SNew(SImage)
												.Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
												.ColorAndOpacity(FSlateColor::UseForeground())
										]
								]
						]

					+ SSplitter::Slot()
						.Value(0.85f)
						[
							SNew(SBorder)
								.Padding(2.0f)
								.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
								[
									SNew(SVerticalBox)
										+ SVerticalBox::Slot()
										.FillContentHeight(0)
										[
											SNew(SHorizontalBox)
												+ SHorizontalBox::Slot()
												.AutoWidth()
												.Padding(5)
												[
													SNew(SPositiveActionButton)
														.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ContentBrowserNewAsset")))
														.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
														.Text(NSLOCTEXT("RPGAssetContentBrowser", "AddAssetButton", "Add"))
														.OnClicked(this, &SRPGAssetContentBrowser::OnAddAssetButtonClicked)
														
												]

											+ SHorizontalBox::Slot()
												.AutoWidth()
												.Padding(FMargin(5))
												[
													SNew(SButton)
														.ButtonStyle(FAppStyle::Get(), "SimpleButton")
														.ToolTipText(NSLOCTEXT("RPGAssetContentBrowser", "RefreshButtonTooltip", "Refresh Asset List"))
														.OnClicked_Lambda([this]() {
														LoadAssetItems();
														return FReply::Handled();
															})
														.ContentPadding(FMargin(1, 0))
														[
															SNew(SImage)
																.Image(FAppStyle::Get().GetBrush("Icons.Refresh"))
																.ColorAndOpacity(FSlateColor::UseForeground())
														]
												]
										]

									+ SVerticalBox::Slot()
										.MaxHeight(300.0f)
										.Padding(FMargin(5, 0, 5, 0))
										[
											SAssignNew(AssetListView, SListView<TSharedPtr<FRPGAssetItem>>)
												.ListItemsSource(&VisibleAssetItems)
												.OnGenerateRow(this, &SRPGAssetContentBrowser::OnGenerateRow)
												.SelectionMode(ESelectionMode::Multi)
												.ListViewStyle(FCoreStyle::Get(), "ListView")
												.HeaderRow
												(
													SNew(SHeaderRow)
													+ SHeaderRow::Column("")
														.FixedWidth(30.f)
														.HAlignHeader(HAlign_Center)
														.VAlignHeader(VAlign_Center)
														.HAlignCell(HAlign_Center)
														.VAlignCell(VAlign_Center)
														[
															SNew(SLayeredImage)
																.ColorAndOpacity(FSlateColor::UseForeground())
																.Image(FCoreStyle::Get().GetBrush("DefaultBrush")) // TODO: Replace with actual icon
														]

													+ SHeaderRow::Column("Name")
														.DefaultLabel(FText::FromString("Name"))
														.FillWidth(0.4f)
														.ShouldGenerateWidget(true)

													+ SHeaderRow::Column("Id")
														.DefaultLabel(FText::FromString("RPG Id"))
														.FillWidth(0.3f)
														.ShouldGenerateWidget(true)

													+ SHeaderRow::Column("Type")
														.DefaultLabel(FText::FromString("Type"))
														.FillWidth(0.3f)
														.ShouldGenerateWidget(true)
												)
												.OnContextMenuOpening(FOnContextMenuOpening::CreateSP(this, &SRPGAssetContentBrowser::OnContextMenuOpening))
										]
								]
						]
				]
		];
}

FReply SRPGAssetContentBrowser::OnAddAssetButtonClicked()
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString("RPG Asset Wizard"))
		.ClientSize(FVector2D(400, 300));

	Window->SetContent(
		SNew(SRPGAssetWizard)
		.OnAssetCreated(FOnRPGAssetCreated::CreateLambda([this, WeakWindow = TWeakPtr<SWindow>(Window)]
		(const URPGPrimaryAsset* NewAsset, FString Type, FString Id, FString Name)
			{
				if (WeakWindow.IsValid())
				{
					WeakWindow.Pin()->RequestDestroyWindow();
				}
			}))
	);

	FSlateApplication::Get().AddWindow(Window);

	return FReply::Handled();
}

TSharedRef<ITableRow> SRPGAssetContentBrowser::OnGenerateRow(TSharedPtr<FRPGAssetItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SRPGAssetRow, OwnerTable)
		.Item(Item)
		.RowColor_Lambda([this, Item]() -> FLinearColor
			{
				if (!Item.IsValid())
				{
					return FColor::White;
				}
				
				if (auto* Section = URPGAssetEditorSave::Get()->SectionMap.Find(Item->GetId()))
				{
					return Section->SectionColor;
				}

				return FColor::White;
			});
}

void SRPGAssetContentBrowser::LoadAssetItems()
{
	AssetItems.Reset();

	for (const auto& Collection : CollectionItems)
	{
		if (Collection->CollectionType != ERPGCollectionItemType::Type)
		{
			continue;
		}
		
		FName Type = Collection->CollectionName;

		FARFilter Filter;
		Filter.PackagePaths.Add(Collection->Path);
		Filter.bRecursivePaths = true;

		TArray<FAssetData> Assets;
		AssetRegistryModule->Get().GetAssets(Filter, Assets);
	}
}

void SRPGAssetContentBrowser::AddAssetItem(const FRPGAssetItem& AssetDataItem)
{
	FName Type = AssetDataItem.GetType();
	AssetItems.FindOrAdd(Type).Add(MakeShared<FRPGAssetItem>(FRPGAssetItem(AssetDataItem)));
	AssetItems[Type].Sort(
		[](const TSharedPtr<FRPGAssetItem>& A, const TSharedPtr<FRPGAssetItem>& B)
		{
			return A->GetId().ToString() < B->GetId().ToString();
		});
}

void SRPGAssetContentBrowser::SetVisibleAssetItemsByType(FName Type)
{
	VisibleAssetItems = AssetItems.Contains(CurrentType) ?
		AssetItems[CurrentType] :
		TArray<TSharedPtr<FRPGAssetItem>>();

	if (CurrentType == Type && AssetListView.IsValid())
	{
		AssetListView->RequestListRefresh();
	}
}

void SRPGAssetContentBrowser::OnAssetAdded(const FAssetData& AssetData)
{
	if (!AssetData.GetClass()->IsChildOf(URPGPrimaryAsset::StaticClass()))
	{
		return;
	}

	FRPGAssetItem NewItem(AssetData);
	AddAssetItem(NewItem);

	SetVisibleAssetItemsByType(NewItem.GetType());
}

void SRPGAssetContentBrowser::OnAssetRemoved(const FAssetData& AssetData)
{
	if (!AssetData.GetClass()->IsChildOf(URPGPrimaryAsset::StaticClass()))
	{
		return;
	}
	
	FRPGAssetItem NewItem(AssetData);

	FName Type = NewItem.GetType();
	if (AssetItems.Contains(Type))
	{
		TArray<TSharedPtr<FRPGAssetItem>>& Items = AssetItems[Type];
		Items.RemoveAll([&](const TSharedPtr<FRPGAssetItem>& Item) { return Item->GetId() == NewItem.GetId(); });

		VisibleAssetItems = Items;

		if (CurrentType == Type && AssetListView.IsValid())
		{
			AssetListView->RequestListRefresh();
		}
	}
}

TSharedPtr<SWidget> SRPGAssetContentBrowser::OnContextMenuOpening()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	if (AssetListView->GetNumItemsSelected() > 0) // Row Actions
	{
		MenuBuilder.BeginSection("Common", FText::FromString("Common Actions"));
		{
			MenuBuilder.AddMenuEntry(
				FText::FromString("Open"),
				FText::FromString("Open this asset"),
				FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Edit"),
				FUIAction(FExecuteAction::CreateSP(this, &SRPGAssetContentBrowser::OnOpenAssets))
			);

			MenuBuilder.AddMenuEntry(
				FText::FromString("Delete"),
				FText::FromString("Delete this asset"),
				FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Delete"),
				FUIAction(FExecuteAction::CreateSP(this, &SRPGAssetContentBrowser::OnDeleteAssets))
			);
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("Section", FText::FromString("Section Actions"));
		{
			MenuBuilder.AddSubMenu(
				FText::FromString("Add to Section"),
				FText::FromString("Add assets to a section"),
				FNewMenuDelegate::CreateLambda([this](FMenuBuilder& SubMenuBuilder)
					{
						// Populate submenu with sections
						for (const auto& SectionPair : URPGAssetEditorSave::Get()->Sections)
						{
							SubMenuBuilder.AddMenuEntry(
								FText::FromName(SectionPair.Key),
								FText::FromString("Add to this section"),
								FSlateIcon(),
								FUIAction(FExecuteAction::CreateLambda([this, &SectionPair]()
									{
										OnAddToSection(SectionPair.Value);
									})
								)
							);
						}
					}),
				false,
				FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Plus")
			);

			
		}
		MenuBuilder.EndSection();
	}
	else // View List Actions
	{

	}

	return MenuBuilder.MakeWidget();
}

void SRPGAssetContentBrowser::OnOpenAssets()
{
	if (!AssetListView.IsValid())
	{
		return;
	}

	// Open all selected assets in editor
	const auto& SelectedItems = AssetListView->GetSelectedItems();
	for (const auto& Item : SelectedItems)
	{
		if (Item.IsValid())
		{
			AssetEditorSubsystem->OpenEditorForAsset(Item->AssetData.GetAsset());
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(TEXT("Faild to open editor for %s."), *Item->GetName().ToString())));
		}
	}
}

void SRPGAssetContentBrowser::OnDeleteAssets()
{
	if (!AssetListView.IsValid())
	{
		return;
	}

	// Close all editors for selected assets
	const auto& SelectedItems = AssetListView->GetSelectedItems();
	TArray<FAssetData> AssetsToDelete;
	for (const auto& Item : SelectedItems)
	{
		if (Item.IsValid())
		{
			AssetEditorSubsystem->CloseAllEditorsForAsset(Item->AssetData.GetAsset());
			AssetsToDelete.Add(Item->AssetData);
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(TEXT("Faild to close editors for %s."), *Item->GetName().ToString())));
		}
	}

	// Delete selected assets
	if (!AssetsToDelete.IsEmpty())
	{
		ObjectTools::DeleteAssets(AssetsToDelete);
	}
}

TSharedRef<ITableRow> SRPGAssetContentBrowser::OnGenerateTreeRow(TSharedPtr<FRPGCollectionItem> Collection, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FRPGCollectionItem>>, OwnerTable)
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(2, 0, 5, 0))
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
							.Image_Lambda([Collection]()
								{
									return FSlateIcon(FRPGAssetEditorStyleSet::Get().GetStyleSetName(), *Collection->GetIconStyleName()).GetIcon();
								})
							.ColorAndOpacity(FSlateColor::UseForeground())
					]

				+ SHorizontalBox::Slot()
					[
						SNew(STextBlock)
							.Text(FText::FromName(Collection->CollectionName))
							.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("RichTextBlock.Bold"))
					]
		];
}

void SRPGAssetContentBrowser::OnGetChildren(TSharedPtr<FRPGCollectionItem> InParent, TArray<TSharedPtr<FRPGCollectionItem>>& OutChildren)
{
	OutChildren = InParent->Children;
}

void SRPGAssetContentBrowser::OnSelectionChanged(TSharedPtr<FRPGCollectionItem> SelectedItem, ESelectInfo::Type SelectInfo)
{
	if (!SelectedItem.IsValid())
	{
		return;
	}

	if (CurrentType == SelectedItem->CollectionName)
	{
		return;
	}

	CurrentType = SelectedItem->CollectionName;
	SetVisibleAssetItemsByType(CurrentType);
}

void SRPGAssetContentBrowser::LoadCollectionItems()
{
	CollectionItems.Reset();
	
	for (const auto& Type : URPGAssetManager::Get().GetAllAssetTypes())
	{
		TSharedPtr<FRPGCollectionItem> CollectionItem = MakeShared<FRPGCollectionItem>();
		CollectionItem->CollectionName = Type;
		CollectionItem->CollectionType = ERPGCollectionItemType::Type;
		CollectionItem->Path = FName(*FString::Printf(TEXT("/Game/DataAssets/%s"), *Type.ToString()));

		CollectionItems.Add(CollectionItem);
	}

	for (const auto& SectionPair : URPGAssetEditorSave::Get()->Sections)
	{
		TSharedPtr<FRPGCollectionItem> CollectionItem = MakeShared<FRPGCollectionItem>();
		CollectionItem->CollectionName = SectionPair.Key;
		CollectionItem->CollectionType = ERPGCollectionItemType::Section;
		CollectionItem->Path = NAME_None;
		
		if (auto* TypeCollection = CollectionItems.FindByPredicate([&SectionPair](const TSharedPtr<FRPGCollectionItem>& Item)
			{
				return Item->CollectionName == SectionPair.Value.SectionType && Item->CollectionType == ERPGCollectionItemType::Type;
			}))
		{
			(*TypeCollection)->Children.Add(CollectionItem);
		}
	}
}

void SRPGAssetContentBrowser::OnAreaExpansionChanged(bool bInIsExpanded)
{
	if (CollectionTreeView.IsValid())
	{

	}
}

FReply SRPGAssetContentBrowser::OnAddSectionClicked()
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString("Create New Section"))
		.ClientSize(FVector2D(600, 500));

	Window->SetContent(
		SNew(SRPGAssetSectionCreator)
		.OnRPGAssetSectionCreated(FOnRPGAssetSectionCreated::CreateLambda([this, WeakWindow = TWeakPtr<SWindow>(Window)]
		(const FRPGAssetSection NewSection)
			{
				auto* EditorSave = URPGAssetEditorSave::Get();
				EditorSave->Sections.FindOrAdd(NewSection.SectionName, NewSection);
				EditorSave->Save();

				if (WeakWindow.IsValid())
				{
					WeakWindow.Pin()->RequestDestroyWindow();
				}
			}))
	);

	FSlateApplication::Get().AddWindow(Window);

	return FReply::Handled();
}

void SRPGAssetContentBrowser::OnAddToSection(const FRPGAssetSection& Section)
{
	if (!AssetListView.IsValid())
	{
		return;
	}

	// Add all selected assets to section
	const auto& SelectedItems = AssetListView->GetSelectedItems();
	auto* EditorSave = URPGAssetEditorSave::Get();
	for (const auto& Item : SelectedItems)
	{
		if (Item.IsValid())
		{
			EditorSave->SectionMap.FindOrAdd(Item->GetId(), Section);
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(TEXT("Faild to add %s to section %s."), *Item->GetName().ToString(), *Section.SectionName.ToString())));
		}
	}

	EditorSave->Save();
}

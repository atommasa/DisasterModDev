// Copyright Ironic Studio. All Rights Reserved.

#include "SRPGIdAssetPicker.h"

#include "SRPGIdCategoryPickerFilter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Assets/RPGPrimaryAsset.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IContentBrowserSingleton.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorClipboard.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "RPGIdAssetPicker"

void SRPGIdAssetPicker::Construct(const FArguments& InArgs)
{
	ObjectPath = InArgs._ObjectPath;
	EntryEnabled = InArgs._IsEnabled;
	OnObjectChanged = InArgs._OnObjectChanged;
	BaseAssetFilter = InArgs._OnShouldFilterAsset;
	LimitedType = InArgs._LimitedType;

	SAssignNew(CategoryFilter, SRPGIdCategoryPickerFilter)
		.LimitedType(LimitedType)
		.OnSelectionChanged(this, &SRPGIdAssetPicker::HandleCategoryChanged);

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SAssignNew(MenuButton, SComboButton)
			.IsEnabled(EntryEnabled)
			.OnGetMenuContent(this, &SRPGIdAssetPicker::BuildMenu)
			.ToolTipText(this, &SRPGIdAssetPicker::GetAssetToolTip)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
				.Text(this, &SRPGIdAssetPicker::GetAssetName)
				.OverflowPolicy(ETextOverflowPolicy::MiddleEllipsis)
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			PropertyCustomizationHelpers::MakeUseSelectedButton(
				FSimpleDelegate::CreateSP(this, &SRPGIdAssetPicker::UseSelected), FText(), EntryEnabled)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.0f, 0.0f)
		[
			PropertyCustomizationHelpers::MakeBrowseButton(
				FSimpleDelegate::CreateSP(this, &SRPGIdAssetPicker::BrowseToCurrent),
				LOCTEXT("BrowseTip", "Browse to this asset in the Content Browser"),
				TAttribute<bool>::CreateSP(this, &SRPGIdAssetPicker::CanBrowse))
		]
	];
}

void SRPGIdAssetPicker::RefreshDisplayedAsset()
{
	if (MenuButton.IsValid())
	{
		MenuButton->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
	}
}

TSharedRef<SWidget> SRPGIdAssetPicker::BuildMenu()
{
	const FAssetData CurrentAsset = GetCurrentAssetData();
	FMenuBuilder Menu(true, nullptr, nullptr, true);

	Menu.BeginSection(NAME_None, LOCTEXT("CurrentAsset", "Current Asset"));
	{
		Menu.AddMenuEntry(LOCTEXT("Edit", "Edit"), LOCTEXT("EditTip", "Edit this asset"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
			FUIAction(FExecuteAction::CreateSP(this, &SRPGIdAssetPicker::EditCurrent),
				FCanExecuteAction::CreateSP(this, &SRPGIdAssetPicker::CanEditOrCopy)));
		Menu.AddMenuEntry(LOCTEXT("Copy", "Copy"), LOCTEXT("CopyTip", "Copy this asset reference"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"),
			FUIAction(FExecuteAction::CreateSP(this, &SRPGIdAssetPicker::CopyCurrent),
				FCanExecuteAction::CreateSP(this, &SRPGIdAssetPicker::CanEditOrCopy)));
		Menu.AddMenuEntry(LOCTEXT("Paste", "Paste"), LOCTEXT("PasteTip", "Paste an RPG asset reference"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Paste"),
			FUIAction(FExecuteAction::CreateSP(this, &SRPGIdAssetPicker::Paste),
				FCanExecuteAction::CreateSP(this, &SRPGIdAssetPicker::CanPaste)));
		Menu.AddMenuEntry(LOCTEXT("Clear", "Clear"), LOCTEXT("ClearTip", "Clear the asset reference"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Delete"),
			FUIAction(FExecuteAction::CreateSP(this, &SRPGIdAssetPicker::Clear)));
	}
	Menu.EndSection();

	FAssetPickerConfig Config;
	Config.SelectionMode = ESelectionMode::Single;
	Config.Filter.ClassPaths.Add(URPGPrimaryAsset::StaticClass()->GetClassPathName());
	Config.Filter.bRecursiveClasses = true;
	Config.InitialAssetSelection = CurrentAsset;
	Config.InitialAssetViewType = EAssetViewType::List;
	Config.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SRPGIdAssetPicker::SelectAsset);
	Config.OnAssetEnterPressed = FOnAssetEnterPressed::CreateSP(this, &SRPGIdAssetPicker::SelectAssetFromKeyboard);
	Config.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SRPGIdAssetPicker::ShouldFilterAsset);
	Config.OnExtendAssetPickerTopBar.BindSP(this, &SRPGIdAssetPicker::ExtendAssetPickerTopBar);
	Config.RefreshAssetViewDelegates.Add(&RefreshAssetView);
	Config.bAllowNullSelection = false;
	Config.bAllowDragging = false;
	Config.bFocusSearchBoxWhenOpened = true;
	Config.bShowBottomToolbar = false;
	Config.SaveSettingsName = TEXT("RPGIdReferencePicker");

	FContentBrowserModule& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	const TSharedRef<SWidget> AssetPicker = ContentBrowser.Get().CreateAssetPicker(Config);

	Menu.BeginSection(NAME_None, LOCTEXT("Browse", "Browse"));
	Menu.AddWidget(
		SNew(SBox)
		.WidthOverride(420.0f)
		.HeightOverride(420.0f)
		[
			AssetPicker
		],
		FText::GetEmpty(), true);
	Menu.EndSection();

	return Menu.MakeWidget();
}

void SRPGIdAssetPicker::ExtendAssetPickerTopBar(TSharedRef<SHorizontalBox> TopBar)
{
	TopBar->AddSlot()
	.AutoWidth()
	.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
	[
		CategoryFilter.ToSharedRef()
	];
}

void SRPGIdAssetPicker::HandleCategoryChanged()
{
	// Filtering can remove the AssetView's current selection. Defer the refresh until the nested
	// Category menu has finished closing so its selection repair cannot dismiss the parent picker.
	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SRPGIdAssetPicker::RefreshAssetViewAfterCategoryChanged));
}

EActiveTimerReturnType SRPGIdAssetPicker::RefreshAssetViewAfterCategoryChanged(double, float)
{
	RefreshAssetView.ExecuteIfBound(true);
	return EActiveTimerReturnType::Stop;
}

bool SRPGIdAssetPicker::ShouldFilterAsset(const FAssetData& AssetData) const
{
	if (BaseAssetFilter.IsBound() && BaseAssetFilter.Execute(AssetData))
	{
		return true;
	}
	return CategoryFilter.IsValid() && CategoryFilter->ShouldFilterAsset(AssetData);
}

FAssetData SRPGIdAssetPicker::GetCurrentAssetData() const
{
	const FString CurrentPath = ObjectPath.Get();
	if (CurrentPath.IsEmpty())
	{
		return FAssetData();
	}

	const FSoftObjectPath SoftPath(FPackageName::ExportTextPathToObjectPath(CurrentPath));
	if (!SoftPath.IsValid())
	{
		return FAssetData();
	}

	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	return AssetRegistry.Get().GetAssetByObjectPath(SoftPath);
}

bool SRPGIdAssetPicker::TryGetClipboardAsset(FAssetData& OutAssetData) const
{
	FString ClipboardText;
	FPropertyEditorClipboard::ClipboardPaste(ClipboardText);
	const FString ObjectPathText = FPackageName::ExportTextPathToObjectPath(ClipboardText);
	if (ObjectPathText.IsEmpty() || ObjectPathText == TEXT("None"))
	{
		return false;
	}

	const FSoftObjectPath SoftPath(ObjectPathText);
	if (!SoftPath.IsValid())
	{
		return false;
	}

	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	OutAssetData = AssetRegistry.Get().GetAssetByObjectPath(SoftPath);
	return OutAssetData.IsValid() && OutAssetData.IsInstanceOf(URPGPrimaryAsset::StaticClass(), EResolveClass::Yes) && !ShouldFilterAsset(OutAssetData);
}

bool SRPGIdAssetPicker::CanEditOrCopy() const
{
	return GetCurrentAssetData().IsValid();
}

FText SRPGIdAssetPicker::GetAssetName() const
{
	const FAssetData CurrentAsset = GetCurrentAssetData();
	if (CurrentAsset.IsValid())
	{
		return FText::FromName(CurrentAsset.AssetName);
	}

	const FSoftObjectPath SoftPath(FPackageName::ExportTextPathToObjectPath(ObjectPath.Get()));
	return SoftPath.IsValid() ? FText::FromString(SoftPath.GetAssetName()) : LOCTEXT("None", "None");
}

FText SRPGIdAssetPicker::GetAssetToolTip() const
{
	const FString CurrentPath = ObjectPath.Get();
	return CurrentPath.IsEmpty() ? LOCTEXT("OpenPicker", "Select an RPG asset") : FText::FromString(CurrentPath);
}

bool SRPGIdAssetPicker::CanPaste() const
{
	FAssetData AssetData;
	return TryGetClipboardAsset(AssetData);
}

bool SRPGIdAssetPicker::CanBrowse() const
{
	return GetCurrentAssetData().IsValid();
}

void SRPGIdAssetPicker::UseSelected()
{
	TArray<FAssetData> SelectedAssets;
	FContentBrowserModule& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowser.Get().GetSelectedAssets(SelectedAssets);
	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (AssetData.IsInstanceOf(URPGPrimaryAsset::StaticClass(), EResolveClass::Yes) && !ShouldFilterAsset(AssetData))
		{
			OnObjectChanged.ExecuteIfBound(AssetData);
			return;
		}
	}
}

void SRPGIdAssetPicker::BrowseToCurrent()
{
	const FAssetData CurrentAsset = GetCurrentAssetData();
	if (CurrentAsset.IsValid())
	{
		GEditor->SyncBrowserToObjects(TArray<FAssetData>{CurrentAsset});
	}
}

void SRPGIdAssetPicker::EditCurrent()
{
	if (UObject* Asset = GetCurrentAssetData().GetAsset())
	{
		GEditor->EditObject(Asset);
	}
	CloseMenu();
}

void SRPGIdAssetPicker::CopyCurrent()
{
	const FAssetData CurrentAsset = GetCurrentAssetData();
	if (CurrentAsset.IsValid())
	{
		FPropertyEditorClipboard::ClipboardCopy(*CurrentAsset.GetExportTextName());
	}
	CloseMenu();
}

void SRPGIdAssetPicker::Paste()
{
	FAssetData AssetData;
	if (TryGetClipboardAsset(AssetData))
	{
		OnObjectChanged.ExecuteIfBound(AssetData);
	}
	CloseMenu();
}

void SRPGIdAssetPicker::Clear()
{
	OnObjectChanged.ExecuteIfBound(FAssetData());
	CloseMenu();
}

void SRPGIdAssetPicker::SelectAsset(const FAssetData& AssetData)
{
	// An AssetView refresh may report that its filtered-out selection disappeared. Clear is an
	// explicit menu action, so an invalid selection here is transient and must not commit or close.
	if (!AssetData.IsValid())
	{
		return;
	}

	OnObjectChanged.ExecuteIfBound(AssetData);
	CloseMenu();
}

void SRPGIdAssetPicker::SelectAssetFromKeyboard(const TArray<FAssetData>& AssetData)
{
	if (!AssetData.IsEmpty())
	{
		SelectAsset(AssetData[0]);
	}
}

void SRPGIdAssetPicker::CloseMenu()
{
	if (MenuButton.IsValid())
	{
		MenuButton->SetIsOpen(false);
	}
}

#undef LOCTEXT_NAMESPACE

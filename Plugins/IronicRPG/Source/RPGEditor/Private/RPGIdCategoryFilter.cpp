// Copyright Ironic Studio. All Rights Reserved.
#include "RPGIdCategoryFilter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Assets/RPGAssetManager.h"
#include "Assets/RPGPrimaryAsset.h"
#include "ContentBrowserItem.h"
#include "Containers/Ticker.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/ConfigCacheIni.h"
#include "RPGSettings.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"
#include <atomic>

#define LOCTEXT_NAMESPACE "RPGIdCategoryFilter"

bool FRPGIdCategoryFilterModel::Refresh(const FRPGIdCategoryStore& Store)
{
	Prefixes = FRPGIdCategoryResolver::ReadNativePrefixes();
	ClassTypes.Reset();
	ScopesByType.Reset();
	ErrorsByType.Reset();
	const auto* Manager = GEngine ? Cast<URPGAssetManager>(GEngine->AssetManager) : nullptr;
	for (const auto& Entry : Prefixes)
	{
		if (const UClass* Class = Manager ? Manager->GetAssetTypeClass(Entry.Key) : nullptr)
		{
			ClassTypes.Add(Class->GetClassPathName(), Entry.Key);
		}
		FString Error;
		TArray<FRPGIdCategoryScope> Scopes;
		if (Store.ListScopes(Entry.Key, Scopes, Error)) { ScopesByType.Add(Entry.Key, MoveTemp(Scopes)); }
		else { ErrorsByType.Add(Entry.Key, MoveTemp(Error)); }
	}
	ScopesByType.KeySort(FNameLexicalLess());
	// An invalid ruleset is unavailable, not proof that a selected category has been deleted.
	const int32 Removed = Selected.RemoveAll([this](const auto& Choice)
	{
		const auto* Scopes = ScopesByType.Find(Choice.AssetType);
		if (!Scopes) { return false; }
		return !Scopes->ContainsByPredicate([&Choice](const auto& Scope)
		{
			return Choice.CategoryKey.IsNone() ? Scope.Kind != ERPGIdSuggestionScope::Category
				: Scope.Kind == ERPGIdSuggestionScope::Category && Scope.CategoryKey == Choice.CategoryKey;
		});
	});
	return Removed > 0;
}

void FRPGIdCategoryFilterModel::Toggle(FRPGIdCategoryFilterChoice Choice)
{
	if (Selected.Contains(Choice)) { Selected.Remove(Choice); }
	else { Selected.Add(Choice); }
}

FString FRPGIdCategoryFilterModel::GetErrors() const
{
	TArray<FString> Errors;
	for (const auto& Entry : ErrorsByType) { Errors.Add(Entry.Key.ToString() + TEXT(": ") + Entry.Value); }
	for (const auto& Choice : Selected)
	{
		if (!Prefixes.Contains(Choice.AssetType)) { Errors.AddUnique(Choice.AssetType.ToString() + TEXT(": Asset type is unavailable.")); }
	}
	if (Prefixes.IsEmpty()) { Errors.Add(TEXT("RPG asset types are not ready.")); }
	return FString::Join(Errors, TEXT("\n"));
}

bool FRPGIdCategoryFilterModel::Matches(const FAssetData& Asset, bool bRegistryReady, bool& bUnknown) const
{
	bUnknown = false;
	if (Selected.IsEmpty()) { return true; }
	if (!bRegistryReady) { bUnknown = true; return false; }
	FName Type, Id;
	// false is mandatory: this lookup must never initiate loading.
	if (const auto* Owner = Cast<URPGPrimaryAsset>(Asset.FastGetAsset(false)))
	{
		Type = Owner->GetAssetType();
		Id = Owner->GetId().Id;
	}
	else
	{
		const FName* KnownType = ClassTypes.Find(Asset.AssetClassPath);
		if (!KnownType)
		{
			bUnknown = Asset.TagsAndValues.Contains(TEXT("RPGId"));
			return false;
		}
		Type = *KnownType;
		if (!Asset.GetTagValue(TEXT("RPGId"), Id)) { bUnknown = true; return false; }
	}
	const auto* Scopes = ScopesByType.Find(Type);
	const auto* Prefix = Prefixes.Find(Type);
	if (!Scopes || !Prefix) { bUnknown = true; return false; }
	const auto Resolution = FRPGIdCategoryResolver::ResolveInScopes(*Prefix, Id, *Scopes);
	if (Resolution.State == ERPGIdCategoryState::InvalidId) { bUnknown = true; return false; }
	if (Resolution.State != ERPGIdCategoryState::Classified && Resolution.State != ERPGIdCategoryState::Unclassified) { return false; }
	return Selected.Contains(FRPGIdCategoryFilterChoice{Type, Resolution.CategoryKey});
}

namespace
{
	class FRPGIdCategoryFrontendFilter : public FFrontendFilter, public TSharedFromThis<FRPGIdCategoryFrontendFilter>
	{
	public:
		explicit FRPGIdCategoryFrontendFilter(TSharedPtr<FFrontendFilterCategory> Category)
			: FFrontendFilter(MoveTemp(Category)), Store(FRPGIdCategoryStore::Get())
		{
			StoreHandle = Store.OnChanged().AddLambda([this]() { bRefresh = true; });
			PropertyHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddLambda([this](UObject* Object, FPropertyChangedEvent&)
			{
				if (Object && (Object->IsA<URPGPrimaryAsset>() || Object->IsA<URPGSettings>() || Object->IsA<URPGAssetManager>()))
				{
					bRefresh = true;
				}
			});
			TransactionHandle = FCoreUObjectDelegates::OnObjectTransacted.AddLambda([this](UObject* Object, const FTransactionObjectEvent&)
			{
				if (Object && Object->IsA<URPGPrimaryAsset>()) { bRefresh = true; }
			});
			LoadedHandle = FCoreUObjectDelegates::OnAssetLoaded.AddLambda([this](UObject* Object)
			{
				if (Object && Object->IsA<URPGPrimaryAsset>()) { bRefresh = true; }
			});
			GarbageCollectedHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddLambda([this]() { bRefresh = true; });
			auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
			AddedHandle = Registry.OnAssetAdded().AddLambda([this](const FAssetData&) { bRefresh = true; });
			RemovedHandle = Registry.OnAssetRemoved().AddLambda([this](const FAssetData&) { bRefresh = true; });
			UpdatedHandle = Registry.OnAssetUpdated().AddLambda([this](const FAssetData&) { bRefresh = true; });
			SavedHandle = Registry.OnAssetUpdatedOnDisk().AddLambda([this](const FAssetData&) { bRefresh = true; });
			RenamedHandle = Registry.OnAssetRenamed().AddLambda([this](const FAssetData&, const FString&) { bRefresh = true; });
			ReadyHandle = Registry.OnFilesLoaded().AddLambda([this]() { bRefresh = true; });
			ScanStartedHandle = Registry.OnScanStarted().AddLambda([this]() { bRefresh = true; });
			ScanEndedHandle = Registry.OnScanEnded().AddLambda([this]() { bRefresh = true; });
			TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FRPGIdCategoryFrontendFilter::Tick), 0.1f);
			Model.Refresh(Store);
		}

		virtual ~FRPGIdCategoryFrontendFilter() override
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			Store.OnChanged().Remove(StoreHandle);
			FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyHandle);
			FCoreUObjectDelegates::OnObjectTransacted.Remove(TransactionHandle);
			FCoreUObjectDelegates::OnAssetLoaded.Remove(LoadedHandle);
			FCoreUObjectDelegates::GetPostGarbageCollect().Remove(GarbageCollectedHandle);
			if (auto* Module = FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry")))
			{
				auto& Registry = Module->Get();
				Registry.OnAssetAdded().Remove(AddedHandle);
				Registry.OnAssetRemoved().Remove(RemovedHandle);
				Registry.OnAssetUpdated().Remove(UpdatedHandle);
				Registry.OnAssetUpdatedOnDisk().Remove(SavedHandle);
				Registry.OnAssetRenamed().Remove(RenamedHandle);
				Registry.OnFilesLoaded().Remove(ReadyHandle);
				Registry.OnScanStarted().Remove(ScanStartedHandle);
				Registry.OnScanEnded().Remove(ScanEndedHandle);
			}
		}

		virtual FString GetName() const override { return TEXT("RPGIdCategory"); }
		virtual FText GetDisplayName() const override
		{
			if (!Model.GetErrors().IsEmpty()) { return LOCTEXT("UnavailableName", "RPG Id Category (unavailable)"); }
			if (!bRegistryReady) { return LOCTEXT("ScanningName", "RPG Id Category (scanning)"); }
			if (bUnknownSeen) { return LOCTEXT("UnknownName", "RPG Id Category (unknown assets excluded)"); }
			return LOCTEXT("Name", "RPG Id Category");
		}
		virtual FText GetToolTipText() const override
		{
			FString Tip = TEXT("Right-click to select RPG categories. Multiple selections match any chosen category. No selection shows all assets.");
			const FString Errors = Model.GetErrors();
			if (!Errors.IsEmpty()) { Tip += TEXT("\n") + Errors + TEXT("\nClear category selections or disable this filter to show all assets."); }
			if (bUnknownSeen) { Tip += TEXT("\nSome assets have missing or invalid RPG Id/type metadata and were excluded."); }
			return FText::FromString(Tip);
		}
		virtual bool PassesFilter(FAssetFilterType Item) const override
		{
			if (Model.GetSelected().IsEmpty()) { return true; }
			FAssetData Data;
			if (!Item.Legacy_TryGetAssetData(Data)) { return false; }
			bool bUnknown = false;
			const bool bMatch = Model.Matches(Data, bRegistryReady, bUnknown);
			if (bUnknown) { bUnknownSeen = true; }
			return bMatch;
		}
		virtual void ActiveStateChanged(bool bActive) override { bRefresh = true; }
		virtual void ModifyContextMenu(FMenuBuilder& Menu) override
		{
			Menu.BeginSection(TEXT("RPGCategories"), LOCTEXT("Categories", "RPG Id Categories"));
			Menu.AddMenuEntry(LOCTEXT("Clear", "Clear category selections (show all)"), FText(), FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &FRPGIdCategoryFrontendFilter::ClearSelections)));
			if (!bRegistryReady) { Menu.AddWidget(SNew(STextBlock).Text(LOCTEXT("NotReady", "Asset Registry is still discovering assets.")), FText()); }
			const FString Errors = Model.GetErrors();
			if (!Errors.IsEmpty()) { Menu.AddWidget(SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(Errors)), FText()); }
			if (bUnknownSeen)
			{
				Menu.AddWidget(SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("Unknown",
					"Some assets lack valid RPG Id/type metadata and were excluded. Check their Id; save normally if their registry tag is missing.")), FText());
			}
			for (const auto& Entry : Model.GetScopes())
			{
				Menu.AddSubMenu(FText::FromName(Entry.Key), FText(),
					FNewMenuDelegate::CreateSPLambda(this, [this, Type = Entry.Key](FMenuBuilder& SubMenu) { BuildTypeMenu(SubMenu, Type); }));
			}
			Menu.EndSection();
		}
		virtual void SaveSettings(const FString& Filename, const FString& Section, const FString& Key) const override
		{
			TArray<FString> Types, Keys;
			for (const auto& Choice : Model.GetSelected()) { Types.Add(Choice.AssetType.ToString()); Keys.Add(Choice.CategoryKey.ToString()); }
			GConfig->SetArray(*Section, *(Key + TEXT(".RPGCategoryTypes")), Types, Filename);
			GConfig->SetArray(*Section, *(Key + TEXT(".RPGCategoryKeys")), Keys, Filename);
		}
		virtual void LoadSettings(const FString& Filename, const FString& Section, const FString& Key) override
		{
			TArray<FString> Types, Keys;
			GConfig->GetArray(*Section, *(Key + TEXT(".RPGCategoryTypes")), Types, Filename);
			GConfig->GetArray(*Section, *(Key + TEXT(".RPGCategoryKeys")), Keys, Filename);
			TArray<FRPGIdCategoryFilterChoice> Choices;
			if (Types.Num() == Keys.Num())
			{
				for (int32 Index = 0; Index < Types.Num(); ++Index) { Choices.AddUnique({FName(*Types[Index]), FName(*Keys[Index])}); }
			}
			Model.SetSelected(MoveTemp(Choices));
			bRefresh = true;
		}

	private:
		bool Tick(float DeltaTime)
		{
			if (!bRefresh.exchange(false)) { return true; }
			bUnknownSeen = false;
			const auto* Module = FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));
			bRegistryReady = Module && !Module->Get().IsLoadingAssets();
			if (Model.Refresh(Store))
			{
				FNotificationInfo Info(LOCTEXT("Pruned", "A selected RPG category no longer exists and was removed from this filter."));
				Info.ExpireDuration = 5.f;
				FSlateNotificationManager::Get().AddNotification(Info);
			}
			BroadcastChangedEvent();
			return true;
		}
		void ClearSelections() { Model.Clear(); bRefresh = true; }
		void BuildTypeMenu(FMenuBuilder& Menu, FName Type)
		{
			const auto* Scopes = Model.GetScopes().Find(Type);
			if (!Scopes) { return; }
			for (const auto& Scope : *Scopes)
			{
				const FRPGIdCategoryFilterChoice Choice{Type, Scope.CategoryKey};
				const FText Label = Scope.Kind == ERPGIdSuggestionScope::AllNumbers
					? LOCTEXT("None", "None (Unclassified)") : FText::FromString(Scope.DisplayName);
				Menu.AddMenuEntry(Label, FText(), FSlateIcon(), FUIAction(
					FExecuteAction::CreateSPLambda(this, [this, Choice]() { Model.Toggle(Choice); bRefresh = true; }), FCanExecuteAction(),
					FIsActionChecked::CreateSPLambda(this, [this, Choice]() { return Model.IsSelected(Choice); })),
					NAME_None, EUserInterfaceActionType::ToggleButton);
			}
		}

	private:
		FRPGIdCategoryStore& Store;
		FRPGIdCategoryFilterModel Model;
		std::atomic<bool> bRefresh{true};
		mutable std::atomic<bool> bUnknownSeen{false};
		bool bRegistryReady = false;
		FDelegateHandle StoreHandle, PropertyHandle, TransactionHandle, LoadedHandle, GarbageCollectedHandle;
		FDelegateHandle AddedHandle, RemovedHandle, UpdatedHandle, SavedHandle, RenamedHandle, ReadyHandle;
		FDelegateHandle ScanStartedHandle, ScanEndedHandle;
		FTSTicker::FDelegateHandle TickerHandle;
	};
}

void URPGIdCategoryFilterExtension::AddFrontEndFilterExtensions(TSharedPtr<FFrontendFilterCategory> DefaultCategory,
	TArray<TSharedRef<FFrontendFilter>>& InOutFilterList) const
{
	InOutFilterList.Add(MakeShared<FRPGIdCategoryFrontendFilter>(MakeShared<FFrontendFilterCategory>(
		LOCTEXT("Group", "Ironic RPG"), LOCTEXT("GroupTip", "RPG authoring filters"))));
}

#undef LOCTEXT_NAMESPACE

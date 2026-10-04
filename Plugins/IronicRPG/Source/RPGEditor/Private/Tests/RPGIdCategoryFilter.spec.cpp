// Copyright Ironic Studio. All Rights Reserved.
#include "RPGIdCategoryFilter.h"
#include "RPGIdCategoryPickerFilter.h"
#include "SRPGIdAssetPicker.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Items/ItemAsset.h"
#include "Characters/CharacterAsset.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SComboButton.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	struct FCategoryFilterFixture
	{
	public:
		FCategoryFilterFixture()
			: Filename(FPaths::ProjectSavedDir() / (TEXT("CategoryFilter-") + FGuid::NewGuid().ToString() + TEXT(".ini"))), Store(Filename)
		{
			FString Error;
			Store.Reload(Error);
		}
		~FCategoryFilterFixture() { IFileManager::Get().Delete(*Filename); }
	public:
		FString Filename;
		FRPGIdCategoryStore Store;
	};

	FRPGIdCategoryRules FilterRules()
	{
		FRPGIdCategoryDefinition A;
		A.CategoryKey = TEXT("Equipment");
		A.DisplayName = TEXT("Equipment");
		A.Ranges = {{1000, 1999}};
		FRPGIdCategoryRules Rules;
		Rules.AssetTypes.FindOrAdd(TEXT("Item")).Categories.Add(A);
		A.CategoryKey = TEXT("Main");
		A.DisplayName = TEXT("Main Character");
		A.Ranges = {{0, 9}};
		Rules.AssetTypes.FindOrAdd(TEXT("Character")).Categories.Add(A);
		return Rules;
	}

	FAssetData Metadata(UClass* Class, const TCHAR* Id, bool bTag = true)
	{
		FAssetDataTagMap Tags;
		if (bTag) { Tags.Add(TEXT("RPGId"), Id); }
		const FName Package(*(TEXT("/Game/CategoryFilterTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		return FAssetData(Package, TEXT("/Game"), TEXT("Unloaded"), Class->GetClassPathName(), MoveTemp(Tags));
	}

	int32 CountWidgetsByType(const TSharedRef<SWidget>& Widget, const FName WidgetType)
	{
		int32 Count = Widget->GetType() == WidgetType ? 1 : 0;
		FChildren* Children = Widget->GetChildren();
		for (int32 Index = 0; Index < Children->Num(); ++Index)
		{
			Count += CountWidgetsByType(Children->GetChildAt(Index), WidgetType);
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCategoryAssetPickerEntryTest, "IronicRPG.RPGId.Category.AssetPickerHasSingleEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCategoryAssetPickerEntryTest::RunTest(const FString& Parameters)
{
	const TSharedRef<SRPGIdAssetPicker> Picker = SNew(SRPGIdAssetPicker)
		.ObjectPath(FString())
		.IsEnabled(true)
		.LimitedType(TEXT("Item"));

	TestEqual(TEXT("The RPG Id field exposes exactly one Asset picker entry"), CountWidgetsByType(Picker, TEXT("SComboButton")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCategoryAssetPickerTransientSelectionTest,
	"IronicRPG.RPGId.Category.AssetPickerIgnoresTransientDeselection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCategoryAssetPickerTransientSelectionTest::RunTest(const FString& Parameters)
{
	int32 CommittedSelections = 0;
	const TSharedRef<SRPGIdAssetPicker> Picker = SNew(SRPGIdAssetPicker)
		.ObjectPath(FString())
		.IsEnabled(true)
		.LimitedType(TEXT("Item"))
		.OnObjectChanged_Lambda([&CommittedSelections](const FAssetData&) { ++CommittedSelections; });

	Picker->SelectAsset(FAssetData());
	TestEqual(TEXT("AssetView filtering cannot commit its transient deselection"), CommittedSelections, 0);

	int32 SynchronousRefreshes = 0;
	Picker->RefreshAssetView = FRefreshAssetViewDelegate::CreateLambda([&SynchronousRefreshes](bool) { ++SynchronousRefreshes; });
	Picker->HandleCategoryChanged();
	TestEqual(TEXT("Category actions do not refresh AssetView inside the nested menu call stack"), SynchronousRefreshes, 0);

	TOptional<bool> RefreshSources;
	Picker->RefreshAssetView = FRefreshAssetViewDelegate::CreateLambda([&RefreshSources](bool bInRefreshSources)
	{
		RefreshSources = bInRefreshSources;
	});
	Picker->RefreshAssetViewAfterCategoryChanged(0.0, 0.0f);
	TestTrue(TEXT("A changed query predicate requests an AssetView refresh"), RefreshSources.IsSet());
	TestTrue(TEXT("A changed query predicate rebuilds source items so All can restore removed assets"), RefreshSources.Get(false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCategoryMigrationTest, "IronicRPG.RPGId.Category.LegacyMapMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCategoryMigrationTest::RunTest(const FString& Parameters)
{
	FCategoryFilterFixture Fixture;
	const FString Original = TEXT("; preserved\n[/Script/RPGEditor.RPGIdCategorySettings]\nSchemaVersion=1\n")
		TEXT("Rules=(Categories=((AssetType=Character,CategoryKey=Main,DisplayName=\"MainCharacter\",Ranges=((End=9))),")
		TEXT("(AssetType=Item,CategoryKey=Equipment,DisplayName=\"Equipment\",Ranges=((Start=1000,End=1999)))))\n");
	FFileHelper::SaveStringToFile(Original, *Fixture.Filename);
	FString Error, Actual;
	if (!TestTrue(TEXT("Schema 1 imports"), Fixture.Store.Reload(Error))) { AddError(Error); return false; }
	TestEqual(TEXT("All registered types get nodes"), Fixture.Store.GetRules().AssetTypes.Num(), FRPGIdCategoryResolver::ReadNativePrefixes().Num());
	TestEqual(TEXT("Old category retains key"), Fixture.Store.Resolve(TEXT("Character"), TEXT("c0009")).CategoryKey, FName(TEXT("Main")));
	TestEqual(TEXT("Old ranges remain classified"), Fixture.Store.Resolve(TEXT("Item"), TEXT("i1000")).State, ERPGIdCategoryState::Classified);
	FFileHelper::LoadFileToString(Actual, *Fixture.Filename);
	TestEqual(TEXT("Opening old settings never rewrites them"), Actual, Original);
	const FString Before = FRPGIdCategoryResolver::Export(Fixture.Store.GetRules());
	TestTrue(TEXT("Repeated migration imports"), Fixture.Store.Reload(Error));
	TestEqual(TEXT("Migration is repeatable"), FRPGIdCategoryResolver::Export(Fixture.Store.GetRules()), Before);
	TestTrue(TEXT("Explicit Save commits schema 2"), Fixture.Store.Save(Fixture.Store.GetRules(), Fixture.Store.GetRevision(), Error));
	FFileHelper::LoadFileToString(Actual, *Fixture.Filename);
	TestTrue(TEXT("Version advanced only by Save"), Actual.Contains(TEXT("SchemaVersion=2")));
	TestTrue(TEXT("Unrelated comment survives"), Actual.StartsWith(TEXT("; preserved\n")));
	TestTrue(TEXT("Schema 2 restarts"), Fixture.Store.Reload(Error));
	TestEqual(TEXT("New schema retains all data"), FRPGIdCategoryResolver::Export(Fixture.Store.GetRules()), Before);
	FFileHelper::SaveStringToFile(Original.Replace(TEXT("AssetType=Item"), TEXT("AssetType=RetiredType")), *Fixture.Filename);
	TestFalse(TEXT("Removed registration is diagnosed"), Fixture.Store.Reload(Error));
	TestTrue(TEXT("Unknown type data not dropped"), Fixture.Store.GetRules().AssetTypes.Contains(TEXT("RetiredType")));
	TestFalse(TEXT("Unknown type cannot be saved"), Fixture.Store.Save(Fixture.Store.GetRules(), Fixture.Store.GetRevision(), Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCategoryFilterTest, "IronicRPG.RPGId.Category.FilterMetadataAndSelections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCategoryFilterTest::RunTest(const FString& Parameters)
{
	FCategoryFilterFixture Fixture;
	FString Error;
	TestTrue(TEXT("Fixture save"), Fixture.Store.Save(FilterRules(), Fixture.Store.GetRevision(), Error));
	FRPGIdCategoryFilterModel Model;
	Model.Refresh(Fixture.Store);
	bool bUnknown;
	const auto Item = Metadata(UItemAsset::StaticClass(), TEXT("i1000"));
	const auto OtherItem = Metadata(UItemAsset::StaticClass(), TEXT("i0100"));
	const auto Character = Metadata(UCharacterAsset::StaticClass(), TEXT("c0009"));
	TestTrue(TEXT("No selection imposes no restriction"), Model.Matches(FAssetData(), false, bUnknown));
	Model.Toggle({TEXT("Item"), TEXT("Equipment")});
	TestTrue(TEXT("Saved metadata matches category"), Model.Matches(Item, true, bUnknown));
	TestFalse(TEXT("Unselected numbers excluded"), Model.Matches(OtherItem, true, bUnknown));
	TestFalse(TEXT("Types remain separate"), Model.Matches(Character, true, bUnknown));
	Model.Toggle({TEXT("Character"), TEXT("Main")});
	TestTrue(TEXT("Multiple types use OR"), Model.Matches(Character, true, bUnknown));
	TestFalse(TEXT("Not ready is not a trustworthy result"), Model.Matches(Item, false, bUnknown));
	TestTrue(TEXT("Not ready diagnosed"), bUnknown);
	Model.Toggle({TEXT("Item"), NAME_None});
	TestTrue(TEXT("Unclassified matches valid complement"), Model.Matches(OtherItem, true, bUnknown));
	TestFalse(TEXT("None is not unclassified"), Model.Matches(Metadata(UItemAsset::StaticClass(), TEXT("None")), true, bUnknown));
	TestFalse(TEXT("Invalid number not unclassified"), Model.Matches(Metadata(UItemAsset::StaticClass(), TEXT("i1x00")), true, bUnknown));
	TestTrue(TEXT("Invalid Id diagnosed"), bUnknown);
	TestFalse(TEXT("Missing tag excluded"), Model.Matches(Metadata(UItemAsset::StaticClass(), TEXT(""), false), true, bUnknown));
	TestTrue(TEXT("Missing tag diagnosed"), bUnknown);
	int32 Loads = 0;
	const auto Handle = FCoreUObjectDelegates::OnAssetLoaded.AddLambda([&Loads](UObject*) { ++Loads; });
	const double Start = FPlatformTime::Seconds();
	for (int32 Index = 0; Index < 10000; ++Index) { Model.Matches(Item, true, bUnknown); }
	const double Milliseconds = (FPlatformTime::Seconds() - Start) * 1000.0;
	FCoreUObjectDelegates::OnAssetLoaded.Remove(Handle);
	TestEqual(TEXT("Filter causes zero asset loads"), Loads, 0);
	TestNull(TEXT("No package created for metadata"), FindPackage(nullptr, *Item.PackageName.ToString()));
	AddInfo(FString::Printf(TEXT("10000 prepared metadata queries: %.2f ms; %d asset loads."), Milliseconds, Loads));
	auto Rules = FilterRules();
	Rules.AssetTypes.FindChecked(TEXT("Item")).Categories[0].DisplayName = TEXT("Gear");
	Fixture.Store.Save(Rules, Fixture.Store.GetRevision(), Error);
	TestFalse(TEXT("Rename retains selection"), Model.Refresh(Fixture.Store));
	TestTrue(TEXT("Stable key still selected"), Model.IsSelected({TEXT("Item"), TEXT("Equipment")}));
	Rules.AssetTypes.FindChecked(TEXT("Item")).Categories.Reset();
	Fixture.Store.Save(Rules, Fixture.Store.GetRevision(), Error);
	TestTrue(TEXT("Deleted category is pruned"), Model.Refresh(Fixture.Store));
	TestFalse(TEXT("Deleted key not retained"), Model.IsSelected({TEXT("Item"), TEXT("Equipment")}));
	TestTrue(TEXT("Other type selection retained"), Model.IsSelected({TEXT("Character"), TEXT("Main")}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCategoryFilterOverlayTest, "IronicRPG.RPGId.Category.FilterLoadedOverlayAndInvalidRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCategoryFilterOverlayTest::RunTest(const FString& Parameters)
{
	FCategoryFilterFixture Fixture;
	FString Error;
	Fixture.Store.Save(FilterRules(), Fixture.Store.GetRevision(), Error);
	FRPGIdCategoryFilterModel Model;
	Model.Refresh(Fixture.Store);
	Model.Toggle({TEXT("Item"), TEXT("Equipment")});
	const FString PackageName = TEXT("/Temp/CategoryOverlay_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	UPackage* Package = CreatePackage(*PackageName);
	Package->MarkAsFullyLoaded();
	TStrongObjectPtr<UItemAsset> Asset(NewObject<UItemAsset>(Package, TEXT("Item"), RF_Public | RF_Transient));
	FProperty* IdProperty = FindFProperty<FProperty>(URPGPrimaryAsset::StaticClass(), TEXT("Id"));
	FRPGId& Id = *IdProperty->ContainerPtrToValuePtr<FRPGId>(Asset.Get());
	Id = FRPGId(TEXT("i1000"));
	const FAssetData Saved(Asset.Get());
	bool bUnknown;
	TestTrue(TEXT("Loaded old Id matches"), Model.Matches(Saved, true, bUnknown));
	Id = FRPGId(TEXT("i0100"));
	TestFalse(TEXT("Unsaved new Id overrides old registry tag"), Model.Matches(Saved, true, bUnknown));
	Id = FRPGId(TEXT("i1000"));
	TestTrue(TEXT("Restoring Id reflects immediately"), Model.Matches(Saved, true, bUnknown));
	TestFalse(TEXT("Classification never dirties package"), Package->IsDirty());
	const FString Broken = TEXT("[/Script/RPGEditor.RPGIdCategorySettings]\nSchemaVersion=99\nRules=()\n");
	FFileHelper::SaveStringToFile(Broken, *Fixture.Filename);
	Fixture.Store.Reload(Error);
	TestFalse(TEXT("Invalid rules are not a deletion"), Model.Refresh(Fixture.Store));
	TestTrue(TEXT("Selection retained during invalid rules"), Model.IsSelected({TEXT("Item"), TEXT("Equipment")}));
	TestFalse(TEXT("Invalid rules exclude matching asset"), Model.Matches(Saved, true, bUnknown));
	TestTrue(TEXT("Invalid rules diagnosed"), !Model.GetErrors().IsEmpty());
	TestFalse(TEXT("Failed parse cannot overwrite source with empty rules"), Fixture.Store.Save({}, Fixture.Store.GetRevision(), Error));
	FString Preserved;
	FFileHelper::LoadFileToString(Preserved, *Fixture.Filename);
	TestEqual(TEXT("Unreadable settings preserved"), Preserved, Broken);
	FFileHelper::SaveStringToFile(TEXT("[/Script/RPGEditor.RPGIdCategorySettings]\nSchemaVersion=2\nRules=")
		+ FRPGIdCategoryResolver::Export(FilterRules()) + TEXT("\n"), *Fixture.Filename);
	TestTrue(TEXT("Repaired source reloads"), Fixture.Store.Reload(Error));
	TestFalse(TEXT("Repair retains selected key"), Model.Refresh(Fixture.Store));
	TestTrue(TEXT("Repair restores matching without asset save"), Model.Matches(Saved, true, bUnknown));
	TestTrue(TEXT("Repair clears unavailable diagnostic"), Model.GetErrors().IsEmpty());
	TestFalse(TEXT("Repair does not dirty the asset"), Package->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCategoryPickerFilterTest, "IronicRPG.RPGId.Category.PickerFilterScopesAndRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCategoryPickerFilterTest::RunTest(const FString& Parameters)
{
	FCategoryFilterFixture Fixture;
	FString Error;
	TestTrue(TEXT("Picker fixture save"), Fixture.Store.Save(FilterRules(), Fixture.Store.GetRevision(), Error));
	FRPGIdCategoryPickerFilter ItemPicker(TEXT("Item"), &Fixture.Store);
	TestTrue(TEXT("Limited picker exposes categories"), ItemPicker.HasChoices());
	TestTrue(TEXT("Limited picker is enabled"), ItemPicker.IsEnabled());
	TestTrue(TEXT("All preserves Item candidate"), ItemPicker.Matches(TEXT("Item"), TEXT("i0100")));
	const FRPGIdCategoryPickerChoice Equipment{TEXT("Item"), ERPGIdSuggestionScope::Category, TEXT("Equipment")};
	ItemPicker.Select(Equipment);
	TestTrue(TEXT("Category matches its range"), ItemPicker.Matches(TEXT("Item"), TEXT("i1000")));
	TestFalse(TEXT("Category excludes another Item range"), ItemPicker.Matches(TEXT("Item"), TEXT("i0100")));
	TestFalse(TEXT("Category excludes another AssetType"), ItemPicker.Matches(TEXT("Character"), TEXT("c0009")));
	const FRPGIdCategoryPickerChoice Unclassified{TEXT("Item"), ERPGIdSuggestionScope::Unclassified, NAME_None};
	ItemPicker.Select(Unclassified);
	TestTrue(TEXT("None matches the valid complement"), ItemPicker.Matches(TEXT("Item"), TEXT("i0100")));
	TestFalse(TEXT("None excludes classified numbers"), ItemPicker.Matches(TEXT("Item"), TEXT("i1000")));
	TestFalse(TEXT("None never accepts an unclaimed Id"), ItemPicker.Matches(TEXT("Item"), NAME_None));
	TestFalse(TEXT("None never accepts malformed Id"), ItemPicker.Matches(TEXT("Item"), TEXT("i1x00")));

	FRPGIdCategoryPickerFilter UnlimitedPicker(NAME_None, &Fixture.Store);
	const TMap<FName, FName> NativePrefixes = FRPGIdCategoryResolver::ReadNativePrefixes();
	TestEqual(TEXT("Unlimited picker lists every registered AssetType"), UnlimitedPicker.GetScopesByType().Num(), NativePrefixes.Num());
	for (const auto& Entry : NativePrefixes)
	{
		TestTrue(FString::Printf(TEXT("Registered AssetType %s is listed"), *Entry.Key.ToString()),
			UnlimitedPicker.GetScopesByType().Contains(Entry.Key));
	}
	const FRPGIdCategoryPickerChoice AllItems{TEXT("Item"), ERPGIdSuggestionScope::AllNumbers, NAME_None};
	UnlimitedPicker.Select(AllItems);
	TestTrue(TEXT("AssetType All includes classified assets"), UnlimitedPicker.Matches(TEXT("Item"), TEXT("i1000")));
	TestTrue(TEXT("AssetType All includes unclassified assets"), UnlimitedPicker.Matches(TEXT("Item"), TEXT("i0100")));
	TestFalse(TEXT("AssetType All excludes other AssetTypes"), UnlimitedPicker.Matches(TEXT("Character"), TEXT("c0009")));
	FName TypeWithoutCategories;
	const FRPGIdCategoryRules DefinedRules = FilterRules();
	for (const auto& Entry : NativePrefixes)
	{
		if (!DefinedRules.AssetTypes.Contains(Entry.Key))
		{
			TypeWithoutCategories = Entry.Key;
			break;
		}
	}
	TestFalse(TEXT("Fixture leaves at least one registered AssetType without Category rules"), TypeWithoutCategories.IsNone());
	const TArray<FRPGIdCategoryScope>* UncategorizedTypeScopes = UnlimitedPicker.GetScopesByType().Find(TypeWithoutCategories);
	TestNotNull(TEXT("AssetType without Category rules remains listed"), UncategorizedTypeScopes);
	if (UncategorizedTypeScopes)
	{
		TestEqual(TEXT("AssetType without Category rules has only one picker scope"), UncategorizedTypeScopes->Num(), 1);
		TestTrue(TEXT("AssetType without Category rules provides All"), UncategorizedTypeScopes->ContainsByPredicate(
			[](const FRPGIdCategoryScope& Scope) { return Scope.Kind == ERPGIdSuggestionScope::AllNumbers; }));
	}
	const FRPGIdCategoryPickerChoice MainCharacter{TEXT("Character"), ERPGIdSuggestionScope::Category, TEXT("Main")};
	UnlimitedPicker.Select(MainCharacter);
	TestTrue(TEXT("Unlimited picker can select a different Type group"), UnlimitedPicker.Matches(TEXT("Character"), TEXT("c0009")));
	TestFalse(TEXT("Unlimited group selection also restricts AssetType"), UnlimitedPicker.Matches(TEXT("Item"), TEXT("i1000")));

	int32 Loads = 0;
	const FDelegateHandle LoadHandle = FCoreUObjectDelegates::OnAssetLoaded.AddLambda([&Loads](UObject*) { ++Loads; });
	for (int32 Index = 0; Index < 10000; ++Index)
	{
		UnlimitedPicker.Matches(TEXT("Character"), TEXT("c0009"));
	}
	FCoreUObjectDelegates::OnAssetLoaded.Remove(LoadHandle);
	TestEqual(TEXT("Prepared picker predicate causes zero asset loads"), Loads, 0);

	ItemPicker.Select(Equipment);
	auto Rules = FilterRules();
	Rules.AssetTypes.FindChecked(TEXT("Item")).Categories[0].DisplayName = TEXT("Renamed Equipment");
	TestTrue(TEXT("Rename saves"), Fixture.Store.Save(Rules, Fixture.Store.GetRevision(), Error));
	TestTrue(TEXT("Stable key survives rename"), ItemPicker.IsSelected(Equipment));
	Rules.AssetTypes.FindChecked(TEXT("Item")).Categories.Reset();
	TestTrue(TEXT("Delete saves"), Fixture.Store.Save(Rules, Fixture.Store.GetRevision(), Error));
	TestFalse(TEXT("Deleted category returns to All"), ItemPicker.GetSelection().IsSet());
	TestTrue(TEXT("Deleted selection explains reset"), ItemPicker.GetStatus().Contains(TEXT("returned to All")));
	return true;
}

namespace
{
	class FCategoryFilterLifecycleCommand : public IAutomationLatentCommand
	{
	public:
		explicit FCategoryFilterLifecycleCommand(FAutomationTestBase* InTest) : Test(InTest)
		{
			FString Error;
			Test->TestTrue(TEXT("Lifecycle fixture save"), Fixture.Store.Save(FilterRules(), Fixture.Store.GetRevision(), Error));
			auto* Previous = FRPGIdCategoryStore::SetTestOverride(&Fixture.Store);
			GetDefault<URPGIdCategoryFilterExtension>()->AddFrontEndFilterExtensions(nullptr, Filters);
			FRPGIdCategoryStore::SetTestOverride(Previous);
			Filters[0]->OnChanged().AddLambda([this]() { ++Changes; });
			SettingsFile = Fixture.Filename + TEXT(".browser.ini");
			GConfig->Add(SettingsFile, FConfigFile());
			GConfig->SetArray(TEXT("Browser"), TEXT("Test.RPGCategoryTypes"), {TEXT("Item")}, SettingsFile);
			GConfig->SetArray(TEXT("Browser"), TEXT("Test.RPGCategoryKeys"), {TEXT("Equipment")}, SettingsFile);
			Filters[0]->LoadSettings(SettingsFile, TEXT("Browser"), TEXT("Test"));
			Started = FPlatformTime::Seconds();
		}
		virtual ~FCategoryFilterLifecycleCommand() override
		{
			Filters.Reset();
			GConfig->UnloadFile(SettingsFile);
			IFileManager::Get().Delete(*SettingsFile);
		}
		virtual bool Update() override
		{
			if (Changes <= LastChanges)
			{
				if (FPlatformTime::Seconds() - Started < 10.0) { return false; }
				Test->AddError(FString::Printf(TEXT("Category filter did not refresh at lifecycle stage %d."), Stage));
				return true;
			}
			LastChanges = Changes;
			FString Error;
			if (Stage == 0)
			{
				Filters[0]->SaveSettings(SettingsFile, TEXT("Restored"), TEXT("Test"));
				TArray<FString> Keys;
				GConfig->GetArray(TEXT("Restored"), TEXT("Test.RPGCategoryKeys"), Keys, SettingsFile);
				Test->TestTrue(TEXT("Per-browser selections round trip"), Keys == TArray<FString>{TEXT("Equipment")});
				auto Rules = FilterRules();
				Rules.AssetTypes.FindChecked(TEXT("Item")).Categories[0].DisplayName = TEXT("Renamed Gear");
				Test->TestTrue(TEXT("Changing settings saves"), Fixture.Store.Save(Rules, Fixture.Store.GetRevision(), Error));
			}
			else if (Stage == 1)
			{
				Test->AddInfo(TEXT("Settings save refreshed the real frontend filter."));
				Asset.Reset(NewObject<UItemAsset>(GetTransientPackage()));
				FPropertyChangedEvent Event(FindFProperty<FProperty>(URPGPrimaryAsset::StaticClass(), TEXT("Id")));
				FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(Asset.Get(), Event);
			}
			else if (Stage == 2)
			{
				Test->AddInfo(TEXT("Asset property change refreshed the real frontend filter."));
				FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().OnAssetUpdatedOnDisk().Broadcast(
					Metadata(UItemAsset::StaticClass(), TEXT("i1000")));
			}
			else
			{
				Test->AddInfo(TEXT("Registry update refreshed the real frontend filter."));
				return true;
			}
			++Stage;
			Started = FPlatformTime::Seconds();
			return false;
		}

	private:
		FAutomationTestBase* Test;
		FCategoryFilterFixture Fixture;
		TArray<TSharedRef<FFrontendFilter>> Filters;
		TStrongObjectPtr<UItemAsset> Asset;
		FString SettingsFile;
		int32 Changes = 0;
		int32 LastChanges = 0;
		int32 Stage = 0;
		double Started = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCategoryFilterLifecycleTest, "IronicRPG.RPGId.Category.FilterEventRefreshAndPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCategoryFilterLifecycleTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FCategoryFilterLifecycleCommand(this));
	return true;
}
#endif

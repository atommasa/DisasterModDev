// Copyright Ironic Studio. All Rights Reserved.
#include "RPGIdCategoryService.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	TMap<FName, FName> TestPrefixes() { return {{TEXT("Item"), TEXT("i")}, {TEXT("MapMarker"), TEXT("mm")}}; }

	FRPGIdCategoryRules TestRules()
	{
		FRPGIdCategoryDefinition Category;
		Category.CategoryKey = TEXT("Consumable");
		Category.DisplayName = TEXT("Consumable");
		Category.Ranges = {{125, 378}, {500, 599}};
		FRPGIdCategoryRules Rules;
		Rules.AssetTypes.FindOrAdd(TEXT("Item")).Categories.Add(Category);
		return Rules;
	}

	struct FCategoryConfigFixture
	{
		FCategoryConfigFixture()
		{
			Filename = FPaths::ProjectSavedDir() / (TEXT("CategoryTest-") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".ini"));
			FFileHelper::SaveStringToFile(Original, *Filename);
		}
		~FCategoryConfigFixture()
		{
			FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*Filename, false);
			IFileManager::Get().Delete(*Filename);
		}
		FString Read() const { FString Text; FFileHelper::LoadFileToString(Text, *Filename); return Text; }

		FString Filename;
		FString Original = TEXT("; Keep this comment\r\n[Unrelated]\r\nValue=Preserved\r\n");
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCategoryRangeTest, "IronicRPG.RPGId.Category.RangesAndComplement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCategoryRangeTest::RunTest(const FString& Parameters)
{
	const auto Prefixes = TestPrefixes();
	auto Rules = TestRules();
	TestTrue(TEXT("Arbitrary ranges accepted"), FRPGIdCategoryResolver::Validate(Rules, Prefixes).IsEmpty());
	for (const auto Id : {TEXT("i0125"), TEXT("i0378"), TEXT("i0500"), TEXT("i0599")})
	{
		TestEqual(TEXT("Inclusive boundary"), FRPGIdCategoryResolver::Resolve(Rules, Prefixes, 4, TEXT("Item"), Id).State,
			ERPGIdCategoryState::Classified);
	}
	for (const auto Id : {TEXT("i0000"), TEXT("i0124"), TEXT("i0379"), TEXT("i9999")})
	{
		TestEqual(TEXT("Outside is unclassified"), FRPGIdCategoryResolver::Resolve(Rules, Prefixes, 4, TEXT("Item"), Id).State,
			ERPGIdCategoryState::Unclassified);
	}
	TArray<FRPGIdCategoryScope> Scopes;
	FString Error;
	TestTrue(TEXT("Scopes available"), FRPGIdCategoryResolver::ListScopes(Rules, Prefixes, 4, TEXT("Item"), Scopes, Error));
	if (!TestEqual(TEXT("Category plus None"), Scopes.Num(), 2)) { return false; }
	TestEqual(TEXT("None means complement"), Scopes.Last().Kind, ERPGIdSuggestionScope::Unclassified);
	int32 Count = 0;
	for (const auto& Range : Scopes.Last().Ranges) { Count += Range.End - Range.Start + 1; }
	TestEqual(TEXT("Complement cardinality"), Count, 9646);
	TestEqual(TEXT("Three complement ranges"), Scopes.Last().Ranges.Num(), 3);
	TSet<FName> Used;
	FName Candidate;
	TestTrue(TEXT("Complement finds lowest number"), FRPGIdCategoryStore::FindAvailable(Scopes.Last(), TEXT("mm"), Used, Candidate));
	TestEqual(TEXT("Full prefix retained"), Candidate, FName(TEXT("mm0000")));
	for (int32 Number = 0; Number < 10000; ++Number)
	{
		Used.Add(FName(*FString::Printf(TEXT("mm%04d"), Number)));
	}
	TestFalse(TEXT("Occupied complement is full"), FRPGIdCategoryStore::FindAvailable(Scopes.Last(), TEXT("mm"), Used, Candidate));
	TestTrue(TEXT("Full complement has no fallback candidate"), Candidate.IsNone());
	TestEqual(TEXT("Occupation does not hide None"), Scopes.Num(), 2);
	Used.Remove(TEXT("mm0379"));
	TestTrue(TEXT("Complement continues after gap"), FRPGIdCategoryStore::FindAvailable(Scopes.Last(), TEXT("mm"), Used, Candidate));
	TestEqual(TEXT("First unoccupied complement number"), Candidate, FName(TEXT("mm0379")));
	Rules.AssetTypes.FindChecked(TEXT("Item")).Categories[0].Ranges = {{0, 9999}};
	FRPGIdCategoryResolver::ListScopes(Rules, Prefixes, 4, TEXT("Item"), Scopes, Error);
	TestEqual(TEXT("Full coverage hides None"), Scopes.Num(), 1);
	Rules.AssetTypes.FindChecked(TEXT("Item")).Categories[0].Ranges = {{0, 0}, {9999, 9999}};
	TestTrue(TEXT("Singleton extremes accepted"), FRPGIdCategoryResolver::Validate(Rules, Prefixes).IsEmpty());
	FRPGIdCategoryResolver::ListScopes({}, Prefixes, 4, TEXT("Item"), Scopes, Error);
	TestEqual(TEXT("No rules uses AllNumbers"), Scopes[0].Kind, ERPGIdSuggestionScope::AllNumbers);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCategoryInvalidTest, "IronicRPG.RPGId.Category.ValidationAndIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCategoryInvalidTest::RunTest(const FString& Parameters)
{
	const auto Prefixes = TestPrefixes();
	for (const auto Range : {FRPGIdCategoryRange{0, 125}, FRPGIdCategoryRange{125, 378}, FRPGIdCategoryRange{-1, 0},
		FRPGIdCategoryRange{9999, 10000}, FRPGIdCategoryRange{300, 100}})
	{
		auto Rules = TestRules();
		Rules.AssetTypes.FindChecked(TEXT("Item")).Categories[0].Ranges.Add(Range);
		TestFalse(TEXT("Invalid or overlapping rejected"), FRPGIdCategoryResolver::Validate(Rules, Prefixes).IsEmpty());
		TestEqual(TEXT("Affected type fails closed"), FRPGIdCategoryResolver::Resolve(Rules, Prefixes, 4, TEXT("Item"), TEXT("i0125")).State,
			ERPGIdCategoryState::InvalidRules);
		TestEqual(TEXT("Other type remains usable"), FRPGIdCategoryResolver::Resolve(Rules, Prefixes, 4, TEXT("MapMarker"), TEXT("mm0125")).State,
			ERPGIdCategoryState::Unclassified);
	}
	auto Rules = TestRules();
	Rules.AssetTypes.FindChecked(TEXT("Item")).Categories[0].Ranges.Add({379, 499});
	TestTrue(TEXT("Adjacent ranges allowed"), FRPGIdCategoryResolver::Validate(Rules, Prefixes).IsEmpty());
	auto Other = Rules.AssetTypes.FindChecked(TEXT("Item")).Categories[0];
	Rules.AssetTypes.FindOrAdd(TEXT("MapMarker")).Categories.Add(Other);
	TestTrue(TEXT("Same numbers in other type allowed"), FRPGIdCategoryResolver::Validate(Rules, Prefixes).IsEmpty());
	Other.Ranges = {{800, 900}};
	Rules.AssetTypes.FindOrAdd(TEXT("MapMarker")).Categories.Add(Other);
	TestFalse(TEXT("Duplicate key/name rejected even without overlap"), FRPGIdCategoryResolver::Validate(Rules, Prefixes).IsEmpty());
	Rules = TestRules();
	Rules.AssetTypes.FindChecked(TEXT("Item")).Categories[0].Ranges.Reset();
	TestFalse(TEXT("Empty ranges rejected"), FRPGIdCategoryResolver::Validate(Rules, Prefixes).IsEmpty());
	Rules = TestRules();
	Rules.AssetTypes.FindChecked(TEXT("Item")).Categories[0].DisplayName = TEXT("  ");
	TestFalse(TEXT("Blank display name rejected"), FRPGIdCategoryResolver::Validate(Rules, Prefixes).IsEmpty());
	Rules = TestRules();
	Rules.AssetTypes.Add(NAME_None, Rules.AssetTypes.FindChecked(TEXT("Item")));
	TestEqual(TEXT("Unattributed invalid entry blocks resolution"),
		FRPGIdCategoryResolver::Resolve(Rules, Prefixes, 4, TEXT("Item"), TEXT("i0125")).State, ERPGIdCategoryState::InvalidRules);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCategoryIdentityTest, "IronicRPG.RPGId.Category.IdentityStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCategoryIdentityTest::RunTest(const FString& Parameters)
{
	const auto Prefixes = TestPrefixes();
	auto Rules = TestRules();
	Rules.AssetTypes.Add(TEXT("MapMarker"), Rules.AssetTypes.FindChecked(TEXT("Item")));
	Rules.AssetTypes.Remove(TEXT("Item"));
	TestEqual(TEXT("Multi-character prefix"), FRPGIdCategoryResolver::Resolve(Rules, Prefixes, 4, TEXT("MapMarker"), TEXT("mm0125")).State,
		ERPGIdCategoryState::Classified);
	for (const auto Id : {TEXT("mm125"), TEXT("mm01250"), TEXT("mm01x5"), TEXT("i0125")})
	{
		TestEqual(TEXT("Wrong format is not unclassified"), FRPGIdCategoryResolver::Resolve(Rules, Prefixes, 4, TEXT("MapMarker"), Id).State,
			ERPGIdCategoryState::InvalidId);
	}
	TestEqual(TEXT("None is unclaimed"), FRPGIdCategoryResolver::Resolve(Rules, Prefixes, 4, TEXT("MapMarker"), NAME_None).State,
		ERPGIdCategoryState::Unclaimed);
	TestEqual(TEXT("Other numeric length not coerced"), FRPGIdCategoryResolver::Resolve(Rules, Prefixes, 5, TEXT("MapMarker"), TEXT("mm00125")).State,
		ERPGIdCategoryState::UnsupportedFormat);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCategoryPersistenceTest, "IronicRPG.RPGId.Category.PersistenceAndDraft",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCategoryPersistenceTest::RunTest(const FString& Parameters)
{
	FCategoryConfigFixture File;
	FRPGIdCategoryStore Store(File.Filename);
	FString Error;
	TestTrue(TEXT("Absent section is empty rules"), Store.Reload(Error));
	int32 Notifications = 0;
	Store.OnChanged().AddLambda([&Notifications]() { ++Notifications; });
	{
		TStrongObjectPtr<URPGIdCategorySettings> Draft(NewObject<URPGIdCategorySettings>(GetTransientPackage(), NAME_None, RF_Transient));
		Draft->Rules = TestRules();
		TestEqual(TEXT("Draft does not write file"), File.Read(), File.Original);
		TestTrue(TEXT("Draft does not publish categories"), Store.GetRules().AssetTypes.FindChecked(TEXT("Item")).Categories.IsEmpty());
	}
	TestEqual(TEXT("Discard draft sends no notifications"), Notifications, 0);
	auto Rules = TestRules();
	Rules.AssetTypes.FindChecked(TEXT("Item")).Categories[0].DisplayName = TEXT("道具 \"A\"");
	TestTrue(TEXT("Native Item catalog available"), FRPGIdCategoryResolver::ReadNativePrefixes().Contains(TEXT("Item")));
	FRPGIdCategoryResolver::IncludeRegisteredTypes(Rules, FRPGIdCategoryResolver::ReadNativePrefixes());
	if (!TestTrue(TEXT("Save succeeds"), Store.Save(Rules, Store.GetRevision(), Error))) { AddError(Error); return false; }
	TestEqual(TEXT("Only successful save publishes"), Notifications, 1);
	TestTrue(TEXT("Unrelated text preserved"), File.Read().StartsWith(File.Original));
	FRPGIdCategoryStore Restart(File.Filename);
	TestTrue(TEXT("Fresh store reload succeeds"), Restart.Reload(Error));
	TestEqual(TEXT("Rules persist exactly"), FRPGIdCategoryResolver::Export(Restart.GetRules()), FRPGIdCategoryResolver::Export(Rules));
	TestEqual(TEXT("Persisted resolution"), Restart.Resolve(TEXT("Item"), TEXT("i0125")).State, ERPGIdCategoryState::Classified);
	const FString Saved = File.Read();
	auto Invalid = Rules;
	Invalid.AssetTypes.FindChecked(TEXT("Item")).Categories[0].Ranges.Add({125, 126});
	TestFalse(TEXT("Invalid draft cannot save"), Store.Save(Invalid, Store.GetRevision(), Error));
	TestEqual(TEXT("Rejected save preserves disk"), File.Read(), Saved);
	TestEqual(TEXT("Rejected save preserves revision notification"), Notifications, 1);
	Store.OnChanged().Clear();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCategoryFailureTest, "IronicRPG.RPGId.Category.SaveFailureAndExternalReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCategoryFailureTest::RunTest(const FString& Parameters)
{
	FCategoryConfigFixture File;
	FRPGIdCategoryStore Store(File.Filename);
	FString Error;
	Store.Reload(Error);
	const uint64 OriginalRevision = Store.GetRevision();
	FFileHelper::SaveStringToFile(File.Original + TEXT("External=Changed\r\n"), *File.Filename);
	TestFalse(TEXT("External update rejects stale save"), Store.Save(TestRules(), OriginalRevision, Error));
	TestTrue(TEXT("External update preserved"), File.Read().Contains(TEXT("External=Changed")));
	Store.Reload(Error);
	TestFalse(TEXT("Old revision after reload rejected"), Store.Save(TestRules(), OriginalRevision, Error));
	FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*File.Filename, true);
	TestFalse(TEXT("Read-only save rejected"), Store.Save(TestRules(), Store.GetRevision(), Error));
	FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*File.Filename, false);
	FRPGIdCategoryStore Failure(File.Filename, [](const FString&, const FString&) { return false; });
	Failure.Reload(Error);
	const FString Before = File.Read();
	const uint64 BeforeRevision = Failure.GetRevision();
	TestFalse(TEXT("Injected atomic replace failure"), Failure.Save(TestRules(), BeforeRevision, Error));
	TestEqual(TEXT("Failed replacement preserves original"), File.Read(), Before);
	TestEqual(TEXT("Failed replacement does not publish"), Failure.GetRevision(), BeforeRevision);
	TArray<FString> Temps;
	IFileManager::Get().FindFiles(Temps, *(File.Filename + TEXT(".categories-*.tmp")), true, false);
	TestEqual(TEXT("Only owned temporary files cleaned up"), Temps.Num(), 0);
	FFileHelper::SaveStringToFile(TEXT("[/Script/RPGEditor.RPGIdCategorySettings]\nSchemaVersion=99\nRules=()\n"), *File.Filename);
	TestFalse(TEXT("Bad external config is not silently empty"), Store.Reload(Error));
	TestEqual(TEXT("Read failure replaces old trust"), Store.Resolve(TEXT("Item"), TEXT("i0125")).State, ERPGIdCategoryState::InvalidRules);
	FFileHelper::SaveStringToFile(TEXT("[/Script/RPGEditor.RPGIdCategorySettings]\nSchemaVersion=1\nRules=broken\n"), *File.Filename);
	TestFalse(TEXT("Malformed struct fails closed"), Store.Reload(Error));
	auto Overlap = TestRules();
	Overlap.AssetTypes.FindChecked(TEXT("Item")).Categories[0].Ranges.Add({125, 126});
	FFileHelper::SaveStringToFile(TEXT("[/Script/RPGEditor.RPGIdCategorySettings]\nSchemaVersion=2\nRules=")
		+ FRPGIdCategoryResolver::Export(Overlap) + TEXT("\n"), *File.Filename);
	TestFalse(TEXT("External semantic errors reported"), Store.Reload(Error));
	TestEqual(TEXT("Bad type unavailable"), Store.Resolve(TEXT("Item"), TEXT("i0125")).State, ERPGIdCategoryState::InvalidRules);
	TestEqual(TEXT("Good type still resolves"), Store.Resolve(TEXT("MapMarker"), TEXT("mm0125")).State, ERPGIdCategoryState::Unclassified);
	return true;
}
#endif

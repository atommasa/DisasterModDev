// Copyright Ironic Studio. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS

#include "RPGReleaseSealService.h"
#include "RPGIdCategoryService.h"
#include "UObject/Package.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace
{
	FRPGReleaseSnapshot Snapshot(const FString& ReleaseId)
	{
		FRPGReleaseSnapshot Value;
		Value.ReleaseId = ReleaseId;
		Value.SaveDataVersion = 1;
		Value.SaveGameMigrations.Add(FRPGIdMigrationStep(0, 1, {}));
		Value.Claims.Add({ TEXT("i1234"), FSoftObjectPath(TEXT("/Game/DataAssets/Item/Test.Test")),
			FSoftClassPath(TEXT("/Script/RPGCore.ItemAsset")) });
		return Value;
	}

	FRPGReleaseHistoryEntry Entry(const FRPGReleaseSnapshot& Value)
	{
		return { FSoftObjectPath(TEXT("/Game/RPGReleaseManifests/") + Value.ReleaseId + TEXT(".") + Value.ReleaseId), Value, Value.ComputeHash() };
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGReleaseHashTest, "IronicRPG.RPGId.Release.SemanticHashAndSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGReleaseHashTest::RunTest(const FString&)
{
	TestTrue(TEXT("Manual Data Asset factory cannot create release manifests"),
		URPGReleaseManifest::StaticClass()->HasAnyClassFlags(CLASS_HideDropDown));
	FText Error;
	FRPGReleaseSnapshot A = Snapshot(TEXT("release_001"));
	A.Claims.Add({ TEXT("c1234"), FSoftObjectPath(TEXT("/Game/DataAssets/Character/Test.Test")),
		FSoftClassPath(TEXT("/Script/RPGCore.CharacterAsset")) });
	TestTrue(TEXT("Valid snapshot"), A.Validate(Error));
	FRPGReleaseSnapshot B = A;
	Swap(B.Claims[0], B.Claims[1]);
	TestEqual(TEXT("Claim ordering does not affect semantic equality"), A.ComputeHash(), B.ComputeHash());
	B.SaveDataVersion = 2;
	B.SaveGameMigrations.Add(FRPGIdMigrationStep(1, 2, {}));
	TestNotEqual(TEXT("Save version changes hash"), A.ComputeHash(), B.ComputeHash());
	B = A;
	B.SaveGameMigrations[0].Redirects.Add(FRPGIdRedirect(FRPGId(TEXT("i1234")), FRPGId(TEXT("i1235"))));
	TestNotEqual(TEXT("Redirect catalog changes hash"), A.ComputeHash(), B.ComputeHash());
	B = A;
	B.PreviousHash = TEXT("different");
	TestNotEqual(TEXT("Previous link changes hash"), A.ComputeHash(), B.ComputeHash());
	B = A;
	const FRPGReleaseClaim Duplicate = B.Claims[0];
	B.Claims.Add(Duplicate);
	TestFalse(TEXT("Duplicate claim refused"), B.Validate(Error));
	B = A;
	B.Schema = 3;
	TestFalse(TEXT("Unknown schema refused"), B.Validate(Error));
	for (const FString& Invalid : { FString(), FString(TEXT("../bad")), FString(TEXT("Release")), FString::ChrN(65, 'a') })
	{
		TestFalse(TEXT("Invalid release token refused"), FRPGReleaseSnapshot::IsValidReleaseId(Invalid));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGReleaseChainTest, "IronicRPG.RPGId.Release.LinearHistoryAndPreparedRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGReleaseChainTest::RunTest(const FString&)
{
	FText Error;
	FRPGReleaseHistory History;
	TestTrue(TEXT("Empty history"), FRPGReleaseSealService::EvaluateHistory(History, TEXT(""), TEXT(""), Error));
	const FRPGReleaseHistoryEntry First = Entry(Snapshot(TEXT("one")));
	History.Entries.Add(First);
	TestTrue(TEXT("Genesis prepared"), FRPGReleaseSealService::EvaluateHistory(History, TEXT(""), TEXT(""), Error));
	TestEqual(TEXT("Prepared recognized"), History.PreparedIndex, 0);
	TestTrue(TEXT("Prepared identity reserved"), History.ReservedIds.Contains(TEXT("i1234")));
	TestTrue(TEXT("Committed genesis"), FRPGReleaseSealService::EvaluateHistory(History, First.Path.ToString(), First.Hash, Error));
	TestEqual(TEXT("Committed has no prepared entry"), History.PreparedIndex, INDEX_NONE);
	FRPGReleaseSnapshot Second = Snapshot(TEXT("two"));
	Second.PreviousHash = First.Hash;
	const FRPGReleaseHistoryEntry Next = Entry(Second);
	History.Entries.Add(Next);
	TestTrue(TEXT("Next append prepared"), FRPGReleaseSealService::EvaluateHistory(History, First.Path.ToString(), First.Hash, Error));
	TestEqual(TEXT("Next prepared index"), History.PreparedIndex, 1);
	TestTrue(TEXT("Linear append committed"), FRPGReleaseSealService::EvaluateHistory(History, Next.Path.ToString(), Next.Hash, Error));
	TestFalse(TEXT("Wrong head path refused"), FRPGReleaseSealService::EvaluateHistory(History, First.Path.ToString(), Next.Hash, Error));
	History.Entries[1].Hash = TEXT("tampered");
	TestFalse(TEXT("Hash mismatch refused"), FRPGReleaseSealService::EvaluateHistory(History, First.Path.ToString(), First.Hash, Error));
	History.Entries[1] = Next;
	Second.ReleaseId = TEXT("fork");
	History.Entries.Add(Entry(Second));
	TestFalse(TEXT("Two pending children refused"), FRPGReleaseSealService::EvaluateHistory(History, First.Path.ToString(), First.Hash, Error));
	History.Entries.Pop();
	History.Entries.RemoveAt(0);
	TestFalse(TEXT("Missing ancestor refused"), FRPGReleaseSealService::EvaluateHistory(History, Next.Path.ToString(), Next.Hash, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGReleaseMigrationLineageTest, "IronicRPG.RPGId.Release.SaveMigrationLineage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGReleaseMigrationLineageTest::RunTest(const FString&)
{
	FText Error;
	const FRPGReleaseSnapshot FirstSnapshot = Snapshot(TEXT("one"));
	const FRPGReleaseHistoryEntry First = Entry(FirstSnapshot);
	FRPGReleaseSnapshot SecondSnapshot = Snapshot(TEXT("two"));
	SecondSnapshot.PreviousHash = First.Hash;
	SecondSnapshot.SaveDataVersion = 2;
	SecondSnapshot.SaveGameMigrations.Add(FRPGIdMigrationStep(1, 2,
		{ FRPGIdRedirect(FRPGId(TEXT("i1234")), FRPGId(TEXT("i1235"))) }));
	SecondSnapshot.Claims[0].Id = TEXT("i1235");
	const FRPGReleaseHistoryEntry Second = Entry(SecondSnapshot);
	FRPGReleaseHistory History;
	History.Entries = { First, Second };
	TestTrue(TEXT("An adjacent version may append one redirect step"),
		FRPGReleaseSealService::EvaluateHistory(History, Second.Path.ToString(), Second.Hash, Error));
	TestTrue(TEXT("The old Id remains retired"), History.RetiredIds.Contains(TEXT("i1234")));
	TestTrue(TEXT("The redirect target is the current Claim"), History.CurrentIds.Contains(TEXT("i1235")));

	FRPGReleaseSnapshot MissingSource = SecondSnapshot;
	MissingSource.ReleaseId = TEXT("missing_source");
	MissingSource.SaveGameMigrations.Last().Redirects[0].OldId = FRPGId(TEXT("i9999"));
	History.Entries = { First, Entry(MissingSource) };
	TestFalse(TEXT("A redirect source must exist in the preceding release"),
		FRPGReleaseSealService::EvaluateHistory(History, History.Entries[1].Path.ToString(), History.Entries[1].Hash, Error));

	FRPGReleaseSnapshot Rewritten = Snapshot(TEXT("rewritten"));
	Rewritten.PreviousHash = First.Hash;
	Rewritten.Claims[0].Id = TEXT("i1235");
	Rewritten.SaveGameMigrations[0].Redirects.Add(FRPGIdRedirect(FRPGId(TEXT("i1234")), FRPGId(TEXT("i1235"))));
	History.Entries = { First, Entry(Rewritten) };
	TestFalse(TEXT("A release cannot rewrite an already committed migration step"),
		FRPGReleaseSealService::EvaluateHistory(History, History.Entries[1].Path.ToString(), History.Entries[1].Hash, Error));

	FRPGReleaseSnapshot MissingTarget = SecondSnapshot;
	MissingTarget.ReleaseId = TEXT("missing_target");
	MissingTarget.Claims[0].Id = TEXT("i9999");
	TestFalse(TEXT("A redirect target must be a current release Claim"), MissingTarget.Validate(Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGReleasePersistenceTest, "IronicRPG.RPGId.Release.PersistenceAndAbort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGReleasePersistenceTest::RunTest(const FString&)
{
	FText Error;
	FRPGReleaseHistory Current;
	if (!TestTrue(TEXT("History readable"), FRPGReleaseSealService::ReadHistory(Current, Error))) { AddError(Error.ToString()); return false; }
	if (!Current.Entries.IsEmpty())
	{
		AddInfo(TEXT("Persistence fixture skipped: authored release history exists; it will not be modified."));
		return true;
	}
	const FString Token = TEXT("automation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	const FString PackageName = TEXT("/Game/RPGReleaseManifests/") + Token;
	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	const FString ConfigName = FPaths::ProjectSavedDir() / (Token + TEXT(".ini"));
	FRPGReleaseSnapshot Value = Snapshot(Token);
	URPGReleaseManifest* Manifest = FRPGReleaseSealService::SavePrepared(Value, PackageName, Error);
	if (!TestNotNull(TEXT("Prepared saved"), Manifest)) { AddError(Error.ToString()); return false; }
	TestTrue(TEXT("Manifest persisted"), IFileManager::Get().FileExists(*Filename));
	TestEqual(TEXT("Persisted semantic hash"), Manifest->GetSemanticHash(), Value.ComputeHash());
	TestFalse(TEXT("Manifest Details is read only"), Manifest->CanEditChange(nullptr));
	TestFalse(TEXT("Normal manifest delete refused"), FRPGReleaseSealService::CanDelete({ Manifest }, Error));
	TestFalse(TEXT("Prepared Id cannot be claimed"), FRPGReleaseSealService::CheckMutation({}, FRPGId(TEXT("i1234")), Error));
	TestFalse(TEXT("Prepared Id cannot change"), FRPGReleaseSealService::CheckMutation(FRPGId(TEXT("i1234")), FRPGId(TEXT("i1235")), Error));
	TestTrue(TEXT("Prepared history reloads"), FRPGReleaseSealService::ReadHistory(Current, Error));
	TestEqual(TEXT("Prepared detected from saved manifest"), Current.PreparedIndex, 0);
	TestNull(TEXT("Manifest overwrite refused"), FRPGReleaseSealService::SavePrepared(Value, PackageName, Error));
	TestTrue(TEXT("Fixture config created"), FFileHelper::SaveStringToFile(TEXT("[Unrelated]\nValue=preserved\n"), *ConfigName));
	TestTrue(TEXT("Head commits to isolated config"), FRPGReleaseSealService::CommitHeadFile(ConfigName, FSoftObjectPath(Manifest).ToString(),
		Manifest->GetSemanticHash(), Error));
	FConfigFile Config;
	Config.Read(ConfigName);
	FString Preserved, Head;
	Config.GetString(TEXT("Unrelated"), TEXT("Value"), Preserved);
	Config.GetString(TEXT("/Script/RPGCore.RPGReleaseSettings"), TEXT("HeadHash"), Head);
	TestEqual(TEXT("Other settings preserved"), Preserved, FString(TEXT("preserved")));
	TestEqual(TEXT("Disk head verified"), Head, Value.ComputeHash());
	const FString CategoryFilename = ConfigName + TEXT(".categories.ini");
	FRPGIdCategoryStore Categories(CategoryFilename);
	FString CategoryError;
	TestTrue(TEXT("Isolated category settings load"), Categories.Reload(CategoryError));
	FRPGIdCategoryDefinition Category;
	Category.CategoryKey = TEXT("ReleaseBand");
	Category.DisplayName = TEXT("Release Band");
	Category.Ranges = {{1200, 1299}};
	FRPGIdCategoryRules Rules;
	Rules.AssetTypes.FindOrAdd(TEXT("Item")).Categories.Add(Category);
	const FString ManifestHash = Manifest->GetSemanticHash();
	FString HeadBefore, HeadAfter;
	FFileHelper::LoadFileToString(HeadBefore, *ConfigName);
	TestTrue(TEXT("Category rules can change with reserved release identities"), Categories.Save(Rules, Categories.GetRevision(), CategoryError));
	TestEqual(TEXT("Reserved identity is classified"), Categories.Resolve(TEXT("Item"), TEXT("i1234")).CategoryKey, Category.CategoryKey);
	TestEqual(TEXT("Category save preserves manifest hash"), Manifest->GetSemanticHash(), ManifestHash);
	TestEqual(TEXT("Category save preserves manifest snapshot"), Manifest->GetSnapshot().ComputeHash(), ManifestHash);
	TestFalse(TEXT("Category save does not dirty manifest"), Manifest->GetOutermost()->IsDirty());
	TestFalse(TEXT("Category save cannot release reserved identity"), FRPGReleaseSealService::CheckMutation({}, FRPGId(TEXT("i1234")), Error));
	FFileHelper::LoadFileToString(HeadAfter, *ConfigName);
	TestEqual(TEXT("Category save preserves head file"), HeadAfter, HeadBefore);
	IFileManager::Get().Delete(*CategoryFilename);
	FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ConfigName, true);
	TestFalse(TEXT("Read-only head refuses commit"), FRPGReleaseSealService::CommitHeadFile(ConfigName, TEXT("different"), TEXT("different"), Error));
	FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ConfigName, false);
	TestTrue(TEXT("Abort exact prepared release"), FRPGReleaseSealService::Run(ERPGReleaseSealAction::Abort, Token, TEXT(""), Error));
	TestFalse(TEXT("Abort removed disk package"), IFileManager::Get().FileExists(*Filename));
	IFileManager::Get().Delete(*ConfigName);
	TestTrue(TEXT("History empty after abort"), FRPGReleaseSealService::ReadHistory(Current, Error));
	TestTrue(TEXT("No release committed by test"), Current.Entries.IsEmpty());
	return true;
}

#endif

// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/SaveGame/RPGSaveGameMigrationTestTypes.h"

#include "Assets/RPGReleaseManifest.h"
#include "DataTypes/RPGIdMigration.h"
#include "Misc/AutomationTest.h"
#include "SaveGame/RPGSaveGame.h"
#include "SaveGame/RPGSaveGameMigration.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRPGSaveGameMigrationTraversesModuleValuesTest,
	"IronicRPG.SaveSystem.Migration.TraversesModuleValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGSaveGameMigrationTraversesModuleValuesTest::RunTest(const FString& Parameters)
{
	URPGSaveGame* SaveGame = NewObject<URPGSaveGame>();
	SaveGame->SaveDataVersion = 0;

	FRPGSaveGameMigrationTestModule Module;
	Module.DirectId = FRPGId(TEXT("i0100"));
	Module.Nested.DirectId = FRPGId(TEXT("i0100"));
	Module.Nested.TransientId = FRPGId(TEXT("i0100"));
	Module.Nested.IdArray.Add(FRPGId(TEXT("i0100")));
	Module.Nested.IdSet.Add(FRPGId(TEXT("i0100")));
	Module.Nested.IdMap.Add(FRPGId(TEXT("i0100")), FRPGId(TEXT("i0100")));
	Module.NestedInstance = FInstancedStruct::Make<FRPGSaveGameMigrationNestedTestData>(Module.Nested);
	SaveGame->SaveModules.Add(TEXT("Test"), FInstancedStruct::Make<FRPGSaveGameMigrationTestModule>(Module));

	const TArray<FRPGIdMigrationStep> Steps = {
		FRPGIdMigrationStep(0, 1, { FRPGIdRedirect(FRPGId(TEXT("i0100")), FRPGId(TEXT("i0101"))) })
	};
	const FRPGSaveGameMigrationResult Result = FRPGSaveGameMigrator::Migrate(*SaveGame, 1, Steps);
	const FRPGSaveGameMigrationTestModule& Migrated = SaveGame->SaveModules.FindChecked(TEXT("Test")).Get<FRPGSaveGameMigrationTestModule>();

	TestTrue(TEXT("The complete save envelope migrates"), Result.IsSuccess());
	TestEqual(TEXT("The authoritative version advances"), SaveGame->SaveDataVersion, static_cast<uint16>(1));
	TestEqual(TEXT("A direct Id migrates"), Migrated.DirectId.Id, FName(TEXT("i0101")));
	TestEqual(TEXT("A nested struct Id migrates"), Migrated.Nested.DirectId.Id, FName(TEXT("i0101")));
	TestEqual(TEXT("A non-SaveGame field is not part of migration"), Migrated.Nested.TransientId.Id, FName(TEXT("i0100")));
	TestEqual(TEXT("An array Id migrates"), Migrated.Nested.IdArray[0].Id, FName(TEXT("i0101")));
	TestTrue(TEXT("A set Id migrates and remains findable"), Migrated.Nested.IdSet.Contains(FRPGId(TEXT("i0101"))));
	TestTrue(TEXT("A map key migrates and remains findable"), Migrated.Nested.IdMap.Contains(FRPGId(TEXT("i0101"))));
	TestEqual(TEXT("A map value migrates"), Migrated.Nested.IdMap.FindChecked(FRPGId(TEXT("i0101"))).Id, FName(TEXT("i0101")));
	TestEqual(TEXT("A nested instanced struct migrates"),
		Migrated.NestedInstance.Get<FRPGSaveGameMigrationNestedTestData>().DirectId.Id, FName(TEXT("i0101")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRPGSaveGameMigrationCollisionIsAtomicTest,
	"IronicRPG.SaveSystem.Migration.ContainerCollisionIsAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGSaveGameMigrationCollisionIsAtomicTest::RunTest(const FString& Parameters)
{
	URPGSaveGame* SaveGame = NewObject<URPGSaveGame>();
	SaveGame->SaveDataVersion = 0;

	FRPGSaveGameMigrationTestModule Module;
	Module.Nested.IdMap.Add(FRPGId(TEXT("i0100")), FRPGId(TEXT("i0200")));
	Module.Nested.IdMap.Add(FRPGId(TEXT("i0101")), FRPGId(TEXT("i0201")));
	SaveGame->SaveModules.Add(TEXT("Test"), FInstancedStruct::Make<FRPGSaveGameMigrationTestModule>(Module));

	const TArray<FRPGIdMigrationStep> Steps = {
		FRPGIdMigrationStep(0, 1, { FRPGIdRedirect(FRPGId(TEXT("i0100")), FRPGId(TEXT("i0101"))) })
	};
	const FRPGSaveGameMigrationResult Result = FRPGSaveGameMigrator::Migrate(*SaveGame, 1, Steps);
	const FRPGSaveGameMigrationTestModule& Preserved = SaveGame->SaveModules.FindChecked(TEXT("Test")).Get<FRPGSaveGameMigrationTestModule>();

	TestEqual(TEXT("A key collision rejects the migration"), Result.Code, ERPGSaveGameMigrationResult::ContainerKeyCollision);
	TestEqual(TEXT("A rejected migration preserves the authoritative version"), SaveGame->SaveDataVersion, static_cast<uint16>(0));
	TestTrue(TEXT("The old key remains present"), Preserved.Nested.IdMap.Contains(FRPGId(TEXT("i0100"))));
	TestTrue(TEXT("The existing target key remains present"), Preserved.Nested.IdMap.Contains(FRPGId(TEXT("i0101"))));
	TestEqual(TEXT("The old key's value remains unchanged"), Preserved.Nested.IdMap.FindChecked(FRPGId(TEXT("i0100"))).Id,
		FName(TEXT("i0200")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRPGSaveGameMigrationUsesReleaseCatalogTest,
	"IronicRPG.SaveSystem.Migration.UsesImmutableReleaseCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGSaveGameMigrationUsesReleaseCatalogTest::RunTest(const FString& Parameters)
{
	URPGSaveGame* SaveGame = NewObject<URPGSaveGame>();
	SaveGame->SaveDataVersion = 1;
	FRPGSaveGameMigrationTestModule Module;
	Module.DirectId = FRPGId(TEXT("i1000"));
	SaveGame->SaveModules.Add(TEXT("Test"), FInstancedStruct::Make<FRPGSaveGameMigrationTestModule>(Module));
	FRPGReleaseMigrationCatalog Catalog;
	Catalog.CurrentVersion = 2;
	Catalog.Steps = {
		FRPGIdMigrationStep(0, 1, {}),
		FRPGIdMigrationStep(1, 2, { FRPGIdRedirect(FRPGId(TEXT("i1000")), FRPGId(TEXT("i1001"))) })
	};

	const FRPGSaveGameMigrationResult Result = FRPGSaveGameMigrator::Migrate(*SaveGame, Catalog);
	const FRPGSaveGameMigrationTestModule& Migrated = SaveGame->SaveModules.FindChecked(TEXT("Test")).Get<FRPGSaveGameMigrationTestModule>();

	TestTrue(TEXT("The release catalog drives migration"), Result.IsSuccess());
	TestEqual(TEXT("The save reaches the catalog head version"), SaveGame->SaveDataVersion, static_cast<uint16>(2));
	TestEqual(TEXT("The release redirect is applied"), Migrated.DirectId.Id, FName(TEXT("i1001")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRPGSaveGameMigrationLegacyBaselineTest,
	"IronicRPG.SaveSystem.Migration.LegacyBaselineAdvancesInMemory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGSaveGameMigrationLegacyBaselineTest::RunTest(const FString& Parameters)
{
	URPGSaveGame* LegacySave = NewObject<URPGSaveGame>();
	FRPGReleaseMigrationCatalog Catalog;
	const FRPGReleaseMigrationCatalogResult CatalogResult = FRPGReleaseMigrationCatalogReader::ReadCurrent(Catalog);
	if (!TestTrue(TEXT("The current release catalog is readable"), CatalogResult.IsSuccess()))
	{
		AddError(CatalogResult.Diagnostic);
		return false;
	}
	TestEqual(TEXT("A save missing the new property begins at the legacy baseline"), LegacySave->SaveDataVersion,
		ERPGSaveGameVersion::Baseline);

	const FRPGSaveGameMigrationResult Result = FRPGSaveGameMigrator::MigrateToCurrent(*LegacySave);

	TestTrue(TEXT("The built-in baseline step succeeds"), Result.IsSuccess());
	TestEqual(TEXT("Migration advances only the in-memory envelope"), LegacySave->SaveDataVersion, Catalog.CurrentVersion);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRPGSaveGameMigrationRejectsBeforeProviderDispatchTest,
	"IronicRPG.SaveSystem.Migration.FailureRejectsBeforeProviderDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGSaveGameMigrationRejectsBeforeProviderDispatchTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	URPGSaveGameMigrationTestProvider* Provider = NewObject<URPGSaveGameMigrationTestProvider>(GameInstance);
	URPGSaveGameMigrationTestSubsystem* SaveSubsystem = NewObject<URPGSaveGameMigrationTestSubsystem>(GameInstance);
	URPGSaveGame* TooNewSave = NewObject<URPGSaveGame>();
	FRPGReleaseMigrationCatalog Catalog;
	const FRPGReleaseMigrationCatalogResult CatalogResult = FRPGReleaseMigrationCatalogReader::ReadCurrent(Catalog);
	if (!TestTrue(TEXT("The current release catalog is readable"), CatalogResult.IsSuccess()))
	{
		AddError(CatalogResult.Diagnostic);
		return false;
	}
	TooNewSave->SaveDataVersion = Catalog.CurrentVersion + 1;
	int32 FailureCount = 0;
	SaveSubsystem->OnSaveGameLoadFailed.AddLambda([&FailureCount](const FString& Diagnostic) { ++FailureCount; });
	AddExpectedError(TEXT("rejected before provider dispatch"), EAutomationExpectedErrorFlags::Contains, 1);

	SaveSubsystem->DispatchLoadedSaveForTest(*TooNewSave);

	TestEqual(TEXT("Migration failure is reported once"), FailureCount, 1);
	TestEqual(TEXT("No provider observes partially migrated state"), Provider->LoadCallCount, 0);
	TestEqual(TEXT("The rejected save remains unchanged"), TooNewSave->SaveDataVersion,
		static_cast<uint16>(Catalog.CurrentVersion + 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRPGSaveGameMigrationSucceedsBeforeProviderDispatchTest,
	"IronicRPG.SaveSystem.Migration.LegacySaveMigratesBeforeProviderDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGSaveGameMigrationSucceedsBeforeProviderDispatchTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	URPGSaveGameMigrationTestProvider* Provider = NewObject<URPGSaveGameMigrationTestProvider>(GameInstance);
	URPGSaveGameMigrationTestSubsystem* SaveSubsystem = NewObject<URPGSaveGameMigrationTestSubsystem>(GameInstance);
	URPGSaveGame* LegacySave = NewObject<URPGSaveGame>();
	FRPGReleaseMigrationCatalog Catalog;
	const FRPGReleaseMigrationCatalogResult CatalogResult = FRPGReleaseMigrationCatalogReader::ReadCurrent(Catalog);
	if (!TestTrue(TEXT("The current release catalog is readable"), CatalogResult.IsSuccess()))
	{
		AddError(CatalogResult.Diagnostic);
		return false;
	}

	SaveSubsystem->DispatchLoadedSaveForTest(*LegacySave);

	TestEqual(TEXT("The envelope reaches current before provider dispatch"), LegacySave->SaveDataVersion, Catalog.CurrentVersion);
	TestEqual(TEXT("A successful migration dispatches each provider once"), Provider->LoadCallCount, 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Assets/RPGReleaseManifest.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGReleaseMigrationCatalogReaderTest,
	"IronicRPG.RPGId.Release.RuntimeCatalogReader",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGReleaseMigrationCatalogReaderTest::RunTest(const FString& Parameters)
{
	FRPGReleaseSnapshot Snapshot;
	Snapshot.ReleaseId = TEXT("release_002");
	Snapshot.SaveDataVersion = 2;
	Snapshot.SaveGameMigrations = {
		FRPGIdMigrationStep(0, 1, {}),
		FRPGIdMigrationStep(1, 2, { FRPGIdRedirect(FRPGId(TEXT("i1000")), FRPGId(TEXT("i1001"))) })
	};
	Snapshot.Claims.Add({ TEXT("i1001"), FSoftObjectPath(TEXT("/Game/DataAssets/Item/DA_Test.DA_Test")),
		FSoftClassPath(TEXT("/Script/RPGCore.ItemAsset")) });
	const FString Hash = Snapshot.ComputeHash();
	FRPGReleaseMigrationCatalog Catalog;
	const FRPGReleaseMigrationCatalogResult Result = FRPGReleaseMigrationCatalogReader::ReadSnapshot(Snapshot, Hash, Catalog);

	TestTrue(TEXT("A valid immutable snapshot becomes a runtime catalog"), Result.IsSuccess());
	TestEqual(TEXT("The catalog exposes the authoritative current version"), Catalog.CurrentVersion, static_cast<uint16>(2));
	TestEqual(TEXT("The complete adjacent chain is retained"), Catalog.Steps.Num(), 2);
	TestEqual(TEXT("The redirect remains in its version step"), Catalog.Steps[1].Redirects[0].NewId.Id, FName(TEXT("i1001")));

	const FRPGReleaseMigrationCatalogResult HashMismatch =
		FRPGReleaseMigrationCatalogReader::ReadSnapshot(Snapshot, TEXT("tampered"), Catalog);
	TestEqual(TEXT("A head hash mismatch is rejected"), HashMismatch.Code,
		ERPGReleaseMigrationCatalogResult::HashMismatch);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

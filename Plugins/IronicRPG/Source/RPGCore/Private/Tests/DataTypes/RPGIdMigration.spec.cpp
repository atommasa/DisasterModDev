// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTypes/RPGIdMigration.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRPGIdMigrationChainResolvesAcrossVersionsTest,
	"IronicRPG.RPGId.SaveMigration.ChainResolvesAcrossVersions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdMigrationChainResolvesAcrossVersionsTest::RunTest(const FString& Parameters)
{
	TArray<FRPGIdMigrationStep> Steps;
	Steps.Add(FRPGIdMigrationStep(0, 1, { FRPGIdRedirect(FRPGId(TEXT("i0100")), FRPGId(TEXT("i0101"))) }));
	Steps.Add(FRPGIdMigrationStep(1, 2, { FRPGIdRedirect(FRPGId(TEXT("i0101")), FRPGId(TEXT("i0102"))) }));

	FRPGIdMigrationChain Chain;
	const FRPGIdMigrationResult BuildResult = FRPGIdMigrationChain::Build(0, 2, Steps, Chain);
	FRPGId MigratedId;

	TestTrue(TEXT("The complete version chain is accepted"), BuildResult.IsSuccess());
	TestTrue(TEXT("A valid Id can be resolved"), Chain.TryResolve(FRPGId(TEXT("i0100")), MigratedId));
	TestEqual(TEXT("Every required step is applied in order"), MigratedId.Id, FName(TEXT("i0102")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRPGIdMigrationChainRejectsMissingStepTest,
	"IronicRPG.RPGId.SaveMigration.MissingStepIsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdMigrationChainRejectsMissingStepTest::RunTest(const FString& Parameters)
{
	const TArray<FRPGIdMigrationStep> Steps = { FRPGIdMigrationStep(0, 1, {}) };
	FRPGIdMigrationChain Chain;

	const FRPGIdMigrationResult Result = FRPGIdMigrationChain::Build(0, 2, Steps, Chain);

	TestEqual(TEXT("Every supported source version needs an exact next step"), Result.Code, ERPGIdMigrationResult::MissingVersionStep);
	FRPGId Unused;
	TestFalse(TEXT("A rejected chain cannot resolve Ids"), Chain.TryResolve(FRPGId(TEXT("i0100")), Unused));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRPGIdMigrationChainRejectsTooNewSourceTest,
	"IronicRPG.RPGId.SaveMigration.TooNewSourceIsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdMigrationChainRejectsTooNewSourceTest::RunTest(const FString& Parameters)
{
	FRPGIdMigrationChain Chain;
	const FRPGIdMigrationResult Result = FRPGIdMigrationChain::Build(2, 1, {}, Chain);

	TestEqual(TEXT("A future save cannot be interpreted as an older schema"), Result.Code, ERPGIdMigrationResult::SourceVersionIsNewer);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRPGIdMigrationChainRejectsCrossTypeRedirectTest,
	"IronicRPG.RPGId.SaveMigration.CrossTypeRedirectIsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdMigrationChainRejectsCrossTypeRedirectTest::RunTest(const FString& Parameters)
{
	const TArray<FRPGIdMigrationStep> Steps = {
		FRPGIdMigrationStep(0, 1, { FRPGIdRedirect(FRPGId(TEXT("i0100")), FRPGId(TEXT("c0100"))) })
	};
	FRPGIdMigrationChain Chain;
	const FRPGIdMigrationResult Result = FRPGIdMigrationChain::Build(0, 1, Steps, Chain);

	TestEqual(TEXT("A redirect cannot change the RPG AssetType"), Result.Code, ERPGIdMigrationResult::RedirectTypeMismatch);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRPGIdMigrationChainRejectsFanInTest,
	"IronicRPG.RPGId.SaveMigration.FanInRedirectIsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdMigrationChainRejectsFanInTest::RunTest(const FString& Parameters)
{
	const TArray<FRPGIdMigrationStep> Steps = {
		FRPGIdMigrationStep(0, 1, {
			FRPGIdRedirect(FRPGId(TEXT("i0100")), FRPGId(TEXT("i0200"))),
			FRPGIdRedirect(FRPGId(TEXT("i0101")), FRPGId(TEXT("i0200")))
		})
	};
	FRPGIdMigrationChain Chain;
	const FRPGIdMigrationResult Result = FRPGIdMigrationChain::Build(0, 1, Steps, Chain);

	TestEqual(TEXT("Phase 1 redirects remain one-to-one"), Result.Code, ERPGIdMigrationResult::DuplicateRedirectTarget);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

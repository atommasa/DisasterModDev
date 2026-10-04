// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RPGIdReferenceMigrationHandoff.h"
#include "RPGReleaseSealWorkflow.h"

#include "Editor.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"

namespace
{
	FRPGIdRedirect MakeRedirect(const TCHAR* OldId = TEXT("i1000"), const TCHAR* NewId = TEXT("i2000"))
	{
		return FRPGIdRedirect(FRPGId(OldId), FRPGId(NewId));
	}

	class FReleaseWorkflowBackend final : public IRPGReleaseSealWorkflowBackend
	{
	public:
		virtual bool ReadPending(TArray<FRPGIdRedirect>& OutRedirects, FText& OutError) override
		{
			if (!bReadSucceeds)
			{
				OutError = FText::FromString(TEXT("read failed"));
				return false;
			}
			OutRedirects = Pending;
			return true;
		}

		virtual bool Run(const ERPGReleaseSealAction Action, const FString& ReleaseId, const FString& SaveVersion,
			TConstArrayView<FRPGIdRedirect> Redirects, FText& OutMessage) override
		{
			++RunCount;
			LastAction = Action;
			LastRedirects = TArray<FRPGIdRedirect>(Redirects);
			OutMessage = FText::FromString(bRunSucceeds ? TEXT("release complete") : TEXT("release failed"));
			return bRunSucceeds;
		}

		virtual bool Consume(TConstArrayView<FRPGIdRedirect> Redirects, FText& OutError) override
		{
			++ConsumeCount;
			Consumed = TArray<FRPGIdRedirect>(Redirects);
			return bConsumeSucceeds;
		}

		TArray<FRPGIdRedirect> Pending;
		TArray<FRPGIdRedirect> LastRedirects;
		TArray<FRPGIdRedirect> Consumed;
		ERPGReleaseSealAction LastAction = ERPGReleaseSealAction::Preview;
		int32 RunCount = 0;
		int32 ConsumeCount = 0;
		bool bReadSucceeds = true;
		bool bRunSucceeds = true;
		bool bConsumeSucceeds = true;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceMigrationHandoffLifecycleTest,
	"IronicRPG.RPGId.SaveMigration.ReferenceHandoff.StagePersistsUndoRedoAndConsume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceMigrationHandoffLifecycleTest::RunTest(const FString& Parameters)
{
	const FString Path = FPaths::ProjectSavedDir() / (TEXT("RPGIdHandoffTest-")
		+ FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".bin"));
	IFileManager::Get().Delete(*Path, false, true);
	FRPGIdReferenceMigrationHandoffStore Store(Path);
	const FRPGIdRedirect Redirect = MakeRedirect();
	FText Error;
	bool bAdded = false;
	{
		const FScopedTransaction Transaction(FText::FromString(TEXT("Test RPG Id redirect handoff")));
		TestTrue(TEXT("Redirect stages"), Store.Stage(Redirect, bAdded, Error));
	}
	TestTrue(TEXT("New redirect is recorded"), bAdded);

	TArray<FRPGIdRedirect> Pending;
	TestTrue(TEXT("Pending redirects can be read"), Store.ReadPending(Pending, Error));
	TestEqual(TEXT("One redirect is pending"), Pending.Num(), 1);
	{
		FRPGIdReferenceMigrationHandoffStore Reloaded(Path);
		Pending.Reset();
		TestTrue(TEXT("Pending redirect survives store reload"), Reloaded.ReadPending(Pending, Error));
		TestEqual(TEXT("Reload preserves redirect"), Pending.Num(), 1);
	}

	TestTrue(TEXT("Handoff participates in migration Undo"), GEditor->UndoTransaction());
	Pending.Reset();
	TestTrue(TEXT("Pending redirects can be read after Undo"), Store.ReadPending(Pending, Error));
	TestTrue(TEXT("Undo removes pending redirect"), Pending.IsEmpty());

	TestTrue(TEXT("Handoff participates in migration Redo"), GEditor->RedoTransaction());
	Pending.Reset();
	TestTrue(TEXT("Pending redirects can be read after Redo"), Store.ReadPending(Pending, Error));
	TestEqual(TEXT("Redo restores pending redirect"), Pending.Num(), 1);

	bool bConflictAdded = false;
	TestFalse(TEXT("A conflicting source is rejected"),
		Store.Stage(MakeRedirect(TEXT("i1000"), TEXT("i3000")), bConflictAdded, Error));
	Pending.Reset();
	TestTrue(TEXT("Handoff remains readable after conflict"), Store.ReadPending(Pending, Error));
	TestEqual(TEXT("Rejected conflict leaves the handoff unchanged"), Pending.Num(), 1);

	bool bSecondAdded = false;
	{
		const FScopedTransaction Transaction(FText::FromString(TEXT("Test second RPG Id redirect handoff")));
		TestTrue(TEXT("A distinct redirect stages"),
			Store.Stage(MakeRedirect(TEXT("i3000"), TEXT("i4000")), bSecondAdded, Error));
	}
	Pending.Reset();
	TestTrue(TEXT("Multiple pending redirects can be read"), Store.ReadPending(Pending, Error));
	TestEqual(TEXT("Distinct migrations accumulate before Seal"), Pending.Num(), 2);
	TestTrue(TEXT("Undo removes only the latest migration handoff"), GEditor->UndoTransaction());
	Pending.Reset();
	Store.ReadPending(Pending, Error);
	TestEqual(TEXT("Earlier pending redirect remains after Undo"), Pending.Num(), 1);
	TestTrue(TEXT("Redo restores the latest migration handoff"), GEditor->RedoTransaction());
	Pending.Reset();
	Store.ReadPending(Pending, Error);
	TestEqual(TEXT("Both pending redirects return after Redo"), Pending.Num(), 2);

	TestTrue(TEXT("Committed redirects can be consumed"), Store.Consume(Pending, Error));
	Pending.Reset();
	TestTrue(TEXT("Handoff remains readable after consume"), Store.ReadPending(Pending, Error));
	TestTrue(TEXT("Consume clears committed redirect"), Pending.IsEmpty());
	IFileManager::Get().Delete(*Path, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGReleaseSealWorkflowRoutingTest,
	"IronicRPG.RPGId.SaveMigration.ReferenceHandoff.ReleaseWorkflowRoutesAndConsumesExactly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGReleaseSealWorkflowRoutingTest::RunTest(const FString& Parameters)
{
	TSharedRef<FReleaseWorkflowBackend> Backend = MakeShared<FReleaseWorkflowBackend>();
	Backend->Pending = {MakeRedirect()};
	FRPGReleaseSealWorkflow Workflow(Backend);

	const FRPGReleaseSealWorkflowResult Preview = Workflow.Execute(
		ERPGReleaseSealAction::Preview, TEXT("release_002"), TEXT("2"));
	TestTrue(TEXT("Preview succeeds"), Preview.bSucceeded);
	TestEqual(TEXT("Preview passes the pending redirect"), Backend->LastRedirects.Num(), 1);
	TestEqual(TEXT("Preview does not consume handoff"), Backend->ConsumeCount, 0);

	Backend->bRunSucceeds = false;
	const FRPGReleaseSealWorkflowResult FailedSeal = Workflow.Execute(
		ERPGReleaseSealAction::Seal, TEXT("release_002"), TEXT("2"));
	TestFalse(TEXT("Failed Seal remains failed"), FailedSeal.bSucceeded);
	TestEqual(TEXT("Failed Seal preserves handoff"), Backend->ConsumeCount, 0);

	Backend->bRunSucceeds = true;
	const FRPGReleaseSealWorkflowResult Seal = Workflow.Execute(
		ERPGReleaseSealAction::Seal, TEXT("release_002"), TEXT("2"));
	TestTrue(TEXT("Successful Seal succeeds"), Seal.bSucceeded);
	TestEqual(TEXT("Seal passes the pending redirect"), Backend->LastRedirects.Num(), 1);
	TestEqual(TEXT("Successful Seal consumes once"), Backend->ConsumeCount, 1);
	TestEqual(TEXT("Consume receives the exact redirect"), Backend->Consumed.Num(), 1);

	Backend->ConsumeCount = 0;
	const FRPGReleaseSealWorkflowResult Resume = Workflow.Execute(
		ERPGReleaseSealAction::Resume, TEXT("release_002"), TEXT(""));
	TestTrue(TEXT("Resume succeeds"), Resume.bSucceeded);
	TestTrue(TEXT("Resume relies on the prepared manifest instead of resubmitting redirects"), Backend->LastRedirects.IsEmpty());
	TestEqual(TEXT("Successful Resume consumes pending handoff"), Backend->ConsumeCount, 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

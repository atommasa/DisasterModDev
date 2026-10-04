// Copyright Ironic Studio. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "RPGIdBlueprintReferenceIndex.h"
#include "RPGIdClaimEditor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdNativeFingerprintContentTest,
	"IronicRPG.RPGId.ReferenceIndex.NativeFingerprintReadsChangedBytesWithIdenticalMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdNativeFingerprintContentTest::RunTest(const FString& Parameters)
{
	using namespace RPGIdReferenceIndexPrivate;
	const FString Filename = FPaths::ProjectSavedDir() / TEXT("Automation/RPGIdReferenceIndex/") + FGuid::NewGuid().ToString() + TEXT(".bin");
	TArray<uint8> Bytes;
	Bytes.Init(17, 2 * 1024 * 1024 + 3);
	FString FirstHash, SecondHash, Error;
	TestTrue(TEXT("Fixture write succeeds"), FFileHelper::SaveArrayToFile(Bytes, *Filename));
	const FDateTime Timestamp = IFileManager::Get().GetTimeStamp(*Filename);
	TestTrue(TEXT("Streaming hash succeeds"), HashNativeBinary(Filename, FirstHash, Error));
	TestEqual(TEXT("Streaming hash covers the complete file"), FirstHash, LexToString(FIoHash::HashBuffer(Bytes)));
	Bytes.Last() ^= 1;
	TestTrue(TEXT("Same-size fixture replacement succeeds"), FFileHelper::SaveArrayToFile(Bytes, *Filename));
	IFileManager::Get().SetTimeStamp(*Filename, Timestamp);
	TestTrue(TEXT("Replacement hash succeeds"), HashNativeBinary(Filename, SecondHash, Error));
	TestTrue(TEXT("Same timestamp and size cannot conceal changed content"), FirstHash != SecondHash);
	TestTrue(TEXT("Fixture cleanup succeeds"), IFileManager::Get().Delete(*Filename));
	TestFalse(TEXT("Missing binary fails closed"), HashNativeBinary(Filename, SecondHash, Error));
	TestTrue(TEXT("Failure clears previous hash"), SecondHash.IsEmpty());
	TestTrue(TEXT("Failure identifies binary"), Error.Contains(Filename));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceIndexPersistenceTest,
	"IronicRPG.RPGId.ReferenceIndex.PersistenceRejectsCorruptionAndPreservesEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceIndexPersistenceTest::RunTest(const FString& Parameters)
{
	using namespace RPGIdReferenceIndexPrivate;

	const FString CachePath = FPaths::ProjectSavedDir() / TEXT("Automation/RPGIdReferenceIndex/Persistence.bin");
	IFileManager::Get().Delete(*CachePath, false, true);
	FRPGIdReferenceIndexCache Actual;
	FString Error;
	TestEqual(TEXT("Missing cache is reported explicitly"), LoadCache(CachePath, Actual, Error),
		ERPGIdReferenceIndexLoadResult::Missing);

	FRPGIdReferenceIndexCache Expected;
	Expected.WorkspaceFingerprint = TEXT("workspace-fingerprint");
	Expected.BuildTime = FDateTime(2026, 9, 7, 12, 34, 56);
	Expected.NativeModules = { TEXT("RPGCore"), TEXT("RPGEditor") };
	FRPGIdReferenceIndexEntry& Entry = Expected.Entries.AddDefaulted_GetRef();
	Entry.PackageName = TEXT("/Game/Test/BP_ReferenceIndex");
	Entry.AssetPath = TEXT("/Game/Test/BP_ReferenceIndex.BP_ReferenceIndex");
	Entry.Evidence.References.Add({ FRPGId(TEXT("i4321")), TEXT("Blueprint:/Game/Test/BP_ReferenceIndex.BP_ReferenceIndex"),
		TEXT("ClassDefaultObject.ItemId") });
	Entry.Evidence.Issues.Add({ RPGIdReferencePrivate::ERPGIdReferenceIssueCode::MalformedBlueprintGraphPin,
		TEXT("/Game/Test/BP_ReferenceIndex.BP_ReferenceIndex"), TEXT("Graph[EventGraph].Node[Test].Pin[Id]") });

	TestTrue(TEXT("Canonical cache is atomically persisted"), SaveCacheAtomically(CachePath, Expected, Error));
	TestEqual(TEXT("Exact cache loads as ready"), LoadCache(CachePath, Actual, Error), ERPGIdReferenceIndexLoadResult::Ready);
	TestEqual(TEXT("Workspace fingerprint round-trips"), Actual.WorkspaceFingerprint, Expected.WorkspaceFingerprint);
	TestEqual(TEXT("Build time round-trips"), Actual.BuildTime, Expected.BuildTime);
	TestEqual(TEXT("Entry count round-trips"), Actual.Entries.Num(), 1);
	if (Actual.Entries.Num() == 1)
	{
		TestEqual(TEXT("Reference Id round-trips"), Actual.Entries[0].Evidence.References[0].Id, FRPGId(TEXT("i4321")));
		TestEqual(TEXT("Issue detail round-trips"), Actual.Entries[0].Evidence.Issues[0].Detail,
			TEXT("Graph[EventGraph].Node[Test].Pin[Id]"));
	}

	TArray<uint8> CorruptBytes;
	TestTrue(TEXT("Persisted cache can be read for corruption fixture"), FFileHelper::LoadFileToArray(CorruptBytes, *CachePath));
	if (CorruptBytes.Num() >= 8)
	{
		TArray<uint8> SchemaMismatchBytes = CorruptBytes;
		SchemaMismatchBytes[4] = 0;
		SchemaMismatchBytes[5] = 0;
		SchemaMismatchBytes[6] = 0;
		SchemaMismatchBytes[7] = 0;
		TestTrue(TEXT("Schema mismatch fixture overwrites the cache"),
			FFileHelper::SaveArrayToFile(SchemaMismatchBytes, *CachePath));
		TestEqual(TEXT("Schema mismatch is rejected explicitly"), LoadCache(CachePath, Actual, Error),
			ERPGIdReferenceIndexLoadResult::SchemaMismatch);

		TestTrue(TEXT("Canonical cache is restored before corruption fixture"), SaveCacheAtomically(CachePath, Expected, Error));
		TestTrue(TEXT("Restored cache can be read for corruption fixture"), FFileHelper::LoadFileToArray(CorruptBytes, *CachePath));
		CorruptBytes.Last() ^= 0x5a;
		TestTrue(TEXT("Corruption fixture overwrites the temporary cache"), FFileHelper::SaveArrayToFile(CorruptBytes, *CachePath));
		TestEqual(TEXT("Checksum corruption is rejected"), LoadCache(CachePath, Actual, Error),
			ERPGIdReferenceIndexLoadResult::Corrupt);
	}

	IFileManager::Get().Delete(*CachePath, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceIndexFingerprintTest,
	"IronicRPG.RPGId.ReferenceIndex.FingerprintInvalidatesCatalogAndNativeChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceIndexFingerprintTest::RunTest(const FString& Parameters)
{
	using namespace RPGIdReferenceIndexPrivate;

	FRPGIdReferenceIndexFingerprintInput Input;
	Input.EngineIdentity = TEXT("UE-5.8-test");
	Input.MountRoots = { TEXT("/Game"), TEXT("/IronicRPG") };
	Input.Catalog = {
		{ TEXT("/Game/A"), TEXT("/Game/A.A"), TEXT("hash-a") },
		{ TEXT("/Game/B"), TEXT("/Game/B.B"), TEXT("hash-b") }
	};
	Input.NativeModules = {
		{ TEXT("RPGCore"), TEXT("module-rpgcore") },
		{ TEXT("RPGEditor"), TEXT("module-rpgeditor") }
	};
	const FString Baseline = BuildWorkspaceFingerprint(Input);

	FRPGIdReferenceIndexFingerprintInput Reordered = Input;
	Algo::Reverse(Reordered.Catalog);
	Algo::Reverse(Reordered.NativeModules);
	TestEqual(TEXT("Fingerprint ordering is canonical"), BuildWorkspaceFingerprint(Reordered), Baseline);

	FRPGIdReferenceIndexFingerprintInput Renamed = Input;
	Renamed.Catalog[0].PackageName = TEXT("/Game/RenamedA");
	Renamed.Catalog[0].AssetPath = TEXT("/Game/RenamedA.RenamedA");
	TestNotEqual(TEXT("Blueprint rename invalidates the workspace fingerprint"), BuildWorkspaceFingerprint(Renamed), Baseline);

	FRPGIdReferenceIndexFingerprintInput Removed = Input;
	Removed.Catalog.RemoveAt(0);
	TestNotEqual(TEXT("Blueprint delete invalidates the workspace fingerprint"), BuildWorkspaceFingerprint(Removed), Baseline);

	FRPGIdReferenceIndexFingerprintInput Added = Input;
	Added.Catalog.Add({ TEXT("/Game/C"), TEXT("/Game/C.C"), TEXT("hash-c") });
	TestNotEqual(TEXT("Blueprint add invalidates the workspace fingerprint"), BuildWorkspaceFingerprint(Added), Baseline);

	FRPGIdReferenceIndexFingerprintInput Saved = Input;
	Saved.Catalog[0].PackageHash = TEXT("hash-a-after-save");
	TestNotEqual(TEXT("Blueprint save invalidates the workspace fingerprint"), BuildWorkspaceFingerprint(Saved), Baseline);

	FRPGIdReferenceIndexFingerprintInput ModuleChanged = Input;
	ModuleChanged.NativeModules[0].BinaryHash = TEXT("module-rpgcore-after-build");
	TestNotEqual(TEXT("Native module change invalidates the workspace fingerprint"), BuildWorkspaceFingerprint(ModuleChanged), Baseline);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceIndexStateTest,
	"IronicRPG.RPGId.ReferenceIndex.StateNeverTreatsIncompleteEvidenceAsReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceIndexStateTest::RunTest(const FString& Parameters)
{
	using namespace RPGIdReferenceIndexPrivate;

	FRPGIdReferenceIndexCache Cache;
	Cache.WorkspaceFingerprint = TEXT("exact");
	FRPGIdReferenceIndexStateModel State;
	State.Bootstrap(ERPGIdReferenceIndexLoadResult::Ready, &Cache, TEXT("exact"), 2, FString());
	TestEqual(TEXT("Exact cache becomes Ready"), State.GetStatus().State, ERPGIdReferenceIndexState::Ready);

	State.Bootstrap(ERPGIdReferenceIndexLoadResult::Ready, &Cache, TEXT("changed"), 2, FString());
	TestEqual(TEXT("Fingerprint mismatch is Stale"), State.GetStatus().State, ERPGIdReferenceIndexState::Stale);
	TestTrue(TEXT("Fingerprint mismatch has a named reason"), State.GetStatus().LastFailure.Contains(TEXT("fingerprint")));

	State.BeginBuild(2);
	State.RecordIndexedPackage();
	TestEqual(TEXT("Partial build remains Building"), State.GetStatus().State, ERPGIdReferenceIndexState::Building);
	TestEqual(TEXT("Partial build reports progress"), State.GetStatus().IndexedPackageCount, 1);
	TestFalse(TEXT("Partial build is never ready"), State.GetStatus().IsReady());
	State.FailBuild(TEXT("A package could not be inspected."));
	TestEqual(TEXT("Failed build is Unavailable"), State.GetStatus().State, ERPGIdReferenceIndexState::Unavailable);

	State.BeginBuild(2);
	State.CancelBuild();
	TestEqual(TEXT("Cancelled build is Unavailable"), State.GetStatus().State, ERPGIdReferenceIndexState::Unavailable);
	TestTrue(TEXT("Cancel retains an actionable reason"), State.GetStatus().LastFailure.Contains(TEXT("cancelled")));

	State.BeginBuild(2);
	State.RecordIndexedPackage();
	State.RecordIndexedPackage();
	State.CompleteBuild();
	TestEqual(TEXT("Complete build becomes Ready"), State.GetStatus().State, ERPGIdReferenceIndexState::Ready);
	State.RecordShadowSuccess(FRPGId(TEXT("i1000")));
	TestEqual(TEXT("Exact shadow comparison is recorded for the current generation"),
		State.GetStatus().SuccessfulShadowComparisonCount, 1);
	State.RecordShadowFailure(FRPGId(TEXT("i2000")), TEXT("Blueprint issue mismatch for /Game/A."));
	TestEqual(TEXT("Shadow mismatch makes the index unavailable"), State.GetStatus().State,
		ERPGIdReferenceIndexState::Unavailable);
	TestEqual(TEXT("Shadow mismatch clears prior exact evidence"), State.GetStatus().SuccessfulShadowComparisonCount, 0);
	TestEqual(TEXT("Shadow mismatch records the failing target"), State.GetStatus().LastShadowTarget, TEXT("i2000"));
	TestTrue(TEXT("Shadow mismatch preserves the diagnostic"), State.GetStatus().LastFailure.Contains(TEXT("/Game/A")));

	State.Bootstrap(ERPGIdReferenceIndexLoadResult::Ready, &Cache, TEXT("exact"), 2, FString());
	State.Invalidate(TEXT("Blueprint asset renamed."));
	TestEqual(TEXT("Lifecycle event invalidates Ready evidence"), State.GetStatus().State, ERPGIdReferenceIndexState::Stale);
	TestEqual(TEXT("Lifecycle invalidation preserves its reason"), State.GetStatus().LastFailure, TEXT("Blueprint asset renamed."));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceIndexBuildAccumulatorTest,
	"IronicRPG.RPGId.ReferenceIndex.BuilderRequiresExactCleanCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceIndexBuildAccumulatorTest::RunTest(const FString& Parameters)
{
	using namespace RPGIdReferenceIndexPrivate;

	const TArray<FRPGIdReferenceIndexCatalogItem> Catalog = {
		{ TEXT("/Game/A"), TEXT("/Game/A.A"), TEXT("hash-a") },
		{ TEXT("/Game/B"), TEXT("/Game/B.B"), TEXT("hash-b") }
	};
	FRPGIdReferenceIndexBuildAccumulator Build(TEXT("workspace"), Catalog, { TEXT("RPGCore") });
	RPGIdReferencePrivate::FRPGIdBlueprintEvidence Evidence;
	Evidence.References.Add({ FRPGId(TEXT("i7000")), TEXT("Blueprint:/Game/A.A"), TEXT("ClassDefaultObject.ItemId") });
	FString Error;
	TestTrue(TEXT("Expected clean package is accepted"), Build.AddEntry(TEXT("/Game/A"), TEXT("/Game/A.A"), Evidence, false, Error));
	TestFalse(TEXT("Duplicate package evidence is rejected"), Build.AddEntry(TEXT("/Game/A"), TEXT("/Game/A.A"), Evidence, false, Error));
	TestFalse(TEXT("Dirty package evidence is rejected"), Build.AddEntry(TEXT("/Game/B"), TEXT("/Game/B.B"), Evidence, true, Error));
	TestFalse(TEXT("Unknown package evidence is rejected"), Build.AddEntry(TEXT("/Game/C"), TEXT("/Game/C.C"), Evidence, false, Error));
	FRPGIdReferenceIndexCache Cache;
	TestFalse(TEXT("Incomplete catalog cannot be finalized"), Build.Finalize(Cache, Error));

	FRPGIdReferenceIndexBuildAccumulator CompleteBuild(TEXT("workspace"), Catalog, { TEXT("RPGCore") });
	TestTrue(TEXT("First exact package is accepted"),
		CompleteBuild.AddEntry(TEXT("/Game/A"), TEXT("/Game/A.A"), Evidence, false, Error));
	TestTrue(TEXT("Second exact package is accepted"),
		CompleteBuild.AddEntry(TEXT("/Game/B"), TEXT("/Game/B.B"), Evidence, false, Error));
	TestTrue(TEXT("Exact clean catalog finalizes"), CompleteBuild.Finalize(Cache, Error));
	TestEqual(TEXT("Final cache retains exact package count"), Cache.Entries.Num(), 2);
	TestEqual(TEXT("Final cache retains build fingerprint"), Cache.WorkspaceFingerprint, TEXT("workspace"));
	TestTrue(TEXT("Final cache records a persistent build time"), Cache.BuildTime.GetTicks() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceIndexShadowComparisonTest,
	"IronicRPG.RPGId.ReferenceIndex.ShadowComparisonRequiresCanonicalExactEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceIndexShadowComparisonTest::RunTest(const FString& Parameters)
{
	using namespace RPGIdReferenceIndexPrivate;

	FRPGIdReferenceIndexEntry EntryA;
	EntryA.PackageName = TEXT("/Game/A");
	EntryA.AssetPath = TEXT("/Game/A.A");
	EntryA.Evidence.References = {
		{ FRPGId(TEXT("i1000")), TEXT("Blueprint:/Game/A.A"), TEXT("ClassDefaultObject.First") },
		{ FRPGId(TEXT("i2000")), TEXT("Blueprint:/Game/A.A"), TEXT("ClassDefaultObject.Second") }
	};
	EntryA.Evidence.Issues.Add({ RPGIdReferencePrivate::ERPGIdReferenceIssueCode::MalformedBlueprintGraphPin,
		TEXT("/Game/A.A"), TEXT("Graph[EventGraph].Node[Literal].Pin[Id]") });
	FRPGIdReferenceIndexEntry EntryB;
	EntryB.PackageName = TEXT("/Game/B");
	EntryB.AssetPath = TEXT("/Game/B.B");

	const TArray<FRPGIdReferenceIndexEntry> Indexed = { EntryA, EntryB };
	TArray<FRPGIdReferenceIndexEntry> Direct = { EntryB, EntryA };
	Algo::Reverse(Direct[1].Evidence.References);
	FRPGIdReferenceIndexShadowComparison Comparison = CompareBlueprintEvidence(Indexed, Direct);
	TestTrue(TEXT("Direct traversal order is normalized to the canonical index"), Comparison.bExact);

	TArray<FRPGIdReferenceIndexEntry> NonCanonicalIndex = Indexed;
	Algo::Reverse(NonCanonicalIndex);
	Comparison = CompareBlueprintEvidence(NonCanonicalIndex, Direct);
	TestFalse(TEXT("A non-canonical persisted catalog is unhealthy"), Comparison.bExact);
	TestTrue(TEXT("Ordering mismatch names the catalog"), Comparison.Difference.Contains(TEXT("catalog ordering")));

	TArray<FRPGIdReferenceIndexEntry> MissingDirectEntry = Direct;
	MissingDirectEntry.RemoveAt(0);
	Comparison = CompareBlueprintEvidence(Indexed, MissingDirectEntry);
	TestFalse(TEXT("A missing direct catalog entry is unhealthy"), Comparison.bExact);
	TestTrue(TEXT("Catalog mismatch reports counts"), Comparison.Difference.Contains(TEXT("2 indexed")));

	TArray<FRPGIdReferenceIndexEntry> ChangedReference = Direct;
	ChangedReference[1].Evidence.References[0].PropertyPath = TEXT("ClassDefaultObject.Changed");
	Comparison = CompareBlueprintEvidence(Indexed, ChangedReference);
	TestFalse(TEXT("A changed reference path is unhealthy"), Comparison.bExact);
	TestTrue(TEXT("Reference mismatch names its package"), Comparison.Difference.Contains(TEXT("/Game/A")));

	TArray<FRPGIdReferenceIndexEntry> ChangedIssue = Direct;
	ChangedIssue[1].Evidence.Issues[0].Detail = TEXT("Graph[Changed]");
	Comparison = CompareBlueprintEvidence(Indexed, ChangedIssue);
	TestFalse(TEXT("A changed structured issue is unhealthy"), Comparison.bExact);
	TestTrue(TEXT("Issue mismatch is identified"), Comparison.Difference.Contains(TEXT("issue")));
	return true;
}

struct FRPGIdReferenceIndexLifecycleTestState
{
	double StartTime = 0.0;
};

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FWaitForRPGIdReferenceIndexReady,
	TSharedPtr<FRPGIdReferenceIndexLifecycleTestState>, State, FAutomationTestBase*, Test);

bool FWaitForRPGIdReferenceIndexReady::Update()
{
	using namespace RPGIdReferenceIndexPrivate;

	const FRPGIdReferenceIndexStatus Status = FRPGIdBlueprintReferenceIndex::GetStatus();
	if (!Status.IsReady())
	{
		if (FPlatformTime::Seconds() - State->StartTime <= 90.0)
		{
			return false;
		}
		Test->AddError(FString::Printf(TEXT("Timed out waiting for the reference index: state=%d progress=%d/%d reason=%s"),
			static_cast<int32>(Status.State), Status.IndexedPackageCount, Status.TotalPackageCount, *Status.LastFailure));
		return true;
	}

	FRPGIdReferenceIndexCache Cache;
	FString Error;
	Test->TestTrue(TEXT("Ready production index serves an exact query snapshot"),
		FRPGIdBlueprintReferenceIndex::QueryExactSnapshot(Cache, Error));
	Test->TestTrue(TEXT("Exact query has no failure reason"), Error.IsEmpty());
	Cache = FRPGIdReferenceIndexCache();
	Test->TestEqual(TEXT("Ready production cache is readable"),
		LoadCache(FRPGIdBlueprintReferenceIndex::GetCachePath(), Cache, Error), ERPGIdReferenceIndexLoadResult::Ready);
	Test->TestEqual(TEXT("Ready production cache contains the exact catalog"), Cache.Entries.Num(), Status.TotalPackageCount);
	for (const FRPGIdReferenceIndexEntry& Entry : Cache.Entries)
	{
		if (const UPackage* Package = FindPackage(nullptr, *Entry.PackageName.ToString()))
		{
			Test->TestFalse(FString::Printf(TEXT("Index build leaves %s clean"), *Entry.PackageName.ToString()), Package->IsDirty());
		}
	}
	const int32 ShadowComparisonCountBefore = FRPGIdBlueprintReferenceIndex::GetStatus().SuccessfulShadowComparisonCount;
	const auto BeforeDirect = RPGIdReferencePrivate::AuditDevelopmentReferences(FRPGId(TEXT("z1000")), RPGIdReferencePrivate::ERPGIdAuditMode::Indexed);
	Test->TestEqual(TEXT("Index-first query performs no explicit Blueprint loads"), BeforeDirect.BlueprintSynchronousLoads, 0);
	Test->TestTrue(TEXT("Index-first query has no coverage gaps"), BeforeDirect.CoverageGaps.IsEmpty());
	FRPGIdReferenceIndexShadowComparison ExactButOldGeneration;
	ExactButOldGeneration.bExact = true;
	Test->TestFalse(TEXT("A shadow result from another workspace generation is discarded"),
		FRPGIdBlueprintReferenceIndex::RecordShadowComparison(FRPGId(TEXT("i9876")), TEXT("old-fingerprint"),
			ExactButOldGeneration));
	Test->TestEqual(TEXT("Discarded shadow result does not alter current-generation evidence"),
		FRPGIdBlueprintReferenceIndex::GetStatus().SuccessfulShadowComparisonCount, ShadowComparisonCountBefore);
	RPGIdReferencePrivate::AuditDevelopmentReferences(FRPGId(TEXT("i9876")), RPGIdReferencePrivate::ERPGIdAuditMode::Shadow);
	const FRPGIdReferenceIndexStatus StatusAfterShadow = FRPGIdBlueprintReferenceIndex::GetStatus();
	Test->TestTrue(TEXT("Production direct audit keeps an exact index healthy"), StatusAfterShadow.IsReady());
	Test->TestEqual(TEXT("Production direct audit records one exact shadow comparison"),
		StatusAfterShadow.SuccessfulShadowComparisonCount, ShadowComparisonCountBefore + 1);
	using namespace RPGIdReferencePrivate;
	const FRPGId Target(TEXT("z1000"));
	const auto Indexed = AuditDevelopmentReferences(Target, ERPGIdAuditMode::Indexed);
	const auto Direct = AuditDevelopmentReferences(Target, ERPGIdAuditMode::Direct);
	for (int32 Run = 0; Run < 3; ++Run)
	{
		const double Start = FPlatformTime::Seconds();
		const auto Warm = AuditDevelopmentReferences(Target, ERPGIdAuditMode::Indexed);
		const double Milliseconds = (FPlatformTime::Seconds() - Start) * 1000.0;
		Test->AddInfo(FString::Printf(TEXT("Phase 4 warm Check %d: %.1fms; Blueprint loads=%d"), Run + 1, Milliseconds, Warm.BlueprintSynchronousLoads));
		Test->TestTrue(TEXT("Warm Check remains complete"), Warm.CoverageGaps.IsEmpty());
		Test->TestEqual(TEXT("Warm Check does not synchronously load Blueprints"), Warm.BlueprintSynchronousLoads, 0);
		if (FParse::Param(FCommandLine::Get(), TEXT("RPGIdAuditPerfAcceptance")))
		{
			Test->TestTrue(TEXT("Local warm Check budget is 250ms"), Milliseconds <= 250.0);
		}
	}
	Test->TestEqual(TEXT("Indexed Blueprint scan performs no synchronous loads"), Indexed.BlueprintSynchronousLoads, 0);
	Test->TestTrue(TEXT("Indexed audit carries a valid generation"), FRPGIdBlueprintReferenceIndex::IsGenerationCurrent(Indexed.IndexGeneration));
	Test->TestEqual(TEXT("Indexed and direct hit counts match"), Indexed.Hits.Num(), Direct.Hits.Num());
	Test->TestEqual(TEXT("Indexed and direct gap counts match"), Indexed.CoverageGaps.Num(), Direct.CoverageGaps.Num());
	for (int32 Index = 0; Index < FMath::Min(Indexed.Hits.Num(), Direct.Hits.Num()); ++Index)
	{
		Test->TestEqual(TEXT("Exact hit source"), Indexed.Hits[Index].Source, Direct.Hits[Index].Source);
		Test->TestEqual(TEXT("Exact hit path"), Indexed.Hits[Index].PropertyPath, Direct.Hits[Index].PropertyPath);
	}
	// Add an in-memory literal to a cached Blueprint, then remove it without saving content.
	bool bTestedOverlay = false;
	for (const auto& Entry : Cache.Entries)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(FSoftObjectPath(Entry.AssetPath).ResolveObject());
		if (!Blueprint || Blueprint->UbergraphPages.IsEmpty()) { continue; }
		UEdGraph* Graph = Blueprint->UbergraphPages[0];
		bTestedOverlay = true;
		UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph, NAME_None, RF_Transient);
		Graph->Nodes.Add(Node);
		FEdGraphPinType Type;
		Type.PinCategory = UEdGraphSchema_K2::PC_Struct;
		Type.PinSubCategoryObject = FRPGId::StaticStruct();
		UEdGraphPin* Pin = Node->CreatePin(EGPD_Input, Type, TEXT("IndexOverlayLiteral"));
		Pin->DefaultValue = TEXT("(Id=\"i9876\")");
		const bool bWasDirty = Blueprint->GetOutermost()->IsDirty();
		Blueprint->GetOutermost()->SetDirtyFlag(true);
		const auto Overlay = AuditDevelopmentReferences(FRPGId(TEXT("i9876")), ERPGIdAuditMode::Indexed);
		Test->TestTrue(TEXT("Loaded graph replaces saved evidence"), Overlay.Hits.ContainsByPredicate([](const FRPGIdReferenceHit& Hit)
			{ return Hit.PropertyPath.Contains(TEXT("IndexOverlayLiteral")); }));
		Test->TestTrue(TEXT("Dirty overlay fails closed"), !Overlay.CoverageGaps.IsEmpty());
		Graph->Nodes.Remove(Node);
		Blueprint->GetOutermost()->SetDirtyFlag(bWasDirty);
		const auto Removed = AuditDevelopmentReferences(FRPGId(TEXT("i9876")), ERPGIdAuditMode::Indexed);
		Test->TestFalse(TEXT("Removed live literal is not cached"), Removed.Hits.ContainsByPredicate([](const FRPGIdReferenceHit& Hit)
			{ return Hit.PropertyPath.Contains(TEXT("IndexOverlayLiteral")); }));
		const auto SavedStatus = Blueprint->Status;
		Blueprint->Status = BS_Error;
		const auto CompileError = AuditDevelopmentReferences(Target, ERPGIdAuditMode::Indexed);
		Blueprint->Status = SavedStatus;
		Test->TestTrue(TEXT("Compile error has a named coverage gap"), CompileError.CoverageGaps.ContainsByPredicate([Blueprint](const FText& Gap)
			{ return Gap.ToString().Contains(Blueprint->GetPathName()); }));
		Graph->Nodes.Add(Node);
		Pin->DefaultValue = TEXT("not-a-struct");
		const auto Malformed = AuditDevelopmentReferences(Target, ERPGIdAuditMode::Indexed);
		Graph->Nodes.Remove(Node);
		Test->TestTrue(TEXT("Malformed live literal blocks a different target"), !Malformed.CoverageGaps.IsEmpty());
		Test->TestEqual(TEXT("Query preserves package dirty state"), Blueprint->GetOutermost()->IsDirty(), bWasDirty);
		break;
	}
	Test->TestTrue(TEXT("A saved Blueprint was available for the overlay test"), bTestedOverlay);
	for (const auto& Entry : Cache.Entries)
	{
		UObject* LoadedAsset = FSoftObjectPath(Entry.AssetPath).ResolveObject();
		if (!LoadedAsset) { continue; }
		UPackage* Package = LoadedAsset->GetOutermost();
		const FIoHash OriginalHash = Package->GetSavedHash();
		Package->SetSavedHash(FIoHash::HashBuffer("external-replacement", 20));
		FRPGIdReferenceIndexCache Mismatched;
		Test->TestFalse(TEXT("Loaded object from another saved revision cannot seed the index"),
			FRPGIdBlueprintReferenceIndex::QueryExactSnapshot(Mismatched, Error));
		Package->SetSavedHash(OriginalHash);
		Test->TestTrue(TEXT("Saved revision mismatch identifies its Blueprint"), Error.Contains(Entry.PackageName.ToString()));
		break;
	}
	FRPGIdBlueprintReferenceIndex::RequestRebuild();
	FRPGIdBlueprintReferenceIndex::CancelBuild();
	const double NotReadyStart = FPlatformTime::Seconds();
	const auto NotReady = AuditDevelopmentReferences(Target, ERPGIdAuditMode::Indexed);
	const double NotReadyMilliseconds = (FPlatformTime::Seconds() - NotReadyStart) * 1000.0;
	Test->AddInfo(FString::Printf(TEXT("Phase 4 cancelled / NotReady Check: %.1fms"), NotReadyMilliseconds));
	if (FParse::Param(FCommandLine::Get(), TEXT("RPGIdAuditPerfAcceptance")))
	{
		Test->TestTrue(TEXT("Local NotReady budget is 250ms"), NotReadyMilliseconds <= 250.0);
	}
	Test->TestTrue(TEXT("Rebuild invalidates the old generation"), !FRPGIdBlueprintReferenceIndex::IsGenerationCurrent(Indexed.IndexGeneration));
	Test->TestTrue(TEXT("NotReady fails closed"), !NotReady.CoverageGaps.IsEmpty());
	Test->TestEqual(TEXT("NotReady does not load Blueprints"), NotReady.BlueprintSynchronousLoads, 0);
	FRPGIdClaimRequest StaleRequest;
	StaleRequest.ExpectedIndexGeneration = Indexed.IndexGeneration;
	Test->TestEqual(TEXT("Apply refuses a stale Check before mutation"), FRPGIdClaimEditor::Execute(StaleRequest).Code, ERPGIdClaimResult::StaleRequest);
	const uint64 BeforeReload = FRPGIdBlueprintReferenceIndex::GetGeneration();
	FRPGIdBlueprintReferenceIndex::NotifyNativeCodeChanged();
	Test->TestTrue(TEXT("Native reload advances generation"), FRPGIdBlueprintReferenceIndex::GetGeneration() != BeforeReload);
	Test->TestFalse(TEXT("Native reload cannot serve a disk-only snapshot"), FRPGIdBlueprintReferenceIndex::QueryExactSnapshot(Cache, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceIndexProductionLifecycleTest,
	"IronicRPG.RPGId.ReferenceIndex.ProductionBuildPersistsExactCleanCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceIndexProductionLifecycleTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FRPGIdReferenceIndexLifecycleTestState> State = MakeShared<FRPGIdReferenceIndexLifecycleTestState>();
	State->StartTime = FPlatformTime::Seconds();
	FRPGIdBlueprintReferenceIndex::RequestRebuild();
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForRPGIdReferenceIndexReady(State, this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceIndexStartupQueryTest,
	"IronicRPG.RPGId.ReferenceIndex.StartupIndexBeforeDirectScan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceIndexStartupQueryTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FRPGIdReferenceIndexLifecycleTestState> State = MakeShared<FRPGIdReferenceIndexLifecycleTestState>();
	State->StartTime = FPlatformTime::Seconds();
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForRPGIdReferenceIndexReady(State, this));
	return true;
}

#endif

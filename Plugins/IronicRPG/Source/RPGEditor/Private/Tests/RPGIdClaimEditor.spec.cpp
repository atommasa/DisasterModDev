// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "RPGIdClaimEditor.h"
#include "RPGIdCategoryService.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "RPGIdClaimAdapter.h"
#include "RPGIdClaimSnapshot.h"
#include "RPGIdReferenceAudit.h"
#include "SRPGIdClaimEditor.h"

#include "Abilities/AbilityAsset.h"
#include "Assets/RPGAssetManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Characters/CharacterAsset.h"
#include "Characters/CharacterDataTypes.h"
#include "Editor.h"
#include "HAL/IConsoleManager.h"
#include "EditorWorldUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "Items/ItemAsset.h"
#include "Items/LootTable.h"
#include "Levels/GameZoneAsset.h"
#include "Levels/GameZonePointComponent.h"
#include "Levels/MapMarkerTypeAsset.h"
#include "Levels/RPGWorldSettings.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "RPGSettings.h"
#include "Serialization/ObjectWriter.h"
#include "Settings/GameZoneSystemSettings.h"
#include "UObject/Package.h"
#include "UObject/PropertyAccessUtil.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace
{
	template <typename ValueType>
	ValueType& GetPropertyValue(UObject& Object, const FName PropertyName)
	{
		FProperty* Property = FindFProperty<FProperty>(Object.GetClass(), PropertyName);
		check(Property);
		return *Property->ContainerPtrToValuePtr<ValueType>(&Object);
	}

	TUniquePtr<FScopedEditorWorld> CreateClaimTestWorld(const TCHAR* Label)
	{
		const FString Name = FString::Printf(TEXT("ClaimZone_%s_%s"), Label, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* Package = CreatePackage(*(TEXT("/Temp/IronicRPG/") + Name));
		const UWorld::InitializationValues Values = UWorld::InitializationValues()
			.AllowAudioPlayback(false)
			.CreatePhysicsScene(false)
			.CreateNavigation(false)
			.CreateAISystem(false)
			.ShouldSimulatePhysics(false)
			.CreateFXSystem(false);
		UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, FName(*Name), Package, false, ERHIFeatureLevel::Num, &Values, true);
		return MakeUnique<FScopedEditorWorld>(World, Values);
	}

	void SetZoneBinding(UGameZoneAsset& Asset, UWorld& World, const FRPGId& Id, const FGuid& BindingId)
	{
		GetPropertyValue<FRPGId>(Asset, TEXT("Id")) = Id;
		GetPropertyValue<TSoftObjectPtr<UWorld>>(Asset, TEXT("LevelToLoad")) = &World;
		GetPropertyValue<FGuid>(Asset, TEXT("GameZoneBindingId")) = BindingId;
		GetPropertyValue<EGameZoneBindingVerificationStatus>(Asset, TEXT("BindingVerificationStatus")) = EGameZoneBindingVerificationStatus::Verified;
	}

	void SetLevelClaim(ARPGWorldSettings& Settings, const FRPGId& Id, const FGuid& BindingId)
	{
		GetPropertyValue<FRPGId>(Settings, TEXT("GameZoneId")) = Id;
		GetPropertyValue<FGuid>(Settings, TEXT("GameZoneBindingId")) = BindingId;
	}

	struct FClaimFixture
	{
public:
		explicit FClaimFixture(UClass* Class = UItemAsset::StaticClass(), const FString& Root = FString())
		{
			CategoryStore = MakeUnique<FRPGIdCategoryStore>(FPaths::ProjectSavedDir()
				/ (TEXT("CategoryClaim-") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".ini")));
			FString CategoryError;
			CategoryStore->Reload(CategoryError);
			PreviousCategoryStore = FRPGIdCategoryStore::SetTestOverride(CategoryStore.Get());
			// Synchronous mutation fixtures create registry changes without ticking the background index.
			// Keep their scanner/transaction assertions on the direct route; index lifecycle has latent tests.
			AuditMode = IConsoleManager::Get().FindConsoleVariable(TEXT("rpg.IdReferenceAudit.Mode"));
			PreviousAuditMode = AuditMode->GetInt();
			AuditMode->Set(1, ECVF_SetByConsole);
			const URPGPrimaryAsset* CDO = Class->GetDefaultObject<URPGPrimaryAsset>();
			const FString Name = TEXT("ClaimTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			const FString Folder = Root.IsEmpty() ? TEXT("/Game/DataAssets/") + CDO->GetAssetType().ToString() : Root;
			UPackage* Package = CreatePackage(*(Folder / Name));
			Asset.Reset(NewObject<URPGPrimaryAsset>(Package, Class, *Name, RF_Public | RF_Standalone | RF_Transactional));
			FAssetRegistryModule::AssetCreated(Asset.Get());
			Package->SetDirtyFlag(false);
		}

		~FClaimFixture()
		{
			FRPGIdCategoryStore::SetTestOverride(PreviousCategoryStore);
			IFileManager::Get().Delete(*CategoryStore->GetFilename());
			AuditMode->Set(PreviousAuditMode, ECVF_SetByConsole);
			FAssetRegistryModule::AssetDeleted(Asset.Get());
			Asset->ClearFlags(RF_Public | RF_Standalone);
			Asset->SetFlags(RF_Transient);
			Asset->GetOutermost()->SetDirtyFlag(false);
		}

		FRPGIdClaimRequest Request(const FRPGId& Candidate = FRPGId()) const
		{
			return { { Asset.Get() }, Asset->GetId(), Candidate };
		}

		void Seed(FName Id)
		{
			// Simulate pre-existing invalid data or an external writer without using the production writer.
			FProperty* Property = FindFProperty<FProperty>(URPGPrimaryAsset::StaticClass(), TEXT("Id"));
			*Property->ContainerPtrToValuePtr<FRPGId>(Asset.Get()) = FRPGId(Id);
		}

public:
		TStrongObjectPtr<URPGPrimaryAsset> Asset;
		IConsoleVariable* AuditMode = nullptr;
		int32 PreviousAuditMode = 0;
		TUniquePtr<FRPGIdCategoryStore> CategoryStore;
		FRPGIdCategoryStore* PreviousCategoryStore = nullptr;
	};

	FRPGId FreeId(const FClaimFixture& Fixture)
	{
		return FRPGIdClaimEditor::Suggest(Fixture.Request()).SuggestedId;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimCategorySuggestionTest, "IronicRPG.RPGId.Claim.CategorySuggestionAndStaleSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimCategorySuggestionTest::RunTest(const FString& Parameters)
{
	FClaimFixture Occupied;
	FClaimFixture Second;
	FClaimFixture Owner;
	const FRPGId First = FreeId(Owner);
	Occupied.Seed(First.Id);
	const FRPGId Next = FreeId(Owner);
	FRPGIdCategoryDefinition Category;
	Category.CategoryKey = TEXT("TestBand");
	Category.DisplayName = TEXT("Test Band");
	const int32 FirstNumber = FCString::Atoi(*First.Id.ToString().Right(4));
	const int32 NextNumber = FCString::Atoi(*Next.Id.ToString().Right(4));
	Category.Ranges = {{FirstNumber, FirstNumber}, {NextNumber, NextNumber}};
	FRPGIdCategoryRules Rules;
	Rules.AssetTypes.FindOrAdd(TEXT("Item")).Categories.Add(Category);
	FString Error;
	TestTrue(TEXT("Save isolated category"), Owner.CategoryStore->Save(Rules, Owner.CategoryStore->GetRevision(), Error));
	FRPGIdCategorySelection Selection{ERPGIdSuggestionScope::Category, Category.CategoryKey, Owner.CategoryStore->GetRevision()};
	TestEqual(TEXT("Must select a scope"), FRPGIdClaimEditor::Suggest(Owner.Request()).Code, ERPGIdClaimResult::InspectionIncomplete);
	TestEqual(TEXT("Skips occupied number across ranges"), FRPGIdClaimEditor::Suggest(Owner.Request(), &Selection).SuggestedId.Id, Next.Id);
	TestTrue(TEXT("Suggestion does not claim"), Owner.Asset->GetId().Id.IsNone());
	TestFalse(TEXT("Suggestion does not dirty"), Owner.Asset->GetOutermost()->IsDirty());
	Second.Seed(Next.Id);
	const auto Full = FRPGIdClaimEditor::Suggest(Owner.Request(), &Selection);
	TestEqual(TEXT("Full scope does not fall back"), Full.Code, ERPGIdClaimResult::NoAvailableId);
	TestTrue(TEXT("Full scope returns no candidate"), Full.SuggestedId.Id.IsNone());
	Owner.CategoryStore->Reload(Error);
	TestEqual(TEXT("Stale menu is rejected"), FRPGIdClaimEditor::Suggest(Owner.Request(), &Selection).Code,
		ERPGIdClaimResult::InspectionIncomplete);
	TestFalse(TEXT("Failure leaves owner clean"), Owner.Asset->GetOutermost()->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimCategoryIsolationTest, "IronicRPG.RPGId.Claim.CategoryFailureDoesNotBlockManualClaim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimCategoryIsolationTest::RunTest(const FString& Parameters)
{
	FClaimFixture Fixture;
	const FRPGId Candidate = FreeId(Fixture);
	if (!TestTrue(TEXT("Free candidate exists"), Candidate.IsValid())) { return false; }
	const FString Broken = TEXT("[/Script/RPGEditor.RPGIdCategorySettings]\nSchemaVersion=99\nRules=()\n");
	TestTrue(TEXT("Write isolated invalid settings"), FFileHelper::SaveStringToFile(Broken, *Fixture.CategoryStore->GetFilename()));
	FString Error;
	TestFalse(TEXT("Broken category config diagnosed"), Fixture.CategoryStore->Reload(Error));
	TestEqual(TEXT("Category Suggest becomes unavailable"), FRPGIdClaimEditor::Suggest(Fixture.Request()).Code,
		ERPGIdClaimResult::InspectionIncomplete);
	TestEqual(TEXT("Manual candidate still previews"), FRPGIdClaimEditor::Preview(Fixture.Request(Candidate)).Code, ERPGIdClaimResult::Success);
	TestFalse(TEXT("Failed Suggest and Preview leave package clean"), Fixture.Asset->GetOutermost()->IsDirty());
	TestEqual(TEXT("Manual Claim still applies"), FRPGIdClaimEditor::Execute(Fixture.Request(Candidate)).Code, ERPGIdClaimResult::Success);
	TestEqual(TEXT("Manual Claim retains requested identity"), Fixture.Asset->GetId(), Candidate);
	TestEqual(TEXT("Identity inspection remains available"), FRPGIdClaimEditor::Inspect(Fixture.Request().Owners).Code, ERPGIdClaimResult::Success);
	GEditor->UndoTransaction();
	TestTrue(TEXT("Claim Undo works while category config is broken"), Fixture.Asset->GetId().Id.IsNone());
	GEditor->RedoTransaction();
	TestEqual(TEXT("Claim Redo works while category config is broken"), Fixture.Asset->GetId(), Candidate);
	GEditor->UndoTransaction();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimReadOnlyAuditTest, "IronicRPG.RPGId.Claim.ReadOnlyAuditPreservesInvalidAndConflicts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimReadOnlyAuditTest::RunTest(const FString& Parameters)
{
	FClaimFixture A;
	A.Seed(TEXT("invalid-existing-id"));
	TestEqual(TEXT("Invalid is diagnosed"), FRPGIdClaimEditor::Inspect(A.Request().Owners).Code, ERPGIdClaimResult::InvalidFormatOrType);
	TestEqual(TEXT("Invalid is preserved"), A.Asset->GetId().Id, FName(TEXT("invalid-existing-id")));
	TestFalse(TEXT("Audit does not dirty"), A.Asset->GetOutermost()->IsDirty());
	TSharedRef<SRPGIdClaimEditor> Widget = SNew(SRPGIdClaimEditor).Owners(A.Request().Owners);
	TestEqual(TEXT("Opening the widget preserves invalid Id"), A.Asset->GetId().Id, FName(TEXT("invalid-existing-id")));
	TestFalse(TEXT("Opening the widget does not dirty"), A.Asset->GetOutermost()->IsDirty());
	A.Seed(NAME_None);
	const FRPGId Candidate = FreeId(A);
	TestTrue(TEXT("Suggestion available"), Candidate.IsValid());
	A.Seed(Candidate.Id);
	FClaimFixture B;
	B.Seed(Candidate.Id);
	const FRPGIdClaimResult Audit = FRPGIdClaimEditor::Inspect(A.Request().Owners);
	TestEqual(TEXT("Conflict diagnosed"), Audit.Code, ERPGIdClaimResult::Collision);
	TestTrue(TEXT("Other owner located"), Audit.Conflicts.Contains(FSoftObjectPath(B.Asset.Get())));
	TestFalse(TEXT("Self is never a conflict"), Audit.Conflicts.Contains(FSoftObjectPath(A.Asset.Get())));
	TestEqual(TEXT("Conflict preserved"), A.Asset->GetId(), Candidate);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimApplyUndoTest, "IronicRPG.RPGId.Claim.ApplyResolvesAndUndoRedoRefreshes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimApplyUndoTest::RunTest(const FString& Parameters)
{
	FClaimFixture Fixture;
	const FRPGId Candidate = FreeId(Fixture);
	if (!TestTrue(TEXT("Free candidate exists"), Candidate.IsValid())) { return false; }
	const FRPGIdClaimRequest Request = Fixture.Request(Candidate);
	const FRPGIdClaimResult Preview = FRPGIdClaimEditor::Preview(Request);
	AddInfo(Preview.Message.ToString());
	TestEqual(TEXT("Preview succeeds"), Preview.Code, ERPGIdClaimResult::Success);
	TestFalse(TEXT("Preview does not dirty"), Fixture.Asset->GetOutermost()->IsDirty());
	TestTrue(TEXT("Preview does not write"), Fixture.Asset->GetId().Id.IsNone());
	TestEqual(TEXT("Apply succeeds"), FRPGIdClaimEditor::Execute(Request).Code, ERPGIdClaimResult::Success);
	TestEqual(TEXT("Committed candidate"), Fixture.Asset->GetId(), Candidate);
	TestTrue(TEXT("Pending save"), Fixture.Asset->GetOutermost()->IsDirty());
	URPGAssetManager& Manager = URPGAssetManager::Get();
	TestEqual(TEXT("New Id resolves immediately"), Manager.GetPrimaryAssetPath(Candidate.ToPrimaryAssetId()), FSoftObjectPath(Fixture.Asset.Get()));
	TestTrue(TEXT("Undo transaction"), GEditor->UndoTransaction());
	TestTrue(TEXT("Undo restores None"), Fixture.Asset->GetId().Id.IsNone());
	TestTrue(TEXT("Undo removes mapping"), Manager.GetPrimaryAssetPath(Candidate.ToPrimaryAssetId()).IsNull());
	TestTrue(TEXT("Redo transaction"), GEditor->RedoTransaction());
	TestEqual(TEXT("Redo restores Id"), Fixture.Asset->GetId(), Candidate);
	TestEqual(TEXT("Redo refreshes mapping"), Manager.GetPrimaryAssetPath(Candidate.ToPrimaryAssetId()), FSoftObjectPath(Fixture.Asset.Get()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimUndoCollisionPolicyTest, "IronicRPG.RPGId.Claim.UndoCollisionIsPreservedAndInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimUndoCollisionPolicyTest::RunTest(const FString& Parameters)
{
	FClaimFixture A;
	FClaimFixture B;
	const FRPGId Candidate = FreeId(A);
	if (!TestTrue(TEXT("Free candidate exists"), Candidate.IsValid())) { return false; }
	TestEqual(TEXT("A claims the candidate"), FRPGIdClaimEditor::Execute(A.Request(Candidate)).Code, ERPGIdClaimResult::Success);
	TestTrue(TEXT("Undo releases A in memory"), GEditor->UndoTransaction());
	TestTrue(TEXT("A is unclaimed after Undo"), A.Asset->GetId().Id.IsNone());

	// Simulate an external owner appearing without adding a transaction, so Redo remains available.
	B.Seed(Candidate.Id);
	TestTrue(TEXT("Redo restores the complete original transaction"), GEditor->RedoTransaction());
	TestEqual(TEXT("A keeps the restored identity"), A.Asset->GetId(), Candidate);
	TestEqual(TEXT("The competing owner is never rewritten"), B.Asset->GetId(), Candidate);
	const FRPGIdClaimResult Audit = FRPGIdClaimEditor::Inspect(A.Request().Owners);
	TestEqual(TEXT("The restored collision is explicitly invalid"), Audit.Code, ERPGIdClaimResult::Collision);
	TestTrue(TEXT("The competing owner is identified"), Audit.Conflicts.Contains(FSoftObjectPath(B.Asset.Get())));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimNoOpTest, "IronicRPG.RPGId.Claim.NoOpAndAbandonedDraftDoNotWrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimNoOpTest::RunTest(const FString& Parameters)
{
	FClaimFixture Fixture;
	const FRPGId Candidate = FreeId(Fixture);
	FRPGIdClaimEditor::Preview(Fixture.Request(Candidate));
	TestTrue(TEXT("Abandoned preview retains None"), Fixture.Asset->GetId().Id.IsNone());
	TestEqual(TEXT("None to None is no-op"), FRPGIdClaimEditor::Execute(Fixture.Request()).Code, ERPGIdClaimResult::NoChange);
	Fixture.Seed(Candidate.Id);
	TestEqual(TEXT("Same Id is no-op"), FRPGIdClaimEditor::Execute(Fixture.Request(Candidate)).Code, ERPGIdClaimResult::NoChange);
	TestFalse(TEXT("Neither operation dirties the package"), Fixture.Asset->GetOutermost()->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimFormatAndSuggestTest, "IronicRPG.RPGId.Claim.FormatAndNumberBandSuggestion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimFormatAndSuggestTest::RunTest(const FString& Parameters)
{
	FClaimFixture Fixture;
	for (const TCHAR* BadId : { TEXT("c1000"), TEXT("i100"), TEXT("i10000"), TEXT("i1X00"), TEXT("i-100") })
	{
		TestEqual(BadId, FRPGIdClaimEditor::Execute(Fixture.Request(FRPGId(BadId))).Code, ERPGIdClaimResult::InvalidFormatOrType);
	}
	const FRPGIdClaimResult Suggestion = FRPGIdClaimEditor::Suggest(Fixture.Request(FRPGId(TEXT("i1000"))));
	TestEqual(TEXT("Optional suggestion succeeds"), Suggestion.Code, ERPGIdClaimResult::Success);
	TestEqual(TEXT("Suggested Id has the configured digit count"), Suggestion.SuggestedId.ToString().Len(), 5);
	TestEqual(TEXT("Suggested Id rebuilds to the native type"), Suggestion.SuggestedId.GetRebuiltIdType(), UItemAsset::GetAssetTypeStatic());
	TestEqual(TEXT("Draft does not constrain the minimum"), Suggestion.SuggestedId, FRPGIdClaimEditor::Suggest(Fixture.Request()).SuggestedId);
	FClaimFixture Occupied;
	Occupied.Seed(TEXT("i9999"));
	TestEqual(TEXT("High occupied draft still searches complete scope"),
		FRPGIdClaimEditor::Suggest(Fixture.Request(FRPGId(TEXT("i9999")))).SuggestedId, Suggestion.SuggestedId);
	TestTrue(TEXT("Suggestion never commits"), Fixture.Asset->GetId().Id.IsNone());
	TestFalse(TEXT("Suggestion never dirties"), Fixture.Asset->GetOutermost()->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimCompetingDraftsTest, "IronicRPG.RPGId.Claim.CompetingAndStaleDraftsRecheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimCompetingDraftsTest::RunTest(const FString& Parameters)
{
	FClaimFixture A;
	FClaimFixture B;
	const FRPGId Candidate = FreeId(A);
	const FRPGIdClaimRequest RequestA = A.Request(Candidate);
	const FRPGIdClaimRequest RequestB = B.Request(Candidate);
	TestTrue(TEXT("A preview"), FRPGIdClaimEditor::Preview(RequestA).CanApply());
	TestTrue(TEXT("B preview"), FRPGIdClaimEditor::Preview(RequestB).CanApply());
	TestEqual(TEXT("A wins"), FRPGIdClaimEditor::Execute(RequestA).Code, ERPGIdClaimResult::Success);
	TestEqual(TEXT("B is refused at commit"), FRPGIdClaimEditor::Execute(RequestB).Code, ERPGIdClaimResult::Collision);
	TestTrue(TEXT("B remains unclaimed"), B.Asset->GetId().Id.IsNone());
	B.Seed(TEXT("i.ChangedExternally"));
	TestEqual(TEXT("Expected old Id is checked"), FRPGIdClaimEditor::Execute(RequestB).Code, ERPGIdClaimResult::StaleRequest);
	TestEqual(TEXT("External value retained"), B.Asset->GetId().Id, FName(TEXT("i.ChangedExternally")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimReferencedMutationTest, "IronicRPG.RPGId.Claim.ReferencedChangeAndClearFailClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimReferencedMutationTest::RunTest(const FString& Parameters)
{
	FClaimFixture Fixture(UAbilityAsset::StaticClass());
	FClaimFixture Reference(UCharacterAsset::StaticClass());
	const FRPGId OldId = FreeId(Fixture);
	Fixture.Seed(OldId.Id);
	GetPropertyValue<FCharacterSaveData>(*Reference.Asset, TEXT("DefaultData")).EquippedAbilities.Add(FGameplayTag(), OldId);
	for (const FRPGId& Candidate : { FRPGId(TEXT("a1234")), FRPGId() })
	{
		const FRPGIdClaimResult Check = FRPGIdClaimEditor::Preview(Fixture.Request(Candidate));
		TestEqual(TEXT("Change/Clear blocked"), Check.Code, ERPGIdClaimResult::ReferencesUnsupported);
		TestTrue(TEXT("Typed scope is reported"), Check.Context.ToString().Contains(TEXT("native RPGPrimaryAsset")));
		TestTrue(TEXT("Stored reference is explicit"), Check.Context.ToString().Contains(TEXT("DefaultData.EquippedAbilities")));
		TestTrue(TEXT("Old saves are outside the guarantee"), Check.Context.ToString().Contains(TEXT("outside")));
		TestEqual(TEXT("Execute preserves the same fail-closed result"), FRPGIdClaimEditor::Execute(Fixture.Request(Candidate)).Code,
			ERPGIdClaimResult::ReferencesUnsupported);
	}
	TestEqual(TEXT("Original retained"), Fixture.Asset->GetId(), OldId);
	TestFalse(TEXT("Failure leaves package clean"), Fixture.Asset->GetOutermost()->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimReferenceContainerAuditTest, "IronicRPG.RPGId.Claim.ReferenceAuditFindsNestedContainersWithoutClaims",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimReferenceContainerAuditTest::RunTest(const FString& Parameters)
{
	const FRPGId Target(TEXT("i4321"));
	FLootTable LootTable;
	FLootData& Loot = LootTable.LootItems.AddDefaulted_GetRef();
	Loot.ItemId = Target;
	TArray<FRPGIdReferenceHit> Hits;
	RPGIdReferencePrivate::ScanStruct(&LootTable, *FLootTable::StaticStruct(), TEXT("ArrayFixture"), FString(), Target, Hits);
	TestEqual(TEXT("Nested array/struct reference is found"), Hits.Num(), 1);
	TestTrue(TEXT("Nested property path is retained"), Hits[0].PropertyPath.Contains(TEXT("LootItems[0].ItemId")));

	FCharacterSaveData CharacterData;
	CharacterData.EquippedAbilities.Add(FGameplayTag(), Target);
	Hits.Reset();
	RPGIdReferencePrivate::ScanStruct(&CharacterData, *FCharacterSaveData::StaticStruct(), TEXT("MapFixture"), FString(), Target, Hits);
	TestEqual(TEXT("Map value reference is found"), Hits.Num(), 1);
	TestTrue(TEXT("Map value path is retained"), Hits[0].PropertyPath.Contains(TEXT("EquippedAbilities{0}.Value")));

	FClaimFixture Owner;
	Owner.Seed(Target.Id);
	Hits.Reset();
	RPGIdReferencePrivate::ScanStruct(Owner.Asset.Get(), *Owner.Asset->GetClass(), TEXT("OwnerFixture"), FString(), Target, Hits);
	TestTrue(TEXT("The owner's IdClaim is excluded from reference hits"), Hits.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimLevelReferenceAuditTest, "IronicRPG.RPGId.Claim.LevelReferenceAuditFindsActorsAndComponentsReadOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimLevelReferenceAuditTest::RunTest(const FString& Parameters)
{
	TUniquePtr<FScopedEditorWorld> WorldScope = CreateClaimTestWorld(TEXT("ReferenceAudit"));
	UWorld* World = WorldScope->GetWorld();
	ARPGWorldSettings* Settings = CastChecked<ARPGWorldSettings>(World->GetWorldSettings());
	const FRPGId Target(TEXT("z8123"));
	GetPropertyValue<FRPGId>(*Settings, TEXT("GameZoneId")) = Target;
	AActor* Actor = World->SpawnActor<AActor>();
	UGameZonePointComponent* Component = NewObject<UGameZonePointComponent>(Actor, TEXT("ClaimReferencePoint"));
	Actor->AddInstanceComponent(Component);
	GetPropertyValue<FGameZonePointData>(*Component, TEXT("PointData")).ReferenceAssetId = Target;
	World->GetOutermost()->SetDirtyFlag(false);

	TArray<FRPGIdReferenceHit> Hits;
	TArray<FText> CoverageGaps;
	RPGIdReferencePrivate::ScanWorld(*World, Target, Hits, CoverageGaps);
	const bool bFoundWorldSettings = Hits.ContainsByPredicate([](const FRPGIdReferenceHit& Hit)
	{
		return Hit.PropertyPath == TEXT("GameZoneId");
	});
	const bool bFoundPointComponent = Hits.ContainsByPredicate([](const FRPGIdReferenceHit& Hit)
	{
		return Hit.PropertyPath == TEXT("PointData.ReferenceAssetId") && Hit.Source.Contains(TEXT("ClaimReferencePoint"));
	});
	TestTrue(TEXT("World Settings reference is found"), bFoundWorldSettings);
	TestTrue(TEXT("Actor component reference is found"), bFoundPointComponent);
	TestTrue(TEXT("Non-World-Partition fixture has complete Level coverage"), CoverageGaps.IsEmpty());
	TestFalse(TEXT("Read-only Level scan does not dirty the package"), World->GetOutermost()->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimExternalDataLayerCoverageTest,
	"IronicRPG.RPGId.Claim.ExternalDataLayerCoverageClosesOnlyAfterEveryPackageIsScanned",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimExternalDataLayerCoverageTest::RunTest(const FString& Parameters)
{
	const FName RegisteredPackage(TEXT("/Game/__ExternalActors__/EDL/11111111/NewWorld/A"));
	const FName UnregisteredPackage(TEXT("/Game/__ExternalActors__/EDL/22222222/NewWorld/B"));
	const TSet<FName> OnDiskPackages = { RegisteredPackage, UnregisteredPackage };
	TArray<FText> CoverageGaps;
	RPGIdReferencePrivate::AppendExternalDataLayerCoverageGaps(OnDiskPackages, { RegisteredPackage }, CoverageGaps);
	TestEqual(TEXT("Only the unregistered package remains a gap"), CoverageGaps.Num(), 1);
	TestTrue(TEXT("The gap identifies the exact package"), CoverageGaps[0].ToString().Contains(UnregisteredPackage.ToString()));

	CoverageGaps.Reset();
	RPGIdReferencePrivate::AppendExternalDataLayerCoverageGaps(OnDiskPackages, OnDiskPackages, CoverageGaps);
	TestTrue(TEXT("Fully scanned EDL packages close the EDL gap"), CoverageGaps.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimLegacyConfigReferenceAuditTest,
	"IronicRPG.RPGId.Claim.LegacyConfigReferencesAreTypedAndMalformedEntriesFailClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimLegacyConfigReferenceAuditTest::RunTest(const FString& Parameters)
{
	FConfigSection Section;
	Section.Add(TEXT("PlayableCharacters"), FConfigValue(TEXT("(Id=\"c4321\")")));
	Section.Add(TEXT("DefaultPartyMembers"), FConfigValue(TEXT("(Id=\"c4321\")")));
	Section.Add(TEXT("DefaultPartyMembers"), FConfigValue(TEXT("not-a-struct")));
	Section.Add(TEXT("DefaultGameZoneContext"), FConfigValue(TEXT("(ZoneId=(Id=\"c4321\"),EntryId=LegacyShape)")));
	FConfigFile Config;
	Config.Add(TEXT("/Script/RPGCore.RPGSettings"), MoveTemp(Section));

	TArray<FRPGIdReferenceHit> Hits;
	TArray<FText> CoverageGaps;
	RPGIdReferencePrivate::ScanLegacyConfig(Config, FRPGId(TEXT("c4321")), Hits, CoverageGaps);
	TestEqual(TEXT("Every valid known legacy reference is found"), Hits.Num(), 3);
	TestTrue(TEXT("Array paths identify their legacy keys"), Hits.ContainsByPredicate([](const FRPGIdReferenceHit& Hit)
	{
		return Hit.PropertyPath == TEXT("PlayableCharacters[0]");
	}));
	TestTrue(TEXT("The old GameZoneContext shape exposes ZoneId without importing retired members"), Hits.ContainsByPredicate([](const FRPGIdReferenceHit& Hit)
	{
		return Hit.PropertyPath == TEXT("DefaultGameZoneContext.ZoneId");
	}));
	TestEqual(TEXT("Malformed known entries retain a fail-closed gap"), CoverageGaps.Num(), 1);
	TestTrue(TEXT("The malformed entry is precisely identified"), CoverageGaps[0].ToString().Contains(TEXT("DefaultPartyMembers[1]")));

	const FRPGIdReferenceAuditResult ProjectAudit = RPGIdReferencePrivate::AuditDevelopmentReferences(
		FRPGId(TEXT("c0000")), RPGIdReferencePrivate::ERPGIdAuditMode::Direct);
	TestTrue(TEXT("The production audit reads the project legacy config hierarchy"), ProjectAudit.Hits.ContainsByPredicate([](const FRPGIdReferenceHit& Hit)
	{
		return Hit.Source == TEXT("LegacyConfig:/Script/RPGCore.RPGSettings") && Hit.PropertyPath == TEXT("PlayableCharacters[0]");
	}));
	TestFalse(TEXT("A loaded and parseable legacy hierarchy has no legacy coverage gap"),
		ProjectAudit.CoverageGaps.ContainsByPredicate([](const FText& Gap)
		{
			return Gap.ToString().Contains(TEXT("legacy Id entries were not inspected"));
		}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimBlueprintDefaultReferenceAuditTest,
	"IronicRPG.RPGId.Claim.BlueprintDefaultsAndTemplatesAreAuditedReadOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimBlueprintDefaultReferenceAuditTest::RunTest(const FString& Parameters)
{
	const FRPGId Target(TEXT("mm4321"));
	const FRPGId OtherTarget(TEXT("mm4322"));
	const FString Name = TEXT("BP_ClaimReference_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	UPackage* Package = CreatePackage(*(TEXT("/Game/ClaimTests/") + Name));
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, *Name, BPTYPE_Normal,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("RPGIdBlueprintReferenceAuditTest"));
	if (!TestNotNull(TEXT("Blueprint reference fixture is created"), Blueprint))
	{
		return false;
	}
	FAssetRegistryModule::AssetCreated(Blueprint);

	FEdGraphPinType IdType;
	IdType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	IdType.PinSubCategoryObject = FRPGId::StaticStruct();
	FString DefaultValue;
	FRPGId::StaticStruct()->ExportText(DefaultValue, &Target, nullptr, nullptr, PPF_None, nullptr);
	TestTrue(TEXT("Blueprint FRPGId member is added"),
		FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("ReferencedId"), IdType, DefaultValue));

	USCS_Node* PointNode = Blueprint->SimpleConstructionScript->CreateNode(UGameZonePointComponent::StaticClass(), TEXT("ReferencePoint"));
	Blueprint->SimpleConstructionScript->AddNode(PointNode);
	UGameZonePointComponent* PointTemplate = CastChecked<UGameZonePointComponent>(PointNode->ComponentTemplate);
	GetPropertyValue<FGameZonePointData>(*PointTemplate, TEXT("PointData")).MarkerTypeId = Target;
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	Package->SetDirtyFlag(false);
	TArray<FRPGIdReferenceHit> DirectHits;
	TArray<FText> DirectGaps;
	RPGIdReferencePrivate::ScanBlueprint(*Blueprint, Target, DirectHits, DirectGaps);
	TestTrue(TEXT("An up-to-date Blueprint has complete default and template coverage"), DirectGaps.IsEmpty());
	const TEnumAsByte<EBlueprintStatus> PreviousStatus = Blueprint->Status;
	Blueprint->Status = BS_Dirty;
	DirectGaps.Reset();
	RPGIdReferencePrivate::ScanBlueprint(*Blueprint, Target, DirectHits, DirectGaps);
	TestTrue(TEXT("A dirty Blueprint produces a named fail-closed gap"), DirectGaps.ContainsByPredicate([Blueprint](const FText& Gap)
	{
		return Gap.ToString().Contains(FSoftObjectPath(Blueprint).ToString());
	}));
	Blueprint->Status = PreviousStatus;

	UEdGraph* Graph = Blueprint->UbergraphPages.IsEmpty() ? nullptr : Blueprint->UbergraphPages[0];
	if (!TestNotNull(TEXT("Blueprint event graph is available"), Graph))
	{
		return false;
	}
	UEdGraphNode* LiteralNode = NewObject<UEdGraphNode>(Graph, TEXT("LiteralReferenceNode"));
	Graph->AddNode(LiteralNode, false, false);
	UEdGraphPin* LiteralPin = LiteralNode->CreatePin(EGPD_Input, IdType, TEXT("LiteralId"));
	LiteralPin->DefaultValue = DefaultValue;
	FString OtherDefaultValue;
	FRPGId::StaticStruct()->ExportText(OtherDefaultValue, &OtherTarget, nullptr, nullptr, PPF_None, nullptr);
	UEdGraphNode* OtherLiteralNode = NewObject<UEdGraphNode>(Graph, TEXT("OtherLiteralReferenceNode"));
	Graph->AddNode(OtherLiteralNode, false, false);
	UEdGraphPin* OtherLiteralPin = OtherLiteralNode->CreatePin(EGPD_Input, IdType, TEXT("OtherLiteralId"));
	OtherLiteralPin->DefaultValue = OtherDefaultValue;

	UEdGraphNode* SplitNode = NewObject<UEdGraphNode>(Graph, TEXT("SplitReferenceNode"));
	Graph->AddNode(SplitNode, false, false);
	UEdGraphPin* SplitParent = SplitNode->CreatePin(EGPD_Input, IdType, TEXT("SplitId"));
	UEdGraphPin* SplitIdPin = SplitNode->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Name, TEXT("SplitId_Id"));
	SplitParent->SubPins.Add(SplitIdPin);
	SplitIdPin->ParentPin = SplitParent;
	SplitIdPin->DefaultValue = Target.ToString();

	UEdGraphNode* ConnectedNode = NewObject<UEdGraphNode>(Graph, TEXT("ConnectedReferenceNode"));
	Graph->AddNode(ConnectedNode, false, false);
	UEdGraphPin* ConnectedPin = ConnectedNode->CreatePin(EGPD_Input, IdType, TEXT("ConnectedId"));
	ConnectedPin->DefaultValue = DefaultValue;
	UEdGraphNode* SourceNode = NewObject<UEdGraphNode>(Graph, TEXT("RuntimeSourceNode"));
	Graph->AddNode(SourceNode, false, false);
	UEdGraphPin* SourcePin = SourceNode->CreatePin(EGPD_Output, IdType, TEXT("RuntimeId"));
	ConnectedPin->MakeLinkTo(SourcePin);

	UEdGraphNode* MalformedNode = NewObject<UEdGraphNode>(Graph, TEXT("MalformedReferenceNode"));
	Graph->AddNode(MalformedNode, false, false);
	UEdGraphPin* MalformedPin = MalformedNode->CreatePin(EGPD_Input, IdType, TEXT("MalformedId"));
	MalformedPin->DefaultValue = TEXT("(Id=\"");
	Package->SetDirtyFlag(false);
	const RPGIdReferencePrivate::FRPGIdBlueprintEvidence Evidence = RPGIdReferencePrivate::ExtractBlueprintEvidence(*Blueprint);
	TestTrue(TEXT("Blueprint evidence retains the primary target independently of a query"),
		Evidence.References.ContainsByPredicate([&Target](const RPGIdReferencePrivate::FRPGIdReferenceRecord& Record)
		{
			return Record.Id == Target;
		}));
	TestTrue(TEXT("Blueprint evidence retains another target in the same extraction"),
		Evidence.References.ContainsByPredicate([&OtherTarget](const RPGIdReferencePrivate::FRPGIdReferenceRecord& Record)
		{
			return Record.Id == OtherTarget;
		}));
	TArray<FRPGIdReferenceHit> ExtractedTargetHits;
	TArray<FText> ExtractedTargetGaps;
	RPGIdReferencePrivate::AppendBlueprintEvidence(Evidence, Target, ExtractedTargetHits, ExtractedTargetGaps);
	TArray<FRPGIdReferenceHit> TargetedTargetHits;
	TArray<FText> TargetedTargetGaps;
	RPGIdReferencePrivate::ScanBlueprint(*Blueprint, Target, TargetedTargetHits, TargetedTargetGaps);
	TestEqual(TEXT("Target-independent evidence reproduces the targeted hit count"), ExtractedTargetHits.Num(), TargetedTargetHits.Num());
	TestEqual(TEXT("Target-independent evidence reproduces the targeted gap count"), ExtractedTargetGaps.Num(), TargetedTargetGaps.Num());
	for (int32 Index = 0; Index < FMath::Min(ExtractedTargetHits.Num(), TargetedTargetHits.Num()); ++Index)
	{
		TestEqual(TEXT("Target-independent evidence preserves each hit source"), ExtractedTargetHits[Index].Source,
			TargetedTargetHits[Index].Source);
		TestEqual(TEXT("Target-independent evidence preserves each hit property path"), ExtractedTargetHits[Index].PropertyPath,
			TargetedTargetHits[Index].PropertyPath);
	}
	for (int32 Index = 0; Index < FMath::Min(ExtractedTargetGaps.Num(), TargetedTargetGaps.Num()); ++Index)
	{
		TestEqual(TEXT("Target-independent evidence preserves each fail-closed gap"), ExtractedTargetGaps[Index].ToString(),
			TargetedTargetGaps[Index].ToString());
	}
	TArray<FRPGIdReferenceHit> ExtractedOtherHits;
	TArray<FText> ExtractedOtherGaps;
	RPGIdReferencePrivate::AppendBlueprintEvidence(Evidence, OtherTarget, ExtractedOtherHits, ExtractedOtherGaps);
	TestEqual(TEXT("A second target is filtered without extracting the Blueprint again"), ExtractedOtherHits.Num(), 1);
	TestTrue(TEXT("The second target retains its exact graph path"), ExtractedOtherHits.ContainsByPredicate([](const FRPGIdReferenceHit& Hit)
	{
		return Hit.PropertyPath.Contains(TEXT("OtherLiteralReferenceNode")) && Hit.PropertyPath.EndsWith(TEXT("Pin[OtherLiteralId]"));
	}));
	TestTrue(TEXT("Malformed evidence remains fail closed for every target"), ExtractedOtherGaps.ContainsByPredicate([](const FText& Gap)
	{
		return Gap.ToString().Contains(TEXT("MalformedReferenceNode")) && Gap.ToString().Contains(TEXT("MalformedId"));
	}));

	const FRPGIdReferenceAuditResult Audit = RPGIdReferencePrivate::AuditDevelopmentReferences(Target, RPGIdReferencePrivate::ERPGIdAuditMode::Direct);
	const FString Source = TEXT("Blueprint:") + FSoftObjectPath(Blueprint).ToString();
	TestTrue(TEXT("Blueprint CDO member default is found"), Audit.Hits.ContainsByPredicate([&Source](const FRPGIdReferenceHit& Hit)
	{
		return Hit.Source == Source && Hit.PropertyPath == TEXT("ClassDefaultObject.ReferencedId");
	}));
	TestTrue(TEXT("Blueprint component template reference is found"), Audit.Hits.ContainsByPredicate([&Source](const FRPGIdReferenceHit& Hit)
	{
		return Hit.Source == Source && Hit.PropertyPath.Contains(TEXT("ReferencePoint"))
			&& Hit.PropertyPath.EndsWith(TEXT("PointData.MarkerTypeId"));
	}));
	TestTrue(TEXT("Unconnected FRPGId graph literal is found"), Audit.Hits.ContainsByPredicate([&Source](const FRPGIdReferenceHit& Hit)
	{
		return Hit.Source == Source && Hit.PropertyPath.Contains(TEXT("LiteralReferenceNode"))
			&& Hit.PropertyPath.EndsWith(TEXT("Pin[LiteralId]"));
	}));
	TestTrue(TEXT("Split FRPGId graph literal is found"), Audit.Hits.ContainsByPredicate([&Source](const FRPGIdReferenceHit& Hit)
	{
		return Hit.Source == Source && Hit.PropertyPath.Contains(TEXT("SplitReferenceNode"))
			&& Hit.PropertyPath.EndsWith(TEXT("Pin[SplitId.Id]"));
	}));
	TestFalse(TEXT("Connected FRPGId pin is outside static literal coverage"), Audit.Hits.ContainsByPredicate([](const FRPGIdReferenceHit& Hit)
	{
		return Hit.PropertyPath.Contains(TEXT("ConnectedReferenceNode"));
	}));
	TestTrue(TEXT("Malformed unconnected FRPGId literal fails closed with a named pin"),
		Audit.CoverageGaps.ContainsByPredicate([](const FText& Gap)
		{
			return Gap.ToString().Contains(TEXT("MalformedReferenceNode")) && Gap.ToString().Contains(TEXT("MalformedId"));
		}));
	TestFalse(TEXT("Successful graph enumeration removes the global graph pin gap"),
		Audit.CoverageGaps.ContainsByPredicate([](const FText& Gap)
		{
			return Gap.ToString().Contains(TEXT("graph pin constants"));
		}));
	TestFalse(TEXT("The retired combined Blueprint gap is removed"), Audit.CoverageGaps.ContainsByPredicate([](const FText& Gap)
	{
		return Gap.ToString().Contains(TEXT("Blueprint defaults, CDO values"));
	}));
	TestFalse(TEXT("Blueprint audit remains read-only"), Package->IsDirty());

	FAssetRegistryModule::AssetDeleted(Blueprint);
	Blueprint->ClearFlags(RF_Public | RF_Standalone);
	Blueprint->SetFlags(RF_Transient);
	Package->SetDirtyFlag(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimOwnerGuardsTest, "IronicRPG.RPGId.Claim.OwnerAndSettingsGuards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimOwnerGuardsTest::RunTest(const FString& Parameters)
{
	FClaimFixture Fixture;
	FRPGIdClaimRequest Multi = Fixture.Request(FRPGId(TEXT("i1000")));
	Multi.Owners.Add(Fixture.Asset.Get());
	TestEqual(TEXT("Multi-selection refused"), FRPGIdClaimEditor::Execute(Multi).Code, ERPGIdClaimResult::UnsupportedOwner);
	TestEqual(TEXT("No owner refused"), FRPGIdClaimEditor::Execute({}).Code, ERPGIdClaimResult::UnsupportedOwner);
	FClaimFixture Outside(UItemAsset::StaticClass(), TEXT("/Game/ClaimTestsOutside"));
	TestEqual(TEXT("Outside path cannot edit"), FRPGIdClaimEditor::Execute(Outside.Request(FRPGId(TEXT("i1000")))).Code,
		ERPGIdClaimResult::UnsupportedOwner);
	Outside.Seed(TEXT("i1000"));
	TestEqual(TEXT("Outside path still collides"), FRPGIdClaimEditor::Preview(Fixture.Request(FRPGId(TEXT("i1000")))).Code,
		ERPGIdClaimResult::Collision);
	FClaimFixture Zone(UGameZoneAsset::StaticClass());
	const FRPGId ZoneCandidate = FreeId(Zone);
	TestEqual(TEXT("Unbound Zone uses the coordinator adapter"), FRPGIdClaimEditor::Execute(Zone.Request(ZoneCandidate)).Code,
		ERPGIdClaimResult::Success);
	{
		TGuardValue<bool> Playing(GIsPlayInEditorWorld, true);
		TestEqual(TEXT("Play refused"), FRPGIdClaimEditor::Execute(Fixture.Request(FRPGId(TEXT("i1000")))).Code, ERPGIdClaimResult::NotEditable);
	}
	{
		TGuardValue<int32> InvalidLength(GetMutableDefault<URPGSettings>()->NumericLen, 0);
		TestEqual(TEXT("Invalid config refused"), FRPGIdClaimEditor::Execute(Fixture.Request(FRPGId(TEXT("i1000")))).Code,
			ERPGIdClaimResult::InspectionIncomplete);
	}
	TestFalse(TEXT("All guard failures preserve clean state"), Fixture.Asset->GetOutermost()->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimReflectionProtectionTest, "IronicRPG.RPGId.Claim.ReflectionReadOnlyDoesNotLockReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimReflectionProtectionTest::RunTest(const FString& Parameters)
{
	FClaimFixture Fixture;
	const FProperty* Claim = FindFProperty<FProperty>(URPGPrimaryAsset::StaticClass(), TEXT("Id"));
	const FProperty* Value = FindFProperty<FProperty>(FRPGId::StaticStruct(), TEXT("Id"));
	TestTrue(TEXT("Claim has engine read-only flag"), Claim->HasAnyPropertyFlags(CPF_EditConst));
	TestFalse(TEXT("Reference value stays writable"), Value->HasAnyPropertyFlags(CPF_EditConst));
	const auto Access = PropertyAccessUtil::CanSetPropertyValue(Claim, PropertyAccessUtil::EditorReadOnlyFlags,
		PropertyAccessUtil::IsObjectTemplate(Fixture.Asset.Get()));
	TestTrue(TEXT("Editor script access rejects Claim"), EnumHasAnyFlags(Access, EPropertyAccessResultFlags::ReadOnly));
	const FRPGId Candidate(TEXT("i7777"));
	const auto Write = PropertyAccessUtil::SetPropertyValue_Object(Claim, Fixture.Asset.Get(), Claim, &Candidate, INDEX_NONE,
		PropertyAccessUtil::EditorReadOnlyFlags, EPropertyAccessChangeNotifyMode::Default);
	TestTrue(TEXT("Script-style whole struct write rejected"), EnumHasAnyFlags(Write, EPropertyAccessResultFlags::ReadOnly));
	TestTrue(TEXT("Owner retained"), Fixture.Asset->GetId().Id.IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimNativeTypesTest, "IronicRPG.RPGId.Claim.FourOrdinaryNativeAdapters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimNativeTypesTest::RunTest(const FString& Parameters)
{
	for (UClass* Class : { UItemAsset::StaticClass(), UCharacterAsset::StaticClass(), UAbilityAsset::StaticClass(),
		UMapMarkerTypeAsset::StaticClass() })
	{
		FClaimFixture Fixture(Class);
		const FRPGId Candidate = FreeId(Fixture);
		TestTrue(*Class->GetName(), Candidate.IsValid());
		const FRPGIdClaimAdapter* Adapter = RPGIdClaimPrivate::FindAdapter(Class);
		if (!TestNotNull(TEXT("Ordinary adapter registered"), Adapter)) { return false; }
		TestTrue(TEXT("Ordinary adapter participates in pending reservations"), Adapter->UsesPendingReservations());
		TestEqual(TEXT("Ordinary references cannot be exempted"), Adapter->ClassifyReference(*Fixture.Asset,
			{ FSoftObjectPath(Fixture.Asset.Get()).ToString(), TEXT("BakedPoints{0}.Value.ZoneId") }), ERPGIdReferenceCapability::Blocking);
		TestEqual(TEXT("Native Claim applies"), FRPGIdClaimEditor::Execute(Fixture.Request(Candidate)).Code, ERPGIdClaimResult::Success);
		TestTrue(TEXT("Ordinary existing Change is available"), FRPGIdClaimEditor::SupportsExistingChange(Fixture.Request().Owners));
		Fixture.Asset->GetOutermost()->SetDirtyFlag(false);
		const FRPGIdClaimResult Suggestion = FRPGIdClaimEditor::Suggest(Fixture.Request(Candidate));
		if (!TestEqual(TEXT("Existing owner can request a new candidate"), Suggestion.Code, ERPGIdClaimResult::Success)) { return false; }
		const FRPGIdClaimResult ChangePreview = FRPGIdClaimEditor::Preview(Fixture.Request(Suggestion.SuggestedId));
		TestEqual(TEXT("Zero-reference Change previews successfully"), ChangePreview.Code, ERPGIdClaimResult::Success);
		TestTrue(TEXT("Change preview names Pending Save"), ChangePreview.Message.ToString().Contains(TEXT("Pending Save")));
		TestEqual(TEXT("Zero-reference Change applies"), FRPGIdClaimEditor::Execute(Fixture.Request(Suggestion.SuggestedId)).Code,
			ERPGIdClaimResult::Success);
		TestEqual(TEXT("Change writes the candidate"), Fixture.Asset->GetId(), Suggestion.SuggestedId);
		TestTrue(TEXT("Change removes the old AssetManager mapping"),
			URPGAssetManager::Get().GetPrimaryAssetPath(Candidate.ToPrimaryAssetId()).IsNull());
		TestEqual(TEXT("Change registers the new AssetManager mapping"),
			URPGAssetManager::Get().GetPrimaryAssetPath(Suggestion.SuggestedId.ToPrimaryAssetId()), FSoftObjectPath(Fixture.Asset.Get()));
		TestTrue(TEXT("Change leaves the owner pending save"), Fixture.Asset->GetOutermost()->IsDirty());
		TestTrue(TEXT("Ordinary Change can be undone"), GEditor->UndoTransaction());
		TestEqual(TEXT("Undo restores the old Id"), Fixture.Asset->GetId(), Candidate);
		TestEqual(TEXT("Undo restores the old AssetManager mapping"),
			URPGAssetManager::Get().GetPrimaryAssetPath(Candidate.ToPrimaryAssetId()), FSoftObjectPath(Fixture.Asset.Get()));
		TestTrue(TEXT("Ordinary Change can be redone"), GEditor->RedoTransaction());
		TestEqual(TEXT("Redo restores the new Id"), Fixture.Asset->GetId(), Suggestion.SuggestedId);
		TestEqual(TEXT("Redo restores the new AssetManager mapping"),
			URPGAssetManager::Get().GetPrimaryAssetPath(Suggestion.SuggestedId.ToPrimaryAssetId()), FSoftObjectPath(Fixture.Asset.Get()));
		Fixture.Asset->GetOutermost()->SetDirtyFlag(false);
		const FRPGIdClaimResult ClearPreview = FRPGIdClaimEditor::Preview(Fixture.Request());
		TestEqual(TEXT("Zero-reference Clear previews successfully"), ClearPreview.Code, ERPGIdClaimResult::Success);
		TestTrue(TEXT("Preview names Pending Save"), ClearPreview.Message.ToString().Contains(TEXT("Pending Save")));
		TestEqual(TEXT("Zero-reference Clear applies"), FRPGIdClaimEditor::Execute(Fixture.Request()).Code, ERPGIdClaimResult::Success);
		TestTrue(TEXT("Clear writes None"), Fixture.Asset->GetId().Id.IsNone());
		TestTrue(TEXT("Clear removes the previous AssetManager mapping"),
			URPGAssetManager::Get().GetPrimaryAssetPath(Suggestion.SuggestedId.ToPrimaryAssetId()).IsNull());
		TestTrue(TEXT("Clear leaves the owner pending save"), Fixture.Asset->GetOutermost()->IsDirty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimOrdinaryDirtyWorkspaceTest, "IronicRPG.RPGId.Claim.OrdinaryMutationRequiresCleanWorkspace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimOrdinaryDirtyWorkspaceTest::RunTest(const FString& Parameters)
{
	FClaimFixture Fixture;
	const FRPGId OldId = FreeId(Fixture);
	Fixture.Seed(OldId.Id);
	const FRPGIdClaimResult Suggestion = FRPGIdClaimEditor::Suggest(Fixture.Request(OldId));
	if (!TestEqual(TEXT("A clean owner obtains a candidate"), Suggestion.Code, ERPGIdClaimResult::Success)) { return false; }
	const FRPGIdClaimRequest Request = Fixture.Request(Suggestion.SuggestedId);
	TestEqual(TEXT("The clean Preview succeeds"), FRPGIdClaimEditor::Preview(Request).Code, ERPGIdClaimResult::Success);
	UPackage* DirtyPackage = CreatePackage(*(TEXT("/Game/DataAssets/Item/DirtyWorkspace_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	DirtyPackage->MarkPackageDirty();
	const FRPGIdClaimResult Blocked = FRPGIdClaimEditor::Execute(Request);
	TestEqual(TEXT("A dirty authored package blocks Change"), Blocked.Code, ERPGIdClaimResult::NotEditable);
	TestTrue(TEXT("The dirty package is identified"), Blocked.Message.ToString().Contains(DirtyPackage->GetName()));
	TestEqual(TEXT("Blocked Change preserves the owner"), Fixture.Asset->GetId(), OldId);
	TestFalse(TEXT("Blocked Change does not dirty the owner"), Fixture.Asset->GetOutermost()->IsDirty());
	DirtyPackage->SetDirtyFlag(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimLegacyFreezeTest, "IronicRPG.RPGId.Claim.LegacyFreezePreservesOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimLegacyFreezeTest::RunTest(const FString& Parameters)
{
	FClaimFixture Legacy(USubGameZoneAsset::StaticClass());
	TestNull(TEXT("Legacy has no authoring adapter"), RPGIdClaimPrivate::FindAdapter(Legacy.Asset->GetClass()));
	TestNull(TEXT("Unknown class has no fallback"), RPGIdClaimPrivate::FindAdapter(UObject::StaticClass()));
	const FRPGId Candidate(FName(*(Legacy.Asset->GetAssetIdPrefix().ToString() + TEXT("1234"))));
	TestEqual(TEXT("Initial Claim denied"), FRPGIdClaimEditor::Execute(Legacy.Request(Candidate)).Code, ERPGIdClaimResult::UnsupportedOwner);
	TestEqual(TEXT("Suggest denied"), FRPGIdClaimEditor::Suggest(Legacy.Request()).Code, ERPGIdClaimResult::UnsupportedOwner);
	Legacy.Seed(Candidate.Id);
	TestEqual(TEXT("Clear denied"), FRPGIdClaimEditor::Execute(Legacy.Request()).Code, ERPGIdClaimResult::UnsupportedOwner);
	TestEqual(TEXT("Change denied"), FRPGIdClaimEditor::Execute(Legacy.Request(FRPGId(TEXT("invalid")))).Code, ERPGIdClaimResult::UnsupportedOwner);
	TMap<FSoftObjectPath, FName> Owners;
	TestTrue(TEXT("Ownership audit succeeds"), RPGIdClaimPrivate::ReadOwners(Owners).CanApply());
	TestEqual(TEXT("Legacy identity remains reserved in ownership"), Owners.FindRef(FSoftObjectPath(Legacy.Asset.Get())), Candidate.Id);
	TestEqual(TEXT("Legacy identity preserved"), Legacy.Asset->GetId(), Candidate);
	TestFalse(TEXT("Legacy operations stay clean"), Legacy.Asset->GetOutermost()->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimSavedOverlayTest, "IronicRPG.RPGId.Claim.SavedAndUnsavedOverlayIsComplete",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimSavedOverlayTest::RunTest(const FString& Parameters)
{
	FAssetDataTagMap Tags;
	Tags.Add(TEXT("RPGId"), TEXT("i1000"));
	const FAssetData Saved(TEXT("/Game/DataAssets/Item/Saved"), TEXT("/Game/DataAssets/Item"), TEXT("Saved"),
		UItemAsset::StaticClass()->GetClassPathName(), Tags);
	const FSoftObjectPath NewPath(TEXT("/OtherMount/Unsaved.Unsaved"));
	TMap<FSoftObjectPath, FName> Loaded;
	Loaded.Add(Saved.GetSoftObjectPath(), TEXT("i2000"));
	Loaded.Add(NewPath, TEXT("i1000"));
	TMap<FSoftObjectPath, FName> Merged;
	TestTrue(TEXT("Merge completes"), RPGIdClaimPrivate::MergeOwners({ Saved }, Loaded, Merged).CanApply());
	TestEqual(TEXT("Same owner counted once"), Merged.Num(), 2);
	TestEqual(TEXT("In-memory value replaces saved old Id"), Merged.FindRef(Saved.GetSoftObjectPath()), FName(TEXT("i2000")));
	TestEqual(TEXT("Unsaved owner outside default mount included"), Merged.FindRef(NewPath), FName(TEXT("i1000")));
	Loaded[Saved.GetSoftObjectPath()] = NAME_None;
	RPGIdClaimPrivate::MergeOwners({ Saved }, Loaded, Merged);
	TestTrue(TEXT("Loaded None also replaces disk Id"), Merged.FindRef(Saved.GetSoftObjectPath()).IsNone());
	RPGIdClaimPrivate::MergeOwners({ Saved }, {}, Merged);
	TestEqual(TEXT("Unloaded owner uses saved tag"), Merged.FindRef(Saved.GetSoftObjectPath()), FName(TEXT("i1000")));
	const FAssetData MissingTag(TEXT("/Game/Missing"), TEXT("/Game"), TEXT("Missing"), UItemAsset::StaticClass()->GetClassPathName());
	TestEqual(TEXT("Missing metadata fails closed"), RPGIdClaimPrivate::MergeOwners({ MissingTag }, {}, Merged).Code,
		ERPGIdClaimResult::InspectionIncomplete);
	TestEqual(TEXT("Failure never publishes a partial snapshot"), Merged.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimCategoryZoneIsolationTest, "IronicRPG.RPGId.Category.RulesPreserveBoundZoneData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimCategoryZoneIsolationTest::RunTest(const FString& Parameters)
{
	FClaimFixture Fixture(UGameZoneAsset::StaticClass());
	auto WorldScope = CreateClaimTestWorld(TEXT("Category"));
	UWorld* World = WorldScope->GetWorld();
	auto* Settings = Cast<ARPGWorldSettings>(World->GetWorldSettings());
	if (!TestNotNull(TEXT("RPG world settings exist"), Settings)) { return false; }
	const FRPGId Id = FreeId(Fixture);
	if (!TestTrue(TEXT("Zone candidate exists"), Id.IsValid())) { return false; }
	auto* Zone = CastChecked<UGameZoneAsset>(Fixture.Asset.Get());
	const FGuid Binding = FGuid::NewGuid();
	SetZoneBinding(*Zone, *World, Id, Binding);
	SetLevelClaim(*Settings, Id, Binding);
	GetPropertyValue<FGuid>(*Zone, TEXT("MapBakeRevision")) = FGuid::NewGuid();
	GetPropertyValue<FString>(*Zone, TEXT("MapBakeInputFingerprint")) = TEXT("category-isolation-input");
	GetPropertyValue<FString>(*Zone, TEXT("MapBakeOutputFingerprint")) = TEXT("category-isolation-output");
	Zone->GetOutermost()->SetDirtyFlag(false);
	World->GetOutermost()->SetDirtyFlag(false);
	TArray<uint8> ZoneBefore, SettingsBefore, ZoneAfter, SettingsAfter;
	FObjectWriter ZoneWriter(Zone, ZoneBefore);
	FObjectWriter SettingsWriter(Settings, SettingsBefore);
	FRPGIdCategoryDefinition Category;
	Category.CategoryKey = TEXT("World");
	Category.DisplayName = TEXT("World");
	Category.Ranges = {{0, 9999}};
	FRPGIdCategoryRules Rules;
	Rules.AssetTypes.FindOrAdd(TEXT("Zone")).Categories.Add(Category);
	FString Error;
	TestTrue(TEXT("Category save succeeds for a bound Zone"), Fixture.CategoryStore->Save(Rules, Fixture.CategoryStore->GetRevision(), Error));
	TestEqual(TEXT("Zone classification updates"), Fixture.CategoryStore->Resolve(TEXT("Zone"), Id.Id).CategoryKey, Category.CategoryKey);
	FObjectWriter ZoneAfterWriter(Zone, ZoneAfter);
	FObjectWriter SettingsAfterWriter(Settings, SettingsAfter);
	TestTrue(TEXT("All serialized Zone fields including Bake remain unchanged"), ZoneBefore == ZoneAfter);
	TestTrue(TEXT("All serialized Level claim fields remain unchanged"), SettingsBefore == SettingsAfter);
	TestFalse(TEXT("Zone remains clean"), Zone->GetOutermost()->IsDirty());
	TestFalse(TEXT("Level remains clean"), World->GetOutermost()->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimGameZoneAdapterTest, "IronicRPG.RPGId.Claim.GameZoneExactPairStillRequiresReferencePreflight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimGameZoneAdapterTest::RunTest(const FString& Parameters)
{
	TestNull(TEXT("Legacy PendingZoneId field is no longer exposed"), UGameZoneAsset::StaticClass()->FindPropertyByName(TEXT("PendingZoneId")));
	TestNull(TEXT("Legacy ChangeZoneId button is no longer exposed"), UGameZoneAsset::StaticClass()->FindFunctionByName(TEXT("ChangeZoneId")));
	FClaimFixture Fixture(UGameZoneAsset::StaticClass());
	TUniquePtr<FScopedEditorWorld> WorldScope = CreateClaimTestWorld(TEXT("Exact"));
	UWorld* World = WorldScope->GetWorld();
	ARPGWorldSettings* Settings = Cast<ARPGWorldSettings>(World->GetWorldSettings());
	if (!TestNotNull(TEXT("Claim fixture uses RPG World Settings"), Settings))
	{
		return false;
	}

	const FRPGId OldId = FreeId(Fixture);
	Fixture.Seed(OldId.Id);
	const FGuid BindingId = FGuid::NewGuid();
	SetZoneBinding(*CastChecked<UGameZoneAsset>(Fixture.Asset.Get()), *World, OldId, BindingId);
	SetLevelClaim(*Settings, OldId, BindingId);
	FGameZoneContext& DefaultContext = GetPropertyValue<FGameZoneContext>(*GetMutableDefault<UGameZoneSystemSettings>(),
		TEXT("DefaultGameZoneContext"));
	TGuardValue<FRPGId> AuthoredReferenceGuard(DefaultContext.ZoneId, OldId);
	Fixture.Asset->GetOutermost()->SetDirtyFlag(false);
	World->GetOutermost()->SetDirtyFlag(false);
	const FRPGIdClaimResult Suggestion = FRPGIdClaimEditor::Suggest(Fixture.Request(FRPGId(TEXT("z8000"))));
	if (!TestEqual(TEXT("Bound GameZone can request a candidate"), Suggestion.Code, ERPGIdClaimResult::Success))
	{
		return false;
	}

	const FRPGIdClaimRequest Request = Fixture.Request(Suggestion.SuggestedId);
	const FRPGIdClaimResult Preview = FRPGIdClaimEditor::Preview(Request);
	TestEqual(TEXT("Exact pair still fails closed while an authored reference remains"), Preview.Code, ERPGIdClaimResult::ReferencesUnsupported);
	TestTrue(TEXT("Preview identifies the exact Level"), Preview.Context.ToString().Contains(World->GetOutermost()->GetName()));
	TestTrue(TEXT("Preview identifies pending Bake"), Preview.Context.ToString().Contains(TEXT("Bake: required")));
	const FRPGIdClaimAdapter* Adapter = RPGIdClaimPrivate::FindAdapter(Fixture.Asset->GetClass());
	if (!TestNotNull(TEXT("GameZone adapter registered"), Adapter)) { return false; }
	TestFalse(TEXT("GameZone keeps its dedicated save coordinator"), Adapter->UsesPendingReservations());
	TestTrue(TEXT("UI queries existing-change capability through service"), FRPGIdClaimEditor::SupportsExistingChange(Request.Owners));
	TestEqual(TEXT("Adapter delegates exact Level classification"),
		Adapter->ClassifyReference(*Fixture.Asset, { FSoftObjectPath(Settings).ToString(), TEXT("GameZoneId") }),
		ERPGIdReferenceCapability::CoordinatorManaged);
	TestEqual(TEXT("Adapter blocks unrelated authored references"),
		Adapter->ClassifyReference(*Fixture.Asset, { FSoftObjectPath(Fixture.Asset.Get()).ToString(), TEXT("OtherData.ZoneId") }),
		ERPGIdReferenceCapability::Blocking);
	TestTrue(TEXT("Exact Level claim is classified as coordinator-managed"),
		RPGIdClaimPrivate::IsCoordinatorManagedZoneReference(*CastChecked<UGameZoneAsset>(Fixture.Asset.Get()),
			{ FSoftObjectPath(Settings).ToString(), TEXT("GameZoneId") }));
	TestTrue(TEXT("Bake-owned ZoneId is classified as coordinator-managed"),
		RPGIdClaimPrivate::IsCoordinatorManagedZoneReference(*CastChecked<UGameZoneAsset>(Fixture.Asset.Get()),
			{ FSoftObjectPath(Fixture.Asset.Get()).ToString(), TEXT("BakedPoints{0}.Value.ZoneId") }));
	TestFalse(TEXT("A different authored ZoneId path is never classified as coordinator-managed"),
		RPGIdClaimPrivate::IsCoordinatorManagedZoneReference(*CastChecked<UGameZoneAsset>(Fixture.Asset.Get()),
			{ FSoftObjectPath(Fixture.Asset.Get()).ToString(), TEXT("OtherData.ZoneId") }));
	TestTrue(TEXT("Preview identifies the authored typed settings reference"),
		Preview.Context.ToString().Contains(TEXT("DefaultGameZoneContext.ZoneId")));
	TestFalse(TEXT("Preview no longer reports the retired graph pin gap"), Preview.Context.ToString().Contains(TEXT("graph pin constants")));
	TestFalse(TEXT("Preview does not change the Asset"), Fixture.Asset->GetOutermost()->IsDirty());
	TestFalse(TEXT("Preview does not change the Level"), World->GetOutermost()->IsDirty());

	const FRPGIdClaimResult Applied = FRPGIdClaimEditor::Execute(Request);
	TestEqual(TEXT("Execute repeats the fail-closed preflight"), Applied.Code, ERPGIdClaimResult::ReferencesUnsupported);
	TestEqual(TEXT("Asset retains the old Id"), Fixture.Asset->GetId(), OldId);
	TestEqual(TEXT("Level retains the old Id"), Settings->GetGameZoneId(), OldId);
	TestEqual(TEXT("Binding generation is preserved"), CastChecked<UGameZoneAsset>(Fixture.Asset.Get())->GetGameZoneBindingId(), BindingId);
	TestEqual(TEXT("Rejected change preserves verified Bake status"), CastChecked<UGameZoneAsset>(Fixture.Asset.Get())->GetBindingVerificationStatus(),
		EGameZoneBindingVerificationStatus::Verified);
	TestFalse(TEXT("Rejected change does not dirty the Asset"), Fixture.Asset->GetOutermost()->IsDirty());
	TestFalse(TEXT("Rejected change does not dirty the Level"), World->GetOutermost()->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimGameZoneDevelopmentReferenceTest,
	"IronicRPG.RPGId.Claim.GameZoneChangeIncludesDevelopmentReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimGameZoneDevelopmentReferenceTest::RunTest(const FString& Parameters)
{
	FClaimFixture Fixture(UGameZoneAsset::StaticClass());
	TUniquePtr<FScopedEditorWorld> WorldScope = CreateClaimTestWorld(TEXT("DevelopmentReference"));
	UWorld* World = WorldScope->GetWorld();
	ARPGWorldSettings* LevelSettings = CastChecked<ARPGWorldSettings>(World->GetWorldSettings());
	const FRPGId OldId = FreeId(Fixture);
	Fixture.Seed(OldId.Id);
	const FGuid BindingId = FGuid::NewGuid();
	SetZoneBinding(*CastChecked<UGameZoneAsset>(Fixture.Asset.Get()), *World, OldId, BindingId);
	SetLevelClaim(*LevelSettings, OldId, BindingId);
	Fixture.Asset->GetOutermost()->SetDirtyFlag(false);
	World->GetOutermost()->SetDirtyFlag(false);

	UGameZoneSystemSettings* ProjectSettings = GetMutableDefault<UGameZoneSystemSettings>();
	TGuardValue<FRPGId> DefaultZoneGuard(ProjectSettings->DefaultGameZoneContext.ZoneId, OldId);
	const FRPGIdClaimResult Suggestion = FRPGIdClaimEditor::Suggest(Fixture.Request(FRPGId(TEXT("z8002"))));
	if (!TestEqual(TEXT("Referenced GameZone obtains an unused candidate"), Suggestion.Code, ERPGIdClaimResult::Success))
	{
		return false;
	}

	const FRPGIdClaimRequest Request = Fixture.Request(Suggestion.SuggestedId);
	const FRPGIdClaimResult Preview = FRPGIdClaimEditor::Preview(Request);
	TestEqual(TEXT("Typed project setting blocks GameZone Change"), Preview.Code, ERPGIdClaimResult::ReferencesUnsupported);
	TestTrue(TEXT("Preview identifies the typed settings source"),
		Preview.Context.ToString().Contains(TEXT("Config:/Script/RPGCore.GameZoneSystemSettings")));
	TestTrue(TEXT("Preview identifies DefaultGameZoneContext.ZoneId"),
		Preview.Context.ToString().Contains(TEXT("DefaultGameZoneContext.ZoneId")));
	TestTrue(TEXT("Preview retains exact binding context"), Preview.Context.ToString().Contains(World->GetOutermost()->GetName()));
	TestFalse(TEXT("Blocked Preview does not dirty the Asset"), Fixture.Asset->GetOutermost()->IsDirty());
	TestFalse(TEXT("Blocked Preview does not dirty the Level"), World->GetOutermost()->IsDirty());

	const FRPGIdClaimResult Applied = FRPGIdClaimEditor::Execute(Request);
	TestEqual(TEXT("Execute repeats the same fail-closed preflight"), Applied.Code, ERPGIdClaimResult::ReferencesUnsupported);
	TestEqual(TEXT("Blocked Execute retains the Asset Id"), Fixture.Asset->GetId(), OldId);
	TestEqual(TEXT("Blocked Execute retains the Level Id"), LevelSettings->GetGameZoneId(), OldId);
	TestFalse(TEXT("Blocked Execute does not dirty the Asset"), Fixture.Asset->GetOutermost()->IsDirty());
	TestFalse(TEXT("Blocked Execute does not dirty the Level"), World->GetOutermost()->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimGameZoneBlueprintGraphReferenceTest,
	"IronicRPG.RPGId.Claim.GameZoneChangeIncludesBlueprintGraphLiteralReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimGameZoneBlueprintGraphReferenceTest::RunTest(const FString& Parameters)
{
	FClaimFixture Fixture(UGameZoneAsset::StaticClass());
	TUniquePtr<FScopedEditorWorld> WorldScope = CreateClaimTestWorld(TEXT("BlueprintGraphReference"));
	UWorld* World = WorldScope->GetWorld();
	ARPGWorldSettings* LevelSettings = CastChecked<ARPGWorldSettings>(World->GetWorldSettings());
	const FRPGId OldId = FreeId(Fixture);
	Fixture.Seed(OldId.Id);
	const FGuid BindingId = FGuid::NewGuid();
	SetZoneBinding(*CastChecked<UGameZoneAsset>(Fixture.Asset.Get()), *World, OldId, BindingId);
	SetLevelClaim(*LevelSettings, OldId, BindingId);

	const FString BlueprintName = TEXT("BP_ZoneGraphReference_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	UPackage* BlueprintPackage = CreatePackage(*(TEXT("/Game/ClaimTests/") + BlueprintName));
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), BlueprintPackage, *BlueprintName,
		BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(),
		TEXT("RPGIdGameZoneBlueprintGraphReferenceTest"));
	if (!TestNotNull(TEXT("Blueprint reference fixture is created"), Blueprint))
	{
		return false;
	}
	FAssetRegistryModule::AssetCreated(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	UEdGraph* Graph = Blueprint->UbergraphPages.IsEmpty() ? nullptr : Blueprint->UbergraphPages[0];
	if (!TestNotNull(TEXT("Blueprint event graph is available"), Graph))
	{
		return false;
	}

	FEdGraphPinType ContextType;
	ContextType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	ContextType.PinSubCategoryObject = FGameZoneContext::StaticStruct();
	FEdGraphPinType IdType;
	IdType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	IdType.PinSubCategoryObject = FRPGId::StaticStruct();
	FString DefaultValue;
	FRPGId::StaticStruct()->ExportText(DefaultValue, &OldId, nullptr, nullptr, PPF_None, nullptr);
	UEdGraphNode* LiteralNode = NewObject<UEdGraphNode>(Graph, TEXT("ZoneLiteralReferenceNode"));
	Graph->AddNode(LiteralNode, false, false);
	UEdGraphPin* ContextPin = LiteralNode->CreatePin(EGPD_Input, ContextType, TEXT("NewContext"));
	UEdGraphPin* LiteralPin = LiteralNode->CreatePin(EGPD_Input, IdType, TEXT("NewContext_ZoneId"));
	ContextPin->SubPins.Add(LiteralPin);
	LiteralPin->ParentPin = ContextPin;
	LiteralPin->DefaultValue = DefaultValue;

	BlueprintPackage->SetDirtyFlag(false);
	Fixture.Asset->GetOutermost()->SetDirtyFlag(false);
	World->GetOutermost()->SetDirtyFlag(false);
	const FRPGIdClaimResult Suggestion = FRPGIdClaimEditor::Suggest(Fixture.Request(FRPGId(TEXT("z8003"))));
	if (!TestEqual(TEXT("Referenced GameZone obtains an unused candidate"), Suggestion.Code, ERPGIdClaimResult::Success))
	{
		return false;
	}

	const FRPGIdClaimResult Preview = FRPGIdClaimEditor::Preview(Fixture.Request(Suggestion.SuggestedId));
	TestEqual(TEXT("Blueprint graph literal blocks GameZone Change"), Preview.Code, ERPGIdClaimResult::ReferencesUnsupported);
	TestTrue(TEXT("Preview identifies the Blueprint graph source"),
		Preview.Context.ToString().Contains(FSoftObjectPath(Blueprint).ToString()));
	TestTrue(TEXT("Preview identifies the exact graph pin"),
		Preview.Context.ToString().Contains(TEXT("ZoneLiteralReferenceNode"))
		&& Preview.Context.ToString().Contains(TEXT("Pin[NewContext_ZoneId]")));
	TestFalse(TEXT("Preview does not dirty the Blueprint"), BlueprintPackage->IsDirty());
	TestFalse(TEXT("Preview does not dirty the Asset"), Fixture.Asset->GetOutermost()->IsDirty());
	TestFalse(TEXT("Preview does not dirty the Level"), World->GetOutermost()->IsDirty());

	FAssetRegistryModule::AssetDeleted(Blueprint);
	Blueprint->ClearFlags(RF_Public | RF_Standalone);
	Blueprint->SetFlags(RF_Transient);
	BlueprintPackage->SetDirtyFlag(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClaimGameZoneFailureTest, "IronicRPG.RPGId.Claim.GameZoneMismatchAndClearFailClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClaimGameZoneFailureTest::RunTest(const FString& Parameters)
{
	FClaimFixture Fixture(UGameZoneAsset::StaticClass());
	TUniquePtr<FScopedEditorWorld> WorldScope = CreateClaimTestWorld(TEXT("Mismatch"));
	UWorld* World = WorldScope->GetWorld();
	ARPGWorldSettings* Settings = CastChecked<ARPGWorldSettings>(World->GetWorldSettings());
	const FRPGId OldId = FreeId(Fixture);
	Fixture.Seed(OldId.Id);
	const FGuid BindingId = FGuid::NewGuid();
	SetZoneBinding(*CastChecked<UGameZoneAsset>(Fixture.Asset.Get()), *World, OldId, BindingId);
	SetLevelClaim(*Settings, FRPGId(TEXT("z9999")), BindingId);
	Fixture.Asset->GetOutermost()->SetDirtyFlag(false);
	World->GetOutermost()->SetDirtyFlag(false);
	const FRPGIdClaimResult Suggestion = FRPGIdClaimEditor::Suggest(Fixture.Request(FRPGId(TEXT("z8001"))));
	if (!TestEqual(TEXT("Mismatch fixture obtains an unused candidate"), Suggestion.Code, ERPGIdClaimResult::Success))
	{
		return false;
	}

	TestEqual(TEXT("Mismatched pair preview is refused"), FRPGIdClaimEditor::Preview(Fixture.Request(Suggestion.SuggestedId)).Code,
		ERPGIdClaimResult::InspectionIncomplete);
	TestEqual(TEXT("Mismatched pair execute is refused"), FRPGIdClaimEditor::Execute(Fixture.Request(Suggestion.SuggestedId)).Code,
		ERPGIdClaimResult::InspectionIncomplete);
	TestEqual(TEXT("Bound Clear is refused"), FRPGIdClaimEditor::Execute(Fixture.Request()).Code,
		ERPGIdClaimResult::ReferencesUnsupported);
	TestEqual(TEXT("Asset retains the old Id"), Fixture.Asset->GetId(), OldId);
	TestEqual(TEXT("Level retains the foreign Id"), Settings->GetGameZoneId(), FRPGId(TEXT("z9999")));
	TestEqual(TEXT("Binding generation is retained"), CastChecked<UGameZoneAsset>(Fixture.Asset.Get())->GetGameZoneBindingId(), BindingId);
	TestFalse(TEXT("Rejected operations do not dirty the Asset"), Fixture.Asset->GetOutermost()->IsDirty());
	TestFalse(TEXT("Rejected operations do not dirty the Level"), World->GetOutermost()->IsDirty());
	return true;
}

#endif

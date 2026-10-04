// Copyright Ironic Studio. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RPGIdReferenceMigrationApply.h"
#include "RPGIdReferenceMigrationWorkspace.h"

#include "Abilities/AbilityAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Characters/CharacterAsset.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Levels/GameZonePointComponent.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RPGIdReferenceAudit.h"
#include "Settings/CharacterSystemSettings.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
	FRPGIdReferenceMigrationPlan MakeReadyPlan()
	{
		FRPGIdReferenceMigrationPlan Plan;
		Plan.OldId = FRPGId(TEXT("i1000"));
		Plan.NewId = FRPGId(TEXT("i2000"));
		Plan.ExpectedOwner = FSoftObjectPath(TEXT("/Game/DataAssets/Item/DA_Old.DA_Old"));
		Plan.ReferenceIndexGeneration = 42;
		FRPGIdReferenceMigrationEdit Edit;
		Edit.Writer = ERPGIdReferenceMigrationWriter::NativeObjectProperty;
		Edit.Source = TEXT("/Game/DataAssets/Character/DA_Hero.DA_Hero");
		Edit.PropertyPath = TEXT("DefaultEquipment[0].ItemId");
		Edit.ExpectedValue = Plan.OldId;
		Edit.ReplacementValue = Plan.NewId;
		Edit.Artifact = { ERPGIdReferenceMigrationArtifactKind::Package, TEXT("/Game/DataAssets/Character/DA_Hero") };
		Plan.Edits.Add(Edit);
		Plan.RequiredArtifacts = {
			Edit.Artifact,
			{ ERPGIdReferenceMigrationArtifactKind::Package, TEXT("/Game/DataAssets/Item/DA_Old") }
		};
		return Plan;
	}

	class FApplyWorkspace final : public IRPGIdReferenceMigrationApplyWorkspace
	{
	public:
		virtual bool Begin(const FRPGIdReferenceMigrationPlan& Plan, FText& OutError) override
		{
			++BeginCount;
			OriginalValue = Value;
			return !bFailBegin;
		}

		virtual bool ApplyEdit(const FRPGIdReferenceMigrationEdit& Edit, FText& OutError) override
		{
			++EditCount;
			Value = Edit.ReplacementValue;
			return !bFailEdit;
		}

		virtual bool ApplyOwner(const FRPGIdReferenceMigrationRequest& Request, FText& OutError) override
		{
			++OwnerCount;
			OwnerValue = Request.NewId;
			return !bFailOwner;
		}

		virtual bool VerifyOldIdAbsent(const FRPGId& OldId, FText& OutError) override
		{
			++VerifyCount;
			return !bFailVerify;
		}

		virtual bool StageRedirect(const FRPGIdRedirect& Redirect, FText& OutError) override
		{
			++StageCount;
			StagedRedirect = Redirect;
			return !bFailStage;
		}

		virtual bool Commit(FText& OutError) override
		{
			++CommitCount;
			return !bFailCommit;
		}

		virtual bool Rollback(FText& OutError) override
		{
			++RollbackCount;
			Value = OriginalValue;
			OwnerValue = FRPGId(TEXT("i1000"));
			return !bFailRollback;
		}

		FRPGId Value = FRPGId(TEXT("i1000"));
		FRPGId OriginalValue;
		FRPGId OwnerValue = FRPGId(TEXT("i1000"));
		FRPGIdRedirect StagedRedirect;
		int32 BeginCount = 0;
		int32 EditCount = 0;
		int32 OwnerCount = 0;
		int32 VerifyCount = 0;
		int32 StageCount = 0;
		int32 CommitCount = 0;
		int32 RollbackCount = 0;
		bool bFailBegin = false;
		bool bFailEdit = false;
		bool bFailOwner = false;
		bool bFailVerify = false;
		bool bFailStage = false;
		bool bFailCommit = false;
		bool bFailRollback = false;
	};

	void SetOwnerId(URPGPrimaryAsset& Asset, const FRPGId& Id)
	{
		FProperty* Property = FindFProperty<FProperty>(URPGPrimaryAsset::StaticClass(), TEXT("Id"));
		*Property->ContainerPtrToValuePtr<FRPGId>(&Asset) = Id;
	}

	TSet<FRPGId>& GetAllowedCharacters(UAbilityAsset& Asset)
	{
		FProperty* Property = FindFProperty<FProperty>(UAbilityAsset::StaticClass(), TEXT("AllowedCharacters"));
		return *Property->ContainerPtrToValuePtr<TSet<FRPGId>>(&Asset);
	}

	ERPGIdReferenceMigrationWriter BlueprintWriterForPath(const FString& Path)
	{
		if (Path.StartsWith(TEXT("ClassDefaultObject.")))
		{
			return ERPGIdReferenceMigrationWriter::BlueprintDefaultProperty;
		}
		if (Path.StartsWith(TEXT("Template[")))
		{
			return ERPGIdReferenceMigrationWriter::BlueprintTemplateProperty;
		}
		return ERPGIdReferenceMigrationWriter::BlueprintGraphLiteral;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceMigrationAtomicApplyTest,
	"IronicRPG.RPGId.SaveMigration.ReferenceApply.AppliesMatchingFreshPlanAtomically",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceMigrationAtomicApplyTest::RunTest(const FString& Parameters)
{
	const FRPGIdReferenceMigrationPlan Preview = MakeReadyPlan();
	FApplyWorkspace Workspace;
	const FRPGIdReferenceMigrationApplyResult Result = FRPGIdReferenceMigrationApplier::ApplyPrepared(Preview, Preview, Workspace);

	TestTrue(TEXT("Matching fresh plan applies"), Result.IsSuccess());
	TestEqual(TEXT("One edit is applied"), Workspace.EditCount, 1);
	TestEqual(TEXT("Owner is changed after references"), Workspace.OwnerCount, 1);
	TestEqual(TEXT("Old Id is audited after mutation"), Workspace.VerifyCount, 1);
	TestEqual(TEXT("Redirect is staged before commit"), Workspace.StageCount, 1);
	TestEqual(TEXT("Successful work commits once"), Workspace.CommitCount, 1);
	TestEqual(TEXT("Successful work does not roll back"), Workspace.RollbackCount, 0);
	TestEqual(TEXT("Redirect source is returned"), Result.Redirect.OldId, Preview.OldId);
	TestEqual(TEXT("Redirect target is returned"), Result.Redirect.NewId, Preview.NewId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceMigrationStalePlanTest,
	"IronicRPG.RPGId.SaveMigration.ReferenceApply.RejectsChangedFreshPlanBeforeMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceMigrationStalePlanTest::RunTest(const FString& Parameters)
{
	const FRPGIdReferenceMigrationPlan Preview = MakeReadyPlan();
	FRPGIdReferenceMigrationPlan Fresh = Preview;
	Fresh.ReferenceIndexGeneration = 43;
	FApplyWorkspace Workspace;
	const FRPGIdReferenceMigrationApplyResult Result = FRPGIdReferenceMigrationApplier::ApplyPrepared(Preview, Fresh, Workspace);

	TestEqual(TEXT("Changed plan is stale"), Result.Code, ERPGIdReferenceMigrationApplyCode::StalePlan);
	TestEqual(TEXT("Stale plan never opens workspace"), Workspace.BeginCount, 0);
	TestEqual(TEXT("Stale plan never mutates"), Workspace.EditCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceMigrationWriterRollbackTest,
	"IronicRPG.RPGId.SaveMigration.ReferenceApply.WriterFailureRollsBackEverything",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceMigrationWriterRollbackTest::RunTest(const FString& Parameters)
{
	const FRPGIdReferenceMigrationPlan Plan = MakeReadyPlan();
	FApplyWorkspace Workspace;
	Workspace.bFailEdit = true;
	const FRPGIdReferenceMigrationApplyResult Result = FRPGIdReferenceMigrationApplier::ApplyPrepared(Plan, Plan, Workspace);

	TestEqual(TEXT("Writer failure is named"), Result.Code, ERPGIdReferenceMigrationApplyCode::WriterFailed);
	TestEqual(TEXT("Writer failure rolls back once"), Workspace.RollbackCount, 1);
	TestEqual(TEXT("Reference value is restored"), Workspace.Value, Plan.OldId);
	TestEqual(TEXT("Owner is not changed after writer failure"), Workspace.OwnerCount, 0);
	TestEqual(TEXT("Failed work never commits"), Workspace.CommitCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceMigrationReverseAuditRollbackTest,
	"IronicRPG.RPGId.SaveMigration.ReferenceApply.ReverseAuditFailureRollsBackEverything",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceMigrationReverseAuditRollbackTest::RunTest(const FString& Parameters)
{
	const FRPGIdReferenceMigrationPlan Plan = MakeReadyPlan();
	FApplyWorkspace Workspace;
	Workspace.bFailVerify = true;
	const FRPGIdReferenceMigrationApplyResult Result = FRPGIdReferenceMigrationApplier::ApplyPrepared(Plan, Plan, Workspace);

	TestEqual(TEXT("Reverse audit failure is named"), Result.Code, ERPGIdReferenceMigrationApplyCode::ReverseAuditFailed);
	TestEqual(TEXT("Reverse audit failure rolls back once"), Workspace.RollbackCount, 1);
	TestEqual(TEXT("Reference value is restored"), Workspace.Value, Plan.OldId);
	TestEqual(TEXT("Owner value is restored"), Workspace.OwnerValue, Plan.OldId);
	TestEqual(TEXT("Failed work never commits"), Workspace.CommitCount, 0);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceMigrationHandoffRollbackTest,
	"IronicRPG.RPGId.SaveMigration.ReferenceApply.HandoffFailureRollsBackEverything",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceMigrationHandoffRollbackTest::RunTest(const FString& Parameters)
{
	const FRPGIdReferenceMigrationPlan Plan = MakeReadyPlan();
	FApplyWorkspace Workspace;
	Workspace.bFailStage = true;
	const FRPGIdReferenceMigrationApplyResult Result = FRPGIdReferenceMigrationApplier::ApplyPrepared(Plan, Plan, Workspace);

	TestEqual(TEXT("Handoff failure is named"), Result.Code, ERPGIdReferenceMigrationApplyCode::HandoffFailed);
	TestEqual(TEXT("Handoff is attempted once"), Workspace.StageCount, 1);
	TestEqual(TEXT("Handoff failure rolls back once"), Workspace.RollbackCount, 1);
	TestEqual(TEXT("Reference value is restored"), Workspace.Value, Plan.OldId);
	TestEqual(TEXT("Owner value is restored"), Workspace.OwnerValue, Plan.OldId);
	TestEqual(TEXT("Failed handoff never commits"), Workspace.CommitCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceMigrationNativeWorkspaceTest,
	"IronicRPG.RPGId.SaveMigration.ReferenceApply.NativeSetOwnerAndUndo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceMigrationNativeWorkspaceTest::RunTest(const FString& Parameters)
{
	const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	UPackage* OwnerPackage = CreatePackage(*(TEXT("/Game/DataAssets/Character/MigrationOwner_") + Suffix));
	UPackage* ReferencePackage = CreatePackage(*(TEXT("/Game/DataAssets/Ability/MigrationReference_") + Suffix));
	UCharacterAsset* Owner = NewObject<UCharacterAsset>(OwnerPackage, *(TEXT("Owner_") + Suffix), RF_Public | RF_Standalone | RF_Transactional);
	UAbilityAsset* Reference = NewObject<UAbilityAsset>(ReferencePackage, *(TEXT("Reference_") + Suffix),
		RF_Public | RF_Standalone | RF_Transactional);
	FAssetRegistryModule::AssetCreated(Owner);
	FAssetRegistryModule::AssetCreated(Reference);

	const FRPGId OldId(TEXT("c9791"));
	const FRPGId NewId(TEXT("c9792"));
	SetOwnerId(*Owner, OldId);
	GetAllowedCharacters(*Reference).Add(OldId);
	OwnerPackage->SetDirtyFlag(false);
	ReferencePackage->SetDirtyFlag(false);

	FRPGIdReferenceMigrationPlan Plan;
	Plan.OldId = OldId;
	Plan.NewId = NewId;
	Plan.ExpectedOwner = FSoftObjectPath(Owner);
	FRPGIdReferenceMigrationEdit Edit;
	Edit.Writer = ERPGIdReferenceMigrationWriter::NativeObjectProperty;
	Edit.Source = FSoftObjectPath(Reference).ToString();
	Edit.PropertyPath = TEXT("AllowedCharacters{0}");
	Edit.ExpectedValue = OldId;
	Edit.ReplacementValue = NewId;
	Edit.Artifact = {ERPGIdReferenceMigrationArtifactKind::Package, ReferencePackage->GetName()};
	Plan.Edits.Add(Edit);
	Plan.RequiredArtifacts = {Edit.Artifact,
		{ERPGIdReferenceMigrationArtifactKind::Package, OwnerPackage->GetName()}};

	TUniquePtr<IRPGIdReferenceMigrationApplyWorkspace> Workspace = RPGIdReferenceMigrationPrivate::CreateProductionWorkspace();
	const FRPGIdReferenceMigrationApplyResult Result = FRPGIdReferenceMigrationApplier::ApplyPrepared(Plan, Plan, *Workspace);
	TestTrue(TEXT("Production native writer applies"), Result.IsSuccess());
	TestEqual(TEXT("Owner receives the new Id"), Owner->GetId(), NewId);
	TestTrue(TEXT("Set receives the new Id and remains searchable"), GetAllowedCharacters(*Reference).Contains(NewId));
	TestFalse(TEXT("Set no longer contains the old Id"), GetAllowedCharacters(*Reference).Contains(OldId));
	TestTrue(TEXT("Owner remains pending save"), OwnerPackage->IsDirty());
	TestTrue(TEXT("Reference remains pending save"), ReferencePackage->IsDirty());

	Workspace.Reset();
	TestTrue(TEXT("Atomic migration is one undo transaction"), GEditor->UndoTransaction());
	TestEqual(TEXT("Undo restores owner"), Owner->GetId(), OldId);
	TestTrue(TEXT("Undo restores set hash and value"), GetAllowedCharacters(*Reference).Contains(OldId));
	TestFalse(TEXT("Undo removes replacement from set"), GetAllowedCharacters(*Reference).Contains(NewId));

	FAssetRegistryModule::AssetDeleted(Reference);
	FAssetRegistryModule::AssetDeleted(Owner);
	Reference->ClearFlags(RF_Public | RF_Standalone);
	Owner->ClearFlags(RF_Public | RF_Standalone);
	Reference->SetFlags(RF_Transient);
	Owner->SetFlags(RF_Transient);
	ReferencePackage->SetDirtyFlag(false);
	OwnerPackage->SetDirtyFlag(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceMigrationBlueprintWorkspaceTest,
	"IronicRPG.RPGId.SaveMigration.ReferenceApply.BlueprintDefaultsTemplatesGraphAndUndo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceMigrationBlueprintWorkspaceTest::RunTest(const FString& Parameters)
{
	const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	UPackage* OwnerPackage = CreatePackage(*(TEXT("/Game/DataAssets/Character/MigrationBPOwner_") + Suffix));
	UCharacterAsset* Owner = NewObject<UCharacterAsset>(OwnerPackage, *(TEXT("MigrationBPOwner_") + Suffix),
		RF_Public | RF_Standalone | RF_Transactional);
	FAssetRegistryModule::AssetCreated(Owner);
	const FRPGId OldId(TEXT("c9781"));
	const FRPGId NewId(TEXT("c9782"));
	SetOwnerId(*Owner, OldId);

	const FString BlueprintName = TEXT("BP_Migration_") + Suffix;
	UPackage* BlueprintPackage = CreatePackage(*(TEXT("/Game/ClaimTests/") + BlueprintName));
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), BlueprintPackage, *BlueprintName,
		BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("RPGIdReferenceMigrationApplyTest"));
	if (!TestNotNull(TEXT("Blueprint fixture is created"), Blueprint))
	{
		return false;
	}
	FAssetRegistryModule::AssetCreated(Blueprint);

	FEdGraphPinType IdType;
	IdType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	IdType.PinSubCategoryObject = FRPGId::StaticStruct();
	FString OldText;
	FRPGId::StaticStruct()->ExportText(OldText, &OldId, nullptr, nullptr, PPF_None, nullptr);
	TestTrue(TEXT("Blueprint default variable is added"),
		FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("MigratedId"), IdType, OldText));
	USCS_Node* PointNode = Blueprint->SimpleConstructionScript->CreateNode(UGameZonePointComponent::StaticClass(), TEXT("MigrationPoint"));
	Blueprint->SimpleConstructionScript->AddNode(PointNode);
	FProperty* PointDataProperty = FindFProperty<FProperty>(UGameZonePointComponent::StaticClass(), TEXT("PointData"));
	PointDataProperty->ContainerPtrToValuePtr<FGameZonePointData>(PointNode->ComponentTemplate)->MarkerTypeId = OldId;
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	UEdGraph* Graph = Blueprint->UbergraphPages.IsEmpty() ? nullptr : Blueprint->UbergraphPages[0];
	if (!TestNotNull(TEXT("Blueprint event graph is available"), Graph))
	{
		return false;
	}
	UEdGraphNode* LiteralNode = NewObject<UEdGraphNode>(Graph, TEXT("MigrationLiteralNode"));
	Graph->AddNode(LiteralNode, false, false);
	UEdGraphPin* LiteralPin = LiteralNode->CreatePin(EGPD_Input, IdType, TEXT("MigrationLiteral"));
	LiteralPin->DefaultValue = OldText;
	BlueprintPackage->SetDirtyFlag(false);
	OwnerPackage->SetDirtyFlag(false);

	TArray<FRPGIdReferenceHit> Hits;
	TArray<FText> Gaps;
	RPGIdReferencePrivate::ScanBlueprint(*Blueprint, OldId, Hits, Gaps);
	TestTrue(TEXT("Blueprint fixture is complete before migration"), Gaps.IsEmpty());
	TestEqual(TEXT("Blueprint fixture exposes all three certified shapes"), Hits.Num(), 3);

	FRPGIdReferenceMigrationPlan Plan;
	Plan.OldId = OldId;
	Plan.NewId = NewId;
	Plan.ExpectedOwner = FSoftObjectPath(Owner);
	for (const FRPGIdReferenceHit& Hit : Hits)
	{
		FRPGIdReferenceMigrationEdit Edit;
		Edit.Writer = BlueprintWriterForPath(Hit.PropertyPath);
		Edit.Source = Hit.Source;
		Edit.PropertyPath = Hit.PropertyPath;
		Edit.ExpectedValue = OldId;
		Edit.ReplacementValue = NewId;
		Edit.Artifact = {ERPGIdReferenceMigrationArtifactKind::Package, BlueprintPackage->GetName()};
		Plan.Edits.Add(Edit);
	}
	Plan.RequiredArtifacts = {
		{ERPGIdReferenceMigrationArtifactKind::Package, BlueprintPackage->GetName()},
		{ERPGIdReferenceMigrationArtifactKind::Package, OwnerPackage->GetName()}
	};

	TUniquePtr<IRPGIdReferenceMigrationApplyWorkspace> Workspace = RPGIdReferenceMigrationPrivate::CreateProductionWorkspace();
	const FRPGIdReferenceMigrationApplyResult Result = FRPGIdReferenceMigrationApplier::ApplyPrepared(Plan, Plan, *Workspace);
	TestTrue(TEXT("All Blueprint writer shapes apply"), Result.IsSuccess());
	Hits.Reset();
	Gaps.Reset();
	RPGIdReferencePrivate::ScanBlueprint(*Blueprint, OldId, Hits, Gaps);
	TestTrue(TEXT("No old Blueprint reference remains"), Hits.IsEmpty());
	Hits.Reset();
	Gaps.Reset();
	RPGIdReferencePrivate::ScanBlueprint(*Blueprint, NewId, Hits, Gaps);
	TestEqual(TEXT("All Blueprint references use the replacement"), Hits.Num(), 3);
	TestEqual(TEXT("Blueprint migration also changes owner"), Owner->GetId(), NewId);

	Workspace.Reset();
	TestTrue(TEXT("Blueprint migration is one undo transaction"), GEditor->UndoTransaction());
	Hits.Reset();
	Gaps.Reset();
	RPGIdReferencePrivate::ScanBlueprint(*Blueprint, OldId, Hits, Gaps);
	TestEqual(TEXT("Undo restores all Blueprint references"), Hits.Num(), 3);
	TestEqual(TEXT("Undo restores Blueprint owner"), Owner->GetId(), OldId);

	FAssetRegistryModule::AssetDeleted(Blueprint);
	FAssetRegistryModule::AssetDeleted(Owner);
	Blueprint->ClearFlags(RF_Public | RF_Standalone);
	Owner->ClearFlags(RF_Public | RF_Standalone);
	Blueprint->SetFlags(RF_Transient);
	Owner->SetFlags(RF_Transient);
	BlueprintPackage->SetDirtyFlag(false);
	OwnerPackage->SetDirtyFlag(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRPGIdReferenceMigrationConfigWorkspaceTest,
	"IronicRPG.RPGId.SaveMigration.ReferenceApply.TypedLegacyConfigUndoRedo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRPGIdReferenceMigrationConfigWorkspaceTest::RunTest(const FString& Parameters)
{
	const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString ConfigFilename = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / (TEXT("RPGIdMigrationConfig-") + Suffix + TEXT(".ini")));
	const FRPGId OldId(TEXT("c9771"));
	const FRPGId NewId(TEXT("c9772"));
	const FString InitialConfig = TEXT(";METADATA=(Diff=true, UseCommands=true)\r\n\r\n"
		"[/Script/RPGCore.RPGSettings]\r\nDefaultPartyMembers=(Id=\"c9771\")\r\n");
	if (!TestTrue(TEXT("Isolated config fixture is written"),
		FFileHelper::SaveStringToFile(InitialConfig, *ConfigFilename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)))
	{
		return false;
	}
	GConfig->LoadFile(ConfigFilename);

	UCharacterSystemSettings* Settings = GetMutableDefault<UCharacterSystemSettings>();
	const TArray<FRPGId> OriginalPlayableCharacters = Settings->PlayableCharacters;
	Settings->PlayableCharacters = {OldId};

	UPackage* OwnerPackage = CreatePackage(*(TEXT("/Game/DataAssets/Character/MigrationConfigOwner_") + Suffix));
	UCharacterAsset* Owner = NewObject<UCharacterAsset>(OwnerPackage, *(TEXT("MigrationConfigOwner_") + Suffix),
		RF_Public | RF_Standalone | RF_Transactional);
	FAssetRegistryModule::AssetCreated(Owner);
	SetOwnerId(*Owner, OldId);
	OwnerPackage->SetDirtyFlag(false);

	FRPGIdReferenceMigrationPlan Plan;
	Plan.OldId = OldId;
	Plan.NewId = NewId;
	Plan.ExpectedOwner = FSoftObjectPath(Owner);
	FRPGIdReferenceMigrationEdit Typed;
	Typed.Writer = ERPGIdReferenceMigrationWriter::TypedConfigProperty;
	Typed.Source = TEXT("Config:/Script/RPGCore.CharacterSystemSettings");
	Typed.PropertyPath = TEXT("PlayableCharacters[0]");
	Typed.ExpectedValue = OldId;
	Typed.ReplacementValue = NewId;
	Typed.Artifact = {ERPGIdReferenceMigrationArtifactKind::ConfigFile, ConfigFilename};
	FRPGIdReferenceMigrationEdit Legacy = Typed;
	Legacy.Writer = ERPGIdReferenceMigrationWriter::LegacyConfigProperty;
	Legacy.Source = TEXT("LegacyConfig:/Script/RPGCore.RPGSettings");
	Legacy.PropertyPath = TEXT("DefaultPartyMembers[0]");
	Plan.Edits = {Typed, Legacy};
	Plan.RequiredArtifacts = {
		Typed.Artifact,
		{ERPGIdReferenceMigrationArtifactKind::Package, OwnerPackage->GetName()}
	};

	TUniquePtr<IRPGIdReferenceMigrationApplyWorkspace> Workspace =
		RPGIdReferenceMigrationPrivate::CreateProductionWorkspace(ConfigFilename);
	const FRPGIdReferenceMigrationApplyResult Result = FRPGIdReferenceMigrationApplier::ApplyPrepared(Plan, Plan, *Workspace);
	TestTrue(TEXT("Typed and legacy config writers apply"), Result.IsSuccess());
	TestEqual(TEXT("Typed config default is replaced"), Settings->PlayableCharacters[0], NewId);
	FString ConfigText;
	TestTrue(TEXT("Committed config can be read"), FFileHelper::LoadFileToString(ConfigText, *ConfigFilename));
	TestTrue(TEXT("Committed config contains replacement"), ConfigText.Contains(NewId.ToString()));
	TestFalse(TEXT("Committed config no longer contains old Id"), ConfigText.Contains(OldId.ToString()));

	Workspace.Reset();
	TestTrue(TEXT("Config migration can be undone"), GEditor->UndoTransaction());
	TestEqual(TEXT("Undo restores typed config default"), Settings->PlayableCharacters[0], OldId);
	FFileHelper::LoadFileToString(ConfigText, *ConfigFilename);
	TestTrue(TEXT("Undo restores legacy config file"), ConfigText.Contains(OldId.ToString()));
	TestFalse(TEXT("Undo removes replacement from config file"), ConfigText.Contains(NewId.ToString()));
	TestTrue(TEXT("Config migration can be redone"), GEditor->RedoTransaction());
	TestEqual(TEXT("Redo restores typed replacement"), Settings->PlayableCharacters[0], NewId);
	FFileHelper::LoadFileToString(ConfigText, *ConfigFilename);
	TestTrue(TEXT("Redo restores legacy replacement"), ConfigText.Contains(NewId.ToString()));
	GEditor->UndoTransaction();

	Settings->PlayableCharacters = OriginalPlayableCharacters;
	GConfig->UnloadFile(ConfigFilename);
	IFileManager::Get().Delete(*ConfigFilename);
	FAssetRegistryModule::AssetDeleted(Owner);
	Owner->ClearFlags(RF_Public | RF_Standalone);
	Owner->SetFlags(RF_Transient);
	OwnerPackage->SetDirtyFlag(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Copyright Ironic Studio. All Rights Reserved.

#include "RPGIdReferenceMigrationPlan.h"

#include "RPGIdClaimSnapshot.h"
#include "RPGReleaseSealService.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "RPGIdReferenceMigrationPlan"

namespace
{
	const TCHAR* BlueprintPrefix = TEXT("Blueprint:");
	const TCHAR* TypedCharacterConfig = TEXT("Config:/Script/RPGCore.CharacterSystemSettings");
	const TCHAR* TypedGameZoneConfig = TEXT("Config:/Script/RPGCore.GameZoneSystemSettings");
	const TCHAR* LegacyConfig = TEXT("LegacyConfig:/Script/RPGCore.RPGSettings");

	void AddIssue(FRPGIdReferenceMigrationPlan& Plan, const ERPGIdReferenceMigrationIssueCode Code, const FString& Detail,
		const FString& Source = FString(), const FString& PropertyPath = FString())
	{
		Plan.Issues.Add({ Code, Detail, Source, PropertyPath });
	}

	FRPGIdReferenceMigrationArtifact PackageArtifact(const FString& PackageName)
	{
		return { ERPGIdReferenceMigrationArtifactKind::Package, PackageName };
	}

	FRPGIdReferenceMigrationArtifact ConfigArtifact()
	{
		return { ERPGIdReferenceMigrationArtifactKind::ConfigFile,
			FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() / TEXT("DefaultIronicRPG.ini")) };
	}

	bool TryPackageFromObjectPath(const FString& ObjectPath, FString& OutPackageName)
	{
		const FSoftObjectPath Path(ObjectPath);
		OutPackageName = Path.GetLongPackageName();
		return !Path.IsNull() && FPackageName::IsValidLongPackageName(OutPackageName)
			&& (OutPackageName.StartsWith(TEXT("/Game/")) || OutPackageName.StartsWith(TEXT("/IronicRPG/")));
	}

	bool TryBuildEdit(const FRPGIdReferenceHit& Hit, FRPGIdReferenceMigrationEdit& OutEdit)
	{
		if (Hit.Source.IsEmpty() || Hit.PropertyPath.IsEmpty())
		{
			return false;
		}

		OutEdit.Source = Hit.Source;
		OutEdit.PropertyPath = Hit.PropertyPath;
		if (Hit.Source == TypedCharacterConfig || Hit.Source == TypedGameZoneConfig)
		{
			OutEdit.Writer = ERPGIdReferenceMigrationWriter::TypedConfigProperty;
			OutEdit.Artifact = ConfigArtifact();
			return true;
		}
		if (Hit.Source == LegacyConfig)
		{
			OutEdit.Writer = ERPGIdReferenceMigrationWriter::LegacyConfigProperty;
			OutEdit.Artifact = ConfigArtifact();
			return true;
		}

		FString PackageName;
		if (Hit.Source.StartsWith(BlueprintPrefix))
		{
			if (!TryPackageFromObjectPath(Hit.Source.RightChop(FCString::Strlen(BlueprintPrefix)), PackageName))
			{
				return false;
			}
			if (Hit.PropertyPath.StartsWith(TEXT("ClassDefaultObject.")))
			{
				OutEdit.Writer = ERPGIdReferenceMigrationWriter::BlueprintDefaultProperty;
			}
			else if (Hit.PropertyPath.StartsWith(TEXT("Template[")))
			{
				OutEdit.Writer = ERPGIdReferenceMigrationWriter::BlueprintTemplateProperty;
			}
			else if (Hit.PropertyPath.StartsWith(TEXT("Graph[")))
			{
				OutEdit.Writer = ERPGIdReferenceMigrationWriter::BlueprintGraphLiteral;
			}
			else
			{
				return false;
			}
			OutEdit.Artifact = PackageArtifact(PackageName);
			return true;
		}

		if (!TryPackageFromObjectPath(Hit.Source, PackageName))
		{
			return false;
		}
		OutEdit.Writer = ERPGIdReferenceMigrationWriter::NativeObjectProperty;
		OutEdit.Artifact = PackageArtifact(PackageName);
		return true;
	}

	bool EditLess(const FRPGIdReferenceMigrationEdit& A, const FRPGIdReferenceMigrationEdit& B)
	{
		return A.Source == B.Source ? A.PropertyPath < B.PropertyPath : A.Source < B.Source;
	}

	bool ArtifactLess(const FRPGIdReferenceMigrationArtifact& A, const FRPGIdReferenceMigrationArtifact& B)
	{
		return A.Kind == B.Kind ? A.Identifier < B.Identifier : A.Kind < B.Kind;
	}

	ERPGIdReferenceMigrationArtifactState InspectWorkspaceArtifact(const FRPGIdReferenceMigrationArtifact& Artifact)
	{
		FString Filename = Artifact.Identifier;
		if (Artifact.Kind == ERPGIdReferenceMigrationArtifactKind::Package
			&& !FPackageName::DoesPackageExist(Artifact.Identifier, &Filename))
		{
			return ERPGIdReferenceMigrationArtifactState::Missing;
		}
		if (Artifact.Kind == ERPGIdReferenceMigrationArtifactKind::ConfigFile && !IFileManager::Get().FileExists(*Filename))
		{
			return ERPGIdReferenceMigrationArtifactState::Missing;
		}
		return IFileManager::Get().IsReadOnly(*Filename)
			? ERPGIdReferenceMigrationArtifactState::ReadOnly : ERPGIdReferenceMigrationArtifactState::Writable;
	}
}

FRPGIdReferenceMigrationPlan FRPGIdReferenceMigrationPlanner::Build(const FRPGIdReferenceMigrationRequest& Request)
{
	TMap<FSoftObjectPath, FName> Owners;
	const FRPGIdClaimResult Ownership = RPGIdClaimPrivate::ReadOwners(Owners);
	if (!Ownership.CanApply())
	{
		FRPGIdReferenceMigrationPlan Plan;
		Plan.OldId = Request.OldId;
		Plan.NewId = Request.NewId;
		Plan.ExpectedOwner = Request.ExpectedOwner;
		AddIssue(Plan, ERPGIdReferenceMigrationIssueCode::WorkspaceInspectionFailed, Ownership.Message.ToString());
		Plan.Summary = LOCTEXT("OwnershipFailed", "Reference migration planning failed before audit because ownership could not be inspected.");
		return Plan;
	}

	const FRPGIdReferenceAuditResult Audit = RPGIdReferencePrivate::AuditDevelopmentReferences(Request.OldId);
	FRPGReleaseHistory History;
	FText ReleaseError;
	if (!FRPGReleaseSealService::ReadHistory(History, ReleaseError))
	{
		FRPGIdReferenceMigrationPlan Plan;
		Plan.OldId = Request.OldId;
		Plan.NewId = Request.NewId;
		Plan.ExpectedOwner = Request.ExpectedOwner;
		AddIssue(Plan, ERPGIdReferenceMigrationIssueCode::ReleaseHistoryInvalid, ReleaseError.ToString());
		Plan.Summary = LOCTEXT("ReleaseHistoryFailed", "Reference migration planning failed because Release history could not be verified.");
		return Plan;
	}
	FRPGIdReferenceMigrationReleaseState ReleaseState;
	ReleaseState.CurrentIds = History.CurrentIds;
	ReleaseState.ReservedIds = History.ReservedIds;
	return BuildFromEvidence(Request, Owners, ReleaseState, Audit, InspectWorkspaceArtifact);
}

FRPGIdReferenceMigrationPlan FRPGIdReferenceMigrationPlanner::BuildFromEvidence(const FRPGIdReferenceMigrationRequest& Request,
	const TMap<FSoftObjectPath, FName>& Owners, const FRPGIdReferenceMigrationReleaseState& ReleaseState,
	const FRPGIdReferenceAuditResult& Audit,
	TFunctionRef<ERPGIdReferenceMigrationArtifactState(const FRPGIdReferenceMigrationArtifact&)> InspectArtifact)
{
	FRPGIdReferenceMigrationPlan Plan;
	Plan.OldId = Request.OldId;
	Plan.NewId = Request.NewId;
	Plan.ExpectedOwner = Request.ExpectedOwner;
	Plan.ReferenceIndexGeneration = Audit.IndexGeneration;

	if (!Request.OldId.IsValid() || !Request.NewId.IsValid() || Request.OldId == Request.NewId || Request.ExpectedOwner.IsNull()
		|| Request.OldId.GetRebuiltIdType().IsNone() || Request.OldId.GetRebuiltIdType() != Request.NewId.GetRebuiltIdType())
	{
		AddIssue(Plan, ERPGIdReferenceMigrationIssueCode::InvalidRedirect,
			TEXT("Old and new Ids must be distinct, valid, same-type values with one expected owner."));
	}
	if (!ReleaseState.CurrentIds.Contains(Request.OldId.Id))
	{
		AddIssue(Plan, ERPGIdReferenceMigrationIssueCode::OldIdNotCurrentRelease,
			TEXT("The old Id is not a Claim in the canonical current Release."));
	}
	if (ReleaseState.ReservedIds.Contains(Request.NewId.Id))
	{
		AddIssue(Plan, ERPGIdReferenceMigrationIssueCode::NewIdReserved,
			TEXT("The new Id is reserved by committed or prepared Release history."));
	}

	TArray<FSoftObjectPath> OldOwners;
	TArray<FSoftObjectPath> NewOwners;
	for (const TPair<FSoftObjectPath, FName>& Owner : Owners)
	{
		if (Owner.Value == Request.OldId.Id)
		{
			OldOwners.Add(Owner.Key);
		}
		if (Owner.Value == Request.NewId.Id)
		{
			NewOwners.Add(Owner.Key);
		}
	}
	if (OldOwners.Num() != 1 || OldOwners[0] != Request.ExpectedOwner)
	{
		AddIssue(Plan, ERPGIdReferenceMigrationIssueCode::OldOwnerMismatch,
			TEXT("The old Id must have exactly one owner and it must match the requested owner."));
	}
	if (!NewOwners.IsEmpty())
	{
		AddIssue(Plan, ERPGIdReferenceMigrationIssueCode::NewIdClaimed,
			TEXT("The new Id is already claimed in the current workspace."));
	}

	for (const FText& Gap : Audit.CoverageGaps)
	{
		AddIssue(Plan, ERPGIdReferenceMigrationIssueCode::CoverageGap, Gap.ToString());
	}

	TSet<FString> SeenReferences;
	for (const FRPGIdReferenceHit& Hit : Audit.Hits)
	{
		const FString ReferenceKey = Hit.Source + TEXT("\n") + Hit.PropertyPath;
		if (SeenReferences.Contains(ReferenceKey))
		{
			continue;
		}
		SeenReferences.Add(ReferenceKey);

		FRPGIdReferenceMigrationEdit Edit;
		if (!TryBuildEdit(Hit, Edit))
		{
			AddIssue(Plan, ERPGIdReferenceMigrationIssueCode::UnsupportedReference,
				TEXT("No certified writer adapter accepts this exact reference evidence."), Hit.Source, Hit.PropertyPath);
			continue;
		}
		Edit.ExpectedValue = Request.OldId;
		Edit.ReplacementValue = Request.NewId;
		Plan.Edits.Add(MoveTemp(Edit));
	}
	Plan.Edits.Sort(EditLess);

	TSet<FRPGIdReferenceMigrationArtifact> Artifacts;
	FString OwnerPackage;
	if (TryPackageFromObjectPath(Request.ExpectedOwner.ToString(), OwnerPackage))
	{
		Artifacts.Add(PackageArtifact(OwnerPackage));
	}
	else
	{
		AddIssue(Plan, ERPGIdReferenceMigrationIssueCode::ArtifactMissing,
			TEXT("The expected owner does not identify a supported workspace package."), Request.ExpectedOwner.ToString());
	}
	for (const FRPGIdReferenceMigrationEdit& Edit : Plan.Edits)
	{
		Artifacts.Add(Edit.Artifact);
	}
	Plan.RequiredArtifacts = Artifacts.Array();
	Plan.RequiredArtifacts.Sort(ArtifactLess);

	for (const FRPGIdReferenceMigrationArtifact& Artifact : Plan.RequiredArtifacts)
	{
		switch (InspectArtifact(Artifact))
		{
		case ERPGIdReferenceMigrationArtifactState::Writable:
			break;
		case ERPGIdReferenceMigrationArtifactState::ReadOnly:
			AddIssue(Plan, ERPGIdReferenceMigrationIssueCode::CheckoutRequired,
				TEXT("The required artifact is read-only; check it out or make it writable, then rebuild the plan."), Artifact.Identifier);
			break;
		case ERPGIdReferenceMigrationArtifactState::Missing:
			AddIssue(Plan, ERPGIdReferenceMigrationIssueCode::ArtifactMissing,
				TEXT("The required artifact does not exist on disk."), Artifact.Identifier);
			break;
		default:
			checkNoEntry();
			break;
		}
	}

	Plan.Summary = Plan.IsReady()
			? FText::Format(LOCTEXT("Ready",
				"Reference migration plan is complete: {0} certified edit(s) across {1} writable artifact(s). No data was changed."),
			FText::AsNumber(Plan.Edits.Num()), FText::AsNumber(Plan.RequiredArtifacts.Num()))
		: FText::Format(LOCTEXT("Blocked", "Reference migration plan is blocked by {0} issue(s). No data was changed."),
			FText::AsNumber(Plan.Issues.Num()));
	return Plan;
}

#undef LOCTEXT_NAMESPACE

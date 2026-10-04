// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RPGIdReferenceAudit.h"

enum class ERPGIdReferenceMigrationWriter : uint8
{
	NativeObjectProperty,
	BlueprintDefaultProperty,
	BlueprintTemplateProperty,
	BlueprintGraphLiteral,
	TypedConfigProperty,
	LegacyConfigProperty
};

enum class ERPGIdReferenceMigrationArtifactKind : uint8
{
	Package,
	ConfigFile
};

enum class ERPGIdReferenceMigrationArtifactState : uint8
{
	Writable,
	ReadOnly,
	Missing
};

struct FRPGIdReferenceMigrationArtifact
{
	ERPGIdReferenceMigrationArtifactKind Kind = ERPGIdReferenceMigrationArtifactKind::Package;
	FString Identifier;

	bool operator==(const FRPGIdReferenceMigrationArtifact& Other) const
	{
		return Kind == Other.Kind && Identifier == Other.Identifier;
	}
};

FORCEINLINE uint32 GetTypeHash(const FRPGIdReferenceMigrationArtifact& Artifact)
{
	return HashCombine(GetTypeHash(Artifact.Kind), GetTypeHash(Artifact.Identifier));
}

struct FRPGIdReferenceMigrationEdit
{
	ERPGIdReferenceMigrationWriter Writer = ERPGIdReferenceMigrationWriter::NativeObjectProperty;
	FString Source;
	FString PropertyPath;
	FRPGId ExpectedValue;
	FRPGId ReplacementValue;
	FRPGIdReferenceMigrationArtifact Artifact;
};

enum class ERPGIdReferenceMigrationIssueCode : uint8
{
	InvalidRedirect,
	WorkspaceInspectionFailed,
	ReleaseHistoryInvalid,
	OldIdNotCurrentRelease,
	NewIdReserved,
	OldOwnerMismatch,
	NewIdClaimed,
	CoverageGap,
	UnsupportedReference,
	CheckoutRequired,
	ArtifactMissing
};

struct FRPGIdReferenceMigrationIssue
{
	ERPGIdReferenceMigrationIssueCode Code = ERPGIdReferenceMigrationIssueCode::InvalidRedirect;
	FString Detail;
	FString Source;
	FString PropertyPath;
};

struct FRPGIdReferenceMigrationRequest
{
	FRPGId OldId;
	FRPGId NewId;
	FSoftObjectPath ExpectedOwner;
};

struct FRPGIdReferenceMigrationReleaseState
{
	TSet<FName> CurrentIds;
	TSet<FName> ReservedIds;
};

struct FRPGIdReferenceMigrationPlan
{
	FRPGId OldId;
	FRPGId NewId;
	FSoftObjectPath ExpectedOwner;
	uint64 ReferenceIndexGeneration = 0;
	TArray<FRPGIdReferenceMigrationEdit> Edits;
	TArray<FRPGIdReferenceMigrationArtifact> RequiredArtifacts;
	TArray<FRPGIdReferenceMigrationIssue> Issues;
	FText Summary;

	bool IsReady() const { return Issues.IsEmpty(); }
};

/** Builds a read-only, deterministic plan. It never checks out, modifies, saves or dirties an artifact. */
class FRPGIdReferenceMigrationPlanner
{
public:
	static FRPGIdReferenceMigrationPlan Build(const FRPGIdReferenceMigrationRequest& Request);

	static FRPGIdReferenceMigrationPlan BuildFromEvidence(const FRPGIdReferenceMigrationRequest& Request,
		const TMap<FSoftObjectPath, FName>& Owners, const FRPGIdReferenceMigrationReleaseState& ReleaseState,
		const FRPGIdReferenceAuditResult& Audit,
		TFunctionRef<ERPGIdReferenceMigrationArtifactState(const FRPGIdReferenceMigrationArtifact&)> InspectArtifact);
};

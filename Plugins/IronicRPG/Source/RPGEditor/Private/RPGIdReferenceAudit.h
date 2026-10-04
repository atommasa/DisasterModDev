// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RPGId.h"

struct FRPGIdReferenceHit
{
	FString Source;
	FString PropertyPath;
};

struct FRPGIdReferenceAuditResult
{
	uint64 IndexGeneration = 0;
	int32 BlueprintSynchronousLoads = 0;
	int32 BlueprintOverlayCount = 0;
	int32 BlueprintCachedCount = 0;
	FText Summary;
	TArray<FRPGIdReferenceHit> Hits;
	TArray<FText> CoverageGaps;
};

class FConfigFile;
class UBlueprint;
class UWorld;

namespace RPGIdReferencePrivate
{
	/** Canonical, target-independent RPG Id evidence extracted from one authored source. */
	struct FRPGIdReferenceRecord
	{
		FRPGId Id;
		FString Source;
		FString PropertyPath;
	};

	enum class ERPGIdReferenceIssueCode : uint8
	{
		BlueprintCompileState,
		BlueprintGeneratedClass,
		BlueprintClassDefaultObject,
		MalformedBlueprintGraphPin
	};

	/** Structured fail-closed evidence. Presentation remains the responsibility of the target query boundary. */
	struct FRPGIdReferenceIssue
	{
		ERPGIdReferenceIssueCode Code = ERPGIdReferenceIssueCode::BlueprintCompileState;
		FString Source;
		FString Detail;
	};

	struct FRPGIdBlueprintEvidence
	{
		TArray<FRPGIdReferenceRecord> References;
		TArray<FRPGIdReferenceIssue> Issues;
	};

	/** Explicit, read-only development reference audit. It may load native RPGPrimaryAsset data assets but never saves or writes them. */
	enum class ERPGIdAuditMode : uint8 { Automatic, Direct, Shadow, Indexed };
	FRPGIdReferenceAuditResult AuditDevelopmentReferences(const FRPGId& Target, ERPGIdAuditMode Mode = ERPGIdAuditMode::Automatic);

	/** Reflection seam used by the audit and container-shape automation tests. */
	void ScanStruct(const void* Container, const UStruct& Struct, const FString& Source, const FString& Prefix, const FRPGId& Target,
		TArray<FRPGIdReferenceHit>& OutHits);

	/** Read-only Level adapter seam. Standard World Partition descriptors are force-loaded for the duration of the scan. */
	void ScanWorld(UWorld& World, const FRPGId& Target, TArray<FRPGIdReferenceHit>& OutHits, TArray<FText>& OutCoverageGaps);

	/** Read-only Blueprint adapter seam. Scans generated-class defaults/templates and unconnected static FRPGId graph literals. */
	void ScanBlueprint(UBlueprint& Blueprint, const FRPGId& Target, TArray<FRPGIdReferenceHit>& OutHits, TArray<FText>& OutCoverageGaps);

	/** Extracts all Blueprint RPG Id references and structured issues once, independently of any query target. */
	FRPGIdBlueprintEvidence ExtractBlueprintEvidence(UBlueprint& Blueprint);

	/** Projects canonical Blueprint evidence onto the existing target-specific audit result contract. */
	void AppendBlueprintEvidence(const FRPGIdBlueprintEvidence& Evidence, const FRPGId& Target,
		TArray<FRPGIdReferenceHit>& OutHits, TArray<FText>& OutCoverageGaps);

	/** Adds a fail-closed gap for every on-disk EDL actor package that was not reached through a registered descriptor container. */
	void AppendExternalDataLayerCoverageGaps(const TSet<FName>& OnDiskPackages, const TSet<FName>& ScannedPackages, TArray<FText>& OutCoverageGaps);

	/** Read-only adapter for the retired RPGSettings Id-bearing config keys. Malformed known entries fail closed. */
	void ScanLegacyConfig(const FConfigFile& Config, const FRPGId& Target, TArray<FRPGIdReferenceHit>& OutHits, TArray<FText>& OutCoverageGaps);
}

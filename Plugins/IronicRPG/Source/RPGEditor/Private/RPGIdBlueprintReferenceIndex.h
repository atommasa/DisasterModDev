// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RPGIdReferenceAudit.h"

namespace RPGIdReferenceIndexPrivate
{
	struct FRPGIdReferenceIndexCatalogItem
	{
		FName PackageName;
		FString AssetPath;
		FString PackageHash;
	};

	struct FRPGIdReferenceIndexNativeModule
	{
		FName ModuleName;
		FString BinaryHash;
	};

	struct FRPGIdReferenceIndexFingerprintInput
	{
		FString EngineIdentity;
		TArray<FString> MountRoots;
		TArray<FRPGIdReferenceIndexCatalogItem> Catalog;
		TArray<FRPGIdReferenceIndexNativeModule> NativeModules;
	};

	struct FRPGIdReferenceIndexEntry
	{
		FName PackageName;
		FString AssetPath;
		RPGIdReferencePrivate::FRPGIdBlueprintEvidence Evidence;
	};

	struct FRPGIdReferenceIndexCache
	{
		FString WorkspaceFingerprint;
		FDateTime BuildTime;
		TArray<FName> NativeModules;
		TArray<FRPGIdReferenceIndexEntry> Entries;
	};

	struct FRPGIdReferenceIndexShadowComparison
	{
		bool bExact = false;
		FString Difference;
	};

	enum class ERPGIdReferenceIndexLoadResult : uint8
	{
		Ready,
		Missing,
		SchemaMismatch,
		Corrupt
	};

	enum class ERPGIdReferenceIndexState : uint8
	{
		Unavailable,
		Building,
		Ready,
		Stale
	};

	struct FRPGIdReferenceIndexStatus
	{
		ERPGIdReferenceIndexState State = ERPGIdReferenceIndexState::Unavailable;
		int32 IndexedPackageCount = 0;
		int32 TotalPackageCount = 0;
		int32 SuccessfulShadowComparisonCount = 0;
		FDateTime LastBuildTime;
		FDateTime LastShadowComparisonTime;
		FString LastShadowTarget;
		FString LastFailure;

		bool IsReady() const
		{
			return State == ERPGIdReferenceIndexState::Ready;
		}
	};

	/** Fail-closed state transition model shared by production lifecycle and Automation. */
	class FRPGIdReferenceIndexStateModel
	{
	public:
		void Bootstrap(ERPGIdReferenceIndexLoadResult LoadResult, const FRPGIdReferenceIndexCache* Cache,
			const FString& CurrentFingerprint, int32 TotalPackageCount, const FString& LoadError);
		void BeginBuild(int32 TotalPackageCount);
		void RecordIndexedPackage();
		void CompleteBuild();
		void FailBuild(const FString& Reason);
		void CancelBuild();
		void Invalidate(const FString& Reason);
		void RecordShadowSuccess(const FRPGId& Target);
		void RecordShadowFailure(const FRPGId& Target, const FString& Reason);
		const FRPGIdReferenceIndexStatus& GetStatus() const;

	private:
		FRPGIdReferenceIndexStatus Status;
	};

	/** Collects one immutable exact-catalog build and refuses partial, dirty or duplicate evidence. */
	class FRPGIdReferenceIndexBuildAccumulator
	{
	public:
		FRPGIdReferenceIndexBuildAccumulator(const FString& WorkspaceFingerprint,
			const TArray<FRPGIdReferenceIndexCatalogItem>& Catalog, const TArray<FName>& NativeModules);
		bool AddEntry(FName PackageName, const FString& AssetPath, const RPGIdReferencePrivate::FRPGIdBlueprintEvidence& Evidence,
			bool bPackageDirty, FString& OutError);
		bool Finalize(FRPGIdReferenceIndexCache& OutCache, FString& OutError) const;

	private:
		FRPGIdReferenceIndexCache Cache;
		TMap<FName, FString> ExpectedAssets;
		TSet<FName> CompletedPackages;
	};

	/** Produces a deterministic workspace-wide identity from canonical catalog and native binary inputs. */
	FString BuildWorkspaceFingerprint(const FRPGIdReferenceIndexFingerprintInput& Input);
	/** Reads every byte on each call; file timestamps and sizes are not freshness evidence. */
	bool HashNativeBinary(const FString& Filename, FString& OutHash, FString& OutError);

	/** Compares canonical persisted evidence with a freshly extracted direct catalog. */
	FRPGIdReferenceIndexShadowComparison CompareBlueprintEvidence(const TArray<FRPGIdReferenceIndexEntry>& IndexedEntries,
		const TArray<FRPGIdReferenceIndexEntry>& DirectEntries);

	/** Writes a canonical cache to a verified temporary file before replacing the destination. */
	bool SaveCacheAtomically(const FString& CachePath, const FRPGIdReferenceIndexCache& Cache, FString& OutError);

	/** Loads only exact-schema, checksum-valid cache data. */
	ERPGIdReferenceIndexLoadResult LoadCache(const FString& CachePath, FRPGIdReferenceIndexCache& OutCache, FString& OutError);
}

/** Editor-local Blueprint evidence index. Interactive audits verify the saved snapshot and replace loaded evidence; commandlets use direct scans. */
class FRPGIdBlueprintReferenceIndex
{
public:
	static void Register();
	static void Unregister();
	static RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexStatus GetStatus();
	static void RequestRebuild();
	static void CancelBuild();
	static void NotifyNativeCodeChanged();
	static bool QueryExactSnapshot(RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexCache& OutCache, FString& OutError);
	static bool RecordShadowComparison(const FRPGId& Target, const FString& ExpectedWorkspaceFingerprint,
		const RPGIdReferenceIndexPrivate::FRPGIdReferenceIndexShadowComparison& Comparison);
	static uint64 GetGeneration();
	static bool IsGenerationCurrent(uint64 Generation);
	static FString GetCachePath();
};

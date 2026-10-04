// Copyright Ironic Studio. All Rights Reserved.
#pragma once

#include "Assets/RPGReleaseManifest.h"
#include "DataTypes/RPGId.h"

struct FRPGReleaseHistoryEntry
{
	FSoftObjectPath Path;
	FRPGReleaseSnapshot Snapshot;
	FString Hash;
};

struct FRPGReleaseHistory
{
	TArray<FRPGReleaseHistoryEntry> Entries;
	TSet<FName> ReservedIds;
	TSet<FName> CurrentIds;
	TSet<FName> RetiredIds;
	int32 PreparedIndex = INDEX_NONE;
};

enum class ERPGReleaseSealAction : uint8 { Preview, Seal, Resume, Abort };

class FRPGReleaseSealService
{
	friend class FRPGReleasePersistenceTest;

public:
	static void Register();
	static void Unregister();
	static bool Run(ERPGReleaseSealAction Action, const FString& ReleaseId, const FString& SaveVersion, FText& OutMessage);
	static bool Run(ERPGReleaseSealAction Action, const FString& ReleaseId, const FString& SaveVersion,
		TConstArrayView<FRPGIdRedirect> Redirects, FText& OutMessage);
	static bool ReadHistory(FRPGReleaseHistory& OutHistory, FText& OutError);
	static bool EvaluateHistory(FRPGReleaseHistory& History, const FString& HeadPath, const FString& HeadHash, FText& OutError);
	static bool CheckMutation(const FRPGId& Previous, const FRPGId& Candidate, FText& OutError);
	static bool ValidateHistory(FText& OutError);
	static bool CanDelete(const TArray<UObject*>& Objects, FText& OutError);

private:
	static URPGReleaseManifest* SavePrepared(const FRPGReleaseSnapshot& Snapshot, const FString& PackageName, FText& OutError);
	static bool CommitHeadFile(const FString& Filename, const FString& HeadPath, const FString& HeadHash, FText& OutError);
	static bool CommitHead(URPGReleaseManifest& Manifest, FText& OutError);
};

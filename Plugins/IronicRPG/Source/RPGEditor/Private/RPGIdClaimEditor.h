// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RPGId.h"

struct FRPGIdCategorySelection;

enum class ERPGIdClaimResult : uint8
{
	Success, NoChange, Unclaimed, InvalidFormatOrType, Collision, InspectionIncomplete,
	StaleRequest, UnsupportedOwner, ReferencesUnsupported, NotEditable, NoAvailableId
};

struct FRPGIdClaimRequest
{
	TArray<TWeakObjectPtr<UObject>> Owners;
	FRPGId ExpectedId;
	FRPGId Candidate;
	uint64 ExpectedIndexGeneration = 0;
};

struct FRPGIdClaimResult
{
public:
	bool CanApply() const { return Code == ERPGIdClaimResult::Success || Code == ERPGIdClaimResult::NoChange; }

public:
	ERPGIdClaimResult Code = ERPGIdClaimResult::InspectionIncomplete;
	FText Message;
	FText Context;
	TArray<FSoftObjectPath> Conflicts;
	TArray<FString> References;
	FRPGId SuggestedId;
	uint64 IndexGeneration = 0;
};

/** Editor-only Claim seam. Reads never mutate assets; Execute always repeats Preview on the game thread.
 * Ordinary assets support initial Claim and unsealed, clean, zero-reference Change/Clear. GameZone changes use the binding coordinator adapter.
 * Results describe current workspace ownership and supported development references. Successful ordinary mutation changes only the owner in memory
 * and remains pending until the owner is saved and the old Id passes a fresh complete audit. Old player saves remain out of scope.
 */
class FRPGIdClaimEditor
{
public:
	static FRPGIdClaimResult Inspect(const TArray<TWeakObjectPtr<UObject>>& Owners);
	static FRPGIdClaimResult Preview(const FRPGIdClaimRequest& Request);
	static FRPGIdClaimResult Execute(const FRPGIdClaimRequest& Request);
	static FRPGIdClaimResult Suggest(const FRPGIdClaimRequest& Request, const FRPGIdCategorySelection* Selection = nullptr);
	/** Cheap presentation queries; these do not authorize mutation or perform an audit. */
	static bool SupportsExistingChange(const TArray<TWeakObjectPtr<UObject>>& Owners);
	static FText GetPolicyText(const TArray<TWeakObjectPtr<UObject>>& Owners);
};

// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "RPGIdClaimEditor.h"

class URPGPrimaryAsset;
class UGameZoneAsset;
struct FRPGIdReferenceHit;

enum class ERPGIdReferenceCapability : uint8
{
	Blocking, Migratable, CoordinatorManaged
};

/** Owner-specific hooks. The Claim service owns common validation and calls mutation only after fresh preflight. */
class FRPGIdClaimAdapter
{
public:
	virtual ~FRPGIdClaimAdapter() = default;
	virtual bool SupportsExistingChange() const { return false; }
	virtual bool UsesPendingReservations() const { return true; }
	virtual FText GetPolicyText() const;
	virtual FText DescribeReferencePreflight(bool bBlocked, int32 ManagedCount) const;
	virtual FRPGIdClaimResult CheckOperation(const FRPGIdClaimRequest& Request) const;
	virtual FRPGIdClaimResult PreviewOwner(const URPGPrimaryAsset& Owner, const FRPGId& Candidate, bool bChanging) const;
	virtual ERPGIdReferenceCapability ClassifyReference(const URPGPrimaryAsset& Owner, const FRPGIdReferenceHit& Hit) const;
	virtual FRPGIdClaimResult Apply(URPGPrimaryAsset& Owner, const FRPGIdClaimRequest& Request,
		TFunctionRef<FRPGIdClaimResult()> ApplyOwnerClaim) const;
};

namespace RPGIdClaimPrivate
{
	/** Exact native-class registry. No fallback for derived, unknown or frozen legacy owners. */
	const FRPGIdClaimAdapter* FindAdapter(const UClass* OwnerClass);
	/** Only references rewritten by the exact GameZone coordinator/Bake may pass. */
	bool IsCoordinatorManagedZoneReference(const UGameZoneAsset& ZoneAsset, const FRPGIdReferenceHit& Hit);
}

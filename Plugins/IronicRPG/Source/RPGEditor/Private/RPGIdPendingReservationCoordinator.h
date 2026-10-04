// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RPGId.h"

class URPGPrimaryAsset;

/**
 * Keeps a saved Id reserved while an ordinary owner has a different in-memory identity.
 * The saved Asset Registry tag is the baseline; transactions and successful saves are reconciled from that fact.
 */
class FRPGIdPendingReservationCoordinator
{
	friend class FRPGIdPendingReservationTest;

public:
	static void Register();
	static void Unregister();
	static void RefreshOwner(const URPGPrimaryAsset& Owner);
	static bool ReadReservedIds(TSet<FName>& OutIds, FText& OutError);
	static bool CheckCandidate(const URPGPrimaryAsset* RequestOwner, const FRPGId& Candidate, FText& OutError);
	static bool CheckOwnerReadyForMutation(const URPGPrimaryAsset& Owner, FText& OutError);
	static bool CheckWorkspaceClean(FText& OutError);

private:
	static void ResetForTests();
};

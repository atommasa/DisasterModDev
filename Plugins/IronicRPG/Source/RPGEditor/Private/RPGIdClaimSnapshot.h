// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "RPGIdClaimEditor.h"
#include "AssetRegistry/AssetData.h"

namespace RPGIdClaimPrivate
{
	// Full workspace snapshot used by Claim checks and release validation. Loaded values replace disk values.
	FRPGIdClaimResult ReadOwners(TMap<FSoftObjectPath, FName>& OutOwners);

	// Internal read seam: loaded values replace disk values for the same object, including None.
	FRPGIdClaimResult MergeOwners(const TArray<FAssetData>& Saved, const TMap<FSoftObjectPath, FName>& Loaded,
		TMap<FSoftObjectPath, FName>& OutOwners);
}

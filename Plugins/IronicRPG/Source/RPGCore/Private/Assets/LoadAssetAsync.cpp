// Copyright Ironic Studio. All Rights Reserved.


#include "Assets/LoadAssetAsync.h"
#include "Assets/RPGAssetLibrary.h"

ULoadAssetAsync* ULoadAssetAsync::LoadAssetAsync(const FRPGId& InId, const TArray<FName> InBundles)
{
	ULoadAssetAsync* BPNode = NewObject<ULoadAssetAsync>();
	BPNode->Id = InId;
	BPNode->Bundles = InBundles;

	return BPNode;
}

void ULoadAssetAsync::Activate()
{
	URPGAssetLibrary::LoadAssetByRPGIdAsync(Id, Bundles, [this](URPGPrimaryAsset* Asset)
		{
			if (Asset)
			{
				OnSuccess.Broadcast(Asset);
			}
			else
			{
				OnFailure.Broadcast(nullptr);
			}
		});
}

// Copyright Ironic Studio. All Rights Reserved.


#include "NarrativeEditorSubsystem.h"
#include "Engine/AssetManager.h"
#include "Characters/CharacterAsset.h"

void UNarrativeEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().OnAssetAdded().AddUObject(this, &UNarrativeEditorSubsystem::OnAssetAdded);
	AssetRegistryModule.Get().OnAssetRemoved().AddUObject(this, &UNarrativeEditorSubsystem::OnAssetRemoved);
}

void UNarrativeEditorSubsystem::Deinitialize()
{
	Super::Deinitialize();

	UE_LOG(LogTemp, Warning, TEXT("NarrativeEditorSubsystem Deinitialized!"));
}

TMap<FRPGId, UCharacterAsset*> UNarrativeEditorSubsystem::TryGetSpeakerAssets()
{
	if (bHasInitializedSpeakerAssets)
	{
		return SpeakerAssetMap;
	}

	TArray<FPrimaryAssetId> CharacterAssets;
	UAssetManager::Get().GetPrimaryAssetIdList(
		FPrimaryAssetType(UCharacterAsset::GetAssetTypeStatic()),
		CharacterAssets
	);

	// Using the Asset Manager to preload all character assets
	TSharedPtr<FStreamableHandle> Handle = UAssetManager::Get().PreloadPrimaryAssets(
		CharacterAssets,
		{},
		true,  // bLoadRecursive to ensure all dependencies are loaded
		FStreamableDelegate(),  // Delegate to call when loading is complete
		0      // Priority
	);

	if (Handle.IsValid())
	{
		Handle->WaitUntilComplete();  // Wait for the assets to be loaded
	}

	// Iterate through the loaded character assets and populate the maps
	for (const FPrimaryAssetId& Id : CharacterAssets)
	{
		auto* Asset = Cast<UCharacterAsset>(UAssetManager::Get().GetPrimaryAssetObject(Id));
		if (Asset)
		{
			SpeakerAssetMap.Add(Asset->GetId(), Asset);
		}
	}

	bHasInitializedSpeakerAssets = true;
	return SpeakerAssetMap;
}

void UNarrativeEditorSubsystem::OnAssetAdded(const FAssetData& AssetData)
{
	if (AssetData.AssetClassPath == UCharacterAsset::StaticClass()->GetClassPathName())
	{
		auto* Asset = Cast<UCharacterAsset>(AssetData.GetAsset());
		if (Asset && Asset->GetId().IsValid() && !SpeakerAssetMap.Contains(Asset->GetId()))
		{
			SpeakerAssetMap.Add(Asset->GetId(), Asset);
		}
	}
}

void UNarrativeEditorSubsystem::OnAssetRemoved(const FAssetData& AssetData)
{
	if (AssetData.AssetClassPath == UCharacterAsset::StaticClass()->GetClassPathName())
	{
		auto* Asset = Cast<UCharacterAsset>(AssetData.GetAsset());
		if (Asset && SpeakerAssetMap.Contains(Asset->GetId()))
		{
			SpeakerAssetMap.Remove(Asset->GetId());

			for (auto It = SpeakerAssetMap.CreateIterator(); It; ++It)
			{
				if (!IsValid(It.Value()))
				{
					It.RemoveCurrent();
				}
			}
		}
	}
}

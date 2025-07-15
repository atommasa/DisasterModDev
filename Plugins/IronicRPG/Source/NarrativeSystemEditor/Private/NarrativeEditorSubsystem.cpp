// Fill out your copyright notice in the Description page of Project Settings.


#include "NarrativeEditorSubsystem.h"
#include "Engine/AssetManager.h"
#include "Characters/CharacterPrimaryAsset.h"

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

TMap<FRPGId, UCharacterPrimaryAsset*> UNarrativeEditorSubsystem::TryGetSpeakerAssets()
{
	if (bHasInitializedSpeakerAssets)
	{
		return SpeakerAssetMap;
	}

	TArray<FPrimaryAssetId> CharacterAssets;
	UAssetManager::Get().GetPrimaryAssetIdList(
		FPrimaryAssetType(UCharacterPrimaryAsset::CharacterAssetType),
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
		auto* Asset = Cast<UCharacterPrimaryAsset>(UAssetManager::Get().GetPrimaryAssetObject(Id));
		if (Asset)
		{
			SpeakerAssetMap.Add(Asset->Id, Asset);
		}
	}

	bHasInitializedSpeakerAssets = true;
	return SpeakerAssetMap;
}

void UNarrativeEditorSubsystem::OnAssetAdded(const FAssetData& AssetData)
{
	if (AssetData.AssetClassPath == UCharacterPrimaryAsset::StaticClass()->GetClassPathName())
	{
		auto* Asset = Cast<UCharacterPrimaryAsset>(AssetData.GetAsset());
		if (Asset && Asset->Id.IsValid() && !SpeakerAssetMap.Contains(Asset->Id))
		{
			SpeakerAssetMap.Add(Asset->Id, Asset);
		}
	}
}

void UNarrativeEditorSubsystem::OnAssetRemoved(const FAssetData& AssetData)
{
	if (AssetData.AssetClassPath == UCharacterPrimaryAsset::StaticClass()->GetClassPathName())
	{
		auto* Asset = Cast<UCharacterPrimaryAsset>(AssetData.GetAsset());
		if (Asset && SpeakerAssetMap.Contains(Asset->Id))
		{
			SpeakerAssetMap.Remove(Asset->Id);

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

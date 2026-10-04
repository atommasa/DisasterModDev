// Copyright Ironic Studio. All Rights Reserved.


#include "Assets/RPGAssetManager.h"
#include "Assets/RPGPrimaryAsset.h"

#include "GameplayTags/RPGGameplayTags.h"
#include "Engine/AssetManagerSettings.h"

DEFINE_LOG_CATEGORY(LogRPGAssetManager);

void URPGAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();
	
	ScanRPGAssetTypes();

	// Initialize native gameplay tags
	FRPGGameplayTags::InitializeNativeGameplayTags();

	// This is where you would typically start loading your initial assets.
	UE_LOG(LogRPGAssetManager, Warning, TEXT("RPGAssetManager: Starting initial loading..."));

	const UAssetManagerSettings* S = GetDefault<UAssetManagerSettings>();
	UE_LOG(LogTemp, Warning, TEXT("PrimaryAssetTypesToScan count = %d"), S->PrimaryAssetTypesToScan.Num());
	for (const auto& T : S->PrimaryAssetTypesToScan)
	{
		UE_LOG(LogTemp, Warning, TEXT("Type=%s BaseClass=%s"), *T.PrimaryAssetType.ToString(), *T.AssetBaseClassLoaded->GetName());
	}
}

void URPGAssetManager::ScanRPGAssetTypes()
{
	TArray<UClass*> Derived;
	GetDerivedClasses(URPGPrimaryAsset::StaticClass(), Derived, /*bRecursive=*/true);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	for (UClass* Class : Derived)
	{
		if (auto* CDO = Cast<URPGPrimaryAsset>(Class->GetDefaultObject()))
		{
			FName Type = CDO->GetAssetType();
			check(Type != NAME_None); // Be sure to add DEFINE_ASSET_TYPE in URPGPrimaryAsset child classes

			FString VirtualPath = TEXT("/Game/DataAssets/") / Type.ToString();
#if WITH_EDITOR
			FString PhysicalPath = FPaths::ProjectContentDir() / TEXT("DataAssets") / Type.ToString();
			if (!FPaths::DirectoryExists(PhysicalPath))
			{
				UE_LOG(LogTemp, Warning, TEXT("Asset path does not exist try to add folder..."));

				// Attempt to create the directory, including any necessary parent directories
				if (PlatformFile.CreateDirectoryTree(*PhysicalPath))
				{
					UE_LOG(LogTemp, Warning, TEXT("Folder created successfully at: %s"), *PhysicalPath);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to create folder at: %s"), *PhysicalPath);
					continue;
				}
			}

			FName Prefix = CDO->GetAssetIdPrefix();

			// Try add new Id prefixes
			if (!IdPrefixMap.Contains(Prefix))
			{
				IdPrefixMap.Add(Prefix, Type);

				UE_LOG(LogRPGAssetManager, Warning, TEXT("Successfully add type %s with prefix %s!"), *Type.ToString(), *Prefix.ToString());
			}

			if (!AssetTypeMap.Contains(Type))
			{
				AssetTypeMap.Add(Type, Class);
				UE_LOG(LogRPGAssetManager, Warning, TEXT("Successfully add type %s with class %s!"), *Type.ToString(), *Class->GetName());
			}
#endif // WITH_EDITOR
			int32 Num = ScanPathForPrimaryAssets(
				CDO->GetAssetType(),
				*VirtualPath,
				Class,
				false,
				false,
				true
			);

			UE_LOG(LogTemp, Warning, TEXT("%s: %d"), *CDO->GetAssetType().ToString(), Num);
		}
	}
#if WITH_EDITOR
	// Write new Id prefixes into ini file
	TryUpdateDefaultConfigFile(*GetDefaultConfigFilename());
#endif // WITH_EDITOR
}

TSharedPtr<FStreamableHandle> URPGAssetManager::LoadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToLoad, const TArray<FName>& LoadBundles, FAssetManagerLoadParams&& LoadParams, UE::FSourceLocation Location)
{
	auto Handle = Super::LoadPrimaryAssets(AssetsToLoad, LoadBundles, MoveTemp(LoadParams), Location);
	
	if (Handle.IsValid())
	{
		AddHandle(Handle);
	}

	return Handle;
}

void URPGAssetManager::StartTracking()
{
	bIsTrackingActive = true;
	ActiveHandles.Empty(); // Clear any existing handles before starting tracking.
}

void URPGAssetManager::AddHandle(TSharedPtr<FStreamableHandle> Handle)
{
	if (Handle.IsValid() && bIsTrackingActive)
	{
		ActiveHandles.Add(Handle);

		Handle->BindCompleteDelegate(FStreamableDelegate::CreateLambda([this, Handle]()
			{
				ActiveHandles.Remove(Handle);

				if (ActiveHandles.IsEmpty())
				{
					bIsTrackingActive = false; // Stop tracking if no active handles remain.
					OnAssetsLoadingComplete.Broadcast(); // Notify that loading is complete.
				}
			}));
	}
}

bool URPGAssetManager::IsLoadingComplete() const
{
	return ActiveHandles.IsEmpty();
}

float URPGAssetManager::GetLoadingProgress() const
{
	float TotalProgress = 0.0f;
	int32 Count = ActiveHandles.Num();

	for (const TSharedPtr<FStreamableHandle>& Handle : ActiveHandles)
	{
		if (Handle.IsValid())
		{
			TotalProgress += Handle->GetProgress();
		}
	}

	return Count != 0 ? TotalProgress / Count : 1.0f; // Average progress of all active handles.
}

void URPGAssetManager::LoadLevelAssets(const FPrimaryAssetId& PersistentLevelId, const TArray<FPrimaryAssetId>& SubLevelIds)
{
	
}

void URPGAssetManager::OnLevelAssetsLoaded(FPrimaryAssetId PersistentLevelId, TArray<FPrimaryAssetId> SubLevelIds)
{
	// This function can be used to handle any logic after the level assets are loaded.
	UE_LOG(LogTemp, Log, TEXT("Level assets loaded for Persistent Level: %s with %d Sub Levels."), *PersistentLevelId.ToString(), SubLevelIds.Num());

	// You can also trigger any events or callbacks here if needed.
	OnAssetsLoadingComplete.Broadcast();
}

FName URPGAssetManager::GetIdPrefix(const FName& IdType) const
{
	if (auto* Prefix = IdPrefixMap.FindKey(IdType))
	{
		return *Prefix;
	}
	else
	{
		return ID_None;
	}
}

FName URPGAssetManager::GetIdTypeFromPrefix(const FName& Prefix) const
{
	if (auto* Type = IdPrefixMap.Find(Prefix))
	{
		return *Type;
	}
	else
	{
		return ID_None;
	}
}

TArray<FName> URPGAssetManager::GetAllAssetTypes() const
{
	TArray<FName> IdTypes;
	IdPrefixMap.GenerateValueArray(IdTypes);

	return IdTypes;
}

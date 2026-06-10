// Copyright Ironic Studio. All Rights Reserved.


#include "GameZoneSubsystem.h"
#include "EngineUtils.h"

#include "RPGSettings.h"
#include "SaveGameSubsystem.h"
#include "LoadingScreenSubsystem.h"

# include "Kismet/GameplayStatics.h"
#include "Engine/LevelStreamingDynamic.h"
#include "GameFramework/PlayerStart.h"

#include "GameZoneSaveModule.h"
#include "Assets/RPGAssetManager.h"
#include "Levels/GameZoneAsset.h"
#include "Levels/RPGPlayerStart.h"

void UGameZoneSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

	Collection.ActivateExternalSubsystem(USaveGameSubsystem::StaticClass());
	Collection.ActivateExternalSubsystem(ULoadingScreenSubsystem::StaticClass());
}

void UGameZoneSubsystem::Deinitialize()
{
    Super::Deinitialize();

}

void UGameZoneSubsystem::EnterGameZone(const FGameZoneContext& NewGameZoneContext, TDelegate<void()> DelegateToCall)
{
	PendingContext = NewGameZoneContext;

	// Check if the new context's ZoneId (Persistent Level) is different from the current one
	// If it is different, we use AGameMode to handle the transition
	// If it is the same, we use UGameZoneSubsystem to handle the transition
	if (CurrentContext.ZoneId == NewGameZoneContext.ZoneId)
	{
		// Start loading the sub-zone if it is different from the current one
		LoadSubZone(NewGameZoneContext, TDelegate<void()>::CreateLambda([this, DelegateToCall]()
			{
				CurrentContext = PendingContext;
				DelegateToCall.ExecuteIfBound();
			}));
	}
	else
	{
		// If the ZoneId is different, we need to load the new zone and sub-zone
		LoadZone(NewGameZoneContext, TDelegate<void()>::CreateLambda([this, DelegateToCall]()
			{
				CurrentContext = PendingContext;
				DelegateToCall.ExecuteIfBound();
			}));
	}
}

void UGameZoneSubsystem::LoadZone(const FGameZoneContext& LoadContext, TDelegate<void()> DelegateToCall)
{
	const FRPGId& ZoneId = LoadContext.ZoneId;
	const TArray<FRPGId>& SubZoneIds = LoadContext.SubZoneIds;

	// Load the zone and sub-zone assets using the RPGAssetManager
	TArray<FPrimaryAssetId> AssetsToLoad;

	const FPrimaryAssetId ZoneAssetId = FPrimaryAssetId(UGameZoneAsset::GetAssetTypeStatic(), ZoneId.Id);
	AssetsToLoad.Add(ZoneAssetId);

	UE_LOG(LogTemp, Log, TEXT("Loading Zone Asset: %s"), *ZoneAssetId.ToString());

	for (const FRPGId& SubZoneId : SubZoneIds)
	{
		const FPrimaryAssetId SubZoneAssetId = FPrimaryAssetId(USubGameZoneAsset::GetAssetTypeStatic(), SubZoneId.Id);
		AssetsToLoad.Add(SubZoneAssetId);

		UE_LOG(LogTemp, Log, TEXT("Loading SubZone Asset: %s"), *SubZoneAssetId.ToString());
	}

	TArray<FName> BundlesToLoad;
	BundlesToLoad.Append(UGameZoneAsset::GetAssetBundles());
	BundlesToLoad.Append(USubGameZoneAsset::GetAssetBundles());

	FAssetManagerLoadParams LoadParams;
	LoadParams.OnComplete = FStreamableDelegateWithHandle::CreateLambda([this, ZoneAssetId, AssetsToLoad, DelegateToCall](TSharedPtr<struct FStreamableHandle>)
		{
			URPGAssetManager& Manager = URPGAssetManager::Get();

			// Load the level associated with the zone asset
			if (UGameZoneAsset* ZoneAsset = Cast<UGameZoneAsset>(Manager.GetPrimaryAssetObject(ZoneAssetId)))
			{
				if (!ZoneAsset || !ZoneAsset->LevelToLoad.IsValid())
				{
					UE_LOG(LogTemp, Warning, TEXT("Zone asset %s has no valid level to load!"), *ZoneAssetId.ToString());
					DelegateToCall.ExecuteIfBound();
					return;
				}

				const FString& LongPackageName = ZoneAsset->LevelToLoad.GetLongPackageName();

				bool bResult = false;
				CurrentStreaming = ULevelStreamingDynamic::LoadLevelInstance(
					GetWorld(),
					LongPackageName,
					FVector::ZeroVector,
					FRotator::ZeroRotator,
					bResult
				);
				
				if (CurrentStreaming)
				{
					CurrentStreaming->OnLevelShown.AddUniqueDynamic(this, &UGameZoneSubsystem::WaitUntilLevelActorInitialized);
				}
			}

			DelegateToCall.ExecuteIfBound();
		});

	URPGAssetManager& AssetManager = URPGAssetManager::Get();
	AssetManager.LoadPrimaryAssets(AssetsToLoad, BundlesToLoad, MoveTemp(LoadParams));
}

void UGameZoneSubsystem::LoadSubZone(const FGameZoneContext& LoadContext, TDelegate<void()> DelegateToCall)
{
	const TArray<FRPGId>& SubZoneIds = LoadContext.SubZoneIds;
	
	// Already in the requested sub-zone
	if (CurrentContext.SubZoneIds == SubZoneIds)
	{
		DelegateToCall.ExecuteIfBound();
		return;
	}

	// Load the sub-zone using the RPGAssetManager
	UAssetManager& AssetManager = UAssetManager::Get();

	TArray<FPrimaryAssetId> SubZoneAssetIds;
	for (const FRPGId& SubZoneId : SubZoneIds)
	{
		SubZoneAssetIds.Add(FPrimaryAssetId(USubGameZoneAsset::GetAssetTypeStatic(), SubZoneId.Id));
	}

	AssetManager.LoadPrimaryAssets(SubZoneAssetIds, USubGameZoneAsset::GetAssetBundles(), FStreamableDelegate::CreateLambda([&]()
		{
			URPGAssetManager& Manager = URPGAssetManager::Get();

			for (const FPrimaryAssetId& SubZoneAssetId : SubZoneAssetIds)
			{
				USubGameZoneAsset* SubZoneAsset = Cast<USubGameZoneAsset>(Manager.GetPrimaryAssetObject(SubZoneAssetId));
				if (SubZoneAsset && SubZoneAsset->LevelToLoad.IsValid())
				{
					FString LongName = SubZoneAsset->LevelToLoad.GetLongPackageName();
					UGameplayStatics::LoadStreamLevel(this, FPackageName::GetShortFName(*LongName), true, true, FLatentActionInfo());
				}
			}

			DelegateToCall.ExecuteIfBound();
		}));
}

void UGameZoneSubsystem::CreateStreamInstance(UWorld* World, const FString& LongPackageName, const FVector& Location, const FRotator& Rotation)
{
	if (!World)
	{
		return;
	}

	const FString ShortLevelName = FPackageName::GetShortName(LongPackageName);
	const FString PackagePath = FPackageName::GetLongPackagePath(LongPackageName);
	FString UniqueLevelPackageName = PackagePath / World->StreamingLevelsPrefix + ShortLevelName;

	// Setup streaming level object that will be used to load the level
	ULevelStreamingDynamic* StreamingLevel = NewObject<ULevelStreamingDynamic>(
		GetWorld(),
		ULevelStreamingDynamic::StaticClass(),
		NAME_None,
		RF_Transient,
		nullptr
	);
	StreamingLevel->SetWorldAssetByPackageName(FName(*UniqueLevelPackageName));
	StreamingLevel->LevelColor = FColor::MakeRandomColor();

	// Set the transform
	StreamingLevel->LevelTransform = FTransform(Rotation, Location);

	// Set the package name to load
	StreamingLevel->PackageNameToLoad = FName(*LongPackageName);

	// Add the streaming level to the world
	World->AddStreamingLevel(StreamingLevel);
}

FTransform UGameZoneSubsystem::GetSaveGameTransform() const
{
	// If we have a saved transform, use that for spawning
	if (CurrentContext.bUseSavedTransform)
	{
		return CurrentContext.SavedTransform;
	}
	// Otherwise, find a PlayerStart with a matching tag
	else
	{
		for (TActorIterator<ARPGPlayerStart> It(GetWorld()); It; ++It)
		{
			ARPGPlayerStart* Start = *It;
			if (!Start)
			{
				continue;
			}

			UE_LOG(LogTemp, Display, TEXT(
				"Found PlayerStart [%s] in World [%s], Level: [%s]"
			),
				*It->GetName(),
				*It->GetWorld()->GetName(),
				*GetNameSafe(It->GetLevel()));

			if (Start->EntryPointTags.HasTag(CurrentContext.EntryTag))
			{
				return Start->GetActorTransform();
				break;
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("No PlayerStart found with tag [%s], spawning at origin!"), *CurrentContext.EntryTag.ToString());
	}

	return FTransform::Identity;
}

UGameZoneAsset* UGameZoneSubsystem::FindZoneAssetForCurrentMap() const
{
	FString MapName = GetWorld()->GetMapName();
	FString Prefix = GetWorld()->StreamingLevelsPrefix;
	if (MapName.StartsWith(Prefix))
	{
		MapName.RightChopInline(Prefix.Len());
	}

	URPGAssetManager& Manager = URPGAssetManager::Get();

	TArray<FPrimaryAssetId> ZoneAssetIds;
	Manager.GetPrimaryAssetIdList(UGameZoneAsset::GetAssetTypeStatic(), ZoneAssetIds);

	for (const FPrimaryAssetId& AssetId : ZoneAssetIds)
	{
		if (UGameZoneAsset* ZoneAsset = Cast<UGameZoneAsset>(Manager.GetPrimaryAssetObject(AssetId)))
		{
			if (ZoneAsset->LevelToLoad.IsValid())
			{
				FString ZoneLevelName = FPackageName::GetShortName(ZoneAsset->LevelToLoad.GetLongPackageName());
				if (ZoneLevelName == MapName)
				{
					return ZoneAsset;
				}
			}
		}
	}

	return nullptr;
}

void UGameZoneSubsystem::WaitUntilLevelActorInitialized()
{
	if (!CurrentStreaming)
	{
		return;
	}

	if (const ULevel* LoadedLevel = CurrentStreaming->GetLoadedLevel())
	{
		LoadCompleteDelegate.Broadcast();
		CurrentStreaming = nullptr;
		return;
	}
	
	// Wait until the next tick to check again
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UGameZoneSubsystem::WaitUntilLevelActorInitialized));
}

FName UGameZoneSubsystem::GetSaveModuleType() const
{
	return FGameZoneSaveModule::StaticStruct()->GetFName();
}

void UGameZoneSubsystem::SaveDataTo(FInstancedStruct& SaveData)
{
	FGameZoneSaveModule ZoneSave;

	ZoneSave.CurrentContext = CurrentContext;

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	// TODO: 選擇使用 Transform 或者 EntryTag 存檔
	if (Pawn)
	{
		ZoneSave.CurrentContext.bUseSavedTransform = true;
		ZoneSave.CurrentContext.SavedTransform = Pawn->GetActorTransform();
	}

	SaveData.InitializeAs<FGameZoneSaveModule>(ZoneSave);
}

void UGameZoneSubsystem::LoadDataFrom(const FInstancedStruct& SaveData)
{
	if (const FGameZoneSaveModule* ZoneSave = SaveData.GetPtr<FGameZoneSaveModule>())
	{
		EnterGameZone(ZoneSave->CurrentContext);
	}
	// If the struct type does not match, we treat it as a new game
	else
	{
		const URPGSettings* Settings = GetDefault<URPGSettings>();
		if (!Settings)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get RPGSettings"));
			return;
		}
		
		// Load default transform
		EnterGameZone(Settings->DefaultGameZoneContext);
	}
}

FSimpleMulticastDelegate& UGameZoneSubsystem::OnLoadComplete()
{
	return LoadCompleteDelegate;
}

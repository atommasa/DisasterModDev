// Copyright Ironic Studio. All Rights Reserved.


#include "SaveGameSubsystem.h"

#include "Assets/RPGReleaseManifest.h"
#include "Kismet/GameplayStatics.h"
#include "SaveGame/RPGSaveGameMigration.h"
#include "SaveGame/RPGSaveGameMetadata.h"
#include "SaveGame/RPGSaveGameVersion.h"

#include "Settings/SaveSystemSettings.h"

DEFINE_LOG_CATEGORY(LogSaveSystem);

void USaveGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

	InitializeMetaDataSaveGame();
}

void USaveGameSubsystem::Deinitialize()
{
	ClearLoadTracking();
	Super::Deinitialize();
}

void USaveGameSubsystem::ClearLoadTracking()
{
	for (const TPair<ISaveable*, FDelegateHandle>& Pair : LoadCompleteHandles)
	{
		if (Pair.Key)
		{
			Pair.Key->OnLoadComplete().Remove(Pair.Value);
		}
	}

	LoadCompleteHandles.Empty();
	PendingSubsystems.Empty();
}

void USaveGameSubsystem::HandleSubsystemLoadCompleted(ISaveable* Provider, uint32 LoadRequestId)
{
	if (LoadRequestId != ActiveLoadRequestId)
	{
		UE_LOG(LogSaveSystem, Verbose, TEXT("Ignoring stale load completion from %s. Request=%u Active=%u"),
			*GetNameSafe(Provider ? Provider->_getUObject() : nullptr), LoadRequestId, ActiveLoadRequestId);
		return;
	}

	if (!Provider || !PendingSubsystems.Remove(Provider))
	{
		return;
	}

	if (FDelegateHandle* Handle = LoadCompleteHandles.Find(Provider))
	{
		Provider->OnLoadComplete().Remove(*Handle);
		LoadCompleteHandles.Remove(Provider);
	}

	UE_LOG(LogSaveSystem, Log, TEXT("Subsystem %s has completed loading."), *Provider->_getUObject()->GetName());
	if (PendingSubsystems.IsEmpty())
	{
		OnSaveGameLoadCompleted.Broadcast();
	}
}

void USaveGameSubsystem::SaveGame(const FString& SlotName, bool bAsync)
{
	if (SlotName.IsEmpty())
	{
		UE_LOG(LogSaveSystem, Warning, TEXT("SaveGame called with an empty SlotName!"));
		return;
	}

	if (!IsSlotNameValid(SlotName))
	{
		UE_LOG(LogSaveSystem, Warning, TEXT("SaveGame called with an invalid SlotName: %s"), *SlotName);
		return;
	}

	URPGSaveGame* SaveGameInstance = Cast<URPGSaveGame>(UGameplayStatics::CreateSaveGameObject(SaveGameClass));
	if (!SaveGameInstance)
	{
		UE_LOG(LogSaveSystem, Error, TEXT("Failed to create save game instance!"));
		return;
	}
	FRPGReleaseMigrationCatalog ReleaseCatalog;
	const FRPGReleaseMigrationCatalogResult CatalogResult = FRPGReleaseMigrationCatalogReader::ReadCurrent(ReleaseCatalog);
	if (!CatalogResult.IsSuccess())
	{
		UE_LOG(LogSaveSystem, Error, TEXT("Save refused because the RPG release migration catalog is invalid: %s"),
			*CatalogResult.Diagnostic);
		return;
	}
	SaveGameInstance->SaveDataVersion = ReleaseCatalog.CurrentVersion;

	UGameInstance* GameInstance = GetGameInstance();

	// Iterate through all GameInstanceSubsystems and find those that implement the ISaveable interface
	for (TObjectIterator<UGameInstanceSubsystem> It; It; ++It)
	{
		if (It->GetGameInstance() != GameInstance)
		{
			continue;
		}

		if (It->GetClass()->ImplementsInterface(USaveable::StaticClass()))
		{
			ISaveable* Provider = Cast<ISaveable>(*It);
			if (!Provider)
			{
				continue;
			}

			// Retrieve the save data from the provider
			FName ModuleType = Provider->GetSaveModuleType();
			if (FInstancedStruct* Module = SaveGameInstance->SaveModules.Find(ModuleType))
			{
				Provider->SaveDataTo(*Module);
			}
			else
			{
				SaveGameInstance->SaveModules.Add(ModuleType, FInstancedStruct());
				FInstancedStruct* NewModule = &SaveGameInstance->SaveModules[ModuleType];

				Provider->SaveDataTo(*NewModule);
			}
		}
	}

	if (bAsync)
	{
		FAsyncSaveGameToSlotDelegate SaveDelegate;
		SaveDelegate.BindUObject(this, &USaveGameSubsystem::OnGameSaved);
		UGameplayStatics::AsyncSaveGameToSlot(SaveGameInstance, SlotName, 0, SaveDelegate);
	}
	else
	{
		bool bSuccess = UGameplayStatics::SaveGameToSlot(SaveGameInstance, SlotName, 0);
		OnGameSaved(SlotName, 0, bSuccess);
	}
}

void USaveGameSubsystem::LoadGame(const FString& SlotName, bool bAsync)
{
	if (SlotName.IsEmpty())
	{
		UE_LOG(LogSaveSystem, Warning, TEXT("LoadGame called with an empty SlotName! This will be treated as a new game."));
	}

	if (bAsync)
	{
		FAsyncLoadGameFromSlotDelegate LoadDelegate;
		LoadDelegate.BindUObject(this, &USaveGameSubsystem::OnGameLoaded);
		UGameplayStatics::AsyncLoadGameFromSlot(SlotName, 0, LoadDelegate);
	}
	else if (URPGSaveGame* LoadedGame = Cast<URPGSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
	{
		OnGameLoaded(SlotName, 0, LoadedGame);
	}
}

void USaveGameSubsystem::SaveGameByIndex(int32 SlotIndex, bool bAsync)
{
	SaveGame(GetValidSlotName(SlotIndex), bAsync);
}

void USaveGameSubsystem::LoadGameByIndex(int32 SlotIndex, bool bAsync)
{
	LoadGame(GetValidSlotName(SlotIndex), bAsync);
}

void USaveGameSubsystem::DeleteSave(const FString& SlotName)
{
	if (SlotName.IsEmpty())
	{
		UE_LOG(LogSaveSystem, Warning, TEXT("DeleteSave called with an empty SlotName!"));
		return;
	}

	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
		DeleteMetaData(SlotName);

		UE_LOG(LogSaveSystem, Log, TEXT("Save deleted from slot: %s"), *SlotName);
	}
	else
	{
		UE_LOG(LogSaveSystem, Warning, TEXT("No save exists in slot: %s"), *SlotName);
	}
}

bool USaveGameSubsystem::DoesSaveExist(const FString& SlotName) const
{
	if (SlotName.IsEmpty())
	{
		UE_LOG(LogSaveSystem, Warning, TEXT("DoesSaveExist called with an empty SlotName!"));
		return false;
	}

	bool bExists = UGameplayStatics::DoesSaveGameExist(SlotName, 0);
	UE_LOG(LogSaveSystem, Log, TEXT("Save existence check for slot %s: %s"), *SlotName, bExists ? TEXT("Exists") : TEXT("Does not exist"));
	return bExists;
}

bool USaveGameSubsystem::IsSlotNameValid(const FString& SlotName) const
{
	const USaveSystemSettings* Settings = GetDefault<USaveSystemSettings>();
	if (!Settings)
	{
		return false;
	}

	const FString& Prefix = Settings->SaveSlotPrefix;
	if (SlotName.IsEmpty())
	{
		return false;
	}

	if (!SlotName.StartsWith(Prefix))
	{
		return false;
	}

	const int32 MaxSaveSlots = Settings->MaxSaveSlots;
	if (FCString::Atoi(*SlotName.RightChop(Prefix.Len())) > MaxSaveSlots)
	{
		return false;
	}

	return true;
}

FString USaveGameSubsystem::GetValidSlotName(int32 SlotIndex) const
{
	const USaveSystemSettings* Settings = GetDefault<USaveSystemSettings>();
	if (!Settings)
	{
		return FString();
	}

	FString SlotName;
	const FString& Prefix = Settings->SaveSlotPrefix;
	if (SlotIndex == 0)
	{
		// Auto-save slot
		SlotName = Prefix + Settings->AutoSaveSlotName;
		return SlotName;
	}

	const int32 MaxSaveSlots = Settings->MaxSaveSlots;
	if (SlotIndex < 1 || SlotIndex > MaxSaveSlots)
	{
		UE_LOG(LogSaveSystem, Warning, TEXT("GetValidSlotName called with an invalid SlotIndex: %d"), SlotIndex);
		return FString();
	}

	const int32 NumLen = FString::FromInt(MaxSaveSlots).Len();
	const FString PaddedIndex = FString::Printf(TEXT("%0*d"), NumLen, SlotIndex);

	SlotName = FString::Printf(TEXT("%s%s"), *Prefix, *PaddedIndex);
	return SlotName;
}

FString USaveGameSubsystem::GetLastestSaveSlotName() const
{
	TArray<FInstancedStruct> MetaData = GetSaveSlotsMetaData();

	TArray<const FRPGSaveGameMetadata*> ValidMetas;
	ValidMetas.Reserve(MetaData.Num());

	for (const FInstancedStruct& Struct : MetaData)
	{
		if (auto* Meta = Struct.GetPtr<FRPGSaveGameMetadata>())
		{
			ValidMetas.Add(Meta);
		}
	}

	// Sort by bIsClear first (non-clear first), then by SaveTime descending
	ValidMetas.Sort([](const FRPGSaveGameMetadata& A, const FRPGSaveGameMetadata& B)
		{
			if (A.bIsClear != B.bIsClear)
			{
				return !A.bIsClear; // Non-clear saves come first
			}
			return A.SaveTime > B.SaveTime;
		});

	if (ValidMetas.Num() > 0)
	{
		const FRPGSaveGameMetadata* LatestMeta = ValidMetas[0];
		if (LatestMeta && !LatestMeta->bIsClear)
		{
			return LatestMeta->SaveSlotName;
		}
	}
	
	return FString();
}

TArray<FInstancedStruct> USaveGameSubsystem::GetSaveSlotsMetaData() const
{
	const USaveSystemSettings* Settings = GetDefault<USaveSystemSettings>();
	if (!Settings)
	{
		return {};
	}

	TArray<FInstancedStruct> MetaData;
	const FString& Prefix = Settings->SaveSlotPrefix;
	const int32 MaxSaveSlots = Settings->MaxSaveSlots;
	const int32 NumLen = FString::FromInt(MaxSaveSlots).Len();

	for (int32 SlotIndex = 1; SlotIndex <= MaxSaveSlots; SlotIndex++)
	{
		const FString& PaddedIndex = FString::Printf(TEXT("%0*d"), NumLen, SlotIndex);
		const FString& SlotName = FString::Printf(TEXT("%s%s"), *Prefix, *PaddedIndex);
		
		MetaData.Add(MetaDataSaveGame->MetaData.FindRef(SlotName));
	}

	return MetaData;
}

FInstancedStruct USaveGameSubsystem::GetAutoSaveSlotMetaData() const
{
	const USaveSystemSettings* Settings = GetDefault<USaveSystemSettings>();
	if (!Settings)
	{
		return {};
	}

	FInstancedStruct MetaData;
	const FString& Prefix = Settings->SaveSlotPrefix;
	const FString& SlotName = FString::Printf(TEXT("%s%s"), *Prefix, *Settings->SaveSlotPrefix);

	MetaData = MetaDataSaveGame->MetaData.FindRef(SlotName);

	return MetaData;
}

void USaveGameSubsystem::OnGameSaved(const FString& SlotName, const int32 UserIndex, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		UpdateMetaData(SlotName);

		UE_LOG(LogSaveSystem, Log, TEXT("Game successfully saved to slot: %s"), *SlotName);
	}
	else
	{
		UE_LOG(LogSaveSystem, Error, TEXT("Failed to save game to slot: %s"), *SlotName);
	}
}

void USaveGameSubsystem::OnGameLoaded(const FString& SlotName, const int32 UserIndex, USaveGame* SaveGame)
{
	URPGSaveGame* SaveGameInstance = Cast<URPGSaveGame>(SaveGame);
	if (SaveGameInstance)
	{
		const FRPGSaveGameMigrationResult MigrationResult = FRPGSaveGameMigrator::MigrateToCurrent(*SaveGameInstance);
		if (!MigrationResult.IsSuccess())
		{
			ClearLoadTracking();
			++ActiveLoadRequestId;
			UE_LOG(LogSaveSystem, Error, TEXT("Save slot '%s' was rejected before provider dispatch: %s"), *SlotName,
				*MigrationResult.Diagnostic);
			OnSaveGameLoadFailed.Broadcast(MigrationResult.Diagnostic);
			return;
		}
	}
	UGameInstance* GameInstance = GetGameInstance();

	// Cancel the bookkeeping for the previous request before registering the new one.
	// Async provider work cannot always be cancelled, so callbacks also verify this id.
	ClearLoadTracking();
	const uint32 ThisLoadRequestId = ++ActiveLoadRequestId;

	for (TObjectIterator<UGameInstanceSubsystem> It; It; ++It)
	{
		if (It->GetGameInstance() != GameInstance || !It->GetClass()->ImplementsInterface(USaveable::StaticClass()))
		{
			continue;
		}

		ISaveable* Provider = Cast<ISaveable>(*It);
		if (!Provider)
		{
			continue;
		}

		PendingSubsystems.Add(Provider);

		TWeakObjectPtr<USaveGameSubsystem> WeakThis(this);
		const FDelegateHandle Handle = Provider->OnLoadComplete().AddLambda(
			[WeakThis, Provider, ThisLoadRequestId]()
			{
				if (USaveGameSubsystem* StrongThis = WeakThis.Get())
				{
					StrongThis->HandleSubsystemLoadCompleted(Provider, ThisLoadRequestId);
				}
			});

		LoadCompleteHandles.Add(Provider, Handle);
	}

	const TArray<ISaveable*> SubsystemsToLoad = PendingSubsystems.Array();
	for (ISaveable* Provider : SubsystemsToLoad)
	{
		const FName ModuleType = Provider->GetSaveModuleType();
		Provider->LoadDataFrom(SaveGameInstance ? SaveGameInstance->SaveModules.FindRef(ModuleType) : FInstancedStruct());
	}

	if (SubsystemsToLoad.IsEmpty())
	{
		OnSaveGameLoadCompleted.Broadcast();
	}

	UpdatePlayTimeBySlot(SlotName);
}

void USaveGameSubsystem::InitializeMetaDataSaveGame()
{
	const USaveSystemSettings* Settings = GetDefault<USaveSystemSettings>();
	if (!Settings)
	{
		return;
	}

	const FString& MetaDataSlotName = Settings->SaveSlotPrefix + Settings->MetaDataSaveSlotName;
	if (UGameplayStatics::DoesSaveGameExist(MetaDataSlotName, 0))
	{
		MetaDataSaveGame = Cast<UMetaDataSaveGame>(UGameplayStatics::LoadGameFromSlot(MetaDataSlotName, 0));
	}
	else
	{
		MetaDataSaveGame = Cast<UMetaDataSaveGame>(UGameplayStatics::CreateSaveGameObject(MetaDataSaveGameClass));
		if (MetaDataSaveGame)
		{
			SaveMetaData();
		}
		else
		{
			UE_LOG(LogSaveSystem, Error, TEXT("Failed to create MetaData save game instance!"));
		}
	}

	check(MetaDataSaveGame);
}

void USaveGameSubsystem::UpdateMetaData(const FString& SlotName)
{
	FRPGSaveGameMetadata NewMetaData;
	if (MetaDataSaveGame->MetaData.Contains(SlotName))
	{
		// Update existing metadata entry
		NewMetaData = MetaDataSaveGame->MetaData[SlotName].GetMutable<FRPGSaveGameMetadata>();
	}
	else
	{
		// New metadata entry
		NewMetaData.SaveSlotName = SlotName;
		NewMetaData.PlayTime = FTimespan::Zero();
		NewMetaData.bIsClear = false;
	}

	// Update save time
	NewMetaData.SaveTime = FDateTime::Now();
	FRPGReleaseMigrationCatalog ReleaseCatalog;
	const FRPGReleaseMigrationCatalogResult CatalogResult = FRPGReleaseMigrationCatalogReader::ReadCurrent(ReleaseCatalog);
	if (!CatalogResult.IsSuccess())
	{
		UE_LOG(LogSaveSystem, Error, TEXT("Metadata update refused because the RPG release migration catalog is invalid: %s"),
			*CatalogResult.Diagnostic);
		return;
	}
	NewMetaData.SaveDataVersion = ReleaseCatalog.CurrentVersion;
	
	// Update play time
	NewMetaData.PlayTime = LoadedPlayTime + (FDateTime::Now() - CurrentSessionStartTime);

	MetaDataSaveGame->MetaData.Add(SlotName, FInstancedStruct::Make<FRPGSaveGameMetadata>(NewMetaData));
	SaveMetaData();
}

void USaveGameSubsystem::DeleteMetaData(const FString& SlotName)
{
	MetaDataSaveGame->MetaData.Remove(SlotName);
	SaveMetaData();
}

void USaveGameSubsystem::SaveMetaData()
{
	const USaveSystemSettings* Settings = GetDefault<USaveSystemSettings>();
	if (!Settings)
	{
		return;
	}

	const FString& MetaDataSlotName = Settings->SaveSlotPrefix + Settings->MetaDataSaveSlotName;
	bool bSuccess = UGameplayStatics::SaveGameToSlot(MetaDataSaveGame, MetaDataSlotName, 0);
	if (!bSuccess)
	{
		UE_LOG(LogSaveSystem, Error, TEXT("Failed to save MetaData in slot: %s"), *MetaDataSlotName);
	}
}

void USaveGameSubsystem::UpdatePlayTimeBySlot(const FString& SlotName)
{
	// Update the current session start time
	CurrentSessionStartTime = FDateTime::Now();

	// Retrieve the play time from the metadata
	if (FRPGSaveGameMetadata* MetaData = GetSaveMetaData(SlotName).GetMutablePtr<FRPGSaveGameMetadata>())
	{
		LoadedPlayTime = MetaData->PlayTime;
	}
	else
	{
		LoadedPlayTime = FTimespan::Zero();
	}
}

FInstancedStruct USaveGameSubsystem::GetSaveMetaData(const FString& SlotName) const
{
	return MetaDataSaveGame->MetaData.FindRef(SlotName);
}

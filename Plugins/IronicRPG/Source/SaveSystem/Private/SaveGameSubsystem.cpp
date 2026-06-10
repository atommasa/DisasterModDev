// Copyright Ironic Studio. All Rights Reserved.


#include "SaveGameSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "SaveGame/RPGSaveGameMetadata.h"

#include "RPGSettings.h"

DEFINE_LOG_CATEGORY(LogSaveSystem);

void USaveGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

	InitializeMetaDataSaveGame();
}

void USaveGameSubsystem::Deinitialize()
{
    Super::Deinitialize();

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
	const FString& Prefix = URPGSettings::GetRPGSettings()->SaveSlotPrefix;
	if (SlotName.IsEmpty())
	{
		return false;
	}

	if (!SlotName.StartsWith(Prefix))
	{
		return false;
	}

	const int32 MaxSaveSlots = URPGSettings::GetRPGSettings()->MaxSaveSlots;
	if (FCString::Atoi(*SlotName.RightChop(Prefix.Len())) > MaxSaveSlots)
	{
		return false;
	}

	return true;
}

FString USaveGameSubsystem::GetValidSlotName(int32 SlotIndex) const
{
	FString SlotName;
	const FString& Prefix = URPGSettings::GetRPGSettings()->SaveSlotPrefix;
	if (SlotIndex == 0)
	{
		// Auto-save slot
		SlotName = Prefix + URPGSettings::GetRPGSettings()->AutoSaveSlotName;
		return SlotName;
	}

	const int32 MaxSaveSlots = URPGSettings::GetRPGSettings()->MaxSaveSlots;
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
	TArray<FInstancedStruct> MetaData;
	const FString& Prefix = URPGSettings::GetRPGSettings()->SaveSlotPrefix;
	const int32 MaxSaveSlots = URPGSettings::GetRPGSettings()->MaxSaveSlots;
	const int32 NumLen = FString::FromInt(MaxSaveSlots).Len();

	for (int32 SlotIndex = 1; SlotIndex <= MaxSaveSlots; SlotIndex++)
	{
		const FString PaddedIndex = FString::Printf(TEXT("%0*d"), NumLen, SlotIndex);
		const FString SlotName = FString::Printf(TEXT("%s%s"), *Prefix, *PaddedIndex);
		
		MetaData.Add(MetaDataSaveGame->MetaData.FindRef(SlotName));
	}

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

			PendingSubsystems.Add(Provider);

			Provider->OnLoadComplete().RemoveAll(Provider);
			Provider->OnLoadComplete().AddLambda([this, Provider]()
				{
					UE_LOG(LogSaveSystem, Warning, TEXT("Subsystem %s has completed loading."), *Provider->_getUObject()->GetName());

					PendingSubsystems.Remove(Provider);
					if (PendingSubsystems.IsEmpty())
					{
						OnSaveGameLoadCompleted.Broadcast();
					}
				});
		}
	}

	const TArray<ISaveable*> SubsystemsToLoad = PendingSubsystems.Array();
	for (ISaveable* Provider : SubsystemsToLoad)
	{
		// Retrieve the save data for the provider
		FName ModuleType = Provider->GetSaveModuleType();
		Provider->LoadDataFrom(SaveGameInstance ? SaveGameInstance->SaveModules.FindRef(ModuleType) : FInstancedStruct());
	}

	// Update play time tracking
	UpdatePlayTimeBySlot(SlotName);
}

void USaveGameSubsystem::InitializeMetaDataSaveGame()
{
	const FString& MetaDataSlotName = URPGSettings::GetRPGSettings()->SaveSlotPrefix + URPGSettings::GetRPGSettings()->MetaDataSaveSlotName;
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
	const FString& MetaDataSlotName = URPGSettings::GetRPGSettings()->SaveSlotPrefix + URPGSettings::GetRPGSettings()->MetaDataSaveSlotName;
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

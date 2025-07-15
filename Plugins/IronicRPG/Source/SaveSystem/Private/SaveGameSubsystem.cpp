// Fill out your copyright notice in the Description page of Project Settings.


#include "SaveGameSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "SaveGame/RPGSaveGame.h"
#include "SaveGame/Saveable.h"

bool USaveGameSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    if (this->GetClass()->IsInBlueprint() && Super::ShouldCreateSubsystem(Outer))
    {
        return true;
    }

    return false;
}

void USaveGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

}

void USaveGameSubsystem::Deinitialize()
{
    Super::Deinitialize();

}

void USaveGameSubsystem::SaveGame(const FString& SlotName, bool bAsync = false)
{
	if (SlotName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("SaveGame called with an empty SlotName!"));
		return;
	}

	auto* SaveGameInstance = Cast<URPGSaveGame>(UGameplayStatics::CreateSaveGameObject(URPGSaveGame::StaticClass()));
	if (!SaveGameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create save game instance!"));
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
			auto ModuleType = Provider->GetSaveModuleType();
			Provider->GetSaveData(&SaveGameInstance->CharacterModule);
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

void USaveGameSubsystem::LoadGame(const FString& SlotName, bool bAsync = false)
{
	if (SlotName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("LoadGame called with an empty SlotName!"));
		return;
	}

	auto* LoadedGame = Cast<URPGSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!LoadedGame)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load game from slot: %s"), *SlotName);
		return;
	}

	if (bAsync)
	{
		FAsyncLoadGameFromSlotDelegate LoadDelegate;
		LoadDelegate.BindUObject(this, &USaveGameSubsystem::OnGameLoaded);
		UGameplayStatics::AsyncLoadGameFromSlot(SlotName, 0, LoadDelegate);
	}
	else
	{
		OnGameLoaded(SlotName, 0, LoadedGame);
	}
}

void USaveGameSubsystem::DeleteSave(const FString& SlotName)
{
	if (SlotName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("DeleteSave called with an empty SlotName!"));
		return;
	}

	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
		UE_LOG(LogTemp, Log, TEXT("Save deleted from slot: %s"), *SlotName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No save exists in slot: %s"), *SlotName);
	}
}

bool USaveGameSubsystem::DoesSaveExist(const FString& SlotName) const
{
	if (SlotName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("DoesSaveExist called with an empty SlotName!"));
		return false;
	}

	bool bExists = UGameplayStatics::DoesSaveGameExist(SlotName, 0);
	UE_LOG(LogTemp, Log, TEXT("Save existence check for slot %s: %s"), *SlotName, bExists ? TEXT("Exists") : TEXT("Does not exist"));
	return bExists;
}

void USaveGameSubsystem::OnGameSaved(const FString& SlotName, const int32 UserIndex, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		UE_LOG(LogTemp, Log, TEXT("Game successfully saved to slot: %s"), *SlotName);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to save game to slot: %s"), *SlotName);
	}
}

void USaveGameSubsystem::OnGameLoaded(const FString& SlotName, const int32 UserIndex, USaveGame* SaveGame)
{
	if (!SaveGame)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load game from slot: %s"), *SlotName);
		return;
	}

	auto* RPGSaveGame = Cast<URPGSaveGame>(SaveGame);
	if (!RPGSaveGame)
	{
		UE_LOG(LogTemp, Warning, TEXT("Loaded game is not of type URPGSaveGame!"));
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

			// Retrieve the save data for the provider
			Provider->ApplySaveData(&RPGSaveGame->CharacterModule);
		}
	}

	// Set the current save slot to the loaded game
	CurrentSaveSlot = RPGSaveGame;

	UE_LOG(LogTemp, Log, TEXT("Game loaded from slot: %s"), *SlotName);
}

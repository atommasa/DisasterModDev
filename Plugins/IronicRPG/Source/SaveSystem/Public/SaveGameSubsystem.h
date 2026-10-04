// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/RPGGameInstanceSubsystem.h"
#include "SaveGame/RPGSaveGameMacros.h"
#include "SaveGame/RPGSaveGame.h"
#include "SaveGame/MetaDataSaveGame.h"
#include "SaveGame/Saveable.h"
#include "SaveGameSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSaveSystem, Log, All);

DECLARE_MULTICAST_DELEGATE(FOnSaveGameLoadCompleted);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSaveGameLoadFailed, const FString&);

/**
 * USaveSubsystem is a subsystem for managing game saves.
 * It provides functionality to save, load, delete, and check the existence of game saves.
 */
UCLASS(Blueprintable)
class SAVESYSTEM_API USaveGameSubsystem : public URPGGameInstanceSubsystem
{
	GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

public:
	// Saves the current game state to the specified slot.
	UFUNCTION(BlueprintCallable, Category = "Save System")
	void SaveGame(const FString& SlotName, bool bAsync = true);

	// Loads the game state from the specified slot.
	UFUNCTION(BlueprintCallable, Category = "Save System")
	void LoadGame(const FString& SlotName, bool bAsync = true);

	// Save game by index, using the prefix from settings.
	// The index is 1-based. If SlotIndex is 0 it will be treated as an auto-save.
	UFUNCTION(BlueprintCallable, Category = "Save System")
	void SaveGameByIndex(int32 SlotIndex, bool bAsync = true);

	// Load game by index, using the prefix from settings.
	// The index is 1-based. If SlotIndex is 0 it will be treated as an auto-save.
	UFUNCTION(BlueprintCallable, Category = "Save System")
	void LoadGameByIndex(int32 SlotIndex, bool bAsync = true);

	// Deletes the save data from the specified slot.
	UFUNCTION(BlueprintCallable, Category = "Save System")
	void DeleteSave(const FString& SlotName);

	// Checks if a save exists in the specified slot.
	UFUNCTION(BlueprintCallable, Category = "Save System")
	bool DoesSaveExist(const FString& SlotName) const;

	// Validates if the provided slot name is valid according to the settings.
	UFUNCTION(BlueprintCallable, Category = "Save System")
	bool IsSlotNameValid(const FString& SlotName) const;

	// Retrieves a valid slot name based on the provided index.
	// The index is 1-based. If SlotIndex is 0 it will return the auto-save slot name.
	UFUNCTION(BlueprintCallable, Category = "Save System")
	virtual FString GetValidSlotName(int32 SlotIndex) const;

	// Retrieves the latest save slot name that is not a clear save.
	UFUNCTION(BlueprintCallable, Category = "Save System")
	virtual FString GetLastestSaveSlotName() const;

	// Retrieves metadata for all save slots.
	// The order of the returned array corresponds to the slot indices.
	// E.g., index 0 is slot 1, index 1 is slot 2, etc.
	UFUNCTION(BlueprintCallable, Category = "Save System")
	TArray<FInstancedStruct> GetSaveSlotsMetaData() const;

	UFUNCTION(BlueprintCallable, Category = "Save System")
	FInstancedStruct GetAutoSaveSlotMetaData() const;

public:
	FOnSaveGameLoadCompleted OnSaveGameLoadCompleted;
	FOnSaveGameLoadFailed OnSaveGameLoadFailed;

	TSet<ISaveable*> PendingSubsystems;

private:
	void ClearLoadTracking();
	void HandleSubsystemLoadCompleted(ISaveable* Provider, uint32 LoadRequestId);

	// Every LoadGame invocation owns a unique request id. Provider callbacks from an
	// older request must never be allowed to complete the current request.
	uint32 ActiveLoadRequestId = 0;
	TMap<ISaveable*, FDelegateHandle> LoadCompleteHandles;
    
protected:
	UFUNCTION()
	void OnGameSaved(const FString& SlotName, const int32 UserIndex, bool bWasSuccessful);

	UFUNCTION()
	void OnGameLoaded(const FString& SlotName, const int32 UserIndex, USaveGame* SaveGame);

protected:
	virtual void InitializeMetaDataSaveGame();
	virtual void UpdateMetaData(const FString& SlotName);
	virtual void DeleteMetaData(const FString& SlotName);
	virtual void SaveMetaData();

	virtual void UpdatePlayTimeBySlot(const FString& SlotName);

public:
	virtual FInstancedStruct GetSaveMetaData(const FString& SlotName) const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Save System")
	TSubclassOf<URPGSaveGame> SaveGameClass = URPGSaveGame::StaticClass();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Save System")
	TSubclassOf<UMetaDataSaveGame> MetaDataSaveGameClass = UMetaDataSaveGame::StaticClass();

	UPROPERTY(BlueprintReadOnly, Category = "Save System")
	TObjectPtr<UMetaDataSaveGame> MetaDataSaveGame;

	// The current game session start time.
	UPROPERTY(BlueprintReadOnly, Category = "Save System")
	FDateTime CurrentSessionStartTime = FDateTime::Now();

	// The playtime loaded from the save game.
	UPROPERTY(BlueprintReadOnly, Category = "Save System")
	FTimespan LoadedPlayTime = FTimespan::Zero();

};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SaveGameSubsystem.generated.h"

class URPGSaveGame;

/**
 * USaveSubsystem is a subsystem for managing game saves.
 * It provides functionality to save, load, delete, and check the existence of game saves.
 */
UCLASS(Blueprintable)
class SAVESYSTEM_API USaveGameSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

public:
	UFUNCTION(BlueprintCallable, Category = "Save System")
	void SaveGame(const FString& SlotName, bool bAsync);

	UFUNCTION(BlueprintCallable, Category = "Save System")
	void LoadGame(const FString& SlotName, bool bAsync);

	UFUNCTION(BlueprintCallable, Category = "Save System")
	void DeleteSave(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "Save System")
	bool DoesSaveExist(const FString& SlotName) const;

	UFUNCTION(BlueprintCallable, Category = "Save System")
	URPGSaveGame* GetCurrentSaveSlot() const { return CurrentSaveSlot; }
    
protected:
	UFUNCTION()
	void OnGameSaved(const FString& SlotName, const int32 UserIndex, bool bWasSuccessful);

	UFUNCTION()
	void OnGameLoaded(const FString& SlotName, const int32 UserIndex, USaveGame* SaveGame);

protected:
	// List of save slots available in the game
	UPROPERTY(BlueprintReadOnly, Category = "Save System")
	TArray<URPGSaveGame*> SaveSlots;

	// Current save slot being used
	UPROPERTY(BlueprintReadOnly, Category = "Save System")
	URPGSaveGame* CurrentSaveSlot = nullptr;

};

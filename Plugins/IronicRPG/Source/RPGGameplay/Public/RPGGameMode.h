// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "Assets/RPGAssetManager.h"
#include "Kismet/GameplayStatics.h"

#include "SaveGameSubsystem.h"
#include "SaveGame/RPGSaveGame.h"

#include "CharacterSubsystem.h"
#include "Characters/BaseCharacter.h"

#include "GameZoneSubsystem.h"

#include "UISubsystem.h"

#include "LoadingScreenSubsystem.h"

#include "RPGGameMode.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRPGGameMode, Log, All);

/**
 * The base game mode for the Ironic RPG plugin.
 */
UCLASS()
class RPGGAMEPLAY_API ARPGGameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;

public:
	// Start the game from the menu
	UFUNCTION(BlueprintCallable, Category = "Game")
	virtual void StartGameSession(const FString& SlotName = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "Game")
	virtual void TeleportTo(const FGameZoneContext& NewGameZoneContext);

	UFUNCTION(BlueprintCallable, Category = "Game")
	virtual void AwakenSpawnablePoints();

protected:
	// Called after a save game is loaded
	UFUNCTION(BlueprintNativeEvent)
	void OnSaveGameLoaded();
	virtual void OnSaveGameLoaded_Implementation();

	UPROPERTY(BlueprintReadOnly, Category = "Game")
	USaveGameSubsystem* SaveSubsystem;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Game")
	UGameZoneSubsystem* GameZoneSubsystem;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "MainMenu")
	UUISubsystem* UISubsystem;

	UFUNCTION(BlueprintNativeEvent, Category = "MainMenu")
	void CreateMainMenuWidget();
	virtual void CreateMainMenuWidget_Implementation();

	// Called when the menu is initialized
	UFUNCTION(BlueprintImplementableEvent, Category = "MainMenu")
	void OnMainMenuInitialized();

	UPROPERTY(EditDefaultsOnly, Category = "MainMenu")
	TSubclassOf<class UMenuBase> MainMenuWidgetClass;

	UPROPERTY(BlueprintReadWrite, Category = "MainMenu")
	TWeakObjectPtr<class UMenuBase> MainMenuWidget;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "LoadingScreen")
	ULoadingScreenSubsystem* LoadingScreenSubsystem;

	UFUNCTION(BlueprintNativeEvent, Category = "Loading")
	void OnStoppedLoading();
	virtual void OnStoppedLoading_Implementation();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Character")
	UCharacterSubsystem* CharacterSubsystem;

	UFUNCTION()
	void OnPartyReady();

};

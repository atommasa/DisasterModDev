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

public:
	ARPGGameMode(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList) override;
	virtual void PostSeamlessTravel() override;

	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

public:
	UFUNCTION(BlueprintPure, Category = "RPG|Game", meta=(WorldContext = "WorldContextObject"))
	static ARPGGameMode* GetRPGGameMode(UObject* WorldContextObject);

	// Start the game from the menu
	UFUNCTION(BlueprintCallable, Category = "RPG|Game")
	virtual void StartGameSession(const FString& SlotName = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "RPG|Game")
	virtual void RequestTeleportTo(const FGameZoneContext& NewContext);

	UFUNCTION(BlueprintCallable, Category = "RPG|Game")
	virtual void AwakenSpawnablePoints();

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "RPG|LoadingScreen")
	void PostStartGameSession();
	virtual void PostStartGameSession_Implementation() {}

protected:
	UPROPERTY(BlueprintReadOnly, Category = "RPG|Game")
	USaveGameSubsystem* SaveSubsystem;

	UPROPERTY(BlueprintReadOnly, Category = "RPG|Game")
	UGameZoneSubsystem* GameZoneSubsystem;

	UPROPERTY(BlueprintReadOnly, Category = "RPG|Character")
	UCharacterSubsystem* CharacterSubsystem;

};

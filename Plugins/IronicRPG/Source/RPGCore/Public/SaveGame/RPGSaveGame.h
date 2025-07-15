// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Characters/CharacterDataTypes.h"
#include "SaveGame/CharacterSaveModule.h"
#include "RPGSaveGame.generated.h"

namespace ERPGSaveGameVersion
{
	enum Type
	{
		Initial,
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
}

UCLASS()
class RPGCORE_API URPGSaveGame : public USaveGame
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	int32 SaveDataVersion = ERPGSaveGameVersion::LatestVersion;

	UPROPERTY(SaveGame)
	FCharacterSaveModule CharacterModule;

	UPROPERTY(SaveGame)
	FDateTime SaveTimestamp;

	URPGSaveGame()
	{
		SaveTimestamp = FDateTime::Now();
	}
};

// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SaveGame/RPGSaveGameVersion.h"
#include "StructUtils/InstancedStruct.h"
#include "RPGSaveGame.generated.h"

UCLASS()
class SAVESYSTEM_API URPGSaveGame : public USaveGame
{
	GENERATED_BODY()
	
public:
	// Zero is the legacy baseline so a slot serialized before this property existed
	// remains distinguishable from saves explicitly written at Current.
	UPROPERTY(SaveGame)
	uint16 SaveDataVersion = ERPGSaveGameVersion::Baseline;

	UPROPERTY(SaveGame)
	TMap<FName, FInstancedStruct> SaveModules;

};

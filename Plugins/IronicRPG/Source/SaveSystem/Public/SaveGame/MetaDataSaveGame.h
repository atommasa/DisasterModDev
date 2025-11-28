// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "InstancedStruct.h"
#include "MetaDataSaveGame.generated.h"

/**
 * 
 */
UCLASS()
class SAVESYSTEM_API UMetaDataSaveGame : public USaveGame
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadOnly, SaveGame)
	TMap<FString, FInstancedStruct> MetaData;

};

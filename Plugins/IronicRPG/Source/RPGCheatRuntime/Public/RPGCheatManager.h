// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "RPGCheatManager.generated.h"

/**
 * 
 */
UCLASS()
class RPGCHEATRUNTIME_API URPGCheatManager : public UCheatManager
{
	GENERATED_BODY()
	
public:
	virtual void InitCheatManager() override;
};

/**
 *
 */
UCLASS(abstract)
class RPGCHEATRUNTIME_API URPGCheatExtension : public UCheatManagerExtension
{
	GENERATED_BODY()

};
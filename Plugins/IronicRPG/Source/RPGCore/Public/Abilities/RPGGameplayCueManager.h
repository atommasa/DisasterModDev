// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueManager.h"
#include "RPGGameplayCueManager.generated.h"

/**
 * This class is the manager for gameplay cues in the RPGCore. It inherits from UGameplayCueManager and can be customized to handle specific gameplay cue logic for the RPG game.
 */
UCLASS()
class RPGCORE_API URPGGameplayCueManager : public UGameplayCueManager
{
	GENERATED_BODY()
	
protected:
	virtual bool ShouldAsyncLoadRuntimeObjectLibraries() const { return false; }

};

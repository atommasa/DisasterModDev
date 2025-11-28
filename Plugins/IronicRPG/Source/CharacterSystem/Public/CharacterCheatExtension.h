// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RPGCheatManager.h"
#include "CharacterCheatExtension.generated.h"

/**
 * 
 */
UCLASS()
class CHARACTERSYSTEM_API UCharacterCheatExtension : public URPGCheatExtension
{
	GENERATED_BODY()
	
public:
	UFUNCTION(exec)
	void SetPlayerCharacter(const FName& Id);
};

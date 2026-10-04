// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Levels/GameZoneContext.h"
#include "GameZoneSystemSettings.generated.h"

class UTexture2D;

/**
 * 
 */
UCLASS(config = IronicRPG, defaultconfig)
class RPGCORE_API UGameZoneSystemSettings : public UObject
{
	GENERATED_BODY()
	
public: // Level settings
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category = "Game Zone")
	FGameZoneContext DefaultGameZoneContext;

public: // Map settings
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category = "Game Zone|Map")
	TSoftObjectPtr<UTexture2D> DefaultMapTexture;

};

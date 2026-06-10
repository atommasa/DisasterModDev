// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "DataTypes/RPGId.h"
#include "NativeGameplayTags.h"
#include "RPGPlayerStart.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(EntryPoint)

/**
 * The ARPGPlayerStart class represents a player start point in the game world.
 * It can be used to specify the location and orientation where players will spawn when they enter the game, respawn after death or teleport to a different location.
 * It also contains a ZoneId property that can be used to associate the player start with a specific zone in the game world.
 */
UCLASS(Blueprintable, ClassGroup = RPG, hidecategories = (Collision, Object))
class RPGCORE_API ARPGPlayerStart : public APlayerStart
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;

public:
	// Tags to identify the entry point type
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(Categories = "EntryPoint"))
	FGameplayTagContainer EntryPointTags;

};

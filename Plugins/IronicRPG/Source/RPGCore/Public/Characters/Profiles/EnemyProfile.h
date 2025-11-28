// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CharacterProfileBase.h"
#include "Items/LootTable.h"
#include "EnemyProfile.generated.h"

/**
 * Struct representing the profile of an enemy character.
 */
USTRUCT(BlueprintType)
struct RPGCORE_API FEnemyProfile : public FCharacterProfileBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FLootTable LootTable;

};
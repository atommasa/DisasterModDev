// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/RPGId.h"
#include "LootTable.generated.h"

/**
 * Struct representing data for a lootable item.
 */
USTRUCT(BlueprintType)
struct RPGCORE_API FLootData
{
	GENERATED_BODY()

public:
	// Item ID to be looted
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(IdType = "Item"))
	FRPGId ItemId;
	
	// Amount of the item to be looted
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin = "1"))
	int32 Amount = 1;

	// Chance to loot the item (0 to 1)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin = "0", ClampMax = "1"))
	float LootRate = 1.f;

};

/**
 * Struct representing a loot table.
 */
USTRUCT(BlueprintType)
struct RPGCORE_API FLootTable
{
	GENERATED_BODY()

public:
	// Items that can be looted
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FLootData> LootItems;

	// Amount of gold that can be looted
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 GoldAmount = 0;
};

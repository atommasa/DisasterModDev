// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SaveSystemSettings.generated.h"

/**
 * 
 */
UCLASS(config = IronicRPG, defaultconfig)
class RPGCORE_API USaveSystemSettings : public UObject
{
	GENERATED_BODY()
	
public: // Save system settings
	// The prefix for save slot names that can be used to identify save files.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category = "Save System")
	FString SaveSlotPrefix = TEXT("save.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category = "Save System")
	FString MetaDataSaveSlotName = TEXT("meta");

	// Auto-save slot name.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category = "Save System")
	FString AutoSaveSlotName = TEXT("auto");

	// The maximum number of save slots allowed.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category = "Save System", meta = (ClampMin = "1"))
	int32 MaxSaveSlots = 10;

};

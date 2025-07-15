// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BaseSaveModule.h"
#include "Saveable.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class USaveable : public UInterface
{
	GENERATED_BODY()
};

/**
 *
 */
class RPGCORE_API ISaveable
{
	GENERATED_BODY()

public:
	virtual ESaveModuleType GetSaveModuleType() const = 0;
	virtual void GetSaveData(FBaseSaveModule* SaveData) const = 0;
	virtual void ApplySaveData(const FBaseSaveModule* SaveData) = 0;
};

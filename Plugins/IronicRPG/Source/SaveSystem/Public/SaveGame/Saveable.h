// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "StructUtils/InstancedStruct.h"
#include "Saveable.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class USaveable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for objects that can provide and apply save data.
 */
class SAVESYSTEM_API ISaveable
{
	GENERATED_BODY()

public:
	virtual FName GetSaveModuleType() const = 0;
	virtual void SaveDataTo(FInstancedStruct& SaveData) = 0;
	virtual void LoadDataFrom(const FInstancedStruct& SaveDatas) = 0;

	virtual FSimpleMulticastDelegate& OnLoadComplete() = 0;
};

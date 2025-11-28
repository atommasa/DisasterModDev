// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Flowable.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UFlowable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class GAMEFRAMEWORK_API IFlowable
{
	GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void StartupSubsystem();
	virtual void StartupSubsystem_Implementation() {}

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void ShutdownSubsystem();
	virtual void ShutdownSubsystem_Implementation() {}

};

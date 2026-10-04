// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RPGGameInstanceSubsystem.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class RPGCORE_API URPGGameInstanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
protected:
	bool ShouldCreateSubsystem(UObject* Outer) const;
    
};

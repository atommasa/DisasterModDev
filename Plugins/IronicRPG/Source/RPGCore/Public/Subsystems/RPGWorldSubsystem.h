// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RPGWorldSubsystem.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class RPGCORE_API URPGWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
protected:
	bool ShouldCreateSubsystem(UObject* Outer) const;
    
};

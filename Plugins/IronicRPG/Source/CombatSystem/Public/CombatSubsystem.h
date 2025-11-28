// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CombatSubsystem.generated.h"

/**
 * A combat subsystem to manage combat-related functionalities.
 */
UCLASS(Abstract, Blueprintable)
class COMBATSYSTEM_API UCombatSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
public:
	/*UFUNCTION(BlueprintCallable)
	void StartCombat(const TArray<ACharacter*>& Enemies);*/

};

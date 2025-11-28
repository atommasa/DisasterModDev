// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RPGAISubsystem.generated.h"

class AFollowLocationTracker;

/**
 * 
 */
UCLASS(Blueprintable)
class RPGAI_API URPGAISubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
protected: // Subsystem Interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	void Initialize(FSubsystemCollectionBase& Collection) override;
	void Deinitialize() override;

public: // Follow
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Follow Location")
	TSubclassOf<AFollowLocationTracker> FollowLocationTrackerClass;

	UPROPERTY(BlueprintReadOnly, Category = "Follow Location")
	TWeakObjectPtr<AFollowLocationTracker> FollowLocationTracker = nullptr;

};

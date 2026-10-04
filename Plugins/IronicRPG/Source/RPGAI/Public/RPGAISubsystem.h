// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/RPGWorldSubsystem.h"
#include "RPGAISubsystem.generated.h"

class ABaseCharacter;
class AFollowLocationTracker;

/**
 * 
 */
UCLASS(Blueprintable)
class RPGAI_API URPGAISubsystem : public URPGWorldSubsystem
{
	GENERATED_BODY()
	
protected: // Subsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

public: // Follow
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Follow Location")
	TSubclassOf<AFollowLocationTracker> FollowLocationTrackerClass;

	UPROPERTY(BlueprintReadOnly, Category = "Follow Location")
	TWeakObjectPtr<AFollowLocationTracker> FollowLocationTracker = nullptr;

protected:
	// Called when the controlled character changes
	UFUNCTION()
	void OnControlledCharacterChanged(const ABaseCharacter* NewCharacter, const ABaseCharacter* OldCharacter);

	/** Assigns leader/follower roles and optionally holds nearby followers until the leader moves. */
	void ConfigurePartyFollow(const ABaseCharacter* LeaderCharacter, bool bWaitForFormationActivation);
};

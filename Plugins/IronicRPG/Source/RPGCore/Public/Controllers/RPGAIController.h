// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "RPGAIController.generated.h"

/**
 * The AI Controller class used by RPG characters.
 */
UCLASS()
class RPGCORE_API ARPGAIController : public AAIController
{
	GENERATED_BODY()

public:
    ARPGAIController(const FObjectInitializer& ObjectInitializer);

    virtual void BeginPlay() override;

    virtual void OnPossess(APawn* InPawn) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	FName DetectedActorKeyName;

	UPROPERTY(Transient)
	FBlackboardKeySelector DetectedActorKey;

private:
	class UAISenseConfig_Sight* SightConfig;
	class UAISenseConfig_Hearing* HearingConfig;

	virtual void ConfigurePerceptionSystem();

	UFUNCTION()
	virtual void OnTargetDetected(AActor* Actor, FAIStimulus Stimulus);

};

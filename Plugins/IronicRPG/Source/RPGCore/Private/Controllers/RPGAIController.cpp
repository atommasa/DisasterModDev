// Copyright Ironic Studio. All Rights Reserved.


#include "Controllers/RPGAIController.h"
#include "Characters/BaseCharacter.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeTypes.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"

ARPGAIController::ARPGAIController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    BrainComponent = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComp"));
    Blackboard = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComp"));

    ConfigurePerceptionSystem();
}

void ARPGAIController::BeginPlay()
{
    Super::BeginPlay();
}

void ARPGAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    
    // If the pawn is ABaseCharacter
    if (auto* BaseCharacter = Cast<ABaseCharacter>(InPawn))
    {
        if (auto* BehaviorTree = BaseCharacter->GetBehaviorTree())
        {
            RunBehaviorTree(BehaviorTree);
			UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
			ensure(BlackboardComp && BlackboardComp->GetBlackboardAsset());

			DetectedActorKey.SelectedKeyName = DetectedActorKeyName;
			DetectedActorKey.AddObjectFilter(this, DetectedActorKeyName, AActor::StaticClass());
			DetectedActorKey.ResolveSelectedKey(*BlackboardComp->GetBlackboardAsset());
			ensureMsgf(DetectedActorKey.GetSelectedKeyID() != FBlackboard::InvalidKey,
				TEXT("Blackboard key '%s' not found in %s"),
				*DetectedActorKeyName.ToString(), *BlackboardComp->GetBlackboardAsset()->GetName());
        }
    }
}

void ARPGAIController::ConfigurePerceptionSystem()
{
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));

    check(SightConfig && HearingConfig);

	SetPerceptionComponent(*CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp")));

	// Default Configure Sight
    SightConfig->SightRadius = 1500.0f;
	SightConfig->LoseSightRadius = 1800.0f;
	SightConfig->AutoSuccessRangeFromLastSeenLocation = 900.0f;
	SightConfig->PeripheralVisionAngleDegrees = 120.0f;
	SightConfig->SetMaxAge(5.0f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	// TODO: DetectionByAffiliation maybe need to adjust

	GetPerceptionComponent()->SetDominantSense(*SightConfig->GetSenseImplementation());
	GetPerceptionComponent()->ConfigureSense(*SightConfig);

	// Default Configure Hearing
    HearingConfig->HearingRange = 1000.0f;
	HearingConfig->SetMaxAge(5.0f);
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	// TODO: DetectionByAffiliation maybe need to adjust

	GetPerceptionComponent()->ConfigureSense(*HearingConfig);

	GetPerceptionComponent()->OnTargetPerceptionUpdated.AddDynamic(this, &ARPGAIController::OnTargetDetected);
}

void ARPGAIController::OnTargetDetected(AActor* Actor, FAIStimulus Stimulus)
{
	if (!GetBlackboardComponent())
	{
		return;
	}

	// Check if the stimulus was successfully sensed
	if (Stimulus.WasSuccessfullySensed())
	{
		GetBlackboardComponent()->SetValueAsObject(DetectedActorKey.SelectedKeyName, Actor);
	}
	// If the stimulus was lost, clear the blackboard key
	else
	{
		GetBlackboardComponent()->ClearValue(DetectedActorKey.SelectedKeyName);
	}
}

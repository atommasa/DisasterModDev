// Fill out your copyright notice in the Description page of Project Settings.


#include "RPGAIController.h"
#include "Characters/BaseCharacter.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTree.h"

ARPGAIController::ARPGAIController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    BrainComponent = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComp"));
    Blackboard = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComp"));
}

void ARPGAIController::BeginPlay()
{
    Super::BeginPlay();
}

void ARPGAIController::OnPossess(APawn* const InPawn)
{
    Super::OnPossess(InPawn);
    
    // If the pawn is ABaseCharacter
    if (auto* BaseCharacter = Cast<ABaseCharacter>(InPawn))
    {
        if (auto* BehaviorTree = BaseCharacter->GetBehaviorTree())
        {
            RunBehaviorTree(BehaviorTree);
        }
    }
}
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "RPGAIController.generated.h"

/**
 * 
 */
UCLASS()
class RPGAI_API ARPGAIController : public AAIController
{
	GENERATED_BODY()

public:
    ARPGAIController(const FObjectInitializer& ObjectInitializer);

    virtual void BeginPlay() override;

    virtual void OnPossess(APawn* const InPawn) override;

};

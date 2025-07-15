// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/BaseCharacter.h"
#include "PlayableCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;

/**
 * 
 */
UCLASS()
class RPGCORE_API APlayableCharacter : public ABaseCharacter
{
	GENERATED_BODY()
	
public:
	// Constructor
	APlayableCharacter(const FObjectInitializer& ObjectInitializer);

public: // Camera
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	UCameraComponent* FollowCamera;

};

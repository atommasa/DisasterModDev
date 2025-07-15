// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "CharacterAbilitySystemComponent.generated.h"

/**
 * This class extends the UAbilitySystemComponent to provide additional functionality specific to character abilities in the RPG game.
 */
UCLASS()
class RPGCORE_API UCharacterAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	// Constructor
	UCharacterAbilitySystemComponent(const FObjectInitializer& ObjectInitializer);

public:
	
	
};

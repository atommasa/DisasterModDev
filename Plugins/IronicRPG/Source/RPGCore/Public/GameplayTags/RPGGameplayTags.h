// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * 
 */
struct FRPGGameplayTags
{
public:
	static const FRPGGameplayTags& Get() { return GameplayTags; }
	static void InitializeNativeGameplayTags();

public: // Native Gameplay Tags


private:
	static FRPGGameplayTags GameplayTags;
};

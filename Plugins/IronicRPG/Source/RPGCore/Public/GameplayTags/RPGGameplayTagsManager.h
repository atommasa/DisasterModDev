// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "RPGGameplayTagsManager.generated.h"

/**
 * 
 */
UCLASS()
class RPGCORE_API URPGGameplayTagsManager : public UAssetManager
{
	GENERATED_BODY()

public:
	static URPGGameplayTagsManager& Get();

protected:
	virtual void StartInitialLoading() override;

};

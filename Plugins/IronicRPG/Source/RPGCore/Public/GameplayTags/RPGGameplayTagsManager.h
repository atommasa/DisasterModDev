// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "RPGGameplayTagsManager.generated.h"

/**
 * Asset manager class to manage RPG gameplay tags.
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

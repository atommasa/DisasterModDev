// Copyright Ironic Studio. All Rights Reserved.


#include "GameplayTags/RPGGameplayTagsManager.h"
#include "GameplayTags/RPGGameplayTags.h"

URPGGameplayTagsManager& URPGGameplayTagsManager::Get()
{
	check(GEngine && GEngine->AssetManager);

	URPGGameplayTagsManager* Manager = Cast<URPGGameplayTagsManager>(GEngine->AssetManager);
	return *Manager;
}

void URPGGameplayTagsManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	// Initialize native gameplay tags
	FRPGGameplayTags::InitializeNativeGameplayTags();
}

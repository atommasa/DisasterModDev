// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayTags/RPGGameplayTagsMacros.h"

/**
 * Defines all of the native gameplay tags used by the RPG system.
 */
struct FRPGGameplayTags
{
	ENABLE_NEW_TAGS(FRPGGameplayTags);

public:
	static const FRPGGameplayTags& Get() { return GameplayTags; }
	static void InitializeNativeGameplayTags();

public: // Native Gameplay Tags
};

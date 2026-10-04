// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/LatentActionManager.h"
#include "RPGCoroutine.h"

namespace RPGFlow
{
	/** Registers a running coroutine as an Unreal latent action. */
	RPGFLOW_API bool RegisterLatentCoroutine(
		UObject* WorldContextObject,
		const FLatentActionInfo& LatentInfo,
		TRPGCoroutine<> Coroutine);
}

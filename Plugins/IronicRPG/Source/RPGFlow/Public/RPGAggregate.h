// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RPGCoroutine.h"

namespace RPGFlow
{
	/**
	 * Waits for all supplied tasks. TRPGCoroutine is eager, so tasks begin when
	 * they are created, before this helper awaits them one by one.
	 */
	inline TRPGCoroutine<> WhenAll(TArray<TRPGCoroutine<>> Tasks)
	{
		for (const TRPGCoroutine<>& Task : Tasks)
		{
			if (Task.IsValid())
			{
				co_await Task;
			}
		}
	}
}

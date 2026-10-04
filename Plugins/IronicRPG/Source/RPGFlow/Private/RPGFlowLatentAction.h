// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/LatentActionManager.h"
#include "RPGCoroutine.h"

class FRPGPendingLatentCoroutine final : public FPendingLatentAction
{
public:
	FRPGPendingLatentCoroutine(
		TRPGCoroutine<> InCoroutine,
		FLatentActionInfo InLatentInfo)
		: Coroutine(MoveTemp(InCoroutine))
		, LatentInfo(MoveTemp(InLatentInfo))
	{
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		const bool bDone = !Coroutine.IsValid() || Coroutine.IsDone();

		if (!bDone)
		{
			return;
		}

		// A canceled latent action must not continue Blueprint execution.
		if (Coroutine.IsValid() && Coroutine.IsCancellationRequested())
		{
			Response.DoneIf(true);
			return;
		}

		Response.FinishAndTriggerIf(
			true,
			LatentInfo.ExecutionFunction,
			LatentInfo.Linkage,
			LatentInfo.CallbackTarget);
	}

	virtual void NotifyObjectDestroyed() override
	{
		Coroutine.Cancel(
			RPGFlow::ERPGCoroutineCancelReason::LatentObjectDestroyed);
	}

	virtual void NotifyActionAborted() override
	{
		Coroutine.Cancel(
			RPGFlow::ERPGCoroutineCancelReason::LatentActionAborted);
	}

private:
	TRPGCoroutine<> Coroutine;
	FLatentActionInfo LatentInfo;
};

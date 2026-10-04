// Copyright Ironic Studio. All Rights Reserved.


#include "RPGFlowLatentAction.h"

#include "RPGLatent.h"

bool RPGFlow::RegisterLatentCoroutine(
	UObject* WorldContextObject,
	const FLatentActionInfo& LatentInfo,
	TRPGCoroutine<> Coroutine)
{
	if (!IsValid(WorldContextObject) ||
		!IsValid(LatentInfo.CallbackTarget) ||
		!Coroutine.IsValid())
	{
		return false;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return false;
	}

	FLatentActionManager& Manager = World->GetLatentActionManager();

	if (Manager.FindExistingAction<FRPGPendingLatentCoroutine>(
		LatentInfo.CallbackTarget,
		LatentInfo.UUID))
	{
		return false;
	}

	Manager.AddNewAction(
		LatentInfo.CallbackTarget,
		LatentInfo.UUID,
		new FRPGPendingLatentCoroutine(MoveTemp(Coroutine), LatentInfo));

	return true;
}

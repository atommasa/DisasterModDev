// Copyright Ironic Studio. All Rights Reserved.


#include "Abilities/AbilityTargetResolver.h"

UWorld* UAbilityTargetResolver::GetWorld() const
{
	if (HasAllFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	return GetOuter()->GetWorld();
}

void UAbilityTargetResolver::StartResolveTargets(AActor* SourceActor)
{
	Source = SourceActor;

	StartResolveTargetsEvent();
}

void UAbilityTargetResolver::EndResolveTargets()
{
	EndResolveTargetsEvent();
}

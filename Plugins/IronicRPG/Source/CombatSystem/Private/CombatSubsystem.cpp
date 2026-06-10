// Copyright Ironic Studio. All Rights Reserved.


#include "CombatSubsystem.h"

#include "Characters/BaseCharacter.h"
#include "Characters/Components/CombatComponent.h"

void UCombatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UCombatSubsystem::AssignTeamTagToActor));
}

void UCombatSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UCombatSubsystem::AssignTeamTagToActor(AActor* Actor)
{
	
}

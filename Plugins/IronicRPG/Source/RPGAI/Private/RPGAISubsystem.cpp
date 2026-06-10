// Copyright Ironic Studio. All Rights Reserved.


#include "RPGAISubsystem.h"
#include "FollowLocationTracker.h"
#include "Kismet/GameplayStatics.h"

bool URPGAISubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (this->GetClass()->IsInBlueprint() && Super::ShouldCreateSubsystem(Outer))
	{
		return true;
	}

	return false;
}

void URPGAISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if WITH_EDITOR
	if (!GetWorld() ||
		GetWorld()->WorldType == EWorldType::Editor ||
		GetWorld()->WorldType == EWorldType::EditorPreview)
	{
		return;
	}
#endif

	if (FollowLocationTrackerClass)
	{
		FollowLocationTracker = GetWorld()->SpawnActor<AFollowLocationTracker>(FollowLocationTrackerClass);
	}
}

void URPGAISubsystem::Deinitialize()
{
	Super::Deinitialize();

	if (FollowLocationTracker.IsValid())
	{
		FollowLocationTracker->Destroy();
		FollowLocationTracker.Reset();
	}
}

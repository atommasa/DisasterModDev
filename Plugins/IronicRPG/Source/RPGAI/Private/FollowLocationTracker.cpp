// Copyright Ironic Studio. All Rights Reserved.


#include "FollowLocationTracker.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"

AFollowLocationTracker::AFollowLocationTracker()
{
	PrimaryActorTick.bCanEverTick = true;
	
}

void AFollowLocationTracker::BeginPlay()
{
	Super::BeginPlay();
	
	LeaderController = UGameplayStatics::GetPlayerController(GetWorld(), 0);

	Locations.Empty();
	Locations.Init(FFollowLocation(), MaxLocations);
}

void AFollowLocationTracker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!LeaderController.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector Origin = LeaderController->GetNavAgentLocation();
	FRotator ForwardRotation;
	if (auto Pawn = LeaderController->GetPawn())
	{
		ForwardRotation = Pawn->GetActorForwardVector().Rotation();
	}
	else
	{
		return;
	}

	for (int32 i = 0; i < MaxLocations; ++i)
	{
		// Calculate the angle for this location
		const float Angle = (2 * PI / MaxLocations) * i;
		const FVector LocalDirection = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0);
		const FVector Direction = ForwardRotation.RotateVector(LocalDirection);

		// Calculate the start and end locations for the line trace
		FVector StartLocation = Origin;
		StartLocation.Z += LeaderController->GetPawn()->GetDefaultHalfHeight();
		FVector EndLocation = StartLocation + Direction * OuterRadius;
		FHitResult HitResult;

		// Define trace parameters
		ECollisionChannel TraceChannel = ECC_Visibility;
		bool bTraceComplex = false;
		TArray<AActor*> ActorsToIgnore;

		// Perform the line trace
		bool bHit = UKismetSystemLibrary::LineTraceSingle(
			World,
			StartLocation,
			EndLocation,
			ETraceTypeQuery::TraceTypeQuery1,
			false,
			ActorsToIgnore,
#if WITH_EDITORONLY_DATA
			bDrawDebug ? EDrawDebugTrace::ForOneFrame : 
#endif // WITH_EDITORONLY_DATA
			EDrawDebugTrace::None,
			HitResult,
			true,
			FColor::Red,
			FColor::Green,
			0.0f
		);

		// Calculate the final location based on the hit result
		FVector FinalLocation = Origin + Direction * PreferredRadius;
		if (bHit)
		{
			FinalLocation = HitResult.ImpactPoint + HitResult.ImpactNormal * 30.0f;
		}

		// Ensure the final location is on the navigation mesh
		FNavLocation NavLocation;
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			if (NavSys->ProjectPointToNavigation(
				FinalLocation,
				NavLocation,
				FVector(100, 100, 100)
			))
			{
				FinalLocation = NavLocation;
			}
		}

		// Update the location in the array
		Locations[i].Location = FinalLocation;
		Locations[i].DistanceToLeader = FVector::Dist(FinalLocation, Origin);

		// Check if the location is inside the inner radius, if so, disable it
		if (Locations[i].DistanceToLeader < InnerRadius)
		{
			Locations[i].Disable();
		}
		else
		{
			Locations[i].bCannotFollow = false;
		}
		
		// If the location is occupied but the controller is no longer valid, unoccupy it
		if (!Locations[i].OccupiedBy.IsValid() || !Locations[i].OccupiedBy->GetPawn())
		{
			Locations[i].Unoccupy();
		}

#if WITH_EDITORONLY_DATA
		if (bDrawDebug)
		{
			FColor DebugColor = Locations[i].bCannotFollow ? FColor::Red : Locations[i].bOccupied ? FColor::Yellow : FColor::Blue;
			DrawDebugSphere(World, FinalLocation, 10.0f, 12, DebugColor, false, 0.0f, 0, 1.0f);
		}
#endif // WITH_EDITORONLY_DATA
	}

#if WITH_EDITORONLY_DATA
	if (bDrawDebug)
	{
		DrawDebugSphere(World, Origin, InnerRadius, 12, FColor::Red, false, 0.0f, 0, 1.0f);
		DrawDebugSphere(World, Origin, PreferredRadius, 12, FColor::Green, false, 0.0f, 0, 1.0f);
		DrawDebugSphere(World, Origin, OuterRadius, 12, FColor::Red, false, 0.0f, 0, 1.0f);
	}
#endif // WITH_EDITORONLY_DATA
}

FVector AFollowLocationTracker::FindNearestLocationAndOccupy(const AController* FollowerController)
{
	if (!FollowerController || !LeaderController.IsValid())
	{
		return FVector::ZeroVector;
	}

	// Get follower's location
	FFollowLocation* FollowPoint = FindLocationByController(FollowerController);

	const FVector LeaderLocation = LeaderController->GetNavAgentLocation();
	const FVector FollowerLocation = FollowerController->GetNavAgentLocation();
	const float DistanceToLeader = FVector::Dist(FollowerLocation, LeaderLocation);

	// Check if the foller is inside the inner radius, if so, move it out to the inner radius
	// i.e. give way to the leader
	if (DistanceToLeader < InnerRadius)
	{
		return FollowerLocation + (FollowerLocation - LeaderLocation).GetSafeNormal() * InnerRadius;
	}
	// If the follower is in front of the leader, return its current location
	// i.e. it is already in a good position
	else if (DistanceToLeader < PreferredRadius)
	{
		if (FollowPoint)
		{
			FollowPoint->Unoccupy();
		}

		return FollowerLocation;
	}

	// If the follower is already occupying a location, return that location
	if (FollowPoint && FollowPoint->HasController() && !FollowPoint->bCannotFollow)
	{
		return FollowPoint->Location;
	}

	// Find the nearest available location
	int32 BestIndex = INDEX_NONE;
	float MinDistance = MAX_flt;

	for (int32 i = 0; i < Locations.Num(); ++i)
	{
		const auto& Location = Locations[i];
		const float Distance = FVector::Dist(FollowerLocation, Location.Location);
		if (!Location.bOccupied && !Location.bCannotFollow && Distance < MinDistance)
		{
			MinDistance = Distance;
			BestIndex = i;
		}
	}

	if (BestIndex != INDEX_NONE)
	{
		Locations[BestIndex].Occupy(FollowerController);
		return Locations[BestIndex].Location;
	}

	return FVector::ZeroVector;
}

FFollowLocation* AFollowLocationTracker::FindLocationByController(const AController* FollowerController)
{
	for (auto& Location : Locations)
	{
		if (Location.OccupiedBy == FollowerController)
		{
			return &Location;
		}
	}

	return nullptr;
}

bool AFollowLocationTracker::IsInFrontOfLeader(const AController* FollowerController, float ThresholdDegrees = 90.0f) const
{
	if (!LeaderController.IsValid() || !FollowerController)
	{
		return false;
	}

	TObjectPtr<APawn> Target = FollowerController->GetPawn();
	TObjectPtr<APawn> Reference = LeaderController->GetPawn();
	if (!Target || !Reference)
	{
		return false;
	}

	const FVector DirectionToTarget = (Target->GetActorLocation() - Reference->GetActorLocation()).GetSafeNormal();
	const FVector ReferenceForward = Reference->GetActorForwardVector();

	const float Dot = FVector::DotProduct(DirectionToTarget, ReferenceForward);
	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));

	return AngleDegrees <= ThresholdDegrees;
}

// Copyright Ironic Studio. All Rights Reserved.

#include "PartyFollow/PartyFollowComponent.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "NavigationSystem.h"


UPartyFollowComponent::UPartyFollowComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPartyFollowComponent::BeginPlay()
{
	Super::BeginPlay();

	SetComponentTickEnabled(FollowRole == EPartyFollowRole::Leader);
}

void UPartyFollowComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (FollowRole == EPartyFollowRole::Leader)
	{
		TickLeader(DeltaTime);
	}
}

void UPartyFollowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseLeaderSideClaim();
	Super::EndPlay(EndPlayReason);
}

void UPartyFollowComponent::SetAsLeader()
{
	ReleaseLeaderSideClaim();
	FollowRole = EPartyFollowRole::Leader;
	LeaderComponent.Reset();
	FollowOrder = INDEX_NONE;
	PreferredSide = EPartyFollowSide::Center;
	FollowState = EPartyFollowState::Waiting;
	CurrentLateralSideSign = 0.0f;
	ClaimedSide = EPartyFollowSide::Center;
	bNeedsReacquireTarget = true;
	LeftSideClaim.Reset();
	RightSideClaim.Reset();
	bLeaderConsideredMoving = true;
	LastLeaderMovementTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	FormationActivationOrigin = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	bFormationActivated = false;
	bWaitForFormationActivation = false;

	ResetTrajectory();
	SetComponentTickEnabled(true);
}

bool UPartyFollowComponent::SetAsFollower(
	const AActor* InLeaderActor,
	int32 InFollowOrder,
	bool bInWaitForFormationActivation)
{
	UPartyFollowComponent* InLeaderComponent = InLeaderActor
		? InLeaderActor->FindComponentByClass<UPartyFollowComponent>()
		: nullptr;

	if (!InLeaderComponent || InLeaderComponent == this)
	{
		return false;
	}

	ReleaseLeaderSideClaim();

	FollowRole = EPartyFollowRole::Follower;
	LeaderComponent = InLeaderComponent;
	TrajectoryPoints.Reset();
	InitializeFollowPreference(InFollowOrder);
	FollowState = EPartyFollowState::Following;
	CurrentLateralSideSign = 0.0f;
	ClaimedSide = EPartyFollowSide::Center;
	bNeedsReacquireTarget = true;
	bWaitForFormationActivation = bInWaitForFormationActivation;
	LeftSideClaim.Reset();
	RightSideClaim.Reset();

	// Followers only answer target queries and do not need a component tick.
	SetComponentTickEnabled(false);
	return true;
}

void UPartyFollowComponent::DisablePartyFollow()
{
	ReleaseLeaderSideClaim();
	FollowRole = EPartyFollowRole::None;
	LeaderComponent.Reset();
	TrajectoryPoints.Reset();
	FollowOrder = INDEX_NONE;
	PreferredSide = EPartyFollowSide::Center;
	FollowState = EPartyFollowState::Waiting;
	CurrentLateralSideSign = 0.0f;
	ClaimedSide = EPartyFollowSide::Center;
	bNeedsReacquireTarget = true;
	bFormationActivated = false;
	bWaitForFormationActivation = false;
	LeftSideClaim.Reset();
	RightSideClaim.Reset();
	SetComponentTickEnabled(false);
}

void UPartyFollowComponent::ResetTrajectory()
{
	TrajectoryPoints.Reset();

	FVector TrajectoryLocation;
	if (TryGetLeaderTrajectoryLocation(TrajectoryLocation))
	{
		RecordTrajectoryPoint(TrajectoryLocation);
	}
}

bool UPartyFollowComponent::GetFollowTarget(FPartyFollowTarget& OutTarget)
{
	OutTarget = FPartyFollowTarget();

	if (FollowRole != EPartyFollowRole::Follower)
	{
		return false;
	}

	const UPartyFollowComponent* Leader = LeaderComponent.Get();
	if (!Leader || Leader->FollowRole != EPartyFollowRole::Leader)
	{
		return false;
	}

	const AActor* Owner = GetOwner();
	const AActor* LeaderOwner = Leader->GetOwner();
	if (!Owner || !LeaderOwner)
	{
		return false;
	}

	const float DistanceToLeader = FVector::Dist2D(
		Owner->GetActorLocation(),
		LeaderOwner->GetActorLocation());

	if (bWaitForFormationActivation)
	{
		if (Leader->HasFormationActivated())
		{
			bWaitForFormationActivation = false;
			SetFollowState(EPartyFollowState::Following);
			bNeedsReacquireTarget = true;
		}
		else if (DistanceToLeader > AvoidanceEnterDistance &&
			DistanceToLeader <= FormationActivationHoldMaxDistance)
		{
			SetFollowState(EPartyFollowState::Waiting);
			return TryBuildHoldTarget(OutTarget);
		}
		else if (DistanceToLeader > FormationActivationHoldMaxDistance)
		{
			// A separated party member must still be allowed to catch up even if
			// the newly assigned formation has not been activated yet.
			bWaitForFormationActivation = false;
			SetFollowState(EPartyFollowState::Following);
		}
	}

	const bool bLeaderIsStationary = Leader->IsLeaderStationary();
	UpdateFollowerState(DistanceToLeader, bLeaderIsStationary);

	if (FollowState == EPartyFollowState::Waiting)
	{
		return TryBuildHoldTarget(OutTarget);
	}

	if (FollowState == EPartyFollowState::Avoiding)
	{
		OutTarget.Mode = EPartyFollowTargetMode::Avoid;
		OutTarget.Location = Owner->GetActorLocation();
		BuildAvoidanceTarget(OutTarget.Location);
		OutTarget.bIsValid = true;

#if WITH_EDITORONLY_DATA
		if (bDrawDebug && GetWorld())
		{
			DrawDebugSphere(GetWorld(), OutTarget.Location, 16.0f, 12, FColor::Red, false, 0.25f);
		}
#endif

		return true;
	}

	FVector TrajectoryLocation;
	FVector TrajectoryTangent;
	if (!Leader->FindTrajectoryPointByDistance(
		GetDesiredFollowDistance(),
		TrajectoryLocation,
		TrajectoryTangent))
	{
		return false;
	}

	FVector FinalLocation;
	float AppliedOffset = 0.0f;
	if (!FindBestLateralLocation(
		TrajectoryLocation,
		TrajectoryTangent,
		FinalLocation,
		AppliedOffset))
	{
		return false;
	}

	if (bLeaderIsStationary &&
		IsInsideSettledArea(FinalLocation, TrajectoryTangent, DistanceToLeader))
	{
		SetFollowState(EPartyFollowState::Settled);
		OutTarget.Mode = EPartyFollowTargetMode::Hold;
		OutTarget.Location = Owner->GetActorLocation();
		OutTarget.TrajectoryLocation = TrajectoryLocation;
		OutTarget.TrajectoryTangent = TrajectoryTangent;
		OutTarget.AppliedLateralOffset = AppliedOffset;
		OutTarget.bIsValid = true;

#if WITH_EDITORONLY_DATA
		if (bDrawDebug && GetWorld())
		{
			DrawDebugSphere(GetWorld(), OutTarget.Location, 18.0f, 12, FColor::Cyan, false, 0.25f);
		}
#endif

		return true;
	}

	if (FollowState == EPartyFollowState::Settled)
	{
		SetFollowState(EPartyFollowState::Following);
	}

	OutTarget.Location = FinalLocation;
	OutTarget.Mode = EPartyFollowTargetMode::Move;
	OutTarget.TrajectoryLocation = TrajectoryLocation;
	OutTarget.TrajectoryTangent = TrajectoryTangent;
	OutTarget.AppliedLateralOffset = AppliedOffset;
	OutTarget.bIsValid = true;

#if WITH_EDITORONLY_DATA
	if (bDrawDebug && GetWorld())
	{
		DrawDebugSphere(GetWorld(), TrajectoryLocation, 10.0f, 12, FColor::Blue, false, 0.25f);
		DrawDebugSphere(GetWorld(), FinalLocation, 14.0f, 12, FColor::Green, false, 0.25f);
		DrawDebugLine(GetWorld(), TrajectoryLocation, FinalLocation, FColor::Cyan, false, 0.25f, 0, 1.5f);
	}
#endif

	return true;
}

float UPartyFollowComponent::GetDesiredFollowDistance() const
{
	if (FollowOrder < 0)
	{
		return BaseFollowDistance;
	}

	return BaseFollowDistance + FollowOrder * FollowDistanceStep;
}

void UPartyFollowComponent::SetPreferredSide(EPartyFollowSide InPreferredSide)
{
	ReleaseLeaderSideClaim();
	PreferredSide = InPreferredSide;

	if (InPreferredSide == EPartyFollowSide::Left)
	{
		CurrentLateralSideSign = -1.0f;
	}
	else if (InPreferredSide == EPartyFollowSide::Right)
	{
		CurrentLateralSideSign = 1.0f;
	}
	else
	{
		CurrentLateralSideSign = 0.0f;
	}

	bNeedsReacquireTarget = true;
}

void UPartyFollowComponent::TickLeader(float DeltaTime)
{
	const AActor* Owner = GetOwner();
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastLeaderMovementTime + DeltaTime;
	const float Speed = Owner ? Owner->GetVelocity().Size2D() : 0.0f;

	if (!bFormationActivated && Owner &&
		FVector::Dist2D(Owner->GetActorLocation(), FormationActivationOrigin) >= FormationActivationMoveDistance)
	{
		bFormationActivated = true;
	}

	if (bLeaderConsideredMoving)
	{
		if (Speed > LeaderStopSpeedThreshold)
		{
			LastLeaderMovementTime = CurrentTime;
		}
		else if (CurrentTime - LastLeaderMovementTime >= LeaderSettleDelay)
		{
			bLeaderConsideredMoving = false;
		}
	}
	else if (Speed >= LeaderResumeSpeedThreshold)
	{
		bLeaderConsideredMoving = true;
		LastLeaderMovementTime = CurrentTime;
	}

	FVector CurrentLocation;
	if (!TryGetLeaderTrajectoryLocation(CurrentLocation))
	{
		return;
	}

	if (TrajectoryPoints.IsEmpty())
	{
		RecordTrajectoryPoint(CurrentLocation);
		return;
	}

	const FVector LastLocation = TrajectoryPoints.Last().Location;
	const float DistanceFromLastPoint = FVector::Dist2D(CurrentLocation, LastLocation);

	if (DistanceFromLastPoint >= TrajectoryResetDistance)
	{
		ResetTrajectory();
		return;
	}

	if (DistanceFromLastPoint >= TrajectorySampleDistance)
	{
		RecordTrajectoryPoint(CurrentLocation);
	}

#if WITH_EDITORONLY_DATA
	if (bDrawDebug && GetWorld())
	{
		for (int32 Index = 1; Index < TrajectoryPoints.Num(); ++Index)
		{
			DrawDebugLine(
				GetWorld(),
				TrajectoryPoints[Index - 1].Location,
				TrajectoryPoints[Index].Location,
				FColor::Yellow,
				false,
				0.0f,
				0,
				2.0f);
		}
	}
#endif
}

void UPartyFollowComponent::RecordTrajectoryPoint(const FVector& Location)
{
	FPartyTrajectoryPoint NewPoint;
	NewPoint.Location = Location;

	if (TrajectoryPoints.IsEmpty())
	{
		NewPoint.Tangent = GetOwner()
			? GetOwner()->GetActorForwardVector().GetSafeNormal2D()
			: FVector::ForwardVector;
	}
	else
	{
		const FPartyTrajectoryPoint& PreviousPoint = TrajectoryPoints.Last();
		const FVector Segment = Location - PreviousPoint.Location;

		NewPoint.SegmentLength = Segment.Size2D();
		NewPoint.Tangent = Segment.GetSafeNormal2D();
		NewPoint.AccumulatedDistance =
			PreviousPoint.AccumulatedDistance + NewPoint.SegmentLength;

		if (NewPoint.Tangent.IsNearlyZero())
		{
			NewPoint.Tangent = PreviousPoint.Tangent;
		}
	}

	TrajectoryPoints.Add(NewPoint);
	TrimTrajectory();
}

void UPartyFollowComponent::TrimTrajectory()
{
	int32 RemoveCount = 0;

	while (RemoveCount < TrajectoryPoints.Num() - 1)
	{
		const float RemainingLength =
			TrajectoryPoints.Last().AccumulatedDistance
			- TrajectoryPoints[RemoveCount].AccumulatedDistance;

		if (RemainingLength <= MaxTrajectoryLength)
		{
			break;
		}

		++RemoveCount;
	}

	if (RemoveCount > 0)
	{
		TrajectoryPoints.RemoveAt(0, RemoveCount, EAllowShrinking::No);
	}
}

void UPartyFollowComponent::InitializeFollowPreference(int32 InFollowOrder)
{
	FollowOrder = FMath::Max(0, InFollowOrder);
	PreferredSide = FollowOrder % 2 == 0
		? EPartyFollowSide::Left
		: EPartyFollowSide::Right;
}

bool UPartyFollowComponent::FindTrajectoryPointByDistance(
	float DistanceBehindLeader,
	FVector& OutLocation,
	FVector& OutTangent) const
{
	if (TrajectoryPoints.IsEmpty())
	{
		return false;
	}

	if (TrajectoryPoints.Num() == 1 || DistanceBehindLeader <= 0.0f)
	{
		OutLocation = TrajectoryPoints.Last().Location;
		OutTangent = TrajectoryPoints.Last().Tangent;
		return true;
	}

	float RemainingDistance = DistanceBehindLeader;

	for (int32 Index = TrajectoryPoints.Num() - 1; Index > 0; --Index)
	{
		const FPartyTrajectoryPoint& CurrentPoint = TrajectoryPoints[Index];
		const FPartyTrajectoryPoint& PreviousPoint = TrajectoryPoints[Index - 1];
		const float SegmentLength = CurrentPoint.SegmentLength;

		if (SegmentLength <= UE_SMALL_NUMBER)
		{
			continue;
		}

		if (RemainingDistance <= SegmentLength)
		{
			const float Alpha = RemainingDistance / SegmentLength;
			OutLocation = FMath::Lerp(CurrentPoint.Location, PreviousPoint.Location, Alpha);
			OutTangent = (CurrentPoint.Location - PreviousPoint.Location).GetSafeNormal2D();
			return true;
		}

		RemainingDistance -= SegmentLength;
	}

	// The trajectory has not grown to the requested distance yet.
	OutLocation = TrajectoryPoints[0].Location;
	OutTangent = TrajectoryPoints[0].Tangent;
	return true;
}

bool UPartyFollowComponent::FindBestLateralLocation(
	const FVector& TrajectoryLocation,
	const FVector& TrajectoryTangent,
	FVector& OutLocation,
	float& OutAppliedOffset)
{
	FVector SafeTangent = TrajectoryTangent.GetSafeNormal2D();
	if (SafeTangent.IsNearlyZero())
	{
		SafeTangent = FVector::ForwardVector;
	}

	const FVector Right = FVector::CrossProduct(FVector::UpVector, SafeTangent).GetSafeNormal();

	float SideSign = CurrentLateralSideSign;
	if (bNeedsReacquireTarget || FMath::IsNearlyZero(SideSign))
	{
		SideSign = ResolveReacquiredSideSign(TrajectoryLocation, SafeTangent);
	}

	const float PreferredOffset = PreferredLateralDistance * SideSign;
	// Keep the semantic side reservation even when a narrow NavMesh forces the
	// physical offset toward the center. Crossing to the other side here would
	// bypass the leader's side ownership and could make followers overlap.
	const TArray<float, TInlineAllocator<3>> CandidateOffsets =
	{
		PreferredOffset,
		PreferredOffset * 0.5f,
		0.0f
	};

	for (const float CandidateOffset : CandidateOffsets)
	{
		const FVector CandidateLocation = TrajectoryLocation + Right * CandidateOffset;
		FVector ProjectedLocation;

		if (ProjectCandidateToNavigation(CandidateLocation, ProjectedLocation))
		{
			OutLocation = ProjectedLocation;
			OutAppliedOffset = CandidateOffset;

			if (!FMath::IsNearlyZero(CandidateOffset))
			{
				CurrentLateralSideSign = FMath::Sign(CandidateOffset);
			}

			bNeedsReacquireTarget = false;
			return true;
		}
	}

	return false;
}

float UPartyFollowComponent::ResolveReacquiredSideSign(
	const FVector& TrajectoryLocation,
	const FVector& TrajectoryTangent)
{
	const float PreferredSign = PreferredSide == EPartyFollowSide::Left
		? -1.0f
		: 1.0f;

	const AActor* Owner = GetOwner();
	if (!Owner || PreferredLateralDistance <= UE_SMALL_NUMBER)
	{
		const EPartyFollowSide DesiredSide = PreferredSign < 0.0f
			? EPartyFollowSide::Left
			: EPartyFollowSide::Right;
		UPartyFollowComponent* Leader = LeaderComponent.Get();
		const EPartyFollowSide GrantedSide = Leader
			? Leader->RequestSideClaim(this, DesiredSide)
			: DesiredSide;
		ClaimedSide = GrantedSide;
		return GrantedSide == EPartyFollowSide::Left ? -1.0f : 1.0f;
	}

	FVector SafeTangent = TrajectoryTangent.GetSafeNormal2D();
	if (SafeTangent.IsNearlyZero())
	{
		SafeTangent = FVector::ForwardVector;
	}

	const FVector Right = FVector::CrossProduct(FVector::UpVector, SafeTangent).GetSafeNormal();
	const FVector LeftCandidate = TrajectoryLocation - Right * PreferredLateralDistance;
	const FVector RightCandidate = TrajectoryLocation + Right * PreferredLateralDistance;

	FVector ProjectedLeft;
	FVector ProjectedRight;
	const bool bHasLeft = ProjectCandidateToNavigation(LeftCandidate, ProjectedLeft);
	const bool bHasRight = ProjectCandidateToNavigation(RightCandidate, ProjectedRight);

	float DesiredSign = PreferredSign;
	if (bHasLeft != bHasRight)
	{
		DesiredSign = bHasLeft ? -1.0f : 1.0f;
	}
	else if (bHasLeft)
	{
		const FVector OwnerLocation = Owner->GetActorLocation();
		const float LeftDistance = FVector::Dist2D(OwnerLocation, ProjectedLeft);
		const float RightDistance = FVector::Dist2D(OwnerLocation, ProjectedRight);

		if (FMath::Abs(LeftDistance - RightDistance) > SideSelectionTieThreshold)
		{
			DesiredSign = LeftDistance < RightDistance ? -1.0f : 1.0f;
		}
	}

	const EPartyFollowSide DesiredSide = DesiredSign < 0.0f
		? EPartyFollowSide::Left
		: EPartyFollowSide::Right;
	UPartyFollowComponent* Leader = LeaderComponent.Get();
	const EPartyFollowSide GrantedSide = Leader
		? Leader->RequestSideClaim(this, DesiredSide)
		: DesiredSide;
	ClaimedSide = GrantedSide;
	return GrantedSide == EPartyFollowSide::Left ? -1.0f : 1.0f;
}

bool UPartyFollowComponent::ProjectCandidateToNavigation(
	const FVector& Candidate,
	FVector& OutProjectedLocation) const
{
	if (!bProjectTargetsToNavigation)
	{
		OutProjectedLocation = Candidate;
		return true;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const UNavigationSystemV1* NavigationSystem =
		FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

	if (!NavigationSystem)
	{
		return false;
	}

	FNavLocation ProjectedLocation;
	if (!NavigationSystem->ProjectPointToNavigation(
		Candidate,
		ProjectedLocation,
		NavigationProjectionExtent))
	{
		return false;
	}

	if (FVector::Dist2D(Candidate, ProjectedLocation.Location) > MaxNavigationProjectionAdjustment)
	{
		return false;
	}

	OutProjectedLocation = ProjectedLocation.Location;
	return true;
}

void UPartyFollowComponent::UpdateFollowerState(float DistanceToLeader, bool bLeaderIsStationary)
{
	switch (FollowState)
	{
	case EPartyFollowState::Following:
		if (DistanceToLeader <= AvoidanceEnterDistance)
		{
			SetFollowState(EPartyFollowState::Avoiding);
		}
		else if (DistanceToLeader <= WaitingEnterDistance)
		{
			SetFollowState(EPartyFollowState::Waiting);
		}
		break;

	case EPartyFollowState::Settled:
		if (DistanceToLeader <= AvoidanceEnterDistance)
		{
			SetFollowState(EPartyFollowState::Avoiding);
		}
		else if (DistanceToLeader <= WaitingEnterDistance)
		{
			SetFollowState(EPartyFollowState::Waiting);
		}
		else if (!bLeaderIsStationary)
		{
			SetFollowState(EPartyFollowState::Following);
		}
		break;

	case EPartyFollowState::Waiting:
		if (DistanceToLeader <= AvoidanceEnterDistance)
		{
			SetFollowState(EPartyFollowState::Avoiding);
		}
		else if (DistanceToLeader >= FollowResumeDistance)
		{
			SetFollowState(EPartyFollowState::Following);
		}
		break;

	case EPartyFollowState::Avoiding:
		if (DistanceToLeader >= FollowResumeDistance)
		{
			SetFollowState(EPartyFollowState::Following);
		}
		else if (DistanceToLeader >= AvoidanceExitDistance)
		{
			SetFollowState(EPartyFollowState::Waiting);
		}
		break;
	}
}

void UPartyFollowComponent::SetFollowState(EPartyFollowState NewState)
{
	if (FollowState == NewState)
	{
		return;
	}

	FollowState = NewState;

	// Any close pass invalidates the previous semantic left/right assignment.
	// The follower chooses the nearest physical side after the leader moves away.
	if (NewState == EPartyFollowState::Waiting ||
		NewState == EPartyFollowState::Avoiding)
	{
		ReleaseLeaderSideClaim();
		bNeedsReacquireTarget = true;
	}
}

bool UPartyFollowComponent::IsLeaderStationary() const
{
	return FollowRole == EPartyFollowRole::Leader && !bLeaderConsideredMoving;
}

bool UPartyFollowComponent::HasFormationActivated() const
{
	return FollowRole == EPartyFollowRole::Leader && bFormationActivated;
}

bool UPartyFollowComponent::TryBuildHoldTarget(FPartyFollowTarget& OutTarget) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	OutTarget.Mode = EPartyFollowTargetMode::Hold;
	OutTarget.Location = Owner->GetActorLocation();
	OutTarget.bIsValid = true;

#if WITH_EDITORONLY_DATA
	if (bDrawDebug && GetWorld())
	{
		DrawDebugSphere(GetWorld(), OutTarget.Location, 16.0f, 12, FColor::Magenta, false, 0.25f);
	}
#endif

	return true;
}

bool UPartyFollowComponent::IsInsideSettledArea(
	const FVector& FollowTarget,
	const FVector& TrajectoryTangent,
	float DistanceToLeader) const
{
	if (DistanceToLeader < IdleMinLeaderDistance || DistanceToLeader > IdleMaxLeaderDistance)
	{
		return false;
	}

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	FVector Tangent = TrajectoryTangent.GetSafeNormal2D();
	if (Tangent.IsNearlyZero())
	{
		Tangent = FVector::ForwardVector;
	}

	const FVector Right = FVector::CrossProduct(FVector::UpVector, Tangent).GetSafeNormal();
	const FVector Error = Owner->GetActorLocation() - FollowTarget;
	const float LongitudinalRatio = FMath::Abs(FVector::DotProduct(Error, Tangent)) /
		FMath::Max(IdleLongitudinalTolerance, 1.0f);
	const float LateralRatio = FMath::Abs(FVector::DotProduct(Error, Right)) /
		FMath::Max(IdleLateralTolerance, 1.0f);

	// An ellipse avoids the sharp corners produced by two independent tolerances.
	return FMath::Square(LongitudinalRatio) + FMath::Square(LateralRatio) <= 1.0f;
}

EPartyFollowSide UPartyFollowComponent::RequestSideClaim(
	UPartyFollowComponent* Requester,
	EPartyFollowSide DesiredSide)
{
	if (FollowRole != EPartyFollowRole::Leader || !Requester)
	{
		return DesiredSide;
	}

	RemoveInvalidSideClaims();

	if (LeftSideClaim.Get() == Requester)
	{
		LeftSideClaim.Reset();
	}
	if (RightSideClaim.Get() == Requester)
	{
		RightSideClaim.Reset();
	}

	const EPartyFollowSide SafeDesiredSide = DesiredSide == EPartyFollowSide::Center
		? EPartyFollowSide::Left
		: DesiredSide;
	const EPartyFollowSide OtherSide = SafeDesiredSide == EPartyFollowSide::Left
		? EPartyFollowSide::Right
		: EPartyFollowSide::Left;

	auto IsAvailable = [this](EPartyFollowSide Side)
	{
		return Side == EPartyFollowSide::Left
			? !LeftSideClaim.IsValid()
			: !RightSideClaim.IsValid();
	};

	EPartyFollowSide GrantedSide = SafeDesiredSide;
	if (!IsAvailable(GrantedSide) && IsAvailable(OtherSide))
	{
		GrantedSide = OtherSide;
	}

	if (GrantedSide == EPartyFollowSide::Left)
	{
		LeftSideClaim = Requester;
	}
	else
	{
		RightSideClaim = Requester;
	}

	return GrantedSide;
}

void UPartyFollowComponent::ReleaseSideClaim(UPartyFollowComponent* Requester)
{
	if (LeftSideClaim.Get() == Requester)
	{
		LeftSideClaim.Reset();
	}
	if (RightSideClaim.Get() == Requester)
	{
		RightSideClaim.Reset();
	}
}

void UPartyFollowComponent::ReleaseLeaderSideClaim()
{
	if (UPartyFollowComponent* Leader = LeaderComponent.Get())
	{
		Leader->ReleaseSideClaim(this);
	}
	ClaimedSide = EPartyFollowSide::Center;
}

void UPartyFollowComponent::RemoveInvalidSideClaims()
{
	if (!LeftSideClaim.IsValid())
	{
		LeftSideClaim.Reset();
	}
	if (!RightSideClaim.IsValid())
	{
		RightSideClaim.Reset();
	}
}

bool UPartyFollowComponent::TryGetLeaderTrajectoryLocation(FVector& OutLocation) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	if (!bProjectLeaderTrajectoryToNavigation)
	{
		OutLocation = OwnerLocation;
		return true;
	}

	UWorld* World = GetWorld();
	const UNavigationSystemV1* NavigationSystem = World
		? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)
		: nullptr;

	if (!NavigationSystem)
	{
		return false;
	}

	FNavLocation ProjectedLocation;
	if (!NavigationSystem->ProjectPointToNavigation(
		OwnerLocation,
		ProjectedLocation,
		LeaderTrajectoryProjectionExtent))
	{
		// While jumping over an area without NavMesh, keep the previous valid point.
		return false;
	}

	OutLocation = ProjectedLocation.Location;
	return true;
}

bool UPartyFollowComponent::BuildAvoidanceTarget(FVector& OutLocation) const
{
	const AActor* Owner = GetOwner();
	const UPartyFollowComponent* Leader = LeaderComponent.Get();
	const AActor* LeaderOwner = Leader ? Leader->GetOwner() : nullptr;

	if (!Owner || !LeaderOwner)
	{
		return false;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	const FVector LeaderLocation = LeaderOwner->GetActorLocation();

	FVector AwayDirection = (OwnerLocation - LeaderLocation).GetSafeNormal2D();
	if (AwayDirection.IsNearlyZero())
	{
		AwayDirection = -LeaderOwner->GetActorForwardVector().GetSafeNormal2D();
	}

	FVector Candidate = LeaderLocation + AwayDirection * AvoidanceExitDistance;
	Candidate.Z = OwnerLocation.Z;
	return ProjectCandidateToNavigation(Candidate, OutLocation);
}

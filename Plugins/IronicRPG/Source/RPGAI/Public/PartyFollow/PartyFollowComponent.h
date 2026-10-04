// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PartyFollowComponent.generated.h"

UENUM(BlueprintType)
enum class EPartyFollowRole : uint8
{
	None,
	Leader,
	Follower
};

UENUM(BlueprintType)
enum class EPartyFollowSide : uint8
{
	Center,
	Left,
	Right
};

UENUM(BlueprintType)
enum class EPartyFollowState : uint8
{
	Following,
	/** Leader is idle and the follower is already inside an acceptable rest area. */
	Settled,
	Waiting,
	Avoiding
};

UENUM(BlueprintType)
enum class EPartyFollowTargetMode : uint8
{
	Move,
	Hold,
	Avoid
};

USTRUCT(BlueprintType)
struct RPGAI_API FPartyTrajectoryPoint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Party Follow")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Party Follow")
	FVector Tangent = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Party Follow")
	float SegmentLength = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Party Follow")
	float AccumulatedDistance = 0.0f;
};

USTRUCT(BlueprintType)
struct RPGAI_API FPartyFollowTarget
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Party Follow")
	EPartyFollowTargetMode Mode = EPartyFollowTargetMode::Hold;

	/** Final navigation-projected location that the BT task should move to. */
	UPROPERTY(BlueprintReadOnly, Category = "Party Follow")
	FVector Location = FVector::ZeroVector;

	/** Un-offset point sampled from the leader trajectory. */
	UPROPERTY(BlueprintReadOnly, Category = "Party Follow")
	FVector TrajectoryLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Party Follow")
	FVector TrajectoryTangent = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Party Follow")
	float AppliedLateralOffset = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Party Follow")
	bool bIsValid = false;
};

/**
 * Records a recent trajectory while acting as the party leader and provides
 * navigation-projected follow targets while acting as a follower.
 *
 * This component never moves its owner. An AI task is expected to periodically
 * call GetFollowTarget and pass the returned location to the path following
 * system.
 */
UCLASS(ClassGroup = (Party), meta = (BlueprintSpawnableComponent))
class RPGAI_API UPartyFollowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPartyFollowComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Makes this character the trajectory source and starts a fresh trajectory. */
	UFUNCTION(BlueprintCallable, Category = "Party Follow")
	void SetAsLeader();

	/**
	 * Makes this character follow InLeaderActor.
	 * FollowOrder is stable within the party and determines the initial side and
	 * longitudinal spacing preference. When bInWaitForFormationActivation is
	 * true, a nearby follower keeps its current location until the leader has
	 * physically moved far enough to activate the newly assigned formation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Party Follow")
	bool SetAsFollower(
		const AActor* InLeaderActor,
		int32 InFollowOrder,
		bool bInWaitForFormationActivation = false);

	UFUNCTION(BlueprintCallable, Category = "Party Follow")
	void DisablePartyFollow();

	/** Clears the current leader trajectory and seeds it at the owner location. */
	UFUNCTION(BlueprintCallable, Category = "Party Follow")
	void ResetTrajectory();

	/** Returns a target command only. This function never requests or applies movement. */
	UFUNCTION(BlueprintCallable, Category = "Party Follow")
	bool GetFollowTarget(FPartyFollowTarget& OutTarget);

	UFUNCTION(BlueprintPure, Category = "Party Follow")
	EPartyFollowRole GetFollowRole() const { return FollowRole; }

	UFUNCTION(BlueprintPure, Category = "Party Follow")
	EPartyFollowSide GetPreferredSide() const { return PreferredSide; }

	/** Sets the stable initial side used until a close leader pass forces reacquisition. */
	UFUNCTION(BlueprintCallable, Category = "Party Follow")
	void SetPreferredSide(EPartyFollowSide InPreferredSide);

	UFUNCTION(BlueprintPure, Category = "Party Follow")
	EPartyFollowState GetFollowState() const { return FollowState; }

	UFUNCTION(BlueprintPure, Category = "Party Follow")
	float GetDesiredFollowDistance() const;

	const TArray<FPartyTrajectoryPoint>& GetTrajectoryPoints() const
	{
		return TrajectoryPoints;
	}

protected:
	void TickLeader(float DeltaTime);
	void RecordTrajectoryPoint(const FVector& Location);
	void TrimTrajectory();
	void InitializeFollowPreference(int32 InFollowOrder);
	void UpdateFollowerState(float DistanceToLeader, bool bLeaderIsStationary);
	void SetFollowState(EPartyFollowState NewState);
	bool IsLeaderStationary() const;
	bool HasFormationActivated() const;
	bool TryBuildHoldTarget(FPartyFollowTarget& OutTarget) const;
	bool IsInsideSettledArea(
		const FVector& FollowTarget,
		const FVector& TrajectoryTangent,
		float DistanceToLeader) const;

	EPartyFollowSide RequestSideClaim(
		UPartyFollowComponent* Requester,
		EPartyFollowSide DesiredSide);
	void ReleaseSideClaim(UPartyFollowComponent* Requester);
	void ReleaseLeaderSideClaim();
	void RemoveInvalidSideClaims();

	bool TryGetLeaderTrajectoryLocation(FVector& OutLocation) const;
	bool BuildAvoidanceTarget(FVector& OutLocation) const;

	bool FindTrajectoryPointByDistance(
		float DistanceBehindLeader,
		FVector& OutLocation,
		FVector& OutTangent) const;

	bool FindBestLateralLocation(
		const FVector& TrajectoryLocation,
		const FVector& TrajectoryTangent,
		FVector& OutLocation,
		float& OutAppliedOffset);

	float ResolveReacquiredSideSign(
		const FVector& TrajectoryLocation,
		const FVector& TrajectoryTangent);

	bool ProjectCandidateToNavigation(
		const FVector& Candidate,
		FVector& OutProjectedLocation) const;

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Party Follow")
	EPartyFollowRole FollowRole = EPartyFollowRole::None;

	UPROPERTY(Transient)
	TWeakObjectPtr<UPartyFollowComponent> LeaderComponent;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Party Follow")
	int32 FollowOrder = INDEX_NONE;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Party Follow")
	EPartyFollowSide PreferredSide = EPartyFollowSide::Center;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Party Follow")
	EPartyFollowState FollowState = EPartyFollowState::Waiting;

	/** Semantic side reserved on the leader. It may remain reserved while the actual offset is compressed. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Party Follow")
	EPartyFollowSide ClaimedSide = EPartyFollowSide::Center;

	/** Current semantic side along the sampled trajectory: -1 left, +1 right. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Party Follow")
	float CurrentLateralSideSign = 0.0f;

	UPROPERTY(Transient)
	bool bNeedsReacquireTarget = true;

	UPROPERTY(Transient)
	TWeakObjectPtr<UPartyFollowComponent> LeftSideClaim;

	UPROPERTY(Transient)
	TWeakObjectPtr<UPartyFollowComponent> RightSideClaim;

	UPROPERTY(Transient)
	bool bLeaderConsideredMoving = true;

	UPROPERTY(Transient)
	float LastLeaderMovementTime = 0.0f;

	/** Leader position from which movement activates a newly assigned formation. */
	UPROPERTY(Transient)
	FVector FormationActivationOrigin = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Party Follow")
	bool bFormationActivated = false;

	/** Used after party construction or a controlled-character handoff. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Party Follow")
	bool bWaitForFormationActivation = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Party Follow")
	TArray<FPartyTrajectoryPoint> TrajectoryPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Trajectory", meta = (ClampMin = "1.0"))
	float TrajectorySampleDistance = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Trajectory", meta = (ClampMin = "100.0"))
	float MaxTrajectoryLength = 2000.0f;

	/** A movement jump larger than this starts a new trajectory instead of connecting both positions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Trajectory", meta = (ClampMin = "100.0"))
	float TrajectoryResetDistance = 800.0f;

	/** Leader locations are projected to the ground before being stored in the trajectory. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Trajectory")
	bool bProjectLeaderTrajectoryToNavigation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Trajectory", meta = (ClampMin = "1.0"))
	FVector LeaderTrajectoryProjectionExtent = FVector(50.0f, 50.0f, 500.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Formation", meta = (ClampMin = "0.0"))
	float BaseFollowDistance = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Formation", meta = (ClampMin = "0.0"))
	float FollowDistanceStep = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Formation", meta = (ClampMin = "0.0"))
	float PreferredLateralDistance = 70.0f;

	/** When both sides are nearly equal, the initial PreferredSide wins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Formation", meta = (ClampMin = "0.0"))
	float SideSelectionTieThreshold = 15.0f;

	/** Leader movement required before a newly assigned formation becomes active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Formation Activation", meta = (ClampMin = "0.0"))
	float FormationActivationMoveDistance = 30.0f;

	/** Nearby followers hold until activation; distant followers may catch up immediately. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Formation Activation", meta = (ClampMin = "0.0"))
	float FormationActivationHoldMaxDistance = 340.0f;

	/** Speed below this value starts the leader idle grace period. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Settling", meta = (ClampMin = "0.0"))
	float LeaderStopSpeedThreshold = 10.0f;

	/** A settled leader becomes moving again once it reaches this speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Settling", meta = (ClampMin = "0.0"))
	float LeaderResumeSpeedThreshold = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Settling", meta = (ClampMin = "0.0"))
	float LeaderSettleDelay = 0.35f;

	/** Allowed error along the trajectory before an idle follower must correct its position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Settling", meta = (ClampMin = "1.0"))
	float IdleLongitudinalTolerance = 90.0f;

	/** Allowed error across the trajectory before an idle follower must correct its position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Settling", meta = (ClampMin = "1.0"))
	float IdleLateralTolerance = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Settling", meta = (ClampMin = "0.0"))
	float IdleMinLeaderDistance = 145.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Settling", meta = (ClampMin = "0.0"))
	float IdleMaxLeaderDistance = 340.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Distance", meta = (ClampMin = "0.0"))
	float AvoidanceEnterDistance = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Distance", meta = (ClampMin = "0.0"))
	float AvoidanceExitDistance = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Distance", meta = (ClampMin = "0.0"))
	float WaitingEnterDistance = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Distance", meta = (ClampMin = "0.0"))
	float FollowResumeDistance = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Navigation")
	bool bProjectTargetsToNavigation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Navigation", meta = (ClampMin = "1.0"))
	FVector NavigationProjectionExtent = FVector(75.0f, 75.0f, 150.0f);

	/** Rejects a projection if NavMesh moves the candidate too far from its intended location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Party Follow|Navigation", meta = (ClampMin = "0.0"))
	float MaxNavigationProjectionAdjustment = 100.0f;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Party Follow|Debug")
	bool bDrawDebug = false;
#endif
};

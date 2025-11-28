// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FollowLocationTracker.generated.h"

UENUM(BlueprintType)
enum class EFollowState : uint8
{
	Waiting, // Waiting for leader start moving

	Invisible, // Leader is not visible

	Leading, // Follower is leading the group, i.e. follower is in front of the leader

	Following, // Follower is following the leader

	Returning // Follower is returning to the leader after being too far away
};

USTRUCT(BlueprintType)
struct FFollowLocation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	bool bOccupied = false;

	UPROPERTY(BlueprintReadOnly)
	bool bCannotFollow = false;

	UPROPERTY(BlueprintReadOnly)
	float DistanceToLeader = 0.f;

	UPROPERTY()
	TWeakObjectPtr<const AController> OccupiedBy = nullptr;

	FFollowLocation() = default;

	FFollowLocation(const FVector& InLocation, bool bInOccupied = false, float InDistanceToLeader = 0.f)
		: Location(InLocation), bOccupied(bInOccupied), DistanceToLeader(InDistanceToLeader) {}

	void Occupy(const AController* FollowerController)
	{
		bOccupied = true;
		OccupiedBy = FollowerController;
	}

	void Unoccupy()
	{
		bOccupied = false;
		OccupiedBy.Reset();
	}

	void Disable()
	{
		Unoccupy();
		bCannotFollow = true;
	}

	bool HasController() const
	{
		return OccupiedBy != nullptr;
	}

};

/**
 * 
 */
UCLASS(notplaceable)
class RPGAI_API AFollowLocationTracker : public AActor
{
	GENERATED_BODY()

public:
	AFollowLocationTracker();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Follow Location Tracker")
	int32 MaxLocations = 8;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Follow Location Tracker")
	float InnerRadius = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Follow Location Tracker")
	float PreferredRadius = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Follow Location Tracker")
	float OuterRadius = 300.0f;

public:
	UPROPERTY(BlueprintReadOnly, Category = "Follow Location Tracker")
	TArray<FFollowLocation> Locations;

	UPROPERTY(BlueprintReadOnly, Category = "Follow Location Tracker")
	TWeakObjectPtr<AController> LeaderController = nullptr;

public:
	UFUNCTION(BlueprintCallable, Category = "Follow Location Tracker")
	FVector FindNearestLocationAndOccupy(const AController* FollowerController);

	FFollowLocation* FindLocationByController(const AController* FollowerController);

protected:
	bool IsInFrontOfLeader(const AController* FollowerController, float ThresholdDegrees) const;

#if WITH_EDITORONLY_DATA
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bDrawDebug = false;

#endif // WITH_EDITORONLY_DATA

};

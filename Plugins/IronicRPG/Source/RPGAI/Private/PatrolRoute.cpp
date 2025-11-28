// Copyright Ironic Studio. All Rights Reserved.


#include "PatrolRoute.h"
#include "Components/DirectionalSplineComponent.h"
#include "AIDataTypes.h"

#include "Components/BillboardComponent.h"

// Sets default values
APatrolRoute::APatrolRoute(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	RouteComp = ObjectInitializer.CreateDefaultSubobject<UDirectionalSplineComponent>(this, TEXT("Patrol Route"));

#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectTransacted.AddUObject(this, &APatrolRoute::OnObjectTransacted);
	UpdatePatrolDataArray();
#endif // WITH_EDITOR

}

// Called when the game starts or when spawned
void APatrolRoute::BeginPlay()
{
	Super::BeginPlay();

}

void APatrolRoute::BeginDestroy()
{
	Super::BeginDestroy();

#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectTransacted.RemoveAll(this);
#endif // WITH_EDITOR
}

FVector APatrolRoute::GetLocationByPercent(float Percent) const
{
	if (RouteComp)
	{
		return RouteComp->GetWorldLocationAtDistanceAlongSpline(RouteComp->GetSplineLength() * Percent);
	}

	return FVector::ZeroVector;
}

#if WITH_EDITOR
void APatrolRoute::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedChainEvent);

	const FName PropertyName = PropertyChangedChainEvent.MemberProperty->GetFName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(APatrolRoute, PatrolData))
	{
		const uint32 CurrentIndex = PropertyChangedChainEvent.GetArrayIndex(PropertyChangedChainEvent.PropertyChain.GetActiveMemberNode()->GetValue()->GetName());

		
	}
}

void APatrolRoute::OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event)
{
	if (RouteComp && RouteComp == Object)
	{
		UpdatePatrolDataArray();
	}
}

void APatrolRoute::UpdatePatrolDataArray()
{
	if (!RouteComp)
	{
		return;
	}

	const int32 NumPoints = RouteComp->GetNumberOfSplinePoints();
	const int32 OldNum = PatrolData.Num();

	PatrolData.SetNum(NumPoints);

	if (NumPoints > OldNum)
	{
		for (int32 i = OldNum; i < NumPoints; i++)
		{
			// New entries are not actor data by default
			PatrolData[i].bIsActorData = false;
		}
	}
}
#endif // WITH_EDITOR
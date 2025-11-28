// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PatrolRoute.generated.h"

UCLASS(Blueprintable, ClassGroup = RPG, hidecategories = Collision)
class RPGAI_API APatrolRoute : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APatrolRoute(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Patrol Route")
	class UDirectionalSplineComponent* RouteComp;

	// Patrol data array corresponding to spline points.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, EditFixedSize, Category = "Patrol Route")
	TArray<struct FPatrolData> PatrolData;

public:
	UFUNCTION(BlueprintCallable, Category = "Patrol Route")
	FVector GetLocationByPercent(float Percent) const;

	UFUNCTION(BlueprintCallable, Category = "Patrol Route")
	class UDirectionalSplineComponent* GetRouteComponent() const { return RouteComp; }

	UFUNCTION(BlueprintCallable, Category = "Patrol Route")
	const TArray<struct FPatrolData>& GetPatrolDataArray() const { return PatrolData; }

#if WITH_EDITOR
protected:
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent) override;

	void OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event);

	virtual void UpdatePatrolDataArray();
#endif // WITH_EDITOR

};

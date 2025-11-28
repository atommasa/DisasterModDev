// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DataTypes/RPGId.h"
#include "SpawnablePoint.generated.h"

/**
 * An abstract actor for spawnable points in the game world.
 */
UCLASS(Abstract, ClassGroup = RPG, hidecategories = Collision)
class RPGCORE_API ASpawnablePoint : public AActor
{
	GENERATED_BODY()

public:
	ASpawnablePoint(const FObjectInitializer& ObjectInitializer);

public:
	// Awaken the spawn point (e.g., spawn the character)
	virtual void Awaken() PURE_VIRTUAL(ASpawnablePoint::Awaken, );

protected:
	// Unique identifier for this spawn point.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn Point", meta=(DisplayPriority = 10))
	FGuid SpawnGuid = FGuid::NewGuid();

	// Whether to spawn on level loaded
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawn Point", meta=(DisplayPriority = 50))
	bool bSpawnOnLevelLoaded = true;

public:
	// Get the spawn point's unique identifier
	UFUNCTION(BlueprintCallable, Category = "Spawnable Point")
	virtual FGuid GetSpawnGuid() const { return SpawnGuid; }

	// Can spawn on level loaded
	UFUNCTION(BlueprintCallable, Category = "Spawnable Point")
	virtual bool CanSpawnOnLevelLoaded() const { return bSpawnOnLevelLoaded; }

#if WITH_EDITORONLY_DATA
protected:
	UPROPERTY()
	class USkeletalMeshComponent* PreviewMesh = nullptr;

	UPROPERTY()
	class UCapsuleComponent* PreviewCapsule = nullptr;

	UPROPERTY()
	class UBillboardComponent* SpawnIcon = nullptr;
#endif // WITH_EDITORONLY_DATA
};

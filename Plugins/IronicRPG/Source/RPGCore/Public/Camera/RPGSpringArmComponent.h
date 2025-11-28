// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpringArmComponent.h"
#include "RPGSpringArmComponent.generated.h"

/**
 * 
 */
UCLASS()
class RPGCORE_API URPGSpringArmComponent : public USpringArmComponent
{
	GENERATED_BODY()
	
protected:
	virtual void UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag, bool bDoRotationLag, float DeltaTime) override;

public:
	UFUNCTION(BlueprintCallable)
	virtual void CameraZoom(float Scale);

	UPROPERTY(BlueprintReadWrite, Category = "Camera")
	FVector CurrentEndLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float ZoomRate = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float MinZoomLength = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float MaxZoomLength = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Collision")
	float CollisionSpeed = 5.0f;
};

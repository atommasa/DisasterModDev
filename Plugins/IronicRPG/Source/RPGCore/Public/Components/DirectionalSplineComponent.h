// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "DirectionalSplineComponent.generated.h"

/**
 * 
 */
UCLASS(ClassGroup = RPG)
class RPGCORE_API UDirectionalSplineComponent : public USplineComponent
{
	GENERATED_BODY()

protected:
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	static void Draw(
		FPrimitiveDrawInterface* PDI,
		const FSceneView* View,
		const FInterpCurveVector& SplineInfo,
		const FMatrix& LocalToWorld,
		const FLinearColor& LineColor,
		uint8 DepthPriorityGroup
	);

};

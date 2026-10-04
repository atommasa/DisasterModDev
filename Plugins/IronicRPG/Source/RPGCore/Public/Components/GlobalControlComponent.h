// Copyright Ironic Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Controllers/BaseControlComponent.h"
#include "GlobalControlComponent.generated.h"

/**
 * 
 */
UCLASS(ClassGroup = (ControlComponents), HideCategories = Control, meta=(BlueprintSpawnableComponent) )
class RPGCORE_API UGlobalControlComponent : public UBaseControlComponent
{
	GENERATED_BODY()

protected:
	virtual void OnControlModeChanged(const int32& NewControlMode) override;

};

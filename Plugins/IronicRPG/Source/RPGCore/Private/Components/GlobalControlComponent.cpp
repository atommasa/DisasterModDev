// Copyright Ironic Studio. All Rights Reserved.


#include "Components/GlobalControlComponent.h"

void UGlobalControlComponent::OnControlModeChanged(const int32& NewControlMode)
{
	NewControlMode != 0 ? EnableAllInputs() : DisableAllInputs();
}
